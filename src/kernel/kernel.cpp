#include "disk/disk.h"
#include "drivers/font.h"
#include "drivers/sse.h"
#include "drivers/vga.h"
#include "fs/lwext4_adapter.h"
#include "memory/kmalloc.h"
#include "print/print.h"
#include "shell/shell.h"
#include "stb_truetype.h"
#include "system/system.h"
#include <cstdint>
#include <ext4.h>

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();

extern "C" void main(uint32_t magic, uint32_t addr) {
  (void)magic; // Suppress unused parameter warning
  (void)addr;  // Suppress unused parameter warning
  init_memory(); // Initialize heap
  vga_clear_screen(0x000000); // Clear GRUB residual pixels

  for (void (**func)() = __init_array_start; func != __init_array_end; func++) {
      (*func)();
  }

  System system;
  system.Initialize();
  system.Run();
  system.Shutdown();
  return;
}
