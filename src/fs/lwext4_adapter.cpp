#include "fs/lwext4_adapter.h"
#include "disk/disk.h"
#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>

static fs::Ext2Disk disk_driver; // Static instance instead of pointer
static struct ext4_blockdev blockdev;
static struct ext4_blockdev_iface iface;

static int disk_open(struct ext4_blockdev *bdev) {
    // No longer need to allocate, the object is static.
    return EOK;
}

static int disk_close(struct ext4_blockdev *bdev) {
    return EOK;
}

static int disk_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    uint64_t start_sector = blk_id;
    uint32_t total_sectors_to_read = blk_cnt;

    uint8_t* buffer = (uint8_t*)buf;
    for (uint32_t i = 0; i < total_sectors_to_read; i++) {
        disk_driver.read_sector(start_sector + i, buffer + i * 512);
    }
    return EOK;
}

static int disk_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    uint64_t start_sector = blk_id;
    uint32_t total_sectors_to_write = blk_cnt;
    
    const uint8_t* buffer = (const uint8_t*)buf;
    for (uint32_t i = 0; i < total_sectors_to_write; i++) {
        disk_driver.write_sector(start_sector + i, buffer + i * 512);
    }
    return EOK;
}

static int disk_ph_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    return disk_read(bdev, buf, blk_id, blk_cnt);
}

static int disk_ph_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    return disk_write(bdev, buf, blk_id, blk_cnt);
}

struct ext4_blockdev* fs::get_lwext4_blockdev() {
    iface.open = disk_open;
    iface.close = disk_close;
    iface.bread = disk_read;
    iface.bwrite = disk_write;
    iface.ph_bsize = 512;
    iface.ph_bcnt = 32768 * 2; // 32MB

    blockdev.bdif = &iface;
    blockdev.part_offset = 0;
    blockdev.part_size = 32768 * 1024;
    
    return &blockdev;
}
