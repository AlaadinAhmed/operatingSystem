#pragma once
#include "vfs.h"
#include <cstddef>
#include <cstdint>
int64_t dev_tty_read(vfs_node *node, uint64_t offest, size_t size, uint8_t *buffer);
