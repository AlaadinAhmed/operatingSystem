#include "drivers/vga.h"
#include "print/print.h"
#include <cstdint>

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();
extern "C" uint8_t __bss_start[];
extern "C" uint8_t __bss_end[];

#ifndef MAIN_OFFSET
#define MAIN_OFFSET 0
#endif

#define LINK_BASE 0

// UEFI magic: 'EFI ' = 0x45464920
#define UEFI_MAGIC 0x45464920
#define BIOS_CUSTOM_MAGIC 0x1337B007
#define MULTIBOOT1_MAGIC 0x2BADB002
#define MULTIBOOT2_MAGIC 0x36d76289

extern "C" void main(uint32_t magic, uint64_t addr) {
  // Skip BSS clearing for UEFI - EFI loader already handles it
  // Also skip if BSS symbols are invalid (position-independent code)
  if (magic != UEFI_MAGIC) {
    for (uint8_t* p = __bss_start; p < __bss_end; p++) {
        *p = 0;
    }
  }
  
  vga_clear_screen(0x000000); 

  // Calculate ImageBase (Load Address) for UEFI relocation
  uint64_t offset = MAIN_OFFSET - LINK_BASE;
  uint64_t current_main_addr = (uint64_t)main;
  uint64_t image_base = current_main_addr - offset;
  
  if (MAIN_OFFSET == 0) {
      image_base = addr; 
  }

  // Run global constructors (skip for UEFI to avoid relocation issues)
  if (magic != UEFI_MAGIC) {
    for (void (**p)() = __init_array_start; p < __init_array_end; p++) {
      uint64_t func_ptr = (uint64_t)*p;
      if (func_ptr > 0) {
          if (func_ptr < 0x1000000) { 
              func_ptr += image_base;
          }
          void (*func)() = (void (*)())func_ptr;
          func();
      }
    }
  }

  // Determine boot method
  const char* boot_method = "Unknown";
  if (magic == UEFI_MAGIC) boot_method = "UEFI";
  else if (magic == BIOS_CUSTOM_MAGIC) boot_method = "BIOS (Custom)";
  else if (magic == MULTIBOOT1_MAGIC) boot_method = "BIOS (Multiboot1)";
  else if (magic == MULTIBOOT2_MAGIC) boot_method = "BIOS (Multiboot2)";

  // Display info on screen
  vga_draw_string(100, 100, "Hello from MyOS!", 0x00FF00);
  
  char buf[64];
  ksprintf(buf, "Boot Method: %s", boot_method);
  vga_draw_string(100, 120, buf, 0x00FFFF);
  
  ksprintf(buf, "Magic: 0x%x", magic);
  vga_draw_string(100, 140, buf, 0xAAAAAA);

  // Serial debug output
  kprintf("Kernel: Booted successfully!\n");
  kprintf("Magic: 0x%x, Boot: %s\n", magic, boot_method);

  // Halt the system
  while(1) {
      __asm__ volatile("hlt");
  }
}


