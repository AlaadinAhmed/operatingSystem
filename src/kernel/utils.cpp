#include <stddef.h>
#include <stdint.h>
#include "print/print.h"

extern "C" {

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
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

void *malloc(size_t size) {
    // printf("malloc(%d)\n", size);
    // Align to 8 bytes
    if (heap_offset % 8 != 0)
        heap_offset += 8 - (heap_offset % 8);

    // Check for collision with stack (at 9MB)
    if ((uint32_t)(heap_ptr + heap_offset + size) >= 0x900000) {
        printf("malloc failed: OOM (Stack Collision)\n");
        return NULL;
    }
    void *ptr = heap_ptr + heap_offset;
    heap_offset += size;
    // printf("malloc returning %x\n", (uint32_t)ptr);
    return ptr;
}

void free(void *ptr) {
    // printf("free(%x)\n", (uint32_t)ptr);
}

void *calloc(size_t nmemb, size_t size) {
    // printf("calloc(%d, %d)\n", nmemb, size);
    size_t total = nmemb * size;
    void *ptr = malloc(total);
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

}

void *operator new(size_t size) {
    return malloc(size);
}

void *operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void *p) {
    free(p);
}

void operator delete(void *p, size_t size) {
    free(p);
}

void operator delete[](void *p) {
    free(p);
}

void operator delete[](void *p, size_t size) {
    free(p);
}

extern "C" void __gxx_personality_v0() {
}

extern "C" void _Unwind_Resume() {
    while(1);
}
