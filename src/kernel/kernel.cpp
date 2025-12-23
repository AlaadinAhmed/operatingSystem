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
  init_memory(); // Initialize heap
  
  kprintf("Multiboot Magic: %x, Info: %x\n", magic, addr);

  if (magic == 0x2BADB002) {
      uint32_t flags = *(uint32_t*)addr;
      kprintf("Multiboot Flags: %x\n", flags);
      if (flags & (1 << 6)) { // Memory Map
          uint32_t mmap_len = *(uint32_t*)(addr + 44);
          uint32_t mmap_addr = *(uint32_t*)(addr + 48);
          kprintf("Memory Map: addr=%x len=%d\n", mmap_addr, mmap_len);
          
          for (uint32_t i = 0; i < mmap_len; ) {
              uint32_t size = *(uint32_t*)(mmap_addr + i);
              uint64_t base_addr = *(uint64_t*)(mmap_addr + i + 4);
              uint64_t length = *(uint64_t*)(mmap_addr + i + 12);
              uint32_t type = *(uint32_t*)(mmap_addr + i + 20);
              
              kprintf("Map: base=%x len=%x type=%d\n", (uint32_t)base_addr, (uint32_t)length, type);
              i += size + 4;
          }
      }
      if (flags & (1 << 3)) { // Modules
          uint32_t mods_count = *(uint32_t*)(addr + 20);
          uint32_t mods_addr = *(uint32_t*)(addr + 24);
          kprintf("Modules: count=%d addr=%x\n", mods_count, mods_addr);
          for (uint32_t i = 0; i < mods_count; i++) {
              uint32_t start = *(uint32_t*)(mods_addr + i * 16);
              uint32_t end = *(uint32_t*)(mods_addr + i * 16 + 4);
              kprintf("Module %d: start=%x end=%x\n", i, start, end);
          }
      }
  }

  for (void (**func)() = __init_array_start; func != __init_array_end; func++) {
      (*func)();
  }

  System system;
  system.Initialize();
  system.Run();
  system.Shutdown();
  return;
}
