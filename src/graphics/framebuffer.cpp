#include "graphics/framebuffer.h"
#include "common/boot_info.h"
#include "drivers/gop/gop.h"
#include "drivers/vga.h"
#include "memory/kmalloc.h"
#include "print/print.h"
#include <cstdint>

namespace framebuffer {

// Internal state
static uint32_t* s_back_buffer = nullptr;
static uint32_t* s_front_buffer = nullptr;
static uint32_t s_width = 0;
static uint32_t s_height = 0;
static uint32_t s_pitch = 0;  // in pixels
static uint32_t s_buffer_size = 0;
static bool s_initialized = false;

void init_framebuffer(const int magic, const uint64_t addr) {
    kprintf("Framebuffer: Initializing framebuffer\n");

    if (magic == UEFI_MAGIC) {
        // UEFI boot - use boot info structure
        s_front_buffer = (uint32_t*)(uintptr_t)g_efi_boot_info.fb_addr;
        s_width = g_efi_boot_info.width;
        s_height = g_efi_boot_info.height;
        s_pitch = g_efi_boot_info.pitch;  // pitch in pixels
        kprintf("Framebuffer: Detected UEFI boot\n");
        gop_setup_graphics(&g_efi_boot_info);
    } else {
        // Legacy VBE boot - read from VBE mode info block
        s_front_buffer = vga_get_framebuffer();
        s_width = *(uint16_t*)(0x5200 + 18);
        s_height = *(uint16_t*)(0x5200 + 20);
        s_pitch = *(uint16_t*)(0x5200 + 16) / 4;  // Convert bytes to pixels
        kprintf("Framebuffer: Detected VBE boot\n");
    }

    kprintf("Framebuffer: Resolution %dx%d, pitch=%d pixels\n", 
            s_width, s_height, s_pitch);

    // Calculate buffer size (pitch * height * 4 bytes per pixel)
    s_buffer_size = s_pitch * s_height * sizeof(uint32_t);
    
    // NOTE: Back buffer allocation disabled - requires ~8MB which exceeds
    // the current kernel heap size. Using single-buffer mode (direct framebuffer).
    // TODO: Enable double-buffering once virtual memory/larger heap is available.
    s_back_buffer = s_front_buffer;  // Single-buffer mode
    kprintf("Framebuffer: Using single-buffer mode (direct)\n");

    s_initialized = true;
    kprintf("Framebuffer: Initialization complete\n");
}

uint32_t* get_back_buffer() {
    return s_back_buffer;
}

uint32_t* get_front_buffer() {
    return s_front_buffer;
}

void commit_framebuffer() {
    if (!s_initialized || s_back_buffer == s_front_buffer) {
        return;  // Single-buffer mode or not initialized
    }
    
    // Copy back buffer to front buffer (hardware framebuffer)
    memcpy(s_front_buffer, s_back_buffer, s_buffer_size);
}

void flush_framebuffer() {
    // Same as commit for now - could add vsync wait in the future
    commit_framebuffer();
}

uint32_t get_width() {
    return s_width;
}

uint32_t get_height() {
    return s_height;
}

uint32_t get_pitch() {
    // Return pitch in bytes for compatibility
    return s_pitch * sizeof(uint32_t);
}

uint32_t get_buffer_size() {
    return s_buffer_size;
}

bool is_initialized() {
    return s_initialized;
}

void clear_screen(uint32_t color) {
    if (!s_initialized) return;
    
    for (uint32_t y = 0; y < s_height; y++) {
        for (uint32_t x = 0; x < s_width; x++) {
            s_back_buffer[y * s_pitch + x] = color;
        }
    }
}

void draw_pixel(int x, int y, uint32_t color) {
    if (!s_initialized) return;
    if (x < 0 || x >= (int)s_width || y < 0 || y >= (int)s_height) return;
    
    s_back_buffer[y * s_pitch + x] = color;
}

void draw_rect(int x, int y, int width, int height, uint32_t color) {
    if (!s_initialized) return;
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < (int)s_width && py >= 0 && py < (int)s_height) {
                s_back_buffer[py * s_pitch + px] = color;
            }
        }
    }
}

} // namespace framebuffer
