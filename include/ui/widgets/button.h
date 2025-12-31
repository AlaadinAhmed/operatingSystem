#pragma once
#include "graphics/font_renderer.h"
#include "ui/window.h"

class Button : public Window {
  bool pressed;
  char *label;
  uint32_t color;
  uint32_t borderColor;
  int borderSize;
  int m_radius;
  stbtt_fontinfo *font;
  void (*onClickCallback)();
  uint32_t textColor;

public:
  Button(int w, int h, int x, int y, const char *text, uint32_t color,
         uint32_t borderColor, int borderSize, stbtt_fontinfo *font,
         void (*onClick)(), int radius, uint32_t textColor);
  void onDraw(Surface *screen) override;
  void onMouseDown(int x, int y, int button) override;
  void onMouseUp(int x, int y, int button) override;
  void setText(const char *text);
  void setColor(uint32_t color);
};
