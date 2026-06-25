#include "fs/lwext4_adapter.h"
#include "disk/disk.h"
#include "disk/nvme_disk.h"
#include "print/print.h"
#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include "memory/kmalloc.h"

// --- Block Device State ---
enum class StorageKind {
    None,
    Nvme,
    Ata,
};

static StorageKind g_storage_kind = StorageKind::None;
static fs::NvmeDisk* g_detected_nvme = nullptr;
static fs::Ext2Disk* g_detected_ata = nullptr;
static struct ext4_blockdev blockdev;
static struct ext4_blockdev_iface iface;
static uint8_t ph_bbuf[4096];
static struct ext4_bcache bc_static;

// --- Disk Callbacks ---
static int disk_open(struct ext4_blockdev *bdev) { 
    (void)bdev; 
    return EOK; 
}

static int disk_close(struct ext4_blockdev *bdev) { 
    (void)bdev; 
    return EOK; 
}

static int disk_lock(struct ext4_blockdev *bdev) { 
    (void)bdev; 
    return EOK; 
}

static int disk_unlock(struct ext4_blockdev *bdev) { 
    (void)bdev; 
    return EOK; 
}

static int disk_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    (void)bdev;
    uint64_t start_sector = blk_id;
    if (blk_cnt == 0) return EOK;
    if (g_storage_kind == StorageKind::Nvme && g_detected_nvme) {
        if (g_detected_nvme->read_sectors((uint32_t)start_sector, (uint8_t*)buf, blk_cnt) != 0) {
            return EIO;
        }
        return EOK;
    }
    if (g_storage_kind == StorageKind::Ata && g_detected_ata) {
        uint64_t part_offset_sectors = g_detected_ata->m_partition_offset / 512;
        start_sector += part_offset_sectors;
        if (g_detected_ata->read_sectors((uint32_t)start_sector, (uint8_t*)buf, blk_cnt) != 0) {
            return EIO;
        }
        return EOK;
    }
    return EIO;
}

static int disk_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    (void)bdev;
    const uint8_t* buffer = (const uint8_t*)buf;

    if (g_storage_kind == StorageKind::Nvme && g_detected_nvme) {
        for (uint32_t i = 0; i < blk_cnt; i++) {
            if (g_detected_nvme->write_sector((uint32_t)(blk_id + i), buffer + i * 512) != 0) {
                return EIO;
            }
        }
        return EOK;
    }
    if (g_storage_kind == StorageKind::Ata && g_detected_ata) {
        uint64_t part_offset_sectors = g_detected_ata->m_partition_offset / 512;
        uint64_t start_sector = part_offset_sectors + blk_id;
        for (uint32_t i = 0; i < blk_cnt; i++) {
            if (g_detected_ata->write_sector((uint32_t)(start_sector + i), buffer + i * 512) != 0) {
                return EIO;
            }
        }
        return EOK;
    }
    return EIO;
}

// --- Public API ---

struct ext4_blockdev* fs::get_lwext4_blockdev() {
    // Prefer NVMe if it was initialized; fall back to the legacy ATA scan.
    if (g_nvme_disk.IsAvailable() && g_nvme_disk.detect_partition()) {
        g_storage_kind = StorageKind::Nvme;
        g_detected_nvme = &g_nvme_disk;
        g_detected_ata = nullptr;
        kprintf("FS: Found partition on NVMe\n");
    } else {
        // Try drive 1 (secondary master) first, then drive 0
    static fs::Ext2Disk disk1(1);
    static fs::Ext2Disk disk0(0);

        kprintf("FS: Scanning for ext4 partitions...\n");
        
        if (disk1.detect_partition()) {
            g_storage_kind = StorageKind::Ata;
            g_detected_ata = &disk1;
            g_detected_nvme = nullptr;
            kprintf("FS: Found partition on drive 1\n");
        } else if (disk0.detect_partition()) {
            g_storage_kind = StorageKind::Ata;
            g_detected_ata = &disk0;
            g_detected_nvme = nullptr;
            kprintf("FS: Found partition on drive 0\n");
        } else {
            kprintf("FS: No ext4 partition found!\n");
            return nullptr;
        }
    }

    // Clear structures
    memset(&blockdev, 0, sizeof(struct ext4_blockdev));
    memset(&iface, 0, sizeof(struct ext4_blockdev_iface));
    memset(ph_bbuf, 0, 4096);
    memset(&bc_static, 0, sizeof(struct ext4_bcache));

    // Configure interface
    iface.open = disk_open;
    iface.close = disk_close;
    iface.bread = disk_read;
    iface.bwrite = disk_write;
    iface.ph_bsize = 512;
    uint64_t partition_size = 0;
    if (g_storage_kind == StorageKind::Nvme && g_detected_nvme) {
        partition_size = g_detected_nvme->m_partition_size;
    } else if (g_storage_kind == StorageKind::Ata && g_detected_ata) {
        partition_size = g_detected_ata->m_partition_size;
    }

    iface.ph_bcnt = (partition_size > 0) ? (partition_size / 512) : (65536 * 2); // 64MB fallback
    iface.ph_bbuf = ph_bbuf;
    iface.lock = disk_lock;
    iface.unlock = disk_unlock;

    blockdev.bdif = &iface;
    blockdev.part_offset = 0;
    blockdev.part_size = (partition_size > 0) ? partition_size : (65536 * 1024); // 64MB fallback

    // Initialize block cache
    struct ext4_bcache *bc = &bc_static;
    ext4_bcache_init_dynamic(bc, 8, 4096);
    ext4_block_bind_bcache(&blockdev, bc);

    return &blockdev;
}

bool fs::mount_filesystem(const char* mount_point) {
    struct ext4_blockdev* bdev = get_lwext4_blockdev();
    if (!bdev) return false;

    ext4_device_unregister_all();
    
    int rc = ext4_device_register(bdev, "ext4_fs");
    if (rc != EOK && rc != 17) {
        kprintf("FS: Device registration failed: %d\n", rc);
        return false;
    }

    rc = ext4_mount("ext4_fs", mount_point, false);
    if (rc != EOK) {
        kprintf("FS: Mount failed: %d\n", rc);
        return false;
    }

    kprintf("FS: Mounted at %s\n", mount_point);
    return true;
}
