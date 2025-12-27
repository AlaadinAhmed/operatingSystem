#pragma once
#include <stdint.h>

// Helper to write/read from I/O ports (you likely already have these)
extern "C" void outl(uint16_t port, uint32_t val);
extern "C" uint32_t inl(uint16_t port);

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function,
                  uint8_t offset);
void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
