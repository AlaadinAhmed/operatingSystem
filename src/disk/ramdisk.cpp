#include "disk/ramdisk.h"
#include "print/print.h"
#include "memory/kmalloc.h"

namespace fs {

// Global ramdisk instance
RamDisk g_ramdisk;

RamDisk::RamDisk() 
    : m_base(nullptr), m_size(0), m_partition_offset(0), m_partition_size(0) {
}

void RamDisk::Init(uint8_t* base, size_t size) {
    m_base = base;
    m_size = size;
    m_partition_offset = 0;
    m_partition_size = size;
    kprintf("RamDisk: Initialized at %p, size %d bytes\n", base, (int)size);
}

int RamDisk::read_sector(uint32_t lba, uint8_t* buffer) {
    if (!m_base) return 1;
    
    uint64_t offset = (uint64_t)lba * 512;
    if (offset + 512 > m_size) {
        return 1;  // Out of bounds
    }
    
    memcpy(buffer, m_base + offset, 512);
    return 0;
}

int RamDisk::read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count) {
    if (!m_base) return 1;
    
    uint64_t offset = (uint64_t)lba * 512;
    uint64_t bytes = (uint64_t)count * 512;
    
    if (offset + bytes > m_size) {
        return 1;  // Out of bounds
    }
    
    memcpy(buffer, m_base + offset, bytes);
    return 0;
}

int RamDisk::write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!m_base) return 1;
    
    uint64_t offset = (uint64_t)lba * 512;
    if (offset + 512 > m_size) {
        return 1;
    }
    
    memcpy(m_base + offset, buffer, 512);
    return 0;
}

void init_ramdisk(uint8_t* base, size_t size) {
    g_ramdisk.Init(base, size);
}

} // namespace fs
