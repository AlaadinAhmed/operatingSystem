#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory operations
#ifndef __EFI__
void *memset(void *s, int c, size_t n) noexcept;
void *memset16(void *s, uint16_t c, size_t count) noexcept;
void *memset32(void *s, uint32_t c, size_t count) noexcept;
void *memset64(void *s, uint64_t c, size_t count) noexcept;
void *memcpy(void *dest, const void *src, size_t n) noexcept;
int memcmp(const void *s1, const void *s2, size_t n);
#endif

// Stack protection fail handler
void __stack_chk_fail(void);

// String operations
size_t strlen(const char *s);
void *memmove(void *dest, const void *src, size_t n);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
int strncmp(const char *s1, const char *s2, size_t n);

// Math / Misc
int abs(int x);

// File loading helpers
unsigned char *read_file_to_memory(const char *mount_point, const char *filename, size_t *file_size);

// lwext4 user-provided memory functions
void *ext4_user_malloc(size_t size);
void *ext4_user_calloc(size_t nmemb, size_t size);
void *ext4_user_realloc(void *ptr, size_t size);
void ext4_user_free(void *ptr);

#ifdef __cplusplus
}
#endif
