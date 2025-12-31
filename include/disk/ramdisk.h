#pragma once
#include <stdint.h>
#include <stddef.h>

namespace fs {

// RamDisk - A block device backed by memory
// Used for initrd/ramdisk loaded via multiboot module
class RamDisk {
public:
    RamDisk();
    
    // Initialize with memory region containing filesystem image
    void Init(uint8_t* base, size_t size);
    
    // Check if ramdisk is available
    bool IsAvailable() const { return m_base != nullptr; }
    
    // Block device operations (512-byte sectors)
    int read_sector(uint32_t lba, uint8_t* buffer);
    int read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count);
    int write_sector(uint32_t lba, const uint8_t* buffer);
    
    // Accessors
    uint8_t* GetBase() const { return m_base; }
    size_t GetSize() const { return m_size; }
    
    // Partition info (for lwext4 compatibility)
    uint64_t m_partition_offset;
    uint64_t m_partition_size;
    
private:
    uint8_t* m_base;
    size_t m_size;
};

// Global ramdisk instance
extern RamDisk g_ramdisk;

// Initialize ramdisk from multiboot module
void init_ramdisk(uint8_t* base, size_t size);

} // namespace fs
