#include "acpi/acpi.h"
#include "arch/x86_64/cpu/cpuid.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "common/boot_info.h"
#include "drivers/ioapic.h"
#include "drivers/pic.h"
#include "graphics/framebuffer.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "print/print.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "system/system.h"
#include "timer/pit.h"
#include <cstdint>
#include <string.h>

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();
extern "C" uint8_t __bss_start[];
extern "C" uint8_t __bss_end[];

struct BootInfo g_efi_boot_info;

#ifndef MAIN_OFFSET
#define MAIN_OFFSET 0
#endif

#define LINK_BASE 0

// Boot magic constants
#define UEFI_MAGIC 0x45464920
#define BIOS_CUSTOM_MAGIC 0x1337B007
#define MULTIBOOT1_MAGIC 0x2BADB002
#define MULTIBOOT2_MAGIC 0x36d76289

// Helper: read RSDP signature from physical memory (via identity map or HHDM)
static void check_rsdp(const char *stage, bool use_hhdm) {
    uint64_t rsdp_phys = (uint64_t)g_efi_boot_info.rsdp;
    volatile uint8_t *ptr;
    if (use_hhdm)
        ptr = (volatile uint8_t *)(rsdp_phys + 0xFFFF800000000000ULL);
    else
        ptr = (volatile uint8_t *)rsdp_phys;
    kprintf("RSDP CHECK [%s]: addr=0x%lx bytes=%x %x %x %x %x %x %x %x\n", stage, rsdp_phys, ptr[0], ptr[1], ptr[2],
            ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
}

extern "C" void main(uint32_t magic, uint64_t addr) {
    kprintf("Kernel: Entered main\n");

    // --- Early Boot Initialization ---

    // Clear BSS for non-UEFI boot (EFI loader already handles it)
    if (magic != UEFI_MAGIC) {
        for (uint8_t *p = __bss_start; p < __bss_end; p++) {
            *p = 0;
        }
    } else {
        initialize_bootinfo((struct BootInfo *)addr);
    }

    // Run global constructors (skip for UEFI)
    if (magic != UEFI_MAGIC) {
        uint64_t offset = MAIN_OFFSET - LINK_BASE;
        uint64_t current_main_addr = (uint64_t)main;
        uint64_t image_base = current_main_addr - offset;

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

    // Initialize framebuffer
    framebuffer::init_framebuffer(magic, addr);

    // Check RSDP right after framebuffer init (identity map still active)
    check_rsdp("after framebuffer", false);

    // Initialize GDT and IDT

    kprintf("Kernel: Initializing PIC and PIT...\n");
    init_pit(1000); // 1000 Hz
    init_pic();
    kprintf("Kernel: PIC and PIT Initialized\n");

    kprintf("Kernel: Initializing IDT...\n");
    init_idt();
    kprintf("Kernel: IDT Initialized\n");

    check_rsdp("before PMM", false);

    kprintf("Kernel: Initializing PMM...\n");
    init_pmm(&g_efi_boot_info);
    kprintf("Kernel: PMM Initialized\n");

    check_rsdp("after PMM", false);

    kprintf("Kernel: Initializing VMM...\n");
    init_vmm();
    kprintf("Kernel: VMM Initialized\n");

    kprintf("Kernel: Initializing GDT...\n");
    init_gdt();
    kprintf("Kernel: GDT Initialized\n");

    // After VMM, identity map is gone - must use HHDM
    check_rsdp("after VMM", true);

    if (check_sse_support()) {
        kprintf("SSE Supported\n");
        init_simd();
    } else {
        kprintf("SSE Not Supported!\n");
    }
    kprintf("Kernel: Initializing APIC...\n");
    apic_init();
    kprintf("Kernel: APIC Initialized\n");

    uint32_t bus_freq_mhz = get_bus_freq_mhz();
    kprintf("Kernel: Bus Frequency %d\n", bus_freq_mhz);

    check_rsdp("after APIC", true);

    acpi_init();
    ioapic_init();

    // Initialize scheduler
    scheduler_init();

    // Set up APIC Timer (frequency 1000 Hz / 1ms ticks)
    kprintf("Kernel: Transitioning to APIC Timer...\n");
    apic_timer_init(1000);

    // Mask PIT in IOAPIC so it doesn't cause duplicate interrupts
    uint32_t pit_gsi = acpi_get_irq_override(0);
    ioapic_set_entry(pit_gsi, 32, 0, 0, 0, 0, 1, 0); // 1 = mask
    kprintf("Kernel: PIT Masked in IOAPIC. APIC Timer is now driving the scheduler ticks.\n");

    // Inside kernel_main...
    ProcessControlBlock *p1 = create_process(worker_thread_1);
    queue_ready_process(p1);
    // --- System Startup ---
    System system;
    system.Initialize();
    asm volatile("sti");
    system.Run();
    system.Shutdown();

    // Should never reach here
    while (1) {
        asm volatile("hlt");
    }
}
