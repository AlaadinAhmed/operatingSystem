#include "disk/nvme_disk.h"

#include "drivers/bus/pci.h"
#include "disk/disk.h"
#include "memory/kmalloc.h"
#include "print/print.h"

namespace fs {

struct NvmeDisk::NvmeCommand {
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsvd2;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));

struct NvmeDisk::NvmeCompletion {
    uint32_t result;
    uint32_t rsvd1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed));

static constexpr uint8_t kNvmeAdminIdentify = 0x06;
static constexpr uint8_t kNvmeAdminCreateIoCompletionQueue = 0x05;
static constexpr uint8_t kNvmeAdminCreateIoSubmissionQueue = 0x01;
static constexpr uint8_t kNvmeIoRead = 0x02;
static constexpr uint8_t kNvmeIoWrite = 0x01;
static constexpr uint32_t kNvmeQueueEntries = 32;
static constexpr uint32_t kNvmeMaxTransferBytes = 4096;
static constexpr uint32_t kNvmeMpsBytes = 4096;

alignas(4096) static NvmeDisk::NvmeCommand g_admin_sq[kNvmeQueueEntries];
alignas(4096) static NvmeDisk::NvmeCompletion g_admin_cq[kNvmeQueueEntries];
alignas(4096) static NvmeDisk::NvmeCommand g_io_sq[kNvmeQueueEntries];
alignas(4096) static NvmeDisk::NvmeCompletion g_io_cq[kNvmeQueueEntries];
alignas(4096) static uint8_t g_admin_data[4096];
alignas(4096) static uint8_t g_io_bounce[4096];

NvmeDisk g_nvme_disk;

NvmeDisk::NvmeDisk()
    : m_partition_offset(0), m_partition_size(0), m_mmio_base(0), m_cap_mqes(0),
      m_dstrd(0), m_queue_depth(0), m_namespace_id(0), m_namespace_blocks(0),
      m_native_block_size(512), m_admin_cq_head(0), m_admin_sq_tail(0),
      m_admin_cq_phase(1), m_io_cq_head(0), m_io_sq_tail(0), m_io_cq_phase(1),
      m_next_cid(1), m_initialized(false) {}

uint32_t NvmeDisk::read32(uint32_t offset) const {
    return *(volatile uint32_t*)(m_mmio_base + offset);
}

uint64_t NvmeDisk::read64(uint32_t offset) const {
    return *(volatile uint64_t*)(m_mmio_base + offset);
}

void NvmeDisk::write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(m_mmio_base + offset) = value;
}

void NvmeDisk::write64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(m_mmio_base + offset) = value;
}

uint32_t NvmeDisk::doorbell_offset(uint32_t qid, bool completion) const {
    uint32_t stride = 1u << m_dstrd;
    uint32_t doorbell_index = qid * 2u + (completion ? 1u : 0u);
    return 0x1000u + doorbell_index * (4u * stride);
}

bool NvmeDisk::wait_for_ready(bool ready) const {
    for (uint32_t i = 0; i < 1000000; ++i) {
        bool current = (read32(0x1C) & 0x1) != 0;
        if (current == ready) {
            return true;
        }
    }
    return false;
}

