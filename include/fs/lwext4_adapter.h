#pragma once
#include <stdint.h>

struct ext4_blockdev;

namespace fs {
    // Initialize and return the block device structure for lwext4
    struct ext4_blockdev* get_lwext4_blockdev();
    
    // Mount filesystem at specified mount point (e.g., "/mp/")
    bool mount_filesystem(const char* mount_point);
}
