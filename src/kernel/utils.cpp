#include <stddef.h>
#include <stdint.h>
#include "print/print.h"
#include "memory/kmalloc.h" // Include kmalloc.h for declarations
#include "fs/lwext4_adapter.h" // For ext4_blockdev
#include <ext4.h> // For ext4_fopen, ext4_fsize, ext4_fread, ext4_fclose, EOK


extern "C" {

void *memset(void *s, int c, size_t n) noexcept {
    unsigned char *p = (unsigned char *)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) noexcept {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--)
        *d++ = *s++;
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

// Simple bump allocator
// static uint8_t heap[1024 * 1024 * 4]; // 4MB heap
static uint8_t* heap_ptr = (uint8_t*)0x200000; // Start heap at 2MB
static size_t heap_offset = 0;

void init_memory() {
    heap_offset = 0;
    // memset(heap, 0, sizeof(heap)); // Don't clear 4MB, too slow and unnecessary
}

void *kmalloc(size_t size) { // Renamed from malloc to kmalloc
    // printf("kmalloc(%d)\n", size);
    // Align to 8 bytes
    if (heap_offset % 8 != 0)
        heap_offset += 8 - (heap_offset % 8);

    // Check for collision with stack (at 9MB)
    if ((uint32_t)(heap_ptr + heap_offset + size) >= 0x900000) {
        kprintf("kmalloc failed: OOM (Stack Collision)\n");
        return NULL;
    }
    void *ptr = heap_ptr + heap_offset;
    heap_offset += size;
    // printf("kmalloc returning %x\n", (uint32_t)ptr);
    return ptr;
}

void kfree(void *ptr) { // Renamed from free to kfree
    // printf("kfree(%x)\n", (uint32_t)ptr);
    // With a bump allocator, free is a no-op unless we implement
    // a more complex memory manager.
}

void *krealloc(void* ptr, size_t size) {
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        return kmalloc(size);
    }

    // This is an unsafe and inefficient realloc for a bump allocator.
    // It assumes realloc only grows and doesn't track old_size.
    // Copying 'size' bytes is a best guess.
    void* new_ptr = kmalloc(size);
    if (new_ptr && ptr) {
        // We don't know the old size, so we copy 'size' bytes.
        // This can lead to reading past the old allocation if old size < size,
        // or losing data if old size > size.
        memcpy(new_ptr, ptr, size);
    }
    kfree(ptr); // Free the old pointer (which is a no-op for bump allocator)
    return new_ptr;
}


void *kcalloc(size_t nmemb, size_t size) { // Renamed from calloc to kcalloc
    // printf("kcalloc(%d, %d)\n", nmemb, size);
    size_t total = nmemb * size;
    void *ptr = kmalloc(total);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void __stack_chk_fail(void) {
    // Panic
    while (1);
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
    while ((*d++ = *src++));
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

int abs(int x) {
    return (x < 0) ? -x : x;
}

// Helper function to read a file from lwext4 into memory
// Returns a pointer to the allocated buffer and sets file_size.
// Returns NULL on error. The caller is responsible for kfree'ing the buffer.
unsigned char* read_file_to_memory(const char* mount_point, const char* filename, size_t* file_size) {
    ext4_file file;
    int rc;

    char full_path[256];
    strcpy(full_path, mount_point);
    int len = strlen(full_path);
    if (len > 0 && full_path[len-1] != '/') {
        full_path[len] = '/';
        full_path[len+1] = '\0';
    }
    strcpy(full_path + strlen(full_path), filename);

    // Open the file
    rc = ext4_fopen(&file, full_path, "rb");
    if (rc != EOK) {
        kprintf("Error opening file %s: %d\n", full_path, rc);
        return NULL;
    }

    // Get file size
    uint64_t size = ext4_fsize(&file);
    if (size == 0) {
        kprintf("File %s is empty or size cannot be determined\n", filename);
        ext4_fclose(&file);
        return NULL;
    }

    // Allocate buffer
    unsigned char* buffer = (unsigned char*)kmalloc(size);
    if (buffer == NULL) {
        kprintf("Memory allocation failed for file %s (size: %u)\n", filename, (uint32_t)size);
        ext4_fclose(&file);
        return NULL;
    }

    // Read file content
    size_t bytes_read;
    rc = ext4_fread(&file, buffer, size, &bytes_read);
    if (rc != EOK || bytes_read != size) {
        kprintf("Error reading file %s: %d, read %u of %u bytes\n", filename, rc, (uint32_t)bytes_read, (uint32_t)size);
        kfree(buffer);
        ext4_fclose(&file);
        return NULL;
    }

    // Close the file
    ext4_fclose(&file);

    *file_size = (size_t)size;
    return buffer;
}

// Implementations for lwext4 user-provided memory functions
void *ext4_user_malloc(size_t size) {
    return kmalloc(size);
}

void *ext4_user_calloc(size_t nmemb, size_t size) {
    return kcalloc(nmemb, size);
}

void *ext4_user_realloc(void *ptr, size_t size) {
    return krealloc(ptr, size);
}

void ext4_user_free(void *ptr) {
    kfree(ptr);
}


} // End of extern "C"

// Renamed to avoid conflicts with standard library functions
void *operator new(size_t size) {
    return kmalloc(size);
}

void *operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void *p) {
    kfree(p);
}

void operator delete(void *p, size_t size) {
    kfree(p);
}

void operator delete[](void *p) {
    kfree(p);
}

void operator delete[](void *p, size_t size) {
    kfree(p);
}

extern "C" void __gxx_personality_v0() {
}

extern "C" void _Unwind_Resume() {
    while(1);
}
