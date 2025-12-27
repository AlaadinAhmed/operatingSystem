#pragma once
#include "common/boot_info.h"
void gop_setup_graphics(struct BootInfo *info);

uint64_t gop_get_framebuffer_addr(int x, int y);

void gop_clear_screen(uint32_t color);

void gop_draw_pixel(int x, int y, uint32_t color);

void gop_draw_char(int x, int y, char c, uint32_t color);

void gop_draw_string(int x, int y, const char *str, uint32_t color);
