#include "arch/x86_64/gdt.h"
#include "memory/kmalloc.h" // For memset if needed, or just use __builtin_memset

struct GDT {
    GDTEntry null;
    GDTEntry kernel_code;
    GDTEntry kernel_data;
    GDTEntry user_code;
    GDTEntry user_data;
    TSSEntry tss;
} __attribute__((packed));

static GDT gdt;
static GDTDescriptor gdt_desc;
static TSS tss;

void init_gdt() {
    // Zero out GDT and TSS
    __builtin_memset(&gdt, 0, sizeof(GDT));
    __builtin_memset(&tss, 0, sizeof(TSS));

    // 1. Kernel Code (0x08)
    // Base=0, Limit=0xFFFFF, Access=0x9A (Present, Ring 0, Code, Readable), Granularity=0xAF (64-bit, 4KB blocks)
    gdt.kernel_code.limit_low = 0xFFFF;
    gdt.kernel_code.base_low = 0;
    gdt.kernel_code.base_middle = 0;
    gdt.kernel_code.access = 0x9A;
    gdt.kernel_code.granularity = 0xAF;
    gdt.kernel_code.base_high = 0;

    // 2. Kernel Data (0x10)
    // Base=0, Limit=0xFFFFF, Access=0x92 (Present, Ring 0, Data, Writable), Granularity=0xCF (32-bit?? No, 64-bit data seg doesn't matter much but usually 0xCF or 0xAF)
    // For x86_64 data segments, usually limit/base are ignored, but good to set them.
    gdt.kernel_data.limit_low = 0xFFFF;
    gdt.kernel_data.base_low = 0;
    gdt.kernel_data.base_middle = 0;
    gdt.kernel_data.access = 0x92;
    gdt.kernel_data.granularity = 0xCF; // 0xAF is also fine
    gdt.kernel_data.base_high = 0;

    // 3. User Code (0x18)
    // Access=0xFA (Present, Ring 3, Code, Readable), Granularity=0xAF
    gdt.user_code.limit_low = 0xFFFF;
    gdt.user_code.base_low = 0;
    gdt.user_code.base_middle = 0;
    gdt.user_code.access = 0xFA;
    gdt.user_code.granularity = 0xAF;
    gdt.user_code.base_high = 0;

    // 4. User Data (0x20)
    // Access=0xF2 (Present, Ring 3, Data, Writable), Granularity=0xCF
    gdt.user_data.limit_low = 0xFFFF;
    gdt.user_data.base_low = 0;
    gdt.user_data.base_middle = 0;
    gdt.user_data.access = 0xF2;
    gdt.user_data.granularity = 0xCF;
    gdt.user_data.base_high = 0;

    // 5. TSS (0x28)
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(TSS) - 1;

    gdt.tss.limit_low = tss_limit & 0xFFFF;
    gdt.tss.base_low = tss_base & 0xFFFF;
    gdt.tss.base_middle = (tss_base >> 16) & 0xFF;
    gdt.tss.access = 0x89; // Present, Ring 0, Available TSS (0x9)
    gdt.tss.granularity = 0x00; // Byte granularity
    gdt.tss.base_high = (tss_base >> 24) & 0xFF;
    gdt.tss.base_upper = (tss_base >> 32) & 0xFFFFFFFF;
    gdt.tss.reserved = 0;

    // Setup GDT Descriptor
    gdt_desc.size = sizeof(GDT) - 1;
    gdt_desc.offset = (uint64_t)&gdt;

    // Load GDT
    asm volatile("lgdt %0" : : "m"(gdt_desc));

    // Reload segments
    asm volatile(
        "pushq $0x08\n"           // Push code segment selector
        "leaq .reload_cs(%%rip), %%rax\n"
        "pushq %%rax\n"           // Push return address
        "lretq\n"                 // Far return to reload CS
        ".reload_cs:\n"
        "mov $0x10, %%ax\n"       // Load data segment selector
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "rax", "memory"
    );

    // Load TSS
    load_tss();
}

void load_tss() {
    asm volatile("ltr %%ax" : : "a"((uint16_t)0x28));
}
