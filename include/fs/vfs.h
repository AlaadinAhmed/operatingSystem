#pragma once
#include <cstdint>
struct vfs_node;
typedef int64_t (*vfs_read_fn)(struct vfs_node *node, uint64_t offset, std::size_t size, uint8_t *buffer);
typedef int64_t (*vfs_write_fn)(struct vfs_node *node, uint64_t offset, std::size_t size, const uint8_t *buffer);
typedef void (*vfs_open_fn)(struct vfs_node *node);
typedef void (*vfs_close_fn)(struct vfs_node *node);
typedef struct vfs_node *(*vfs_finddir_fn)(struct vfs_node *node, const char *name);
struct vfs_node {
    char name[128];
    uint32_t flags;
    uint64_t length;
    void *device_data;
    vfs_read_fn read;
    vfs_write_fn write;
    vfs_open_fn open;
    vfs_close_fn close;
    vfs_finddir_fn finddir;
    struct vfs_node *ptr;
};
struct FileDescriptor {
    vfs_node *node;
    uint64_t offset;
    uint32_t flags;
};
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02

extern vfs_node *g_vfs_root;
void vfs_init();

vfs_node *vfs_lookup(vfs_node *root, const char *path);
vfs_node *vfs_open(const char *path, int flags);
