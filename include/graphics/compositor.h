#pragma once
#include "graphics/surface.h"
#include "ui/window.h"

#define MAX_WINDOWS 32

class Compositor {
  Window *m_windows[MAX_WINDOWS];
  int m_windowCount;

public:
  Compositor();

  void addWindow(Window *window);
  void removeWindow(Window *window);
  void render(Surface *framebuffer);
};
