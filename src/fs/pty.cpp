#include "fs/pty.h"
#include "vfs.h"
#include <cstring>
PtyPair g_pty_table[MAX_PTYS];
void    init_pty(vfs_node *dev_dir) {
    // 1. Reset all PTY table entries
    for (uint32_t i = 0; i < MAX_PTYS; i++) {
        g_pty_table[i].id                      = i;
        g_pty_table[i].in_use                  = false;
        g_pty_table[i].locked                  = true;
        g_pty_table[i].master_open             = false;
        g_pty_table[i].slave_open              = false;
        g_pty_table[i].master_node.device_data = &g_pty_table[i];
        g_pty_table[i].slave_node.device_data  = &g_pty_table[i];
    }

    // 2. Setup the /dev/pts/ directory node
    memset(&g_devpts_dir, 0, sizeof(vfs_node));
    vfs_strncpy(g_devpts_dir.name, "pts", 128);
    g_devpts_dir.flags = VFS_DIRECTORY;
    g_devpts_dir.finddir =
        devpts_finddir; // Resolves /dev/pts/0, /dev/pts/1, etc.

    // 3. Setup /dev/ptmx device node
    memset(&g_ptmx_node, 0, sizeof(vfs_node));
    vfs_strncpy(g_ptmx_node.name, "ptmx", 128);
    g_ptmx_node.flags = VFS_CHARDEVICE;
    // open hook points to ptmx_open_node to allocate a dynamic pair when opened
    g_ptmx_node.open  = [](vfs_node *node) {
        // Handled during sys_open resolution
    };

    // 4. If you have a /dev directory node, register "pts" and "ptmx" under
    // /dev/
    if (dev_dir && dev_dir->finddir) {
        // Attach devpts and ptmx to your DevFS directory structure
    }
}
int64_t pty_master_read(vfs_node *node, uint64_t offset, size_t size,
                        uint8_t *buffer) {
    PtyPair *pty = static_cast<PtyPair *>(node->device_data);
    if (!pty || !pty->in_use)
        return -1;

    size_t read_bytes = 0;
    while (read_bytes < size) {
        uint8_t byte;
        if (!pty->slave_to_master.pop(byte))
            break;
        buffer[read_bytes++] = byte;
    }
    return read_bytes;
}
int64_t pty_master_write(vfs_node *node, uint64_t offest, size_t size,
                         const int8_t *buffer) {
    PtyPair *pty = static_cast<PtyPair *>(node->device_data);
    if (!pty || !pty->in_use) {
        return -1;
    }
    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
        if (!pty->master_to_slave.push(buffer[i])) {
            break;
        }
        written++;
    }
    return written;
}
void pty_master_close(vfs_node *node) {
    PtyPair *pty = static_cast<PtyPair *>(node->device_data);
    if (pty) {
        pty->master_open = false;
        if (!pty->slave_open) {
            pty->in_use = false;
        }
    }
}
int64_t pty_slave_read(vfs_node *node, uint64_t offset, size_t size,
                       uint8_t *buffer) {
    PtyPair *pty = static_cast<PtyPair *>(node->device_data);
    if (!pty || !pty->in_use)
        return -1;

    size_t read_bytes = 0;
    while (read_bytes < size) {
        uint8_t byte;
        if (!pty->master_to_slave.pop(byte))
            break;
        buffer[read_bytes++] = byte;
    }
    return read_bytes;
}
int64_t pty_slave_write(vfs_node *node, uint64_t offset, size_t size,
                        const uint8_t *buffer) {
    PtyPair *pty = static_cast<PtyPair *>(node->device_data);
    if (!pty || !pty->in_use)
        return -1;

    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
        if (!pty->slave_to_master.push(buffer[i]))
            break;
        written++;
    }
    return written;
}
void pty_slave_close(vfs_node *node) {
    PtyPair *pty = static_cast<PtyPair *>(node->device_data);
    if (pty) {
        pty->slave_open = false;
        if (!pty->master_open) {
            pty->in_use = false;
        }
    }
}
vfs_node *ptmx_open_node() {
    for (uint32_t i = 0; i < MAX_PTYS; i++) {
        if (!g_pty_table[i].in_use) {
            PtyPair *pty     = &g_pty_table[i];
            pty->id          = i;
            pty->in_use      = true;
            pty->locked      = true;
            pty->master_open = true;

            vfs_strncpy(pty->master_node.name, "ptmx", 128);
            pty->master_node.flags       = VFS_CHARDEVICE;
            pty->master_node.device_data = pty;
            pty->master_node.read        = pty_master_read;
            pty->master_node.write       = pty_master_write;
            pty->master_node.close       = pty_master_close;

            pty->slave_node.flags        = VFS_CHARDEVICE;
            pty->slave_node.device_data  = pty;
            pty->slave_node.read         = pty_slave_read;
            pty->slave_node.write        = pty_slave_write;
            pty->slave_node.close        = pty_slave_close;
            return &pty->master_node;
        }
    }
    return nullptr;
}
vfs_node *devpts_finddir(vfs_node *dir_node, const char *name) {
    uint32_t id = 0;
    for (size_t i = 0; name[i] != '\0'; i++) {
        if (name[i] < '0' || name[i] > '9') {
            return nullptr;
        }
        id = id * 10 + (name[i] - '0');
    }
    if (id >= MAX_PTYS || !g_pty_table[id].in_use) {
        nullptr;
    }
    PtyPair *pty = &g_pty_table[id];

    if (pty->locked) {
        return nullptr;
    }
    pty->slave_open = true;
    return &pty->slave_node;
}
