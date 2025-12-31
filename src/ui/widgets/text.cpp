#include "ui/widgets/text.h"
#include "graphics/font_renderer.h"
#include "memory/kmalloc.h"

// Helper for strdup since we might not have it in standard lib yet or it's in a
// different header
static char *my_strdup(const char *s) {
  if (!s)
    return nullptr;
  int len = 0;
  while (s[len])
    len++;
  char *new_str = (char *)kmalloc(len + 1);
  for (int i = 0; i <= len; i++)
    new_str[i] = s[i];
  return new_str;
}

Text::Text(int x, int y, const char *text, uint32_t color, float fontSize,
           stbtt_fontinfo *font, int width)
    : Window(width, 0, x, y), m_color(color), m_fontSize(fontSize),
      m_font(font), m_alignment(Alignment::Left), m_alpha(255),
      m_autoSize(width == 0) {
  m_text = my_strdup(text);

  // Calculate width and height based on text if autoSize
  if (m_font && m_text) {
    int textW = font_renderer::get_text_width(m_text, m_fontSize, m_font);
    int h = (int)m_fontSize; // Approximate
    if (m_autoSize) {
      resize(textW, h);
    } else {
      resize(width, h);
    }
  }
}

Text::~Text() {
  if (m_text)
    kfree(m_text);
}

void Text::setText(const char *text) {
  if (m_text)
    kfree(m_text);
  m_text = my_strdup(text);

  if (m_font && m_text) {
    int w = font_renderer::get_text_width(m_text, m_fontSize, m_font);
    int h = (int)m_fontSize;
    if (m_autoSize) {
      resize(w, h);
    }
  }
}

void Text::setAlignment(Alignment align) { m_alignment = align; }

void Text::setAlpha(uint8_t alpha) { m_alpha = alpha; }

void Text::setWidth(int width) {
  m_autoSize = (width == 0);
  if (!m_autoSize) {
    resize(width, getHeight());
  } else if (m_font && m_text) {
    int w = font_renderer::get_text_width(m_text, m_fontSize, m_font);
    resize(w, getHeight());
  }
}

void Text::onDraw(Surface *screen) {
  if (!screen)
    screen = getSurface();
  if (!screen)
    return;

  // Clear background to transparent
  for (int i = 0; i < screen->width * screen->height; i++) {
    screen->pixels[i] = 0x00000000;
  }

  if (m_text && m_font) {
    int x = 0; // Relative to window surface
    int textWidth = font_renderer::get_text_width(m_text, m_fontSize, m_font);

    if (m_alignment == Alignment::Center) {
      x += (getWidth() - textWidth) / 2;
    } else if (m_alignment == Alignment::Right) {
      x += getWidth() - textWidth;
    }

    // Apply alpha to color (assuming 0xAARRGGBB format for font_renderer)
    uint8_t colorAlpha = (m_color >> 24) & 0xFF;
    if (colorAlpha == 0)
      colorAlpha = 255;

    uint8_t finalAlpha = (colorAlpha * m_alpha) / 255;
    uint32_t drawColor = (m_color & 0x00FFFFFF) | ((uint32_t)finalAlpha << 24);

    font_renderer::draw_text(screen, x, 0, m_text, drawColor, m_fontSize,
                             m_font);
  }
}
