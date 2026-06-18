#include "drivers/bus/pci.h"
#include "drivers/nvme/nvme.h"
#include "print/print.h"
uint64_t nvme_base_address;
void find_nvme_devices() {
    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            for (int func = 0; func < 8; func++) {
                uint16_t vendorID = pci_read(bus, dev, func, 0x00);
                if (vendorID == 0xFFFF) {
                    continue; // No device present
                }
                uint16_t classCode = pci_read(bus, dev, func, 0x0A) >> 8;
                uint16_t subclass = pci_read(bus, dev, func, 0x0A) & 0xFF;
                if (classCode == 0x01 && subclass == 0x08) {
                    kprintf("NVMe Device Found: Bus %d, Device %d, Function %d\n", bus, dev, func);
                    uint32_t bar0 = pci_read(bus, dev, func, 0x10);
                    uint32_t bar1 = pci_read(bus, dev, func, 0x14);
                    nvme_base_address = ((uint64_t)bar1 << 32) | (~0xF & bar0);
                }
            }
        }
    }
    kprintf("No NVMe Device Found!\n");
}
void init_base_add_reg() {
    base_addr_reg *regs = (base_addr_reg *)nvme_base_address;
    kprintf("NVMe Controller Found, Version: 0x%X\n", regs->vs);
}
