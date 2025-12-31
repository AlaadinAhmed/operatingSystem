#pragma once
#include "common/boot_info.h"
#include "globals.h"
#include <cstdint>

namespace framebuffer {

// Initialize framebuffer - call once during kernel startup
void init_framebuffer(const int magic, const uint64_t addr);

// Buffer access
uint32_t* get_back_buffer();   // Get back buffer for drawing
uint32_t* get_front_buffer();  // Get hardware framebuffer

// Buffer operations
void commit_framebuffer();     // Copy back buffer to front buffer
void flush_framebuffer();      // Force immediate screen refresh

// Drawing functions
void clear_screen(uint32_t color);
void draw_pixel(int x, int y, uint32_t color);
void draw_rect(int x, int y, int width, int height, uint32_t color);
void draw_rounded_rect(int x, int y, int width, int height, int radius, uint32_t color);

// Dimension accessors
uint32_t get_width();
uint32_t get_height();
uint32_t get_pitch();          // Returns pitch in bytes
uint32_t get_buffer_size();    // Total buffer size in bytes

// Check if framebuffer is initialized
bool is_initialized();

} // namespace framebuffer
