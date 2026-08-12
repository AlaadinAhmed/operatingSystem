#include "graphics/framebuffer.h"
#include "common/boot_info.h"
#include "drivers/gop/gop.h"
#include "drivers/vga.h"
#include "graphics/surface.h"
#include "memory/kmalloc.h"
#include "print/print.h"
#include "mem/vmm.h"
#include <cstdint>

namespace framebuffer {

// Internal state
static Surface s_front_surface;
static Surface s_back_surface;
static uint32_t s_buffer_size = 0;
static bool s_initialized = false;

void init_framebuffer(const int magic, const uint64_t addr) {
  kprintf("Framebuffer: Initializing framebuffer\n");

  if (magic == UEFI_MAGIC) {
    // UEFI boot - use boot info structure
    s_front_surface.pixels = (uint32_t *)phys_to_kvirt(g_efi_boot_info.fb_addr);
    s_front_surface.width = g_efi_boot_info.width;
    s_front_surface.height = g_efi_boot_info.height;
    s_front_surface.pitch = g_efi_boot_info.pitch; // pitch in pixels
    kprintf("Framebuffer: Detected UEFI boot\n");
    gop_setup_graphics(&g_efi_boot_info);
  } else {
    // Legacy VBE boot - read from VBE mode info block
    s_front_surface.pixels = vga_get_framebuffer();
    s_front_surface.width = *(uint16_t *)(0x5200 + 18);
    s_front_surface.height = *(uint16_t *)(0x5200 + 20);
    s_front_surface.pitch =
        *(uint16_t *)(0x5200 + 16) / 4; // Convert bytes to pixels
    kprintf("Framebuffer: Detected VBE boot\n");
  }

  kprintf("Framebuffer: Resolution %dx%d, pitch=%d pixels\n",
          s_front_surface.width, s_front_surface.height, s_front_surface.pitch);

  // Calculate buffer size (pitch * height * 4 bytes per pixel)
  s_buffer_size =
      s_front_surface.pitch * s_front_surface.height * sizeof(uint32_t);

  // NOTE: Back buffer allocation disabled - requires ~8MB which exceeds
  // the current kernel heap size. Using single-buffer mode (direct
  // framebuffer).
  // TODO: Enable double-buffering once virtual memory/larger heap is available.
  s_back_surface = s_front_surface; // Single-buffer mode
  kprintf("Framebuffer: Using single-buffer mode (direct)\n");

  s_initialized = true;
  kprintf("Framebuffer: Initialization complete\n");
}

uint32_t *get_back_buffer() { return s_back_surface.pixels; }

uint32_t *get_front_buffer() { return s_front_surface.pixels; }

void update_to_hhdm() {
    // Intentionally empty.
    // s_front_surface.pixels is already mapped to the higher-half
    // via phys_to_kvirt() during init_framebuffer().
}

void commit_framebuffer() {
  if (!s_initialized || s_back_surface.pixels == s_front_surface.pixels) {
    return; // Single-buffer mode or not initialized
  }

  // Copy back buffer to front buffer (hardware framebuffer)
  memcpy(s_front_surface.pixels, s_back_surface.pixels, s_buffer_size);
}

void flush_framebuffer() {
  // Same as commit for now - could add vsync wait in the future
  commit_framebuffer();
}

uint32_t get_width() { return s_front_surface.width; }

uint32_t get_height() { return s_front_surface.height; }

uint32_t get_pitch() {
  // Return pitch in bytes for compatibility
  return s_front_surface.pitch * sizeof(uint32_t);
}

uint32_t get_buffer_size() { return s_buffer_size; }

bool is_initialized() { return s_initialized; }

void clear_screen(uint32_t color) {
  if (!s_initialized)
    return;

  for (uint32_t y = 0; y < s_back_surface.height; y++) {
    for (uint32_t x = 0; x < s_back_surface.width; x++) {
      s_back_surface.pixels[y * s_back_surface.pitch + x] = color;
    }
  }
}

void draw_pixel(int x, int y, uint32_t color) {
  if (!s_initialized)
    return;
  if (x < 0 || x >= (int)s_back_surface.width || y < 0 ||
      y >= (int)s_back_surface.height)
    return;

  uint32_t *pixelPtr = &s_back_surface.pixels[y * s_back_surface.pitch + x];

  uint8_t alpha = (color >> 24) & 0xFF;
  if (alpha == 0)
    alpha = 255; // Default to opaque

  if (alpha == 255) {
    *pixelPtr = color;
  } else {
    uint32_t bg = *pixelPtr;
    uint8_t bgR = (bg >> 16) & 0xFF;
    uint8_t bgG = (bg >> 8) & 0xFF;
    uint8_t bgB = (bg) & 0xFF;

    uint8_t fgR = (color >> 16) & 0xFF;
    uint8_t fgG = (color >> 8) & 0xFF;
    uint8_t fgB = (color) & 0xFF;

    uint8_t outR = (alpha * fgR + (255 - alpha) * bgR) / 255;
    uint8_t outG = (alpha * fgG + (255 - alpha) * bgG) / 255;
    uint8_t outB = (alpha * fgB + (255 - alpha) * bgB) / 255;

    *pixelPtr = (outR << 16) | (outG << 8) | outB;
  }
}

void draw_rect(int x, int y, int width, int height, uint32_t color) {
  if (!s_initialized)
    return;

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int px = x + col;
      int py = y + row;
      if (px >= 0 && px < (int)s_back_surface.width && py >= 0 &&
          py < (int)s_back_surface.height) {
        s_back_surface.pixels[py * s_back_surface.pitch + px] = color;
      }
    }
  }
}