bool NvmeDisk::submit_admin_command(const NvmeCommand& cmd, uint8_t* data, size_t data_size) {
    if (!m_mmio_base || !m_queue_depth) {
        return false;
    }

    uint16_t cid = cmd.cid;
    uint16_t submit_tail = m_admin_sq_tail;
    g_admin_sq[submit_tail] = cmd;
    m_admin_sq_tail = (m_admin_sq_tail + 1) % m_queue_depth;
    __asm__ volatile("mfence" ::: "memory");
    write32(doorbell_offset(0, false), m_admin_sq_tail);

    for (uint32_t i = 0; i < 1000000; ++i) {
        volatile NvmeCompletion* cqe = &g_admin_cq[m_admin_cq_head];
        uint16_t status = cqe->status;
        if ((status & 1u) != (m_admin_cq_phase & 1u)) {
            continue;
        }

        if (cqe->cid != cid) {
            continue;
        }

        uint16_t status_code = (status >> 1) & 0x7FFFu;
        if (status_code != 0) {
            kprintf("NVMe: admin command failed: %x\n", status_code);
            return false;
        }

        if (data && data_size > 0) {
            (void)data;
            (void)data_size;
        }

        m_admin_cq_head = (m_admin_cq_head + 1) % m_queue_depth;
        if (m_admin_cq_head == 0) {
            m_admin_cq_phase ^= 1;
        }
        write32(doorbell_offset(0, true), m_admin_cq_head);
        return true;
    }

    for (uint16_t i = 0; i < m_queue_depth; ++i) {
        volatile NvmeCompletion* cqe = &g_admin_cq[i];
        uint16_t status = cqe->status;
        if (cqe->cid == cid && ((status >> 1) & 0x7FFFu) == 0) {
            m_admin_cq_head = (i + 1) % m_queue_depth;
            write32(doorbell_offset(0, true), m_admin_cq_head);
            return true;
        }
    }

    return false;
}

bool NvmeDisk::submit_io_read(uint64_t slba, uint16_t nlb, void* buffer) {
    NvmeCommand cmd = {};
    cmd.opcode = kNvmeIoRead;
    cmd.cid = m_next_cid++;
    cmd.nsid = m_namespace_id;
    cmd.prp1 = (uint64_t)buffer;
    cmd.cdw10 = (uint32_t)(slba & 0xFFFFFFFFu);
    cmd.cdw11 = (uint32_t)(slba >> 32);
    cmd.cdw12 = nlb;

    uint16_t submit_tail = m_io_sq_tail;
    g_io_sq[submit_tail] = cmd;
    m_io_sq_tail = (m_io_sq_tail + 1) % m_queue_depth;
    __asm__ volatile("mfence" ::: "memory");
    write32(doorbell_offset(1, false), m_io_sq_tail);

    for (uint32_t i = 0; i < 1000000; ++i) {
        volatile NvmeCompletion* cqe = &g_io_cq[m_io_cq_head];
        uint16_t status = cqe->status;
        if ((status & 1u) != (m_io_cq_phase & 1u)) {
            continue;
        }

        if (cqe->cid != cmd.cid) {
            continue;
        }

        uint16_t status_code = (status >> 1) & 0x7FFFu;
        if (status_code != 0) {
            kprintf("NVMe: read failed: %x\n", status_code);
            return false;
        }

        m_io_cq_head = (m_io_cq_head + 1) % m_queue_depth;
        if (m_io_cq_head == 0) {
            m_io_cq_phase ^= 1;
        }
        write32(doorbell_offset(1, true), m_io_cq_head);
        return true;
    }

    for (uint16_t i = 0; i < m_queue_depth; ++i) {
        volatile NvmeCompletion* cqe = &g_io_cq[i];
        uint16_t status = cqe->status;
        if (cqe->cid == cmd.cid && ((status >> 1) & 0x7FFFu) == 0) {
            m_io_cq_head = (i + 1) % m_queue_depth;
            write32(doorbell_offset(1, true), m_io_cq_head);
            return true;
        }
    }

    return false;
}

