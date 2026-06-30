#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "common/boot_info.h"
#include "drivers/pic.h"
#include "graphics/framebuffer.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "print/print.h"
#include "system/system.h"
#include <cstdint>

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

    // Initialize GDT and IDT
    kprintf("Kernel: Initializing GDT...\n");
    init_gdt();
    kprintf("Kernel: GDT Initialized\n");

    kprintf("Kernel: Initializing PIC...\n");
    init_pic();
    kprintf("Kernel: PIC Initialized\n");

    kprintf("Kernel: Initializing IDT...\n");
    init_idt();
    kprintf("Kernel: IDT Initialized\n");

    kprintf("Kernel: Initializing PMM...\n");
    init_pmm(&g_efi_boot_info);
    kprintf("Kernel: PMM Initialized\n");
    kprintf("Kernel: Initializing VMM...\n");
    init_vmm();
    kprintf("Kernel: VMM Initialized\n");
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
