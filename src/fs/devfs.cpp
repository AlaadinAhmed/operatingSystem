#include "drivers/keyboard.h"
#include "fs/devfs.h"
int64_t dev_tty_read(vfs_node *node, uint64_t offest, size_t size, uint8_t *buffer) {
    size_t bytes_read = 0;
    while (bytes_read < size) {
        char c = kgetchar();
        buffer[bytes_read++] = c;
        if (c == '\n') {
            break;
        }
    }
    return bytes_read;
}
