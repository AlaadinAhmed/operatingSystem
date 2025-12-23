#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h> // For size_t

#ifdef __cplusplus
extern "C" {
#endif

void* kmalloc(size_t size);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);
void init_memory();
int abs(int x);

// Declarations for utility functions implemented in utils.cpp
extern "C" {
extern void* memcpy(void* dest, const void* src, size_t n) noexcept;
extern void* memset(void* s, int c, size_t n) noexcept;
}

// Helper function to read a file from lwext4 into memory
unsigned char* read_file_to_memory(const char* mount_point, const char* filename, size_t* file_size);

#ifdef __cplusplus
}
#endif

#endif // KMALLOC_H
