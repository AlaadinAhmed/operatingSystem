#ifndef SSE_H
#define SSE_H

// This is a workaround to avoid including the problematic mm_malloc.h header
// which requires malloc/free, not available in our freestanding kernel.
#define _MM_MALLOC_H_INCLUDED

#include <xmmintrin.h>
#include <stdint.h>

void sse_memcpy(void* dest, const void* src, uint32_t size_in_bytes);
void sse_clear_buffer(void* buffer, uint32_t size_in_bytes);
void sse_fill_buffer32(void* buffer, uint32_t color, uint32_t count);

#endif
