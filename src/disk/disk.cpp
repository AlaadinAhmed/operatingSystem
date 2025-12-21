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

void Ext2Disk::read_sector(uint32_t lba, uint8_t* buffer) {
    // printf("Read LBA: %d\n", lba);
    // Select drive (Master) and LBA bits 24-27
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
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

    // Wait for BSY to clear and DRQ to set
    // Note: This is a very basic poll. In a real OS, use interrupts or timeout.
    int timeout = 100000;
    while (!(inb(0x1F7) & 0x08)) {
        if (--timeout == 0) {
            printf("Disk Read Timeout! LBA: %d\n", lba);
            return;
        }
    }

    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t tmp;
        asm volatile ("inw %1, %0" : "=a"(tmp) : "Nd"((uint16_t)0x1F0));
        ((uint16_t*)buffer)[i] = tmp;
    }
    // printf("Read Done\n");
}

void Ext2Disk::write_sector(uint32_t lba, const uint8_t* buffer) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F1, 0x00);
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // Command: Write Sectors with Retry

    while (!(inb(0x1F7) & 0x08));

    for (int i = 0; i < 256; i++) {
        uint16_t tmp = ((uint16_t*)buffer)[i];
        asm volatile ("outw %0, %1" : : "a"(tmp), "Nd"((uint16_t)0x1F0));
    }
}

Ext2Disk::Ext2Disk() {
}

void Ext2Disk::mount() {
  // We loaded Superblock (Sector 3) to 0x1000.
  // So Superblock is at 0x1000.
  Superblock *sb = (Superblock *)(0x1000);
  if (sb->s_magic == 0xEF53) {
    uint32_t total_blocks = sb->s_blocks_count;
    // printf("Ext2 File Detected\n");
    // printf("Total Blocks: %d\n", total_blocks);
    // printf("Inodes Count: %d\n", sb->s_inodes_count);
    
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
    // printf("Inode Table Block: %d\n", bgd->bg_inode_table);
    // printf("Free Blocks: %d\n", bgd->bg_free_blocks_count);
    
    // 1. Read Root Inode (Inode #2)
    // Assuming Block Size is 1024 (s_log_block_size = 0)
    uint32_t block_size = 1024 << sb->s_log_block_size;
    // printf("Block Size: %d\n", block_size);
    
    uint32_t sectors_per_block = block_size / 512;
    uint32_t inode_table_lba = bgd->bg_inode_table * sectors_per_block;
    // printf("Reading Inode Table at LBA: %d\n", inode_table_lba);
    
    uint8_t buffer[1024];
    read_sector(inode_table_lba, buffer);
    
    // Inode size
    uint16_t inode_size = sb->s_inode_size;
    if (sb->s_rev_level == 0) inode_size = 128;
    // printf("Inode Size: %d\n", inode_size);
    
    // Root inode is index 1 (2nd inode)
    Ext2Inode* root_inode = (Ext2Inode*)(buffer + inode_size);
    
    // printf("Root Inode Block[0]: %d\n", root_inode->i_block[0]);
    
    // 2. Read Directory Content
    uint32_t dir_lba = root_inode->i_block[0] * sectors_per_block;
    // printf("Reading Directory at LBA: %d\n", dir_lba);
    read_sector(dir_lba, buffer);
    
    // 3. Iterate Entries
    Ext2DirEntry* entry = (Ext2DirEntry*)buffer;
    uint32_t offset = 0;
    
    // printf("Files in Root:\n");
    while (offset < 512 && entry->inode != 0) {
        char name[256];
        // Safety check for name length
        int len = entry->name_len;
        if (len > 255) len = 255;
        
        for(int i=0; i<len; i++) name[i] = entry->name[i];
        name[len] = '\0';
        
        // printf("- %s (Inode: %d, RecLen: %d)\n", name, entry->inode, entry->rec_len);
        
        if (entry->rec_len == 0) {
            printf("Error: Zero RecLen\n");
            break; 
        }
        offset += entry->rec_len;
        entry = (Ext2DirEntry*)(buffer + offset);
    }
  } else {
    printf("Not an Ext2 File System (Magic: %x)\n", sb->s_magic);
  }
}
