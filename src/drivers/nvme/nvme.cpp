#include "drivers/bus/pci.h"
#include "drivers/nvme/nvme.h"
#include "disk/nvme_disk.h"
#include "print/print.h"

uint64_t nvme_base_address;

void find_nvme_devices() {
    nvme_base_address = 0;

    for (int bus = 0; bus < 256; ++bus) {
        for (int dev = 0; dev < 32; ++dev) {
            for (int func = 0; func < 8; ++func) {
                uint32_t vendor = pci_read(bus, dev, func, 0x00) & 0xFFFFu;
                if (vendor == 0xFFFFu) {
                    continue;
                }

                uint32_t class_reg = pci_read(bus, dev, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFFu;
                uint8_t subclass = (class_reg >> 16) & 0xFFu;
                if (class_code != 0x01 || subclass != 0x08) {
                    continue;
                }

                kprintf("NVMe Device Found: Bus %d, Device %d, Function %d\n", bus, dev, func);
                if (fs::g_nvme_disk.Initialize(bus, dev, func)) {
                    uint32_t bar0 = pci_read(bus, dev, func, 0x10);
                    uint32_t bar1 = pci_read(bus, dev, func, 0x14);
                    nvme_base_address = ((uint64_t)bar1 << 32) | (bar0 & ~0xFu);
                    kprintf("NVMe: initialized successfully\n");
                    return;
                }

                kprintf("NVMe: controller initialization failed\n");
            }
        }
    }

    kprintf("No NVMe Device Found!\n");
}

void init_base_add_reg() {
    if (!nvme_base_address) {
        kprintf("NVMe: no controller initialized\n");
        return;
    }

    base_addr_reg *regs = (base_addr_reg *)nvme_base_address;
    kprintf("NVMe Controller Found, Version: 0x%X\n", regs->vs);
}
