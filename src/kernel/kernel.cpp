#include "disk/disk.h"
#include "drivers/font.h"
#include "drivers/vga.h"
#include "fs/lwext4_adapter.h"
#include "print/print.h"
#include <cstdint>
#include <ext4.h>
// #define STB_TRUETYPE_IMPLEMENTATION
#include "drivers/roboto_font.h"
#include "stb_truetype.h"

// Simple roundf implementation for freestanding environment
static float roundf(float x) {
  return (x >= 0.0f) ? (int)(x + 0.5f) : (int)(x - 0.5f);
}

stbtt_fontinfo font;

void init_ttf() {
  if (!stbtt_InitFont(&font, roboto_font, 0)) {
    print("Font init failed\n");
  }
}

void draw_ttf_text(const char *text, int x, int y, float size, uint32_t color) {
  float scale = stbtt_ScaleForPixelHeight(&font, size); // Set font size

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
  ascent = roundf(ascent * scale);
  descent = roundf(descent * scale);

  int xpos = x;
  static unsigned char glyph_buffer[128 * 128]; // 16KB static buffer

  while (*text) {
    int advance, lsb, x0, y0, x1, y1;
    int codepoint = *text;
    stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);
    stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1,
                                &y1);

    int w = x1 - x0;
    int h = y1 - y0;

    if (w > 0 && h > 0 && w <= 128 && h <= 128) {
      stbtt_MakeCodepointBitmap(&font, glyph_buffer, w, h, w, scale, scale,
                                codepoint);

      for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
          int alpha = glyph_buffer[j * w + i];
          if (alpha > 128) {
            vga_draw_pixel(xpos + x0 + i, y + y0 + j + ascent, color);
          }
        }
      }
    }

    xpos += roundf(advance * scale);
    if (*text + 1)
      xpos += scale * stbtt_GetCodepointKernAdvance(&font, *text, *(text + 1));

    // DEBUG: Print char being drawn
    // char c = *text;
    // char debug_buf[2] = {c, '\0'};
    // print(debug_buf);

    ++text;
  }
  // print("\n");
}

extern "C" void init_memory();

extern "C" void main() {
  init_memory();

  // VBE Mode Info is at 0x5200
  // Framebuffer address is at offset 40 (0x28) in VbeModeInfoBlock
  uint32_t *fb = *(uint32_t **)(0x5200 + 40);
  uint16_t width = *(uint16_t *)(0x5200 + 18);
  uint16_t height = *(uint16_t *)(0x5200 + 20);
  uint16_t pitch = *(uint16_t *)(0x5200 + 16);

  // Clear screen to black
  vga_clear_screen(0x000000);

  // DEBUG: Draw a red rectangle to verify VGA works
  vga_draw_rectangle(10, 10, 100, 100, 0xFF0000);

  print("Attempting to init font...\n");
  init_font(); // Keep original init_font if it does something else, but we need
               // init_ttf
  init_ttf();
  print("Font init called.\n");

  Initialize lwext4 struct ext4_blockdev *bdev = fs::get_lwext4_blockdev();
  ext4_device_register(bdev, "ext4_fs");

  int r = ext4_mount("ext4_fs", "/", false);
  if (r == EOK) {
    // Open a file
    ext4_file f;
    r = ext4_fopen(&f, "/logo.bmp", "r");
    if (r == EOK) {
      ext4_fclose(&f);
    }
  }
  long timeLapsed = 0;
  print("Starting Main Loop\n");
  while (1) {
    vga_clear_screen(0x000000);
    // Redraw the debug rectangle every frame to be sure
    vga_draw_rectangle(10, 10, 100, 100, 0xFF0000);

    draw_ttf_text("Hello World", 100, 400, 20, 0xFFFFFF);
    timeLapsed++;

    if (timeLapsed % 100 == 0) { // Every 100 iterations
      draw_ttf_text("Hello World", 100, 400, 40, 0xFFFFFF);
    } else {
      draw_ttf_text("Hello World", 100, 400, 10, 0xFFFFFF);
    }
    char buffer[64];               // Allocate a fixed buffer on the stack
    int seconds = timeLapsed / 10; // Speed up for debugging
    ksprintf(buffer, "Time Lapsed: %ds", seconds);

    // DEBUG: Print buffer to serial to verify ksprintf
    if (timeLapsed % 10 == 0) {
      print("Buffer content: [");
      print(buffer);
      print("]\n");
    }

    draw_ttf_text(buffer, 100, 500, 20, 0xFFFFFF);

    if (timeLapsed % 10 == 0) {
      print(buffer);
      print("\n");
    }
  }
}
