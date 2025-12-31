#pragma once
#include <stdint.h>
struct IDTGateDescriptor {
  uint16_t offset_low;  // Lower 16 bits of handler function address
  uint16_t selector;    // Kernel segment selector
  uint8_t ist;          // This must always be zero
  uint8_t type_attr;    // Type and attributes
  uint16_t offset_mid;  // Upper 16 bits of handler function address
  uint32_t offset_high; // Highest 32 bits of handler function address
  uint32_t reserved;    // Reserved, must be zero
};
struct IDTR {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));
void init_idt();
