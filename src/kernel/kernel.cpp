#include "disk/disk.h"
#include "drivers/vga.h"
#include "ext4"
#include "print/print.h"
extern "C" void main() {
  // VBE Mode Info is at 0x5200
  // Framebuffer address is at offset 40 (0x28) in VbeModeInfoBlock
  // set_vbe_mode(0x118); // Set 1024x768x32bpp Mode - Handled by bootloader
  // Clear screen to black
  vga_clear_screen(0x000000);
  // Draw a white rectangle
  vga_draw_rectangle(100, 100, 200, 150, 0xFFFFFF);
  // Draw a red circle
  vga_draw_circle(400, 300, 75, 0xFF0000);
  // Draw some text
  vga_draw_char(110, 110, '2', 0x000000);
  vga_draw_string(120, 120, "Welcome to My OS Kernel!", 0x000000, 3);
  // Hang the system

  print("Kernel initialized successfully!\n");
  fs::Ext2Disk disk;
  disk.mount();
  while (1)
    ;
}
