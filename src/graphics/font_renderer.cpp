#include "graphics/font_renderer.h"
#include "drivers/vga.h"

namespace font_renderer {

static unsigned char s_fontScratchBuffer[128 * 128];

void draw_text(Surface *target, int x, int y, const char *text, uint32_t color,
               float size, stbtt_fontinfo *font) {
  if (!font || !target)
    return;

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);

  float scale = stbtt_ScaleForPixelHeight(font, size);
  int baseline = y + (int)(ascent * scale);

  int cursor_x = x;
  int cursor_y = baseline;

  while (*text) {
    char c = *text++;
    int ax, lsb;
    stbtt_GetCodepointHMetrics(font, c, &ax, &lsb);

    int c_x1, c_y1, c_x2, c_y2;
    stbtt_GetCodepointBitmapBox(font, c, scale, scale, &c_x1, &c_y1, &c_x2,
                                &c_y2);

    int bitmap_width = c_x2 - c_x1;
    int bitmap_height = c_y2 - c_y1;

    if (bitmap_width <= 0 || bitmap_height <= 0 || bitmap_width > 128 ||
        bitmap_height > 128) {
      cursor_x += (int)(ax * scale);
      continue;
    }

    stbtt_MakeCodepointBitmap(font, s_fontScratchBuffer, bitmap_width,
                              bitmap_height, 128, scale, scale, c);

    for (int by = 0; by < bitmap_height; by++) {
      for (int bx = 0; bx < bitmap_width; bx++) {
        unsigned char opacity = s_fontScratchBuffer[by * 128 + bx];
        if (opacity > 0) {
          int px = cursor_x + bx + c_x1;
          int py = cursor_y + by + c_y1;

          // Bounds check against target surface
          if (px < 0 || py < 0 || px >= target->width || py >= target->height)
            continue;

          // Calculate alpha
          uint8_t textAlpha = (color >> 24) & 0xFF;
          if (textAlpha == 0)
            textAlpha = 255;

          uint8_t finalAlpha = (opacity * textAlpha) / 255;
          if (finalAlpha == 0)
            continue;

          // Blend with target surface
          uint32_t *pixelPtr = &target->pixels[py * target->pitch + px];
          uint32_t bg = *pixelPtr;

          uint8_t bgR = (bg >> 16) & 0xFF;
          uint8_t bgG = (bg >> 8) & 0xFF;
          uint8_t bgB = (bg) & 0xFF;

          uint8_t fgR = (color >> 16) & 0xFF;
          uint8_t fgG = (color >> 8) & 0xFF;
          uint8_t fgB = (color) & 0xFF;

          uint8_t bgAlpha = (bg >> 24) & 0xFF;
          uint8_t outAlpha = finalAlpha + (bgAlpha * (255 - finalAlpha)) / 255;

          uint8_t outR = (finalAlpha * fgR + (255 - finalAlpha) * bgR) / 255;
          uint8_t outG = (finalAlpha * fgG + (255 - finalAlpha) * bgG) / 255;
          uint8_t outB = (finalAlpha * fgB + (255 - finalAlpha) * bgB) / 255;

          *pixelPtr = (outAlpha << 24) | (outR << 16) | (outG << 8) | outB;
        }
      }
    }
    cursor_x += (int)(ax * scale);
  }
}

int get_text_width(const char *text, float size, stbtt_fontinfo *font) {
  if (!font)
    return 0;
  float scale = stbtt_ScaleForPixelHeight(font, size);
  int width = 0;
  while (*text) {
    char c = *text++;
    int ax, lsb;
    stbtt_GetCodepointHMetrics(font, c, &ax, &lsb);
    width += (int)(ax * scale);
  }
  return width;
}

} // namespace font_renderer
