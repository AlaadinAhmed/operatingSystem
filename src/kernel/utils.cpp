#include "drivers/vga.h"       // For debug checkpoints
#include "fs/lwext4_adapter.h" // For ext4_blockdev
#include "fs/vfs.h"
#include "kernel/utils.h"
#include "memory/kmalloc.h" // Include kmalloc.h for declarations
#include "print/print.h"
#include <ext4.h> // For ext4_fopen, ext4_fsize, ext4_fread, ext4_fclose, EOK
#include <stddef.h>
#include <stdint.h>

extern "C" {

// For EFI builds, gnu-efi provides these functions
#ifndef __EFI__

void *memset(void *s, int c, size_t n) noexcept {
    uint8_t *p = (uint8_t *)s;

    // Fill small buffers or unaligned start byte-by-byte
    while (n > 0 && ((uintptr_t)p & 7)) {
        *p++ = (uint8_t)c;
        n--;
    }

    // Fill 64-bit chunks
    if (n >= 8) {
        uint64_t c64 = (uint8_t)c;
        c64 |= c64 << 8;
        c64 |= c64 << 16;
        c64 |= c64 << 32;

        uint64_t *p64 = (uint64_t *)p;
        while (n >= 8) {
            *p64++ = c64;
            n -= 8;
        }
        p = (uint8_t *)p64;
    }

    // Fill remaining bytes
    while (n > 0) {
        *p++ = (uint8_t)c;
        n--;
    }

    return s;
}

void *memset16(void *s, uint16_t c, size_t count) noexcept {
    uint16_t *p = (uint16_t *)s;
    while (count--) {
        *p++ = c;
    }
    return s;
}

void *memset32(void *s, uint32_t c, size_t count) noexcept {
    uint32_t *p = (uint32_t *)s;

    // Fast 64-bit block fills
    if (count >= 2) {
        uint64_t c64 = ((uint64_t)c << 32) | c;
        uint64_t *p64 = (uint64_t *)p;
        while (count >= 2) {
            *p64++ = c64;
            count -= 2;
        }
        p = (uint32_t *)p64;
    }

    // Fill remaining elements
    while (count--) {
        *p++ = c;
    }
    return s;
}

void *memset64(void *s, uint64_t c, size_t count) noexcept {
    uint64_t *p = (uint64_t *)s;
    while (count--) {
        *p++ = c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) noexcept {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    // Copy small buffers or unaligned start byte-by-byte
    while (n > 0 && (((uintptr_t)d & 7) || ((uintptr_t)s & 7))) {
        *d++ = *s++;
        n--;
    }

    // Copy 64-bit chunks
    if (n >= 8) {
        uint64_t *d64 = (uint64_t *)d;
        const uint64_t *s64 = (const uint64_t *)s;
        while (n >= 8) {
            *d64++ = *s64++;
            n -= 8;
        }
        d = (uint8_t *)d64;
        s = (const uint8_t *)s64;
    }

    // Copy remaining bytes
    while (n > 0) {
        *d++ = *s++;
        n--;
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

#endif /* __EFI__ */

void __stack_chk_fail(void) {
    // Panic
    while (1)
        ;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

void *memmove(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        const char *lasts = s + (n - 1);
        char *lastd = d + (n - 1);
        while (n--)
            *lastd-- = *lasts--;
    }
    return dest;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    char *b = (char *)base;
    for (size_t i = 1; i < nmemb; i++) {
        for (size_t j = i; j > 0; j--) {
            char *p1 = b + (j - 1) * size;
            char *p2 = b + j * size;
            if (compar(p1, p2) > 0) {
                // Swap
                for (size_t k = 0; k < size; k++) {
                    char tmp = p1[k];
                    p1[k] = p2[k];
                    p2[k] = tmp;
                }
            } else {
                break;
            }
        }
    }
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n--) {
        if (*s1 != *s2)
            return *(const unsigned char *)s1 - *(const unsigned char *)s2;
        if (*s1 == 0)
            break;
        s1++;
        s2++;
    }
    return 0;
}

int abs(int x) { return (x < 0) ? -x : x; }

// Returns a pointer to the allocated buffer and sets file_size.
// Returns NULL on error. The caller is responsible for kfree'ing the buffer.
unsigned char *read_file_to_memory(const char *mount_point, const char *filename, size_t *file_size) {
    char full_path[256];
    strcpy(full_path, mount_point);
    int len = strlen(full_path);
    if (len > 0 && full_path[len - 1] != '/') {
        full_path[len] = '/';
        full_path[len + 1] = '\0';
    }
    strcpy(full_path + strlen(full_path), filename);

    vfs_node *node = vfs_open(full_path, 0);
    if (!node) {
        kprintf("read_file_to_memory: Failed to open %s via VFS\n", full_path);
        return NULL;
    }

    uint64_t size = node->length;
    if (size == 0) {
        if (node->close) node->close(node);
        return NULL;
    }

    unsigned char *buffer = (unsigned char *)kmalloc(size);
    if (buffer == NULL) {
        kprintf("read_file_to_memory: OOM for %s\n", full_path);
        if (node->close) node->close(node);
        return NULL;
    }

    int64_t bytes_read = 0;
    if (node->read) {
        bytes_read = node->read(node, 0, size, buffer);
    }

    if (bytes_read != (int64_t)size) {
        kfree(buffer);
        if (node->close) node->close(node);
        return NULL;
    }

    if (node->close) node->close(node);
    *file_size = (size_t)size;
    return buffer;
}

// Implementations for lwext4 user-provided memory functions
void *ext4_user_malloc(size_t size) { return kmalloc(size); }

void *ext4_user_calloc(size_t nmemb, size_t size) { return kcalloc(nmemb, size); }

void *ext4_user_realloc(void *ptr, size_t size) { return krealloc(ptr, size); }

void ext4_user_free(void *ptr) { kfree(ptr); }

} // End of extern "C"

extern "C" void __gxx_personality_v0() {}

extern "C" void _Unwind_Resume() {
    while (1)
        ;
}

// C++ static local variable guard stubs
// These are used by the compiler for thread-safe initialization of static
// locals
extern "C" int __cxa_guard_acquire(uint64_t *guard) {
    if (*guard == 0) {
        *guard = 1; // Mark as initializing
        return 1;   // Return 1 to indicate initialization should proceed
    }
    return 0; // Already initialized
}

extern "C" void __cxa_guard_release(uint64_t *guard) {
    *guard = 2; // Mark as fully initialized
}

extern "C" void __cxa_guard_abort(uint64_t *guard) {
    *guard = 0; // Reset on failure
}
