#include "drivers/ioapic.h"
#include "acpi/acpi.h"
#include "mem/vmm.h"
#include "print/print.h"

static uint64_t s_ioapic_virt = 0;

static void ioapic_write(uint8_t offset, uint32_t value) {
    if (s_ioapic_virt == 0) return;
    volatile uint32_t* ioregsel = (volatile uint32_t*)s_ioapic_virt;
    volatile uint32_t* iowin    = (volatile uint32_t*)(s_ioapic_virt + 0x10);
    *ioregsel = offset;
    *iowin = value;
}

static uint32_t ioapic_read(uint8_t offset) {
    if (s_ioapic_virt == 0) return 0;
    volatile uint32_t* ioregsel = (volatile uint32_t*)s_ioapic_virt;
    volatile uint32_t* iowin    = (volatile uint32_t*)(s_ioapic_virt + 0x10);
    *ioregsel = offset;
    return *iowin;
}

void ioapic_init() {
    uint32_t phys_base = acpi_get_ioapic_base();
    if (phys_base == 0) {
        kprintf("IOAPIC: Not found via ACPI!\n");
        return;
    }

    s_ioapic_virt = (uint64_t)vmm_map_mmio(phys_base, 0x1000, VMM_PRESENT | VMM_WRITE | VMM_PCD);
    kprintf("IOAPIC: Mapped at virtual 0x%lx\n", s_ioapic_virt);

    uint32_t ver_reg = ioapic_read(IOAPIC_REG_VER);
    uint32_t max_intr = (ver_reg >> 16) & 0xFF;
    kprintf("IOAPIC: Max interrupts: %d\n", max_intr + 1);

    // Mask all interrupts initially
    for (uint32_t i = 0; i <= max_intr; i++) {
        ioapic_write(IOAPIC_REG_REDTBL + 2 * i, 0x10000); // Masked
        ioapic_write(IOAPIC_REG_REDTBL + 2 * i + 1, 0);
    }

    // Now map standard ISA IRQs to vectors
    // Keyboard: IRQ 1 -> Vector 33
    uint32_t kbd_gsi = acpi_get_irq_override(1);
    ioapic_set_entry(kbd_gsi, 33, 0, 0, 0, 0, 0, 0);

    // Mouse: IRQ 12 -> Vector 44
    uint32_t mouse_gsi = acpi_get_irq_override(12);
    ioapic_set_entry(mouse_gsi, 44, 0, 0, 0, 0, 0, 0);

    // PIT: IRQ 0 -> Vector 32
    uint32_t pit_gsi = acpi_get_irq_override(0);
    ioapic_set_entry(pit_gsi, 32, 0, 0, 0, 0, 0, 0);

    kprintf("IOAPIC: Initialization complete.\n");
}

void ioapic_set_entry(uint8_t irq, uint8_t vector, uint8_t delivery_mode, uint8_t dest_mode, uint8_t pin_polarity, uint8_t trigger_mode, uint8_t mask, uint8_t destination) {
    uint32_t low_index = IOAPIC_REG_REDTBL + irq * 2;
    uint32_t high_index = IOAPIC_REG_REDTBL + irq * 2 + 1;

    uint32_t high = destination << 24;
    uint32_t low = vector | (delivery_mode << 8) | (dest_mode << 11) | (pin_polarity << 13) | (trigger_mode << 15) | (mask << 16);

    ioapic_write(high_index, high);
    ioapic_write(low_index, low);
}