void draw_rounded_rect(int x, int y, int width, int height, int radius,
                       uint32_t color) {
  if (!s_initialized)
    return;

  // Ensure radius isn't too big
  if (radius * 2 > width)
    radius = width / 2;
  if (radius * 2 > height)
    radius = height / 2;

  int r = radius;

  // Draw body (cross shape)
  // Center vertical strip (full height minus corners) - wait, safer to draw
  // horizontal strips Top horizontal strip (between corners)
  draw_rect(x + r, y, width - 2 * r, r, color);
  // Bottom horizontal strip
  draw_rect(x + r, y + height - r, width - 2 * r, r, color);
  // Middle large rect (full width, height - 2r)
  draw_rect(x, y + r, width, height - 2 * r, color);

  // Draw corners using Bresenham's circle algorithm
  int f = 1 - r;
  int ddF_x = 1;
  int ddF_y = -2 * r;
  int cx = 0;
  int cy = r;

  while (cx <= cy) {
    // Top Left Corner
    // Line at y + r - cy, from x + r - cx to x + r
    draw_rect(x + r - cx, y + r - cy, cx, 1, color);
    // Line at y + r - cx, from x + r - cy to x + r
    draw_rect(x + r - cy, y + r - cx, cy, 1, color);

    // Top Right Corner
    // Line at y + r - cy, from x + width - r to x + width - r + cx
    draw_rect(x + width - r, y + r - cy, cx, 1, color);
    // Line at y + r - cx, from x + width - r to x + width - r + cy
    draw_rect(x + width - r, y + r - cx, cy, 1, color);

    // Bottom Left Corner
    // Line at y + height - r + cy - 1, from x + r - cx to x + r
    // Note: -1 because y + height is out of bounds
    draw_rect(x + r - cx, y + height - r + cy - 1, cx, 1, color);
    draw_rect(x + r - cy, y + height - r + cx - 1, cy, 1, color);

    // Bottom Right Corner
    draw_rect(x + width - r, y + height - r + cy - 1, cx, 1, color);
    draw_rect(x + width - r, y + height - r + cx - 1, cy, 1, color);

    if (f >= 0) {
      cy--;
      ddF_y += 2;
      f += ddF_y;
    }
    cx++;
    ddF_x += 2;
    f += ddF_x;
  }
}

} // namespace framebuffer
