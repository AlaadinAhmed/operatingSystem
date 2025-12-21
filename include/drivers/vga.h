#ifndef VGA_H
#define VGA_H

#include <stdint.h>

void vga_clear_screen(uint32_t color);
void vga_draw_pixel(int x, int y, uint32_t color);
void vga_draw_circle(int x, int y, int radius, uint32_t color);
void vga_draw_rectangle(int x, int y, int width, int height, uint32_t color);
void vga_draw_char(int x, int y, char c, uint32_t color, int scale = 1);
void vga_draw_string(int x, int y, const char *str, uint32_t color,
                     int scale = 1);

// Dummy function for compatibility
void set_vbe_mode(int mode);

#endif
