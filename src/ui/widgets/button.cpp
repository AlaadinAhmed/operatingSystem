#include "ui/widgets/button.h"
#include "graphics/font_renderer.h"
#include "memory/kmalloc.h"

// Helper for string length
static int strlen(const char *str) {
  int len = 0;
  while (str[len])
    len++;
  return len;
}

// Helper for string copy
static char *strdup(const char *str) {
  int len = strlen(str);
  char *new_str = (char *)kmalloc(len + 1);
  for (int i = 0; i <= len; i++) {
    new_str[i] = str[i];
  }
  return new_str;
}

Button::Button(int w, int h, int x, int y, const char *text, uint32_t color,
               uint32_t borderColor, int borderSize, stbtt_fontinfo *font,
               void (*onClick)(), int radius, uint32_t textColor)
    : Window(w, h, x, y), pressed(false), color(color),
      borderColor(borderColor), borderSize(borderSize), m_radius(radius),
      font(font), onClickCallback(onClick), textColor(textColor) {
  label = strdup(text);
}

void Button::onDraw(Surface *screen) {
  if (!screen)
    screen = getSurface();
  if (!screen || !screen->pixels)
    return;

  uint32_t drawColor =
      pressed ? (color & 0xFEFEFE) >> 1 : color; // Darker if pressed

  // Fill background with rounded corners
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      bool drawn = false;

      // Check corners
      if (m_radius > 0) {
        int dx = -1, dy = -1;
        if (x < m_radius && y < m_radius) { // TL
          dx = m_radius - x;
          dy = m_radius - y;
        } else if (x >= width - m_radius && y < m_radius) { // TR
          dx = x - (width - 1 - m_radius);
          dy = m_radius - y;
        } else if (x < m_radius && y >= height - m_radius) { // BL
          dx = m_radius - x;
          dy = y - (height - 1 - m_radius);
        } else if (x >= width - m_radius && y >= height - m_radius) { // BR
          dx = x - (width - 1 - m_radius);
          dy = y - (height - 1 - m_radius);
        }

        if (dx >= 0 && dy >= 0) {
          int distSq = dx * dx + dy * dy;
          if (distSq > m_radius * m_radius) {
            // Outside corner - transparent
            screen->pixels[y * screen->pitch + x] = 0x00000000;
            drawn = true;
          } else if (borderSize > 0 && distSq > (m_radius - borderSize) *
                                                    (m_radius - borderSize)) {
            // Border
            screen->pixels[y * screen->pitch + x] = borderColor;
            drawn = true;
          }
        }
      }

      if (!drawn) {
        // Check linear borders
        if (borderSize > 0 && (x < borderSize || x >= width - borderSize ||
                               y < borderSize || y >= height - borderSize)) {
          // If we are in a corner region but inside the radius, we might
          // overwrite border? The corner logic above handles the rounded part.
          // Here we handle the straight parts.
          // We need to be careful not to overwrite the "inner" part if we are
          // in a corner bounding box but not "in the corner" Actually, simpler
          // approach: If not drawn yet, just check standard border rects. But
          // we need to exclude the corner squares if radius > 0? No, the corner
          // logic only handles the actual rounded area.

          // Let's simplify:
          // If we are here, we are NOT outside the rounded corner.
          // We just need to check if we are in the border region.

          // Distance to edge
          int distToEdge = x;
          if (y < distToEdge)
            distToEdge = y;
          if (width - 1 - x < distToEdge)
            distToEdge = width - 1 - x;
          if (height - 1 - y < distToEdge)
            distToEdge = height - 1 - y;

          if (distToEdge < borderSize) {
            screen->pixels[y * screen->pitch + x] = borderColor;
          } else {
            screen->pixels[y * screen->pitch + x] = drawColor;
          }
        } else {
          screen->pixels[y * screen->pitch + x] = drawColor;
        }
      }
    }
  }

  // Draw Text (Centered)
  if (label && font) {
    float fontSize = 16.0f;
    int textWidth = font_renderer::get_text_width(label, fontSize, font);

    // Center in the button (relative to 0,0)
    int textX = (width - textWidth) / 2;

    // Vertical centering is now handled by font_renderer using baseline
    // We want the middle of the text to be at height/2
    // But font_renderer takes the baseline Y.
    // Let's pass the top-left of the text box and let font_renderer handle
    // baseline? No, I changed font_renderer to take Y as top-left. So we
    // calculate top-left Y.
    int textHeight = (int)fontSize; // Approximate
    int textY = (height - textHeight) / 2;

    // Use Black (0x000000) for text color
    font_renderer::draw_text(screen, textX, textY, label, textColor, fontSize,
                             font);
  }
}

void Button::onMouseDown(int x, int y, int button) {
  if (IsPointInside(x, y)) {
    pressed = true;
    // Request redraw (in a real OS, we'd invalidate the rect)
    onDraw(nullptr); // nullptr because we use global framebuffer for now
  }
}

void Button::onMouseUp(int x, int y, int button) {
  if (pressed) {
    if (IsPointInside(x, y)) {
      if (onClickCallback) {
        onClickCallback();
      }
    }
    pressed = false;
    onDraw(nullptr);
  }
}

void Button::setText(const char *text) {
  if (label) {
    kfree(label);
  }
  label = strdup(text);
  onDraw(nullptr);
}

void Button::setColor(uint32_t newColor) {
  color = newColor;
  onDraw(nullptr);
}
