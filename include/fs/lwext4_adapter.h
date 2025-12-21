#pragma once
#include <stdint.h>

// Forward declaration
struct ext4_blockdev;

namespace fs {
    // Initialize and return the block device structure for lwext4
    struct ext4_blockdev* get_lwext4_blockdev();
}
