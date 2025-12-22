#include "drivers/sse.h"

// Clears a buffer using SSE unaligned stores.
void sse_clear_buffer(void* buffer, uint32_t size_in_bytes) {
    __m128i zero = _mm_setzero_si128();
    __m128i* ptr = (__m128i*)buffer;
    uint32_t count = size_in_bytes / 16;

    for (uint32_t i = 0; i < count; ++i) {
        _mm_storeu_si128(ptr + i, zero);
    }
}

// Copies memory using SSE unaligned loads and stores.
void sse_memcpy(void* dest, const void* src, uint32_t size_in_bytes) {
    __m128i* d = (__m128i*)dest;
    const __m128i* s = (const __m128i*)src;
    uint32_t count = size_in_bytes / 16;

    for (uint32_t i = 0; i < count; ++i) {
        __m128i data = _mm_loadu_si128(s + i);
        _mm_storeu_si128(d + i, data);
    }
}
