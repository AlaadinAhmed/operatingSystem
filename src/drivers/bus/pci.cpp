#include "bus/pci.h"
#include <stdint.h>

// Helper to write/read from I/O ports (you likely already have these)
extern "C" void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

extern "C" uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function,
                  uint8_t offset) {
  // 1. Construct the 32-bit address:
  // Bit 31: Enable bit (must be 1)
  // Bits 23-16: Bus
  // Bits 15-11: Device
  // Bits 10-8: Function
  // Bits 7-2: Offset (must be 4-byte aligned)
  uint32_t address = (uint32_t)((uint32_t)bus << 16) |
                     ((uint32_t)device << 11) | ((uint32_t)function << 8) |
                     (offset & 0xFC) | ((uint32_t)0x80000000);

  // 2. Write the address to the Address Port
  outl(0xCF8, address);

  // 3. Read the data from the Data Port
  return inl(0xCFC);
}

void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
  uint32_t address = (uint32_t)((uint32_t)bus << 16) |
                     ((uint32_t)device << 11) | ((uint32_t)function << 8) |
                     (offset & 0xFC) | ((uint32_t)0x80000000);

  outl(0xCF8, address);
  outl(0xCFC, value);
}
