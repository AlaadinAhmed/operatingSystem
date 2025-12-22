#include <xmmintrin.h>

void fast_clear(uint32_t *buffer, uint32_t color) {
  // Fill an SSE register with 4 copies of the color
  __m128i color_v = _mm_set1_epi32(color);

  // Process 4 pixels at a time (16 bytes per loop)
  for (int i = 0; i < (1920 * 1080); i += 4) {
    _mm_store_si128((__m128i *)&buffer[i], color_v);
  }
}
