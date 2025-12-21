#include "drivers/vga.h"
#include "font.h"

// VBE Mode Info Block is at 0x5200
#define VBE_MODE_INFO 0x5200

static uint32_t *get_framebuffer() {
  return *(uint32_t **)(VBE_MODE_INFO + 40);
}

static uint16_t get_pitch() { return *(uint16_t *)(VBE_MODE_INFO + 16); }

static uint16_t get_width() { return *(uint16_t *)(VBE_MODE_INFO + 18); }

static uint16_t get_height() { return *(uint16_t *)(VBE_MODE_INFO + 20); }

void set_vbe_mode(int mode) {
  // Mode is already set by bootloader.
  // We could potentially check if it matches, but for now we do nothing.
}

void vga_draw_pixel(int x, int y, uint32_t color) {
  uint32_t *fb = get_framebuffer();
  uint16_t pitch = get_pitch();

  // Assuming 32 BPP (4 bytes per pixel)
  // Offset in bytes = y * pitch + x * 4
  // We cast fb to char* to add byte offset, then cast back to uint32_t*
  uint32_t offset = y * pitch + x * 4;
  *(uint32_t *)((char *)fb + offset) = color;
}

void vga_clear_screen(uint32_t color) {
  uint32_t *fb = get_framebuffer();
  uint16_t width = get_width();
  uint16_t height = get_height();
  uint16_t pitch = get_pitch();

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint32_t offset = y * pitch + x * 4;
      *(uint32_t *)((char *)fb + offset) = color;
    }
  }
}

void vga_draw_rectangle(int x, int y, int width, int height, uint32_t color) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      vga_draw_pixel(x + col, y + row, color);
    }
  }
}

void vga_draw_circle(int x, int y, int radius, uint32_t color) {
  for (int row = 0; row < radius; row++) {
    for (int col = 0; col < radius; col++) {
      if (row * row + col * col <= radius * radius) {
        vga_draw_pixel(x + col, y + row, color);
        vga_draw_pixel(x - col, y + row, color);
        vga_draw_pixel(x + col, y - row, color);
        vga_draw_pixel(x - col, y - row, color);
      }
    }
  }
}

void vga_draw_char(int x, int y, char c, uint32_t color, int scale) {
  // Basic 8x8 font drawing
  // font8x8_basic is defined in font.h
  // It's a 1D array where each char is 8 bytes

  if (c < 0 || c > 127)
    return;

  const uint8_t *glyph = font8x8_basic[(int)c];

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // Check if bit is set (MSB first)
      if ((glyph[row] >> (7 - col)) & 1) {
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            vga_draw_pixel(x + col * scale + sx, y + row * scale + sy, color);
          }
          // vga_draw_pixel(x + col, y + row, color);
        }
      }
    }
  }
}

void vga_draw_string(int x, int y, const char *str, uint32_t color, int scale) {
  int cursor_x = x;
  int cursor_y = y;

  for (int i = 0; str[i] != '\0'; i++) {
    if (str[i] == '\n') {
      cursor_x = x;
      cursor_y += 8;
      continue;
    }
    vga_draw_char(cursor_x, cursor_y, str[i], color, scale);
    cursor_x += 8;
  }
}
