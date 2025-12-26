#include "fs/lwext4_adapter.h"
#include "disk/disk.h"
#include "print/print.h"
#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include "memory/kmalloc.h"
#include "drivers/vga.h"

static fs::Ext2Disk disk_driver(1); // Initialize as secondary disk (device 1)
static struct ext4_blockdev blockdev;
static struct ext4_blockdev_iface iface;
static uint8_t ph_bbuf[4096];
static struct ext4_bcache bc_static;

// Disk callbacks for lwext4
static int disk_open(struct ext4_blockdev *bdev) {
    (void)bdev;
    return EOK;
}

static int disk_close(struct ext4_blockdev *bdev) {
    (void)bdev;
    return EOK;
}

static int disk_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    (void)bdev;
    // blk_id and blk_cnt are in 512-byte sectors (since ph_bsize = 512)
    uint32_t lba = (uint32_t)blk_id;
    uint32_t sector_count = blk_cnt;
    
    if (disk_driver.read_sectors(lba, (uint8_t*)buf, sector_count) != 0) {
        return EIO;
    }
    return EOK;
}

static int disk_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    (void)bdev;
    (void)buf;
    (void)blk_id;
    (void)blk_cnt;
    return EOK; // Read-only for now
}

static int disk_lock(struct ext4_blockdev *bdev) {
    (void)bdev;
    return EOK;
}

static int disk_unlock(struct ext4_blockdev *bdev) {
    (void)bdev;
    return EOK;
}

struct ext4_blockdev* fs::get_lwext4_blockdev() {
    // Clear all static structures
    memset(&blockdev, 0, sizeof(struct ext4_blockdev));
    memset(&iface, 0, sizeof(struct ext4_blockdev_iface));
    memset(ph_bbuf, 0, 4096);
    memset(&bc_static, 0, sizeof(struct ext4_bcache));

    // Set up interface callbacks
    iface.open = disk_open;
    iface.close = disk_close;
    iface.bread = disk_read;
    iface.bwrite = disk_write;
    iface.ph_bsize = 512;
    iface.ph_bcnt = 28672 * 2; // 28MB in 512B blocks = 57344 blocks
    iface.ph_bbuf = ph_bbuf;
    iface.lock = disk_lock;
    iface.unlock = disk_unlock;

    blockdev.bdif = &iface;
    blockdev.part_offset = 0;
    blockdev.part_size = 28672 * 1024; // 28MB in bytes

    // Initialize block cache
    struct ext4_bcache *bc = &bc_static;
    ext4_bcache_init_dynamic(bc, 8, 4096);
    ext4_block_bind_bcache(&blockdev, bc);

    return &blockdev;
}
