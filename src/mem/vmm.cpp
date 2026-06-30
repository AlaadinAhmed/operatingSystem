#include "common/boot_info.h"
#include "mem/vmm.h"
#include "print/print.h"
#include <cstring>

// Tell the compiler that the global boot info from main.cpp exists [cite: 339, 382]
extern struct BootInfo g_efi_boot_info;

// Forward declaration of your PMM allocator function

void vmm_map_page(PageTable *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    if (!(pml4->entries[pml4_index] & VMM_PRESENT)) {
        void *newtable = pmm_alloc_page(); // Fixed typo
        memset(newtable, 0, PAGE_SIZE);
        pml4->entries[pml4_index] = (uint64_t)newtable | VMM_PRESENT | VMM_WRITE;
    }
    PageTable *pdpt = (PageTable *)(pml4->entries[pml4_index] & ~0xFFFULL); //

    if (!(pdpt->entries[pdpt_index] & VMM_PRESENT)) {
        void *newtable = pmm_alloc_page();
        memset(newtable, 0, PAGE_SIZE);
        pdpt->entries[pdpt_index] = (uint64_t)newtable | VMM_PRESENT | VMM_WRITE;
    }
    PageTable *pd = (PageTable *)(pdpt->entries[pdpt_index] & ~0xFFFULL); //

    if (!(pd->entries[pd_index] & VMM_PRESENT)) {
        void *newtable = pmm_alloc_page();
        memset(newtable, 0, PAGE_SIZE);
        pd->entries[pd_index] = (uint64_t)newtable | VMM_PRESENT | VMM_WRITE;
    }
    PageTable *pt = (PageTable *)(pd->entries[pd_index] & ~0xFFFULL); //

    // CRITICAL FIX: Changed & 0xFFF to & ~0xFFFULL to preserve the physical frame address!
    pt->entries[pt_index] = (phys & ~0xFFFULL) | flags | VMM_PRESENT;
}

void init_vmm() {

    uint64_t pat_msr = 0x277;
    uint64_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(pat_msr));
    // Clear out bits 16-23 (Index 2) and replace them with 0x01 (Write-Combining code)
    low &= 0xFF00FFFF;
    low |= 0x00010000;
    // Write our new custom layout profile straight back to the CPU execution core
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(pat_msr));

    PageTable *new_pml4 = (PageTable *)pmm_alloc_page();
    memset(new_pml4, 0, PAGE_SIZE); // Ensure the root PML4 is completely clean first!

    uint64_t high_offset = 0xFFFFFFFF80000000;
    for (uint64_t phys = 0; phys < 0x1000000; phys += 4096) {
        vmm_map_page(new_pml4, phys + high_offset, phys, VMM_WRITE);
        vmm_map_page(new_pml4, phys, phys, VMM_WRITE);
    }

    uint64_t fb_phys = g_efi_boot_info.fb_addr;
    for (uint64_t offset = 0; offset < 0x2000000; offset += 4096) {
        vmm_map_page(new_pml4, fb_phys + offset, fb_phys + offset, VMM_WRITE | VMM_WRITE_COMBINING);
    }

    // This will now execute perfectly without a triple fault because the addresses are correct!
    asm volatile("mov %0, %%cr3" : : "r"(new_pml4) : "memory");
    kprintf("VMM: Swapped over to permanent 4-level kernel paging tables safely!\n");
}
