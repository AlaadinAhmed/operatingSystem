#include "ui/window.h"
#include "memory/kmalloc.h"

Window::Window(int w, int h, int x, int y)
    : width(w), height(h), posX(x), posY(y), zIndex(0), title(nullptr) {
  m_surface.width = w;
  m_surface.height = h;
  m_surface.pitch = w;
  m_surface.pixels = (uint32_t *)kmalloc(w * h * sizeof(uint32_t));

  // Clear to transparent
  if (m_surface.pixels) {
    for (int i = 0; i < w * h; i++)
      m_surface.pixels[i] = 0x00000000;
  }
}

Window::~Window() {
  if (m_surface.pixels) {
    kfree(m_surface.pixels);
    m_surface.pixels = nullptr;
  }
}

void Window::move(int x, int y) {
  posX = x;
  posY = y;
}

void Window::resize(int w, int h) {
  if (w == width && h == height)
    return;

  if (m_surface.pixels) {
    kfree(m_surface.pixels);
  }

  width = w;
  height = h;
  m_surface.width = w;
  m_surface.height = h;
  m_surface.pitch = w;
  m_surface.pixels = (uint32_t *)kmalloc(w * h * sizeof(uint32_t));

  if (m_surface.pixels) {
    for (int i = 0; i < w * h; i++)
      m_surface.pixels[i] = 0x00000000;
  }
}

int Window::getWidth() { return width; }
int Window::getHeight() { return height; }
int Window::getX() { return posX; }
int Window::getY() { return posY; }

bool Window::IsPointInside(int x, int y) {
  return (x >= posX && x < posX + width && y >= posY && y < posY + height);
}

void Window::onDraw(Surface *screen) {}
void Window::onMouseDown(int x, int y, int button) {}
void Window::onMouseUp(int x, int y, int button) {}
void Window::onMouseMove(int x, int y) {}
