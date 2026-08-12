#include "drivers/sse.h"

// Clears a buffer using fast hardware string instructions (ERMS)
void sse_clear_buffer(void* buffer, uint32_t size_in_bytes) {
    uint64_t qwords = size_in_bytes / 8;
    uint64_t remainder = size_in_bytes % 8;
    void* d = buffer;
    asm volatile(
        "rep stosq\n\t"
        "mov %3, %%rcx\n\t"
        "rep stosb"
        : "+D"(d), "+c"(qwords)
        : "a"(0ULL), "r"(remainder)
        : "memory"
    );
}

// Copies memory using fast hardware string instructions (ERMS)
void sse_memcpy(void* dest, const void* src, uint32_t size_in_bytes) {
    uint64_t qwords = size_in_bytes / 8;
    uint64_t remainder = size_in_bytes % 8;
    void* d = dest;
    const void* s = src;
    asm volatile(
        "rep movsq\n\t"
        "mov %3, %%rcx\n\t"
        "rep movsb"
        : "+D"(d), "+S"(s), "+c"(qwords)
        : "r"(remainder)
        : "memory"
    );
}

// Fills a buffer with a 32-bit value using fast hardware string instructions (ERMS)
void sse_fill_buffer32(void* buffer, uint32_t color, uint32_t count) {
    void* d = buffer;
    asm volatile(
        "rep stosl"
        : "+D"(d), "+c"(count)
        : "a"(color)
        : "memory"
    );
}
