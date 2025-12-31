#include "graphics/compositor.h"
#include "drivers/vga.h"
#include "graphics/framebuffer.h"

Compositor::Compositor() : m_windowCount(0) {
  for (int i = 0; i < MAX_WINDOWS; i++)
    m_windows[i] = nullptr;
}

void Compositor::addWindow(Window *window) {
  if (m_windowCount < MAX_WINDOWS) {
    m_windows[m_windowCount++] = window;
  }
}

void Compositor::removeWindow(Window *window) {
  // Simple remove (shift remaining)
  for (int i = 0; i < m_windowCount; i++) {
    if (m_windows[i] == window) {
      for (int j = i; j < m_windowCount - 1; j++) {
        m_windows[j] = m_windows[j + 1];
      }
      m_windowCount--;
      return;
    }
  }
}

void Compositor::render(Surface *framebuffer) {
  // Clear screen (or draw background)
  // For now, assume background is already cleared or drawn by System

  // Iterate windows and blend them
  for (int i = 0; i < m_windowCount; i++) {
    Window *win = m_windows[i];
    if (!win)
      continue;

    // Draw window content to its surface first
    win->onDraw(win->getSurface());

    // Blend window surface to framebuffer
    Surface *winSurf = win->getSurface();
    if (!winSurf || !winSurf->pixels)
      continue;

    int wx = win->getX();
    int wy = win->getY();

    for (int y = 0; y < winSurf->height; y++) {
      for (int x = 0; x < winSurf->width; x++) {
        int destX = wx + x;
        int destY = wy + y;

        if (destX >= 0 && destX < framebuffer->width && destY >= 0 &&
            destY < framebuffer->height) {
          uint32_t color = winSurf->pixels[y * winSurf->pitch + x];

          // Manual alpha blending since we are accessing raw buffers
          uint8_t alpha = (color >> 24) & 0xFF;
          // if (alpha == 0) alpha = 255; // REMOVED: 0 is transparent

          if (alpha == 0)
            continue; // Skip transparent pixels

          if (alpha == 255) {
            framebuffer->pixels[destY * framebuffer->pitch + destX] = color;
          } else {
            uint32_t bg =
                framebuffer->pixels[destY * framebuffer->pitch + destX];
            uint8_t bgR = (bg >> 16) & 0xFF;
            uint8_t bgG = (bg >> 8) & 0xFF;
            uint8_t bgB = (bg) & 0xFF;

            uint8_t fgR = (color >> 16) & 0xFF;
            uint8_t fgG = (color >> 8) & 0xFF;
            uint8_t fgB = (color) & 0xFF;

            uint8_t outR = (alpha * fgR + (255 - alpha) * bgR) / 255;
            uint8_t outG = (alpha * fgG + (255 - alpha) * bgG) / 255;
            uint8_t outB = (alpha * fgB + (255 - alpha) * bgB) / 255;

            framebuffer->pixels[destY * framebuffer->pitch + destX] =
                (outR << 16) | (outG << 8) | outB;
          }
        }
      }
    }
  }
}
