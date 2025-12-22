#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// --- Double Buffering API ---
// Gets the hardware framebuffer address
uint32_t* vga_get_framebuffer();

// Drawing functions that operate on a given buffer
void vga_draw_pixel(uint32_t* buffer, int x, int y, uint32_t color);
void vga_draw_rectangle(uint32_t* buffer, int x, int y, int width, int height, uint32_t color);
void vga_draw_circle(uint32_t* buffer, int x, int y, int radius, uint32_t color);
void vga_clear_buffer(uint32_t* buffer, uint32_t color);
void fast_clear_buffer(uint32_t* buffer);
// --- End Double Buffering API ---


// --- Original API (deprecated for animation, used for startup) ---
void vga_clear_screen(uint32_t color);
void vga_draw_char(int x, int y, char c, uint32_t color, int scale = 1);
void vga_draw_string(int x, int y, const char *str, uint32_t color,
                     int scale = 1);
void vga_console_putc(char c);
void fast_clear();
// --- End Original API ---


#endif
