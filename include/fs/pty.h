#pragma once
#include "utils/ring_buffer.h"
#include "vfs.h"
#include <cstdint>
constexpr size_t PTY_BUFFER_SIZE = 4096;
constexpr size_t MAX_PTYS        = 32;
struct PtyPair {
    uint32_t id; // Matches /dev/pts/N
    bool     in_use = false;
    bool     locked = true; // POSIX: Slaves start locked until TIOCSPTLCK ioctl
    bool     master_open = false;
    bool     slave_open  = false;

    // Direct bidirectional streams
    RingBuffer<uint8_t, PTY_BUFFER_SIZE>
        master_to_slave; // Input: Terminal -> Shell
    RingBuffer<uint8_t, PTY_BUFFER_SIZE>
             slave_to_master; // Output: Shell -> Terminal

    // Associated VFS character device nodes
    vfs_node master_node;
    vfs_node slave_node;
};

extern PtyPair  g_pty_table[MAX_PTYS];
// Virtual directory node for /dev/pts/
static vfs_node g_devpts_dir;
// Master node entry point for /dev/ptmx
static vfs_node g_ptmx_node;
void            init_pty(vfs_node *dev_dir);
vfs_node       *ptmx_open_node();
int64_t         pty_master_read(vfs_node *node, uint64_t offest, size_t size,
                                uint8_t *buffer);
int64_t         pty_master_write(vfs_node *node, uint64_t offest, size_t size,
                                 const uint8_t *buffer);
void            pty_master_close(vfs_node *node);
int64_t         pty_slave_read(vfs_node *node, uint64_t offest, size_t size,
                               uint8_t *buffer);
int64_t         pty_slave_write(vfs_node *node, uint64_t offest, size_t size,
                                const uint8_t *buffer);
vfs_node       *devpts_finddir(vfs_node *dir_node, const char *name);
