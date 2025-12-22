#include "shell/shell.h"
#include "print/print.h"
#include "drivers/keyboard.h"
#include "drivers/vga.h"
#include <ext4.h>
#include "fs/lwext4_adapter.h"
#include <ext4_blockdev.h>

// --- Minimal C-style string functions ---
static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) {
            return 0;
        }
    }
    return (char*)s;
}
// --- End string functions ---

#define MAX_CMD_LEN 256

// Simple command parsing
static int parse_command(char* cmd, char** argv) {
    int argc = 0;
    char* current_arg = cmd;
    char* next_space;

    while ((next_space = strchr(current_arg, ' '))) {
        *next_space = '\0'; // Null-terminate the argument
        argv[argc++] = current_arg;
        current_arg = next_space + 1;
        // Skip multiple spaces
        while (*current_arg == ' ') current_arg++;
    }
    
    if (*current_arg) {
        argv[argc++] = current_arg;
    }
    
    return argc;
}


void shell_main(bool fs_ready) {
    char cmd_buffer[MAX_CMD_LEN];
    int buffer_pos = 0;

    if (fs_ready) {
        kprintf("Filesystem mounted successfully.\n");
    }

    kprintf("Starting basic shell...\n");

    while (1) {
        kprintf("> ");
        buffer_pos = 0;

        // Read a line from the user
        while (1) {
            char c = kgetchar();
            if (c == '\n') {
                cmd_buffer[buffer_pos] = '\0';
                putchar('\n');
                break;
            } else if (c == '\b') {
                if (buffer_pos > 0) {
                    buffer_pos--;
                    putchar('\b'); // Handle backspace on screen
                }
            } else if (buffer_pos < MAX_CMD_LEN - 1) {
                cmd_buffer[buffer_pos++] = c;
                putchar(c); // Echo character back
            }
        }

        if (buffer_pos == 0) {
            continue;
        }

        // Parse and execute
        char* argv[32]; // Max 32 arguments
        int argc = parse_command(cmd_buffer, argv);

        if (argc == 0) {
            continue;
        }

        if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                kprintf("%s ", argv[i]);
            }
            kprintf("\n");
        } else if (strcmp(argv[0], "clear") == 0) {
            vga_clear_screen(0x000000);
        } else if (strcmp(argv[0], "helloworld") == 0) {
            kprintf("Hello, world!\n");
        } else if (strcmp(argv[0], "ls") == 0) {
            if (!fs_ready) {
                kprintf("Filesystem not ready.\n");
                continue;
            }
            const char* path = "/";
            if (argc > 1) {
                path = argv[1];
            }
            ext4_dir dir;
            int r = ext4_dir_open(&dir, path);
            if (r != EOK) {
                kprintf("Error opening directory '%s': %d\n", path, r);
                continue;
            }
            kprintf("Dir open. Size: %d\n", (int)dir.f.fsize);
            ext4_dir_entry_rewind(&dir);
            const ext4_direntry* de;
            while ((de = ext4_dir_entry_next(&dir))) {
                kprintf("Entry: len=%d\n", de->name_length);
                char name_buf[256];
                int len = de->name_length;
                if (len > 255) len = 255;
                for(int i=0; i<len; i++) name_buf[i] = de->name[i];
                name_buf[len] = '\0';
                kprintf("%s\n", name_buf);
            }
            ext4_dir_close(&dir);
        } else if (strcmp(argv[0], "cat") == 0) {
            if (!fs_ready) {
                kprintf("Filesystem not ready.\n");
                continue;
            }
            if (argc < 2) {
                kprintf("Usage: cat <filename>\n");
                continue;
            }
            ext4_file f;
            if (ext4_fopen(&f, argv[1], "r") != EOK) {
                kprintf("Error opening file: %s\n", argv[1]);
                continue;
            }
            char read_buf[257];
            size_t bytes_read;
            do {
                bytes_read = 0;
                ext4_fread(&f, read_buf, 256, &bytes_read);
                read_buf[bytes_read] = '\0';
                kprintf("%s", read_buf);
            } while (bytes_read > 0);
            ext4_fclose(&f);
            kprintf("\n");
        } else if (strcmp(argv[0], "stat") == 0) {
            if (!fs_ready) {
                kprintf("Filesystem not ready.\n");
                continue;
            }
            if (argc < 2) {
                kprintf("Usage: stat <path>\n");
                continue;
            }
            struct ext4_inode inode;
            uint32_t ino;
            int r = ext4_raw_inode_fill(argv[1], &ino, &inode);
            if (r != EOK) {
                kprintf("Error getting inode: %d\n", r);
                continue;
            }
            kprintf("Inode: %d\n", ino);
            kprintf("Mode: %x\n", inode.mode);
            kprintf("UID: %d\n", inode.uid);
            kprintf("Size: %d\n", inode.size_lo);
            kprintf("Links: %d\n", inode.links_count);
            kprintf("Blocks: %d\n", inode.blocks_count_lo);
        } else if (strcmp(argv[0], "read_block") == 0) {
            if (argc < 2) {
                kprintf("Usage: read_block <block_id>\n");
                continue;
            }
            
            uint64_t block_id = 0;
            char* p = argv[1];
            while (*p >= '0' && *p <= '9') {
                block_id = block_id * 10 + (*p - '0');
                p++;
            }
            
            struct ext4_blockdev* bdev = fs::get_lwext4_blockdev();
            uint8_t buf[512]; 
            
            int r = bdev->bdif->bread(bdev, buf, block_id, 1);
            if (r != EOK) {
                kprintf("Error reading block: %d\n", r);
                continue;
            }
            
            kprintf("Block %d data:\n", (int)block_id);
            for (int i = 0; i < 512; i++) {
                kprintf("%x ", buf[i]);
                if ((i + 1) % 16 == 0) kprintf("\n");
            }
            kprintf("\n");
        } else {
            kprintf("Unknown command: %s\n", argv[0]);
        }
    }
}
