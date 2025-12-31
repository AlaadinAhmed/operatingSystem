#pragma once
#include "graphics/surface.h"
#include <cstdint>
class Window {
protected:
  int width;
  int height;
  int posX;
  int posY;
  int zIndex;
  char *title;

  // Buffer
  Surface m_surface;

public:
  Window(int w, int h, int x, int y);
  virtual ~Window();

  bool IsPointInside(int x, int y);
  virtual void onDraw(Surface *screen);
  virtual void onMouseDown(int x, int y, int button);
  virtual void onMouseUp(int x, int y, int button);
  virtual void onMouseMove(int x, int y);

  Surface *getSurface() { return &m_surface; }
  void move(int x, int y);
  void resize(int w, int h);
  int getWidth();
  int getHeight();
  int getX();
  int getY();
};
