#include "drivers/keyboard.h"
#include "drivers/input/scancodes.h"
#include "print/print.h"
#include <stdint.h>

// Helper functions to read from I/O ports
// Helper functions to read from I/O ports
// inb is defined in print.h or utils

// Scancode map for US keyboard layout (Unshifted)
static const char scancode_to_ascii[] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '\b', '\t', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '\n', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   '-', 0,   0,   0,
    '+', 0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0};

// Scancode map for US keyboard layout (Shifted)
static const char shift_scancode_to_ascii[] = {
    0,   27,   '!',  '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',
    '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}',  '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '"',  '~',  0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<',
    '>', '?',  0,    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,
    '+', 0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0};

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;
static int super_pressed = 0;
static int caps_lock = 0;

void init_keyboard() {
  // Keyboard initialization if needed (e.g., setting LED states)
  // For now, this is empty.
}

// Reads a single character from the keyboard.
// This is a blocking, polling-based implementation.
char kgetchar() {
  while (1) {
    // Check the status port (0x64) to see if the output buffer is full
    if (inb(0x64) & 0x01) {
      uint8_t scancode = inb(0x60); // Read the scancode from the data port

      // Handle Shift Press
      if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        continue;
      }
      // Handle Shift Release
      if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        continue;
      }

      // Ignore other key releases (scancodes >= 0x80)
      if (scancode & 0x80) {
        continue;
      }

      // We only care about key presses (scancodes < 0x80)
      if (scancode < sizeof(scancode_to_ascii)) {
        char c = 0;
        if (shift_pressed) {
          c = shift_scancode_to_ascii[scancode];
        } else {
          c = scancode_to_ascii[scancode];
        }

        // Handle Caps Lock for letters
        if (caps_lock) {
          if (c >= 'a' && c <= 'z')
            c -= 32;
          else if (c >= 'A' && c <= 'Z')
            c += 32;
        }

        if (c) {
          return c;
        }
      }
    }
  }
}

void keyboard_on_interrupt() {
  static uint8_t state = NORMAL;
  uint8_t status = inb(0x64);
  if (status & 0x01) {
    uint8_t scancode = inb(0x60);
    keyboard_handler(scancode, state);
  }
}

void keyboard_handler(uint8_t scancode, uint8_t &state) {
  switch (state) {
  case NORMAL:
    if (scancode == 0xE0) {
      state = EXPECT_E0;
      return;
    }

    // Handle Shift
    if (scancode == 0x2A || scancode == 0x36) {
      shift_pressed = 1;
      return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
      shift_pressed = 0;
      return;
    }

    // Handle Left Ctrl
    if (scancode == 0x1D) {
      ctrl_pressed = 1;
      kprintf("CTRL Pressed\n");
      return;
    }
    if (scancode == 0x9D) {
      ctrl_pressed = 0;
      kprintf("CTRL Released\n");
      return;
    }

    // Handle Left Alt
    if (scancode == 0x38) {
      alt_pressed = 1;
      kprintf("ALT Pressed\n");
      return;
    }
    if (scancode == 0xB8) {
      alt_pressed = 0;
      kprintf("ALT Released\n");
      return;
    }

    // Handle Caps Lock (Toggle on press)
    if (scancode == 0x3A) {
      caps_lock = !caps_lock;
      kprintf("Caps Lock: %s\n", caps_lock ? "ON" : "OFF");
      return;
    }

    // Handle Function Keys (F1-F12)
    if (scancode >= 0x3B && scancode <= 0x44) { // F1-F10
      kprintf("F%d Pressed\n", scancode - 0x3B + 1);
      return;
    }
    if (scancode == 0x57) {
      kprintf("F11 Pressed\n");
      return;
    }
    if (scancode == 0x58) {
      kprintf("F12 Pressed\n");
      return;
    }

    // Ignore other key releases (scancodes >= 0x80)
    if (scancode & 0x80) {
      return;
    }

    // We only care about key presses (scancodes < 0x80)
    if (scancode < sizeof(scancode_to_ascii)) {
      char c = 0;
      if (shift_pressed) {
        c = shift_scancode_to_ascii[scancode];
      } else {
        c = scancode_to_ascii[scancode];
      }

      // Handle Caps Lock for letters
      if (caps_lock) {
        if (c >= 'a' && c <= 'z')
          c -= 32;
        else if (c >= 'A' && c <= 'Z')
          c += 32;
      }

      if (c) {
        kprintf("%c", c);
      }
    }
    break;
  case EXPECT_E0:
    // Handle Right Ctrl
    if (scancode == 0x1D) {
      ctrl_pressed = 1;
      kprintf("Right CTRL Pressed\n");
      state = NORMAL;
      return;
    }
    if (scancode == 0x9D) {
      ctrl_pressed = 0;
      kprintf("Right CTRL Released\n");
      state = NORMAL;
      return;
    }

    // Handle Right Alt
    if (scancode == 0x38) {
      alt_pressed = 1;
      kprintf("Right ALT Pressed\n");
      state = NORMAL;
      return;
    }
    if (scancode == 0xB8) {
      alt_pressed = 0;
      kprintf("Right ALT Released\n");
      state = NORMAL;
      return;
    }

    // Handle Super (GUI) keys
    if (scancode == 0x5B) {
      super_pressed = 1;
      kprintf("Left SUPER Pressed\n");

      state = NORMAL;
      return;
    }
    if (scancode == 0xDB) {
      super_pressed = 0;
      kprintf("Left SUPER Released\n");
      state = NORMAL;
      return;
    }
    if (scancode == 0x5C) {
      super_pressed = 1;
      kprintf("Right SUPER Pressed\n");
      state = NORMAL;
      return;
    }
    if (scancode == 0xDC) {
      super_pressed = 0;
      kprintf("Right SUPER Released\n");
      state = NORMAL;
      return;
    }

    // Reset state after handling extended scancode (or ignoring it for now)
    state = NORMAL;
    break;
  case EXPECT_E1_2:
    state = NORMAL;
    break;
  }
}
