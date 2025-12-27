#include "drivers/audio/audio.h"
#include "drivers/audio/intel_hda.h"
#include "drivers/audio/ac97.h"
#include "drivers/bus/pci.h"
#include "drivers/font.h"
#include "memory/kmalloc.h"
#include "print/print.h"

static uint64_t g_hda_base = 0;
static IntelHDA* g_hda_driver = nullptr;
static AC97* g_ac97_driver = nullptr;

uint64_t get_audio_base() { return g_hda_base; }

// Local PCI Helpers
struct PCIDevice {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    bool found;
};

uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return pci_read(bus, slot, func, offset);
}

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read(bus, slot, func, offset & 0xFC);
    return (uint16_t)((val >> ((offset & 3) * 8)) & 0xFFFF);
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t val = pci_read(bus, slot, func, offset & 0xFC);
    val &= ~(0xFFFF << ((offset & 3) * 8));
    val |= ((uint32_t)value << ((offset & 3) * 8));
    pci_write(bus, slot, func, offset & 0xFC, val);
}

PCIDevice find_pci_device(uint16_t vendor, uint16_t device) {
    PCIDevice dev = {0, 0, 0, false};
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint16_t v = pci_read_word(bus, slot, func, 0);
                if (v == 0xFFFF) continue;
                uint16_t d = pci_read_word(bus, slot, func, 2);
                if (v == vendor && d == device) {
                    dev.bus = bus;
                    dev.slot = slot;
                    dev.func = func;
                    dev.found = true;
                    return dev;
                }
            }
        }
    }
    return dev;
}

PCIDevice find_pci_device_by_class(uint8_t class_code, uint8_t subclass) {
    PCIDevice dev = {0, 0, 0, false};
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint16_t v = pci_read_word(bus, slot, func, 0);
                if (v == 0xFFFF) continue;
                
                uint16_t class_word = pci_read_word(bus, slot, func, 0x0A);
                uint8_t c = (class_word >> 8) & 0xFF;
                uint8_t s = class_word & 0xFF;
                
                if (c == class_code && s == subclass) {
                    dev.bus = bus;
                    dev.slot = slot;
                    dev.func = func;
                    dev.found = true;
                    return dev;
                }
            }
        }
    }
    return dev;
}

void find_audio_device() {
    if (g_hda_driver || g_ac97_driver) return;

    kprintf("Scanning for Audio Device...\n");
    
    // Try HDA first (Class 0x04, Subclass 0x03)
    PCIDevice audio_device = find_pci_device(0x8086, 0x2668); // Intel ICH6 HDA
    if (!audio_device.found) {
        audio_device = find_pci_device_by_class(0x04, 0x03); // Generic HDA
    }

    if (audio_device.found) {
        uint32_t bar0 = pci_read_dword(audio_device.bus, audio_device.slot, audio_device.func, 0x10);
        kprintf("Found HDA Controller at: 0x%lx\n", (uint64_t)bar0);

        uint16_t cmd = pci_read_word(audio_device.bus, audio_device.slot, audio_device.func, 0x04);
        cmd |= 0x07; // Bus Master, Memory Space, I/O Space
        pci_write_word(audio_device.bus, audio_device.slot, audio_device.func, 0x04, cmd);
        kprintf("Bus Master & Memory Space Enabled.\n");

        g_hda_base = bar0 & 0xFFFFFFF0;
        
        static uint8_t driver_mem[sizeof(IntelHDA)];
        g_hda_driver = (IntelHDA*)driver_mem;
        *g_hda_driver = IntelHDA(g_hda_base);
        g_hda_driver->Initialize();
        return;
    }

    // Try AC'97 (Class 0x04, Subclass 0x01)
    audio_device = find_pci_device(0x8086, 0x2415); // Intel ICH AC'97
    if (!audio_device.found) {
        audio_device = find_pci_device_by_class(0x04, 0x01); // Generic AC'97
    }

    if (audio_device.found) {
        uint32_t bar0 = pci_read_dword(audio_device.bus, audio_device.slot, audio_device.func, 0x10);
        uint32_t bar1 = pci_read_dword(audio_device.bus, audio_device.slot, audio_device.func, 0x14);
        
        // AC'97 uses I/O ports (BAR0 = NAM, BAR1 = NABM)
        uint16_t nambar = bar0 & 0xFFFE;
        uint16_t nabmbar = bar1 & 0xFFFE;
        
        kprintf("Found AC'97 Controller: NAM=0x%x, NABM=0x%x\n", nambar, nabmbar);

        uint16_t cmd = pci_read_word(audio_device.bus, audio_device.slot, audio_device.func, 0x04);
        cmd |= 0x05; // Bus Master, I/O Space
        pci_write_word(audio_device.bus, audio_device.slot, audio_device.func, 0x04, cmd);
        kprintf("Bus Master & I/O Space Enabled.\n");
        
        static uint8_t ac97_mem[sizeof(AC97)];
        g_ac97_driver = (AC97*)ac97_mem;
        *g_ac97_driver = AC97(nabmbar, nambar);
        g_ac97_driver->Initialize();
        return;
    }

    kprintf("No Audio Controller found!\n");
}

void init_audio() {
    find_audio_device();
}

void play_test_sound() {
    if (g_hda_driver) {
        g_hda_driver->PlayTestSound();
    } else if (g_ac97_driver) {
        g_ac97_driver->PlayTestSound();
    }
}
