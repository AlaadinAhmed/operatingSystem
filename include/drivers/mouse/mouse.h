#pragma once

struct MouseState {
  int x;
  int y;
  bool buttons[3]; // Left, Right, Middle
};
void mouse_init(float sensitivity = 10.0f);
MouseState mouse_update();
void mouse_get_position(int &x, int &y);
void mouse_set_position(int x, int y);
bool mouse_is_button_pressed(int button); // 0: left, 1: right, 2: middle
void mouse_on_interrupt();
