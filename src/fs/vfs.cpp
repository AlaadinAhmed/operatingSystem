#include "fs/vfs.h"
#include "kernel/utils.h"
#include "memory/kmalloc.h"
#include "print/print.h"
#include <ext4.h>
#include <string.h>

vfs_node *g_vfs_root = nullptr;

// Custom strncpy to avoid unresolved external references in freestanding mode
static void vfs_strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    if (i < n) {
        dest[i] = '\0';
    }
}

// --- Helper VFS Callbacks for Ext4 ---

void ext4_vfs_open(vfs_node *node) {
    if (!node || !node->device_data) return;
    const char *path = (const char *)node->device_data;

    // If it's a directory, we don't open an ext4_file.
    if (node->flags & VFS_DIRECTORY) {
        return;
    }

    // Check if already open
    if (node->ptr) {
        return;
    }

    ext4_file *file = new ext4_file;
    // Try opening as read-write, then fallback to read-only
    int rc = ext4_fopen(file, path, "r+");
    if (rc != EOK) {
        rc = ext4_fopen(file, path, "r");
    }

    if (rc == EOK) {
        node->ptr = (vfs_node *)file;
        node->length = ext4_fsize(file);
    } else {
        delete file;
        node->ptr = nullptr;
    }
}

void ext4_vfs_close(vfs_node *node) {
    if (!node) return;
    if (node->ptr) {
        ext4_file *file = (ext4_file *)node->ptr;
        ext4_fclose(file);
        delete file;
        node->ptr = nullptr;
    }
    if (node->device_data) {
        char *path = (char *)node->device_data;
        delete[] path;
        node->device_data = nullptr;
    }
    // Delete the dynamically allocated node itself
    delete node;
}

int64_t ext4_vfs_read(vfs_node *node, uint64_t offset, std::size_t size, uint8_t *buffer) {
    if (!node || !node->ptr) return -1;
    ext4_file *file = (ext4_file *)node->ptr;

    int rc = ext4_fseek(file, offset, SEEK_SET);
    if (rc != EOK) {
        return -1;
    }

    size_t bytes_read = 0;
    rc = ext4_fread(file, buffer, size, &bytes_read);
    if (rc != EOK) {
        return -1;
    }
    return (int64_t)bytes_read;
}

int64_t ext4_vfs_write(vfs_node *node, uint64_t offset, std::size_t size, const uint8_t *buffer) {
    if (!node || !node->ptr) return -1;
    ext4_file *file = (ext4_file *)node->ptr;

    int rc = ext4_fseek(file, offset, SEEK_SET);
    if (rc != EOK) {
        return -1;
    }

    size_t bytes_written = 0;
    rc = ext4_fwrite(file, buffer, size, &bytes_written);
    if (rc != EOK) {
        return -1;
    }
    node->length = ext4_fsize(file);
    return (int64_t)bytes_written;
}

vfs_node *ext4_vfs_finddir(vfs_node *parent, const char *name) {
    if (!parent || !parent->device_data || !name) return nullptr;
    const char *parent_path = (const char *)parent->device_data;

    char child_path[256];
    if (strlen(parent_path) + 1 + strlen(name) >= 256) {
        return nullptr;
    }

    if (strcmp(parent_path, "/") == 0) {
        ksprintf(child_path, "/%s", name);
    } else {
        int len = strlen(parent_path);
        if (len > 0 && parent_path[len - 1] == '/') {
            ksprintf(child_path, "%s%s", parent_path, name);
        } else {
            ksprintf(child_path, "%s/%s", parent_path, name);
        }
    }

    // Check file/directory type safely using raw inode data
    struct ext4_inode inode;
    uint32_t ino;
    int rc = ext4_raw_inode_fill(child_path, &ino, &inode);
    if (rc != EOK) {
        return nullptr;
    }

    uint16_t type = inode.mode & 0xF000; // EXT4_INODE_MODE_TYPE_MASK

    if (type == 0x4000) { // EXT4_INODE_MODE_DIRECTORY
        vfs_node *child = new vfs_node;
        memset(child, 0, sizeof(vfs_node));
        vfs_strncpy(child->name, name, sizeof(child->name) - 1);
        child->flags = VFS_DIRECTORY;
        child->finddir = ext4_vfs_finddir;
        child->open = ext4_vfs_open;
        child->close = ext4_vfs_close;
        char *path_copy = new char[strlen(child_path) + 1];
        strcpy(path_copy, child_path);
        child->device_data = path_copy;
        return child;
    }

    if (type == 0x8000) { // EXT4_INODE_MODE_FILE
        uint64_t size = ((uint64_t)inode.size_hi << 32) | inode.size_lo;

        vfs_node *child = new vfs_node;
        memset(child, 0, sizeof(vfs_node));
        vfs_strncpy(child->name, name, sizeof(child->name) - 1);
        child->flags = VFS_FILE;
        child->length = size;
        child->read = ext4_vfs_read;
        child->write = ext4_vfs_write;
        child->open = ext4_vfs_open;
        child->close = ext4_vfs_close;
        char *path_copy = new char[strlen(child_path) + 1];
        strcpy(path_copy, child_path);
        child->device_data = path_copy;
        return child;
    }

    return nullptr;
}

// --- VFS Root Callback ---

vfs_node *root_vfs_finddir(vfs_node *parent, const char *name) {
    if (strcmp(name, "mp") == 0) {
        vfs_node *mp_node = new vfs_node;
        memset(mp_node, 0, sizeof(vfs_node));
        strcpy(mp_node->name, "mp");
        mp_node->flags = VFS_DIRECTORY;
        mp_node->finddir = ext4_vfs_finddir;
        mp_node->open = ext4_vfs_open;
        mp_node->close = ext4_vfs_close;
        char *path = new char[5];
        strcpy(path, "/mp");
        mp_node->device_data = path;
        return mp_node;
    }
    return nullptr;
}

// --- Core VFS API Implementation ---

void vfs_init() {
    if (g_vfs_root) return;

    g_vfs_root = new vfs_node;
    memset(g_vfs_root, 0, sizeof(vfs_node));
    strcpy(g_vfs_root->name, "/");
    g_vfs_root->flags = VFS_DIRECTORY;
    g_vfs_root->finddir = root_vfs_finddir;
    g_vfs_root->device_data = (void *)"/";
}

vfs_node *vfs_lookup(vfs_node *root, const char *path) {
    if (!root || !path) {
        return nullptr;
    }
    if (path[0] == '/') {
        path++;
    }
    if (strlen(path) == 0) {
        return root;
    }
    char token[128];
    size_t i = 0;
    while (path[i] != '/' && path[i] != '\0') {
        token[i] = path[i];
        i++;
    }
    token[i] = '\0';
    if (root->finddir) {
        vfs_node *child = root->finddir(root, token);
        if (!child) {
            return nullptr;
        }
        if (path[i] == '/') {
            return vfs_lookup(child, &path[i + 1]);
        }
        return child;
    }
    return nullptr;
}

vfs_node *vfs_open(const char *path, int flags) {
    vfs_node *node = vfs_lookup(g_vfs_root, path);
    if (!node)
        return nullptr;

    // Trigger driver-specific open callback if set
    if (node->open) {
        node->open(node);
    }

    return node;
}