bool NvmeDisk::submit_io_write(uint64_t slba, uint16_t nlb, const void* buffer) {
    NvmeCommand cmd = {};
    cmd.opcode = kNvmeIoWrite;
    cmd.cid = m_next_cid++;
    cmd.nsid = m_namespace_id;
    cmd.prp1 = (uint64_t)buffer;
    cmd.cdw10 = (uint32_t)(slba & 0xFFFFFFFFu);
    cmd.cdw11 = (uint32_t)(slba >> 32);
    cmd.cdw12 = nlb;

    uint16_t submit_tail = m_io_sq_tail;
    g_io_sq[submit_tail] = cmd;
    m_io_sq_tail = (m_io_sq_tail + 1) % m_queue_depth;
    __asm__ volatile("mfence" ::: "memory");
    write32(doorbell_offset(1, false), m_io_sq_tail);

    for (uint32_t i = 0; i < 1000000; ++i) {
        volatile NvmeCompletion* cqe = &g_io_cq[m_io_cq_head];
        uint16_t status = cqe->status;
        if ((status & 1u) != (m_io_cq_phase & 1u)) {
            continue;
        }

        if (cqe->cid != cmd.cid) {
            continue;
        }

        uint16_t status_code = (status >> 1) & 0x7FFFu;
        if (status_code != 0) {
            kprintf("NVMe: write failed: %x\n", status_code);
            return false;
        }

        m_io_cq_head = (m_io_cq_head + 1) % m_queue_depth;
        if (m_io_cq_head == 0) {
            m_io_cq_phase ^= 1;
        }
        write32(doorbell_offset(1, true), m_io_cq_head);
        return true;
    }

    for (uint16_t i = 0; i < m_queue_depth; ++i) {
        volatile NvmeCompletion* cqe = &g_io_cq[i];
        uint16_t status = cqe->status;
        if (cqe->cid == cmd.cid && ((status >> 1) & 0x7FFFu) == 0) {
            m_io_cq_head = (i + 1) % m_queue_depth;
            write32(doorbell_offset(1, true), m_io_cq_head);
            return true;
        }
    }

    return false;
}

bool NvmeDisk::read_native_block(uint64_t native_lba, uint8_t* buffer) {
    if (!m_initialized || native_lba >= m_namespace_blocks) {
        return false;
    }
    return submit_io_read(native_lba, 0, buffer);
}

bool NvmeDisk::write_native_block(uint64_t native_lba, const uint8_t* buffer) {
    if (!m_initialized || native_lba >= m_namespace_blocks) {
        return false;
    }
    return submit_io_write(native_lba, 0, buffer);
}

bool NvmeDisk::identify_controller() {
    NvmeCommand cmd = {};
    cmd.opcode = kNvmeAdminIdentify;
    cmd.cid = m_next_cid++;
    cmd.nsid = 0;
    cmd.prp1 = (uint64_t)g_admin_data;
    cmd.cdw10 = 1; // CNS = Identify Controller
    return submit_admin_command(cmd, g_admin_data, sizeof(g_admin_data));
}

bool NvmeDisk::identify_namespace() {
    NvmeCommand cmd = {};
    cmd.opcode = kNvmeAdminIdentify;
    cmd.cid = m_next_cid++;
    cmd.nsid = m_namespace_id;
    cmd.prp1 = (uint64_t)g_admin_data;
    cmd.cdw10 = 0; // CNS = Identify Namespace
    return submit_admin_command(cmd, g_admin_data, sizeof(g_admin_data));
}

bool NvmeDisk::create_io_queues() {
    NvmeCommand cq_cmd = {};
    cq_cmd.opcode = kNvmeAdminCreateIoCompletionQueue;
    cq_cmd.cid = m_next_cid++;
    cq_cmd.prp1 = (uint64_t)g_io_cq;
    cq_cmd.cdw10 = ((uint32_t)(m_queue_depth - 1) << 16) | 1u;
    cq_cmd.cdw11 = 1u; // physically contiguous
    if (!submit_admin_command(cq_cmd, nullptr, 0)) {
        return false;
    }

    NvmeCommand sq_cmd = {};
    sq_cmd.opcode = kNvmeAdminCreateIoSubmissionQueue;
    sq_cmd.cid = m_next_cid++;
    sq_cmd.prp1 = (uint64_t)g_io_sq;
    sq_cmd.cdw10 = ((uint32_t)(m_queue_depth - 1) << 16) | 1u;
    sq_cmd.cdw11 = 1u | (1u << 16); // physically contiguous + CQID 1
    return submit_admin_command(sq_cmd, nullptr, 0);
}

