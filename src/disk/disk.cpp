#include "disk/disk.h"
#include "print/print.h"

using namespace fs;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

static int wait_disk_ready() {
    int timeout = 1000000;
    while ((inb(0x1F7) & 0xC0) != 0x40) {
        if (--timeout == 0) return 1;
    }
    return 0;
}

void Ext2Disk::read_sector(uint32_t lba, uint8_t* buffer) {
    // Select drive (Master/Slave) and LBA bits 24-27
    outb(0x1F6, 0xE0 | (m_drive_id << 4) | ((lba >> 24) & 0x0F));
    // Add a 400ns delay by reading the status port 4 times
    for(int k=0; k<1000; k++) inb(0x1F7);

    if (wait_disk_ready()) {
        kprintf("Disk Read Timeout (Ready)! LBA: %d\n", lba);
        return;
    }
    
    // Null byte to port 0x1F1 (Error/Features) - usually not needed but good practice
    outb(0x1F1, 0x00);
    // Sector count
    outb(0x1F2, 1);
    // LBA bits 0-7
    outb(0x1F3, (uint8_t)lba);
    // LBA bits 8-15
    outb(0x1F4, (uint8_t)(lba >> 8));
    // LBA bits 16-23
    outb(0x1F5, (uint8_t)(lba >> 16));
    // Command: Read Sectors with Retry
    outb(0x1F7, 0x20);

    // Add a 400ns delay
    inb(0x1F7); inb(0x1F7); inb(0x1F7); inb(0x1F7);

    // Wait for BSY to clear and DRQ to set
    // Note: This is a very basic poll. In a real OS, use interrupts or timeout.
    int timeout = 1000000; // Increased timeout
    while (!(inb(0x1F7) & 0x08)) {
        if (--timeout == 0) {
            kprintf("Disk Read Timeout! LBA: %d Status: %x\n", lba, inb(0x1F7));
            return;
        }
    }

    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t tmp;
        asm volatile ("inw %1, %0" : "=a"(tmp) : "Nd"((uint16_t)0x1F0));
        ((uint16_t*)buffer)[i] = tmp;
    }
}

void Ext2Disk::write_sector(uint32_t lba, const uint8_t* buffer) {
    outb(0x1F6, 0xE0 | (m_drive_id << 4) | ((lba >> 24) & 0x0F));
    // Add a 400ns delay
    inb(0x1F7); inb(0x1F7); inb(0x1F7); inb(0x1F7);

    if (wait_disk_ready()) {
        kprintf("Disk Write Timeout (Ready)! LBA: %d\n", lba);
        return;
    }

    outb(0x1F1, 0x00);
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // Command: Write Sectors with Retry

    // Add a 400ns delay
    inb(0x1F7); inb(0x1F7); inb(0x1F7); inb(0x1F7);

    int timeout = 1000000;
    while (!(inb(0x1F7) & 0x08)) {
        uint8_t status = inb(0x1F7);
        if (status & 0x01) {
             kprintf("Disk Write Error! LBA: %d Status: %x Error: %x\n", lba, status, inb(0x1F1));
             return;
        }
        if (--timeout == 0) {
            kprintf("Disk Write Timeout! LBA: %d Status: %x\n", lba, status);
            return;
        }
    }

    for (int i = 0; i < 256; i++) {
        uint16_t tmp = ((uint16_t*)buffer)[i];
        asm volatile ("outw %0, %1" : : "a"(tmp), "Nd"((uint16_t)0x1F0));
    }

    // Wait for write to complete and check for errors
    outb(0x1F7, 0xE7); // Cache Flush (optional, but good for data integrity)
    if (wait_disk_ready()) {
         kprintf("Disk Write Timeout (Finish)! LBA: %d\n", lba);
    }
}

Ext2Disk::Ext2Disk(uint8_t drive_id) : m_drive_id(drive_id) {
}

void Ext2Disk::mount() {
  // We loaded Superblock (Sector 3) to 0x1000.
  // So Superblock is at 0x1000.
  Superblock *sb = (Superblock *)(0x1000);
  if (sb->s_magic == 0xEF53) {
    uint32_t total_blocks = sb->s_blocks_count;
    // kprintf("Ext2 File Detected\n");
    // kprintf("Total Blocks: %d\n", total_blocks);
    // kprintf("Inodes Count: %d\n", sb->s_inodes_count);
    
    // Group Descriptor Table follows Superblock.
    // Superblock is 1024 bytes.
    // If we loaded Sector 3 (1024-1535) to 0x1000.
    // Then 0x1000 contains bytes 1024-1535.
    // 0x1200 contains bytes 1536-2047.
    // Group Descriptor starts at byte 2048 (Block 2).
    // That is Sector 5.
    // We read 4 sectors starting from Sector 3.
    // Sector 3 -> 0x1000
    // Sector 4 -> 0x1200
    // Sector 5 -> 0x1400 (Start of GDT)
    
    Ext2GroupDescriptor *bgd = (Ext2GroupDescriptor *)(0x1400);
    // kprintf("Inode Table Block: %d\n", bgd->bg_inode_table);
    // kprintf("Free Blocks: %d\n", bgd->bg_free_blocks_count);
    
    // 1. Read Root Inode (Inode #2)
    // Assuming Block Size is 1024 (s_log_block_size = 0)
    uint32_t block_size = 1024 << sb->s_log_block_size;
    // kprintf("Block Size: %d\n", block_size);
    
    uint32_t sectors_per_block = block_size / 512;
    uint32_t inode_table_lba = bgd->bg_inode_table * sectors_per_block;
    // kprintf("Reading Inode Table at LBA: %d\n", inode_table_lba);
    
    uint8_t buffer[1024];
    read_sector(inode_table_lba, buffer);
    
    // Inode size
    uint16_t inode_size = sb->s_inode_size;
    if (sb->s_rev_level == 0) inode_size = 128;
    // kprintf("Inode Size: %d\n", inode_size);
    
    // Root inode is index 1 (2nd inode)
    Ext2Inode* root_inode = (Ext2Inode*)(buffer + inode_size);
    
    // kprintf("Root Inode Block[0]: %d\n", root_inode->i_block[0]);
    
    // 2. Read Directory Content
    uint32_t dir_lba = root_inode->i_block[0] * sectors_per_block;
    // kprintf("Reading Directory at LBA: %d\n", dir_lba);
    read_sector(dir_lba, buffer);
    
    // 3. Iterate Entries
    Ext2DirEntry* entry = (Ext2DirEntry*)buffer;
    uint32_t offset = 0;
    
    // kprintf("Files in Root:\n");
    while (offset < 512 && entry->inode != 0) {
        char name[256];
        // Safety check for name length
        int len = entry->name_len;
        if (len > 255) len = 255;
        
        for(int i=0; i<len; i++) name[i] = entry->name[i];
        name[len] = '\0';
        
        // kprintf("- %s (Inode: %d, RecLen: %d)\n", name, entry->inode, entry->rec_len);
        
        if (entry->rec_len == 0) {
            kprintf("Error: Zero RecLen\n");
            break; 
        }
        offset += entry->rec_len;
        entry = (Ext2DirEntry*)(buffer + offset);
    }
  } else {
    kprintf("Not an Ext2 File System (Magic: %x)\n", sb->s_magic);
  }
}
