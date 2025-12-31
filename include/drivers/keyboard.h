#pragma once
#include <cstdint>
void init_keyboard();
char kgetchar();
void keyboard_on_interrupt();
void keyboard_handler(uint8_t scancode, uint8_t &state);