bool NvmeDisk::Initialize(uint8_t bus, uint8_t device, uint8_t function) {
    kprintf("NVMe: probing PCI %d:%d.%d\n", bus, device, function);
    uint32_t vendor = pci_read(bus, device, function, 0x00) & 0xFFFF;
    if (vendor == 0xFFFF) {
        kprintf("NVMe: no vendor at PCI address\n");
        return false;
    }

    uint32_t class_reg = pci_read(bus, device, function, 0x08);
    uint8_t class_code = (class_reg >> 24) & 0xFF;
    uint8_t subclass = (class_reg >> 16) & 0xFF;
    if (class_code != 0x01 || subclass != 0x08) {
        return false;
    }

    uint32_t command = pci_read(bus, device, function, 0x04);
    pci_write(bus, device, function, 0x04, command | 0x6u);

    uint32_t bar0 = pci_read(bus, device, function, 0x10);
    uint32_t bar1 = pci_read(bus, device, function, 0x14);
    m_mmio_base = ((uint64_t)bar1 << 32) | (bar0 & ~0xFu);
    if (!m_mmio_base) {
        kprintf("NVMe: BAR0/1 produced null MMIO base\n");
        return false;
    }

    kprintf("NVMe: MMIO base 0x%lx\n", m_mmio_base);

    uint64_t cap = read64(0x00);
    m_cap_mqes = cap & 0xFFFFu;
    m_dstrd = (cap >> 32) & 0xFu;
    m_queue_depth = (m_cap_mqes + 1u < kNvmeQueueEntries) ? (uint16_t)(m_cap_mqes + 1u) : (uint16_t)kNvmeQueueEntries;
    if (m_queue_depth < 2) {
        kprintf("NVMe: invalid queue depth %u\n", m_queue_depth);
        return false;
    }
    kprintf("NVMe: CAP mqes=%d dstrd=%d queue_depth=%d\n", (int)m_cap_mqes, (int)m_dstrd, (int)m_queue_depth);

    if (read32(0x1C) & 0x1) {
        kprintf("NVMe: disabling controller for reset\n");
        write32(0x14, 0);
        if (!wait_for_ready(false)) {
            kprintf("NVMe: controller did not clear RDY\n");
            return false;
        }
    }

    kprintf("NVMe: programming admin queues\n");
    write32(0x24, ((m_queue_depth - 1u) << 16) | (m_queue_depth - 1u));
    write64(0x28, (uint64_t)g_admin_sq);
    write64(0x30, (uint64_t)g_admin_cq);

    uint32_t cc = 0;
    cc |= 1u; // EN
    cc |= 0u << 4; // CSS = NVM command set
    cc |= 0u << 7; // MPS = 4K
    cc |= 6u << 16; // IOSQES = 64 bytes
    cc |= 4u << 20; // IOCQES = 16 bytes
    write32(0x14, cc);
    if (!wait_for_ready(true)) {
        kprintf("NVMe: controller did not set RDY\n");
        return false;
    }

    kprintf("NVMe: issuing identify controller\n");
    if (!identify_controller()) {
        kprintf("NVMe: identify controller failed\n");
        return false;
    }

    uint32_t nn = *(uint32_t*)(g_admin_data + 0x4C);
    if (nn == 0) {
        kprintf("NVMe: controller reports zero namespaces\n");
        return false;
    }

    m_namespace_id = 1;
    kprintf("NVMe: issuing identify namespace %u\n", m_namespace_id);
    if (!identify_namespace()) {
        kprintf("NVMe: identify namespace failed\n");
        return false;
    }

    uint8_t flbas = g_admin_data[0x28];
    uint8_t format = flbas & 0x0F;
    uint32_t lbaf = *(uint32_t*)(g_admin_data + 0x80 + (format * 4u));
    uint8_t lbads = (lbaf >> 16) & 0xFFu;
    m_native_block_size = 1u << lbads;
    if (m_native_block_size < 512 || m_native_block_size > 4096 || (m_native_block_size & (m_native_block_size - 1)) != 0) {
        kprintf("NVMe: unsupported native block size %d\n", (int)m_native_block_size);
        return false;
    }

    uint64_t nsze = *(uint64_t*)(g_admin_data + 0x00);
    m_namespace_blocks = nsze;
    if (m_namespace_blocks == 0) {
        kprintf("NVMe: namespace size is zero\n");
        return false;
    }

    kprintf("NVMe: namespace blocks=%lx native_block_size=%d\n", m_namespace_blocks, (int)m_native_block_size);

    kprintf("NVMe: creating IO queues\n");
    if (!create_io_queues()) {
        kprintf("NVMe: create IO queues failed\n");
        return false;
    }

    m_initialized = true;
    kprintf("NVMe: controller ready, block size=%d\n", (int)m_native_block_size);
    return true;
}

