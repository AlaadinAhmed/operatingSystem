#include "fs/lwext4_adapter.h"
#include "disk/disk.h"
#include "print/print.h"
#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include "memory/kmalloc.h"

static fs::Ext2Disk disk_driver(1); // Initialize as slave drive (device 1)
static struct ext4_blockdev blockdev;
static struct ext4_blockdev_iface iface;
static uint8_t ph_bbuf[4096];
static struct ext4_bcache bc_static;

static int disk_open(struct ext4_blockdev *bdev) {
    // No longer need to allocate, the object is static.
    return EOK;
}

static int disk_close(struct ext4_blockdev *bdev) {
    return EOK;
}

static int disk_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    // blk_id is logical block ID within the EXT4 filesystem
    // Convert part_offset from bytes to 512-byte sectors
    uint64_t part_offset_sectors = bdev->part_offset / 512;
    uint64_t start_sector = part_offset_sectors + blk_id;
    uint32_t total_sectors_to_read = blk_cnt;

    if (blk_cnt == 0) return EOK;

    uint8_t* buffer = (uint8_t*)buf;
    
    if (disk_driver.read_sectors(start_sector, buffer, blk_cnt) != 0) {
        kprintf("disk_read failed at sector %d count %d\n", (int)start_sector, blk_cnt);
        return EIO;
    }
    return EOK;
}

static int disk_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    // blk_id is logical block ID within the EXT4 filesystem
    // Convert part_offset from bytes to 512-byte sectors
    uint64_t part_offset_sectors = bdev->part_offset / 512;
    uint64_t start_sector = part_offset_sectors + blk_id;
    uint32_t total_sectors_to_write = blk_cnt;
    
    const uint8_t* buffer = (const uint8_t*)buf;
    for (uint32_t i = 0; i < total_sectors_to_write; i++) {
        if (disk_driver.write_sector(start_sector + i, buffer + i * 512) != 0) {
            return EIO;
        }
    }
    return EOK;
}

static int disk_ph_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    return disk_read(bdev, buf, blk_id, blk_cnt);
}

static int disk_ph_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    return disk_write(bdev, buf, blk_id, blk_cnt);
}

static uint8_t bc_mem[4096 * 16]; // 64KB cache memory

struct ext4_blockdev* fs::get_lwext4_blockdev() {
    iface.open = disk_open;
    iface.close = disk_close;
    iface.bread = disk_read;
    iface.bwrite = disk_write;
    iface.ph_bsize = 512;
    iface.ph_bcnt = 28672 * 2; // 28MB in 512B blocks = 57344 blocks
    iface.ph_bbuf = ph_bbuf;

    blockdev.bdif = &iface;
    blockdev.part_offset = 0; // Directly mount the image, no partition offset
    blockdev.part_size = 28672 * 1024; // 28MB in bytes
    
    // Initialize and bind block cache
    // Use fixed address for bc to avoid BSS/Heap issues
    struct ext4_bcache *bc = &bc_static;
    
    // We need to ensure this memory is zeroed before use?
    // ext4_bcache_init_dynamic does memset(bc, 0, ...).
    // But we need to make sure 0x300000 is available.
    // Heap starts at 0x200000.
    // If heap grows, it might hit 0x300000.
    // Let's use 0x400000 (4MB) for bc, and move heap to 0x500000?
    // Or just use kmalloc again?
    // kmalloc works now (BSS cleared).
    // Let's try kmalloc again. It's cleaner.
    // If kmalloc fails, we know heap is broken.
    
    // bc = (struct ext4_bcache *)kmalloc(sizeof(struct ext4_bcache));
    // if (!bc) {
    //     kprintf("Failed to allocate block cache structure\n");
    //     return &blockdev;
    // }

    ext4_bcache_init_dynamic(bc, 1, 4096); // 1 block cache
    
    int r = ext4_block_bind_bcache(&blockdev, bc);
    if (r != EOK) {
        kprintf("Failed to bind block cache: %d\n", r);
    }

    return &blockdev;
}
