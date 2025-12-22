#include "drivers/keyboard.h"
#include <stdint.h>

// Helper functions to read from I/O ports
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Scancode map for US keyboard layout (Unshifted)
static const char scancode_to_ascii[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Scancode map for US keyboard layout (Shifted)
static const char shift_scancode_to_ascii[] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0
};

static int shift_pressed = 0;

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
                
                if (c) {
                    return c;
                }
            }
        }
    }
}
