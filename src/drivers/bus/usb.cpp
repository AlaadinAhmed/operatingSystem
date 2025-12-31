#include "drivers/bus/usb.h"
#include "drivers/bus/pci.h"
#include "print/print.h"
void find_xhci() {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t dev = 0; dev < 32; dev++) {
      for (uint8_t func = 0; func < 8; func++) {

        uint32_t reg08 = pci_read(bus, dev, func, 0x08);
        uint8_t baseClass = (reg08 >> 24) & 0xFF;
        uint8_t subClass = (reg08 >> 16) & 0xFF;
        uint8_t progIF = (reg08 >> 8) & 0xFF;

        // 0x0C = Serial Bus, 0x03 = USB, 0x30 = xHCI
        if (baseClass == 0x0C && subClass == 0x03 && progIF == 0x30) {
          kprintf("Found xHCI Controller at %d:%d:%d\n", bus, dev, func);

          // Now read BAR0 to get the physical memory address of the controller
          uint32_t bar0 = pci_read(bus, dev, func, 0x10);
          kprintf("xHCI Base Address: 0x%x\n", bar0 & 0xFFFFFFF0);
        }
      }
    }
  }
}
