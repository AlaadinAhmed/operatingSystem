#include "drivers/vga.h"
#include "drivers/font_data.h"
#include <cstdint>

// VBE Mode Info Block is at 0x5200
#define VBE_MODE_INFO 0x5200

// Public function to get the hardware framebuffer address
uint32_t *vga_get_framebuffer() { return *(uint32_t **)(VBE_MODE_INFO + 40); }

static uint16_t get_pitch() { return *(uint16_t *)(VBE_MODE_INFO + 16); }
static uint16_t get_width() { return *(uint16_t *)(VBE_MODE_INFO + 18); }
static uint16_t get_height() { return *(uint16_t *)(VBE_MODE_INFO + 20); }

// --- Buffer-based drawing functions ---

void vga_draw_pixel(uint32_t *buffer, int x, int y, uint32_t color) {
  if (x < 0 || x >= get_width() || y < 0 || y >= get_height())
    return;

  uint16_t pitch = get_pitch();
  uint32_t offset = y * pitch + x * 4;
  *(uint32_t *)((char *)buffer + offset) = color;
}

void vga_clear_buffer(uint32_t *buffer, uint32_t color) {
  uint16_t width = get_width();
  uint16_t height = get_height();
  uint16_t pitch = get_pitch();

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint32_t offset = y * pitch + x * 4;
      *(uint32_t *)((char *)buffer + offset) = color;
    }
  }
}

void fast_clear_buffer(uint32_t *buffer) { vga_clear_buffer(buffer, 0x000000); }

void vga_draw_rectangle(uint32_t *buffer, int x, int y, int width, int height,
                        uint32_t color) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      vga_draw_pixel(buffer, x + col, y + row, color);
    }
  }
}

// Helper function to draw a horizontal line, used by the optimized circle
// function.
static void vga_draw_hline(uint32_t *buffer, int x1, int x2, int y,
                           uint32_t color) {
  if (x1 > x2) {
    int temp = x1;
    x1 = x2;
    x2 = temp;
  }
  for (int x = x1; x <= x2; x++) {
    vga_draw_pixel(buffer, x, y, color);
  }
}

// Optimized algorithm for drawing a filled circle.
void vga_draw_circle(uint32_t *buffer, int x0, int y0, int radius,
                     uint32_t color) {
  if (radius < 0)
    return;
  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;
  while (y >= x) {
    vga_draw_hline(buffer, x0 - x, x0 + x, y0 - y, color);
    vga_draw_hline(buffer, x0 - x, x0 + x, y0 + y, color);
    vga_draw_hline(buffer, x0 - y, x0 + y, y0 - x, color);
    vga_draw_hline(buffer, x0 - y, x0 + y, y0 + x, color);
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    } else {
      d = d + 4 * x + 6;
    }
  }
}

// --- Old functions updated to use the new buffer-based ones ---

void vga_clear_screen(uint32_t color) {
  vga_clear_buffer(vga_get_framebuffer(), color);
}

void fast_clear() { fast_clear_buffer(vga_get_framebuffer()); }

void vga_draw_char(int x, int y, char c, uint32_t color, int scale) {
  if (c < 0 || c > 127)
    return;

  uint32_t *buffer = vga_get_framebuffer();
  const uint8_t *glyph = font8x8_basic[(int)c];

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if ((glyph[row] >> (7 - col)) & 1) {
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            vga_draw_pixel(buffer, x + col * scale + sx, y + row * scale + sy,
                           color);
          }
        }
      }
    }
  }
}

static int console_x = 0;
static int console_y = 0;

void vga_console_putc(char c) {
  if (c == '\n') {
    console_x = 0;
    console_y += 16; // 8 * scale 2
    return;
  }
  if (c == '\b') {
    if (console_x >= 16) {
      console_x -= 16;
      // Erase the character
      vga_draw_rectangle(vga_get_framebuffer(), console_x, console_y, 16, 16,
                         0x000000);
    } else if (console_y >= 16) {
      console_y -= 16;
      console_x = get_width() - 16;
      // Erase the character
      vga_draw_rectangle(vga_get_framebuffer(), console_x, console_y, 16, 16,
                         0x000000);
    }
    return;
  }
  vga_draw_char(console_x, console_y, c, 0xFFFFFF, 2);
  console_x += 16;
  if (console_x >= get_width()) {
    console_x = 0;
    console_y += 16;
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
    cursor_x += 8 * scale;
  }
}