bool NvmeDisk::detect_partition() {
    uint8_t buffer[512];
    if (read_sector(0, buffer) != 0) {
        return false;
    }

    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        if (read_sector(2, buffer) == 0) {
            Superblock* sb = (Superblock*)buffer;
            if (sb->s_magic == 0xEF53) {
                m_partition_offset = 0;
                m_partition_size = 0;
                return true;
            }
        }
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        uint8_t* entry = buffer + 0x1BE + (i * 16);
        uint8_t type = entry[4];
        uint32_t lba_start = *(uint32_t*)(entry + 8);
        uint32_t sector_count = *(uint32_t*)(entry + 12);

        if (type != 0x83) {
            continue;
        }

        uint8_t sb_buffer[512];
        if (read_sector(lba_start + 2, sb_buffer) == 0) {
            Superblock* sb = (Superblock*)sb_buffer;
            if (sb->s_magic == 0xEF53) {
                m_partition_offset = (uint64_t)lba_start * 512u;
                m_partition_size = (uint64_t)sector_count * 512u;
                return true;
            }
        }
    }

    return false;
}

int NvmeDisk::read_sector(uint32_t lba, uint8_t* buffer) {
    return read_sectors(lba, buffer, 1);
}

int NvmeDisk::read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count) {
    if (!m_initialized || !buffer || count == 0) {
        return 1;
    }

    uint64_t part_offset_sectors = m_partition_offset / 512u;
    uint64_t cursor = (uint64_t)lba + part_offset_sectors;
    uint64_t remaining = (uint64_t)count * 512u;
    uint8_t* out = buffer;

    while (remaining > 0) {
        uint64_t byte_offset = cursor * 512u;
        uint64_t native_lba = byte_offset / m_native_block_size;
        uint32_t native_offset = (uint32_t)(byte_offset % m_native_block_size);
        uint32_t chunk = m_native_block_size - native_offset;
        if (chunk > remaining) {
            chunk = (uint32_t)remaining;
        }

        if (!read_native_block(native_lba, g_io_bounce)) {
            return 1;
        }

        memcpy(out, g_io_bounce + native_offset, chunk);
        out += chunk;
        cursor += chunk / 512u;
        remaining -= chunk;
    }

    return 0;
}

int NvmeDisk::write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!m_initialized || !buffer) {
        return 1;
    }

    uint64_t part_offset_sectors = m_partition_offset / 512u;
    uint64_t cursor = ((uint64_t)lba + part_offset_sectors) * 512u;
    uint64_t remaining = 512u;
    const uint8_t* in = buffer;

    while (remaining > 0) {
        uint64_t native_lba = cursor / m_native_block_size;
        uint32_t native_offset = (uint32_t)(cursor % m_native_block_size);
        uint32_t chunk = m_native_block_size - native_offset;
        if (chunk > remaining) {
            chunk = (uint32_t)remaining;
        }

        if (chunk != m_native_block_size) {
            if (!read_native_block(native_lba, g_io_bounce)) {
                return 1;
            }
            memcpy(g_io_bounce + native_offset, in, chunk);
            if (!write_native_block(native_lba, g_io_bounce)) {
                return 1;
            }
        } else {
            memcpy(g_io_bounce, in, chunk);
            if (!write_native_block(native_lba, g_io_bounce)) {
                return 1;
            }
        }

        cursor += chunk;
        in += chunk;
        remaining -= chunk;
    }

    return 0;
}

} // namespace fs
