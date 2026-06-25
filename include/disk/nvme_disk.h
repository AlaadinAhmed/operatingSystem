#pragma once

#include <cstdint>
#include <stddef.h>

namespace fs {

class NvmeDisk {
public:
    struct NvmeCommand;
    struct NvmeCompletion;

    NvmeDisk();

    bool Initialize(uint8_t bus, uint8_t device, uint8_t function);
    bool IsAvailable() const { return m_initialized; }

    bool detect_partition();
    int read_sector(uint32_t lba, uint8_t* buffer);
    int read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count);
    int write_sector(uint32_t lba, const uint8_t* buffer);

    uint64_t m_partition_offset;
    uint64_t m_partition_size;

private:
    bool submit_admin_command(const NvmeCommand& cmd, uint8_t* data, size_t data_size);
    bool submit_io_read(uint64_t slba, uint16_t nlb, void* buffer);
    bool submit_io_write(uint64_t slba, uint16_t nlb, const void* buffer);
    bool read_native_block(uint64_t native_lba, uint8_t* buffer);
    bool write_native_block(uint64_t native_lba, const uint8_t* buffer);
    bool identify_controller();
    bool identify_namespace();
    bool create_io_queues();
    bool wait_for_ready(bool ready) const;
    uint32_t read32(uint32_t offset) const;
    uint64_t read64(uint32_t offset) const;
    void write32(uint32_t offset, uint32_t value);
    void write64(uint32_t offset, uint64_t value);
    uint32_t doorbell_offset(uint32_t qid, bool completion) const;

    uint64_t m_mmio_base;
    uint32_t m_cap_mqes;
    uint32_t m_dstrd;
    uint16_t m_queue_depth;
    uint32_t m_namespace_id;
    uint64_t m_namespace_blocks;
    uint32_t m_native_block_size;
    uint16_t m_admin_cq_head;
    uint16_t m_admin_sq_tail;
    uint16_t m_admin_cq_phase;
    uint16_t m_io_cq_head;
    uint16_t m_io_sq_tail;
    uint16_t m_io_cq_phase;
    uint16_t m_next_cid;
    bool m_initialized;
};

extern NvmeDisk g_nvme_disk;

} // namespace fs
