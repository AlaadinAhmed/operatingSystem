#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h> // For size_t
#include <stdint.h> // For uint8_t, uint16_t, etc.

#ifdef __cplusplus
extern "C" {
#endif

void* kmalloc(size_t size);
void* krealloc(void* ptr, size_t size);
void* kcalloc(size_t nmemb, size_t size);
void kfree(void* ptr);
void init_memory();
int abs(int x);

extern "C" {
extern void* memcpy(void* dest, const void* src, size_t n) noexcept;
extern void* memset(void* s, int c, size_t n) noexcept;
extern void* memset16(void* s, uint16_t c, size_t count) noexcept;
extern void* memset32(void* s, uint32_t c, size_t count) noexcept;
extern void* memset64(void* s, uint64_t c, size_t count) noexcept;
}

// Helper function to read a file from lwext4 into memory
unsigned char* read_file_to_memory(const char* mount_point, const char* filename, size_t* file_size);

#ifdef __cplusplus
}

// C++ Overloads for easy typed memory filling
// NOTE: "count" is the number of elements to write, NOT bytes!
inline void* memory_fill(uint16_t* dest, uint16_t value, size_t count) {
    return memset16((void*)dest, value, count);
}
inline void* memory_fill(uint32_t* dest, uint32_t value, size_t count) {
    return memset32((void*)dest, value, count);
}
inline void* memory_fill(uint64_t* dest, uint64_t value, size_t count) {
    return memset64((void*)dest, value, count);
}
inline void* memory_fill(uint8_t* dest, uint8_t value, size_t count) {
    return memset((void*)dest, (int)value, count);
}

#endif // __cplusplus

#endif // KMALLOC_H
