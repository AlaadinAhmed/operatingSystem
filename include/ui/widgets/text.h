#pragma once
#include "graphics/font_renderer.h"
#include "ui/window.h"

enum class Alignment { Left, Center, Right };

class Text : public Window {
  char *m_text;
  uint32_t m_color;
  float m_fontSize;
  stbtt_fontinfo *m_font;
  Alignment m_alignment;
  uint8_t m_alpha;
  bool m_autoSize;

public:
  Text(int x, int y, const char *text, uint32_t color, float fontSize,
       stbtt_fontinfo *font, int width = 0);
  ~Text();

  void onDraw(Surface *screen) override;
  void setText(const char *text);
  void setAlignment(Alignment align);
  void setAlpha(uint8_t alpha);
  void setWidth(int width);
};
