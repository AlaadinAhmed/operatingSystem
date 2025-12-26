#include "disk/disk.h"
#include "print/print.h"

using namespace fs;

// Port bases: Primary IDE = 0x1F0, Secondary IDE = 0x170
static uint16_t get_data_port(uint8_t drive_id) {
    return (drive_id < 2) ? 0x1F0 : 0x170;  // drive 0,1 = primary; 2,3 = secondary
}

static uint16_t get_status_port(uint8_t drive_id) {
    return (drive_id < 2) ? 0x1F7 : 0x177;
}

static int wait_disk_ready(uint8_t drive_id) {
    uint16_t status_port = get_status_port(drive_id);
    int timeout = 5000000;
    while ((inb(status_port) & 0xC0) != 0x40) {
        if (--timeout == 0) {
            return 1;
        }
    }
    return 0;
}

int Ext2Disk::read_sector(uint32_t lba, uint8_t* buffer) {
    uint16_t base = get_data_port(m_drive_id);
    uint8_t slave_bit = (m_drive_id & 1) << 4;  // bit 4 = slave select

    // Select drive and LBA bits 24-27
    outb(base + 6, 0xE0 | slave_bit | ((lba >> 24) & 0x0F));
    for(int k=0; k<1000; k++) inb(base + 7);

    if (wait_disk_ready(m_drive_id)) {
        return 1;
    }
    
    outb(base + 1, 0x00);           // Error/Features
    outb(base + 2, 1);              // Sector count
    outb(base + 3, (uint8_t)lba);   // LBA 0-7
    outb(base + 4, (uint8_t)(lba >> 8));   // LBA 8-15
    outb(base + 5, (uint8_t)(lba >> 16));  // LBA 16-23
    outb(base + 7, 0x20);           // Read Sectors command

    inb(base + 7); inb(base + 7); inb(base + 7); inb(base + 7);

    int timeout = 5000000;
    while (!(inb(base + 7) & 0x08)) {
        if (--timeout == 0) {
            return 1;
        }
    }

    // Read 256 words (512 bytes)
    asm volatile ("rep insw" : "+D"(buffer) : "d"(base), "c"(256) : "memory");
    return 0;
}

int Ext2Disk::read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count) {
    if (count == 0) return 0;
    
    uint16_t base = get_data_port(m_drive_id);
    uint8_t slave_bit = (m_drive_id & 1) << 4;

    // Handle large counts by splitting
    while (count > 255) {
         if (read_sectors(lba, buffer, 255)) return 1;
         lba += 255;
         buffer += 255 * 512;
         count -= 255;
    }

    // Select drive and LBA bits 24-27
    outb(base + 6, 0xE0 | slave_bit | ((lba >> 24) & 0x0F));
    for(int k=0; k<1000; k++) inb(base + 7);

    if (wait_disk_ready(m_drive_id)) {
        return 1;
    }
    
    outb(base + 1, 0x00);
    outb(base + 2, (uint8_t)count);
    outb(base + 3, (uint8_t)lba);
    outb(base + 4, (uint8_t)(lba >> 8));
    outb(base + 5, (uint8_t)(lba >> 16));
    outb(base + 7, 0x20); // Read Sectors

    uint8_t* ptr = buffer;
    for (uint32_t i = 0; i < count; i++) {
        inb(base + 7); inb(base + 7); inb(base + 7); inb(base + 7);

        int timeout = 5000000;
        while (!(inb(base + 7) & 0x08)) {
            if (--timeout == 0) {
                return 1;
            }
        }
        asm volatile ("rep insw" : "+D"(ptr) : "d"(base), "c"(256) : "memory");
    }
    return 0;
}

int Ext2Disk::write_sector(uint32_t lba, const uint8_t* buffer) {
    uint16_t base = get_data_port(m_drive_id);
    uint8_t slave_bit = (m_drive_id & 1) << 4;

    outb(base + 6, 0xE0 | slave_bit | ((lba >> 24) & 0x0F));
    inb(base + 7); inb(base + 7); inb(base + 7); inb(base + 7);

    if (wait_disk_ready(m_drive_id)) {
        return 1;
    }

    outb(base + 1, 0x00);
    outb(base + 2, 1);
    outb(base + 3, (uint8_t)lba);
    outb(base + 4, (uint8_t)(lba >> 8));
    outb(base + 5, (uint8_t)(lba >> 16));
    outb(base + 7, 0x30); // Write Sectors command

    inb(base + 7); inb(base + 7); inb(base + 7); inb(base + 7);

    int timeout = 1000000;
    while (!(inb(base + 7) & 0x08)) {
        uint8_t status = inb(base + 7);
        if (status & 0x01) {
             return 1;
        }
        if (--timeout == 0) {
            return 1;
        }
    }

    asm volatile ("rep outsw" : : "S"(buffer), "d"(base), "c"(256) : "memory");

    // Wait for write to complete
    outb(base + 7, 0xE7); // Cache Flush
    if (wait_disk_ready(m_drive_id)) {
         return 1;
    }
    return 0;
}

Ext2Disk::Ext2Disk(uint8_t drive_id) : m_drive_id(drive_id), m_partition_offset(0), m_partition_size(0) {
}

bool Ext2Disk::detect_partition() {
    uint8_t buffer[512];
    if (read_sector(0, buffer) != 0) return false;

    // Check for MBR signature
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        // No MBR, check if the whole disk is an ext4 filesystem
        if (read_sector(2, buffer) == 0) { // Superblock is at 1024 bytes (Sector 2)
            Superblock* sb = (Superblock*)buffer;
            if (sb->s_magic == 0xEF53) {
                m_partition_offset = 0;
                m_partition_size = 0; // Unknown/Whole disk
                return true;
            }
        }
        return false;
    }

    // Scan partition table (4 entries at 0x1BE)
    for (int i = 0; i < 4; i++) {
        uint8_t* entry = buffer + 0x1BE + (i * 16);
        uint8_t type = entry[4];
        uint32_t lba_start = *(uint32_t*)(entry + 8);
        uint32_t sector_count = *(uint32_t*)(entry + 12);

        if (type == 0x83) { // Linux partition
            // Verify if it's ext4
            uint8_t sb_buffer[512];
            if (read_sector(lba_start + 2, sb_buffer) == 0) {
                Superblock* sb = (Superblock*)sb_buffer;
                if (sb->s_magic == 0xEF53) {
                    m_partition_offset = (uint64_t)lba_start * 512;
                    m_partition_size = (uint64_t)sector_count * 512;
                    kprintf("Found ext4 partition at LBA %d, size %d sectors\n", lba_start, sector_count);
                    return true;
                }
            }
        }
    }

    return false;
}

void Ext2Disk::mount() {
  // ... existing mount logic if needed, but we mostly use lwext4 now
}

