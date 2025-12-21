#include "fs/lwext4_adapter.h"
#include "disk/disk.h"
#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>

static fs::Ext2Disk* disk_driver = nullptr;
static struct ext4_blockdev blockdev;
static struct ext4_blockdev_iface iface;

static int disk_open(struct ext4_blockdev *bdev) {
    if (!disk_driver) {
        disk_driver = new fs::Ext2Disk();
    }
    return EOK;
}

static int disk_close(struct ext4_blockdev *bdev) {
    return EOK;
}

static int disk_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    uint8_t* buffer = (uint8_t*)buf;
    for (uint32_t i = 0; i < blk_cnt; i++) {
        disk_driver->read_sector(blk_id + i, buffer + i * 512);
    }
    return EOK;
}

static int disk_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    const uint8_t* buffer = (const uint8_t*)buf;
    for (uint32_t i = 0; i < blk_cnt; i++) {
        disk_driver->write_sector(blk_id + i, buffer + i * 512);
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
    iface.ph_bread = disk_ph_read;
    iface.ph_bwrite = disk_ph_write;
    iface.ph_bsize = 512;
    iface.ph_bcnt = 1440 * 2; // 1.44MB floppy

    blockdev.bdif = &iface;
    blockdev.part_offset = 0;
    blockdev.part_size = 1440 * 1024;
    
    return &blockdev;
}
