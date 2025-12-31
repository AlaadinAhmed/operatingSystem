#include "mouse/mouse.h"
#include "mouse/mouse.h"
#include "drivers/pic.h"
#include "print/print.h"
#include <cstdint>

// Screen dimensions (should match VBE mode)
static const int SCREEN_WIDTH = 1920;
static const int SCREEN_HEIGHT = 1080;

struct MousePacket {
  uint8_t buttons;
  int8_t delta_x;
  int8_t delta_y;
};

static float m_sensitivity = 1.0f;
static MouseState mouse_state = {SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, {false, false, false}};

// Wait for controller input buffer to be empty
static void mouse_wait_write() {
  int timeout = 100000;
  while (timeout-- > 0) {
    if ((inb(0x64) & 2) == 0) return;
  }
}

// Wait for controller output buffer to have data
static void mouse_wait_read() {
  int timeout = 100000;
  while (timeout-- > 0) {
    if (inb(0x64) & 1) return;
  }
}

// Send command to PS/2 controller
static void mouse_write_cmd(uint8_t cmd) {
  mouse_wait_write();
  outb(0x64, cmd);
}

// Send data to PS/2 controller data port
static void mouse_write_data(uint8_t data) {
  mouse_wait_write();
  outb(0x60, data);
}

// Read data from PS/2 controller
static uint8_t mouse_read_data() {
  mouse_wait_read();
  return inb(0x60);
}

void mouse_init(float sensitivity) {
  m_sensitivity = sensitivity;
  
  // Initialize mouse position to center of screen
  mouse_state.x = SCREEN_WIDTH / 2;
  mouse_state.y = SCREEN_HEIGHT / 2;
  mouse_state.buttons[0] = false;
  mouse_state.buttons[1] = false;
  mouse_state.buttons[2] = false;

  // Enable auxiliary device (mouse)
  mouse_write_cmd(0xA8);
  
  // Get current controller config
  mouse_write_cmd(0x20);
  uint8_t status = mouse_read_data();
  
  // Enable IRQ12 and disable mouse clock disable
  status |= 2;    // Enable IRQ12 (mouse interrupt)
  status &= ~0x20; // Enable mouse clock
  
  // Write back controller config
  mouse_write_cmd(0x60);
  mouse_write_data(status);
  
  // Set mouse defaults
  mouse_write_cmd(0xD4);  // Command to write to mouse
  mouse_write_data(0xF6); // Set defaults
  mouse_read_data();       // Read ACK
  
  // Enable mouse data reporting
  mouse_write_cmd(0xD4);  // Command to write to mouse
  mouse_write_data(0xF4); // Enable data reporting
  mouse_read_data();       // Read ACK
  
  // Unmask IRQ12 (Mouse Interrupt)
  pic_clear_mask(12);

  kprintf("Mouse initialized\n");
}

// Interrupt handler called from ISR44
void mouse_on_interrupt() {
  static int packet_byte = 0;
  static MousePacket packet;

  // Read data from port 0x60
  uint8_t data = inb(0x60);

  if (packet_byte == 0) {
    // First byte: must have bit 3 set (always 1), contains buttons
    if (!(data & 0x08)) {
      // Invalid packet start, skip this byte
      return;
    }
    packet.buttons = data;
    packet_byte++;
  } else if (packet_byte == 1) {
    // Second byte: delta X
    packet.delta_x = (int8_t)data;
    packet_byte++;
  } else if (packet_byte == 2) {
    // Third byte: delta Y
    packet.delta_y = (int8_t)data;
    packet_byte = 0;

    // Process the complete packet
    mouse_state.x += packet.delta_x;
    mouse_state.y -= packet.delta_y; // Invert Y (mouse Y is inverted)

    // Clamp to screen bounds
    if (mouse_state.x < 0) mouse_state.x = 0;
    if (mouse_state.y < 0) mouse_state.y = 0;
    if (mouse_state.x >= SCREEN_WIDTH) mouse_state.x = SCREEN_WIDTH - 1;
    if (mouse_state.y >= SCREEN_HEIGHT) mouse_state.y = SCREEN_HEIGHT - 1;

    // Update button states
    mouse_state.buttons[0] = (packet.buttons & 0x01) != 0; // Left
    mouse_state.buttons[1] = (packet.buttons & 0x02) != 0; // Right
    mouse_state.buttons[2] = (packet.buttons & 0x04) != 0; // Middle
  }
}

MouseState mouse_update() {
  // Return the current state (updated by interrupt handler)
  return mouse_state;
}

void mouse_get_position(int &x, int &y) {
  x = mouse_state.x;
  y = mouse_state.y;
}

void mouse_set_position(int x, int y) {
  mouse_state.x = x;
  mouse_state.y = y;
}

bool mouse_is_button_pressed(int button) {
  if (button >= 0 && button < 3) {
    return mouse_state.buttons[button];
  }
  return false;
}
