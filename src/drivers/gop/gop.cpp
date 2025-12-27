#include "gop.h"
#include "drivers/font_data.h"
#include "print/print.h"
struct Pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};
void gop_setup_graphics(struct BootInfo *info) {
  g_efi_boot_info.fb_addr = info->fb_addr;
  g_efi_boot_info.width = info->width;
  g_efi_boot_info.height = info->height;
  g_efi_boot_info.pitch = info->pitch;
}
uint64_t gop_get_framebuffer_addr(int x, int y) {
  uint64_t fb_addr = g_efi_boot_info.fb_addr +
                     ((((uint64_t)y * g_efi_boot_info.pitch) + x) * 4);
  return fb_addr;
}

void gop_clear_screen(uint32_t color) {
  uint32_t *fb = (uint32_t *)((uint64_t)g_efi_boot_info.fb_addr);
  uint32_t width = g_efi_boot_info.width;
  uint32_t height = g_efi_boot_info.height;
  uint32_t pitch = g_efi_boot_info.pitch;
  uint32_t totalpixels = pitch * height;
  for (uint32_t i = 0; i < totalpixels; i++) {
    fb[i] = color;
  }
}

void gop_draw_pixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= (int)g_efi_boot_info.width || y < 0 ||
      y >= (int)g_efi_boot_info.height)
    return;
  uint32_t *fb = (uint32_t *)((uint64_t)g_efi_boot_info.fb_addr);
  uint32_t pitch = g_efi_boot_info.pitch;
  uint32_t offset = y * pitch + x;
  fb[offset] = color;
}

void gop_draw_char(int x, int y, char c, uint32_t color) {
  extern const uint16_t font16x16_basic[128][16];
  extern const uint8_t font16x16_width[128];

  if (c < 0 || c >= 128)
    return;

  const uint16_t *char_bitmap = font16x16_basic[(int)c];
  uint8_t char_width = font16x16_width[(int)c];

  for (int row = 0; row < 16; row++) {
    uint16_t row_data = char_bitmap[row];
    for (int col = 0; col < char_width; col++) {
      if (row_data & (1 << (15 - col))) {
        gop_draw_pixel(x + col, y + row, color);
      }
    }
  }
}

void gop_draw_string(int x, int y, const char *str, uint32_t color) {
  int cursor_x = x;
  int cursor_y = y;
  while (*str) {
    if (*str == '\n') {
      cursor_x = x;
      cursor_y += 16; // Move to next line
    } else {
      gop_draw_char(cursor_x, cursor_y, *str, color);
      cursor_x += font16x16_width[(int)(*str)];
    }
    str++;
  }
}
