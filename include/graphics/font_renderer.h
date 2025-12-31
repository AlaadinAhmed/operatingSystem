#pragma once
#include "graphics/surface.h"
#include "stb_truetype.h"
#include <cstdint>

namespace font_renderer {

void draw_text(Surface *target, int x, int y, const char *text, uint32_t color,
               float size, stbtt_fontinfo *font);
int get_text_width(const char *text, float size, stbtt_fontinfo *font);

} // namespace font_renderer
