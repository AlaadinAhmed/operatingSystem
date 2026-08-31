#include "graphics/framebuffer.h"
#include "mem/pmm.h"
#include "common/boot_info.h"
#include "mem/vmm.h"
#include "print/print.h"
#include <cstring>

// Tell the compiler that the global boot info from main.cpp exists [cite: 339, 382]
extern struct BootInfo g_efi_boot_info;
PageTable *g_kernel_pml4 = nullptr;
// Forward declaration of your PMM allocator function

void *vmm_map_mmio(uint64_t phys, uint64_t size_bytes, uint64_t flags) {
    if (!g_kernel_pml4)
        return nullptr;
    uint64_t aligned_phys = phys & ~0xFFFULL;
    uint64_t end = (phys + size_bytes + 0xFFF) & ~0xFFFULL;

    for (uint64_t p = aligned_phys; p < end; p += 4096) {
        vmm_map_page(g_kernel_pml4, p + KERNEL_VIRTUAL_OFFSET, p, flags | VMM_PRESENT);
    }
    return (void *)(phys + KERNEL_VIRTUAL_OFFSET);
}

void vmm_map_page(PageTable *pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    PageTable *pml4 = (PageTable *)phys_to_kvirt((uint64_t)pml4_phys);

    if (!(pml4->entries[pml4_index] & VMM_PRESENT)) {
        void *newtable = pmm_alloc_page(); // Fixed typo
        memset(phys_to_kvirt((uint64_t)newtable), 0, PAGE_SIZE);
        pml4->entries[pml4_index] = (uint64_t)newtable | VMM_PRESENT | VMM_WRITE;
    }
    PageTable *pdpt = (PageTable *)phys_to_kvirt(pml4->entries[pml4_index] & ~0xFFFULL); //

    if (!(pdpt->entries[pdpt_index] & VMM_PRESENT)) {
        void *newtable = pmm_alloc_page();
        memset(phys_to_kvirt((uint64_t)newtable), 0, PAGE_SIZE);
        pdpt->entries[pdpt_index] = (uint64_t)newtable | VMM_PRESENT | VMM_WRITE;
    }
    PageTable *pd = (PageTable *)phys_to_kvirt(pdpt->entries[pdpt_index] & ~0xFFFULL); //

    if (!(pd->entries[pd_index] & VMM_PRESENT)) {
        void *newtable = pmm_alloc_page();
        memset(phys_to_kvirt((uint64_t)newtable), 0, PAGE_SIZE);
        pd->entries[pd_index] = (uint64_t)newtable | VMM_PRESENT | VMM_WRITE;
    }
    PageTable *pt = (PageTable *)phys_to_kvirt(pd->entries[pd_index] & ~0xFFFULL); //

    // CRITICAL FIX: Changed & 0xFFF to & ~0xFFFULL to preserve the physical frame address!
    pt->entries[pt_index] = (phys & ~0xFFFULL) | flags | VMM_PRESENT;
    
    // Flush the TLB for this virtual address
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    vmm_map_page(g_kernel_pml4, virt, phys, flags);
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
    memset(phys_to_kvirt((uint64_t)new_pml4), 0, PAGE_SIZE); // Ensure the root PML4 is completely clean first!
    g_kernel_pml4 = new_pml4;

    uint64_t high_offset = 0xFFFFFFFF80000000;
    for (uint64_t phys = 0; phys < 0x1000000; phys += 4096) {
        vmm_map_page(new_pml4, phys + high_offset, phys, VMM_WRITE);
        vmm_map_page(new_pml4, phys, phys, VMM_WRITE); // Identity map (temporary)
    }

    uint64_t total_ram = g_pmm.total_pages * 4096;
    uint64_t hhdm_map_size = total_ram + 0x40000000; // Add 1GB extra for ACPI/MMIO
    for (uint64_t phys = 0; phys < hhdm_map_size; phys += 4096) {
        vmm_map_page(new_pml4, phys + KERNEL_VIRTUAL_OFFSET, phys, VMM_WRITE); // Map full HHDM
    }

    uint64_t fb_phys = g_efi_boot_info.fb_addr;
    for (uint64_t offset = 0; offset < 0x2000000; offset += 4096) {
        vmm_map_page(new_pml4, fb_phys + offset + KERNEL_VIRTUAL_OFFSET, fb_phys + offset, VMM_WRITE | VMM_WRITE_COMBINING);
        vmm_map_page(new_pml4, fb_phys + offset, fb_phys + offset, VMM_WRITE | VMM_WRITE_COMBINING); // Identity map (temporary)
    }

    // This will now execute perfectly without a triple fault because the addresses are correct!
    asm volatile("mov %0, %%cr3" : : "r"(new_pml4) : "memory");
    kprintf("VMM: Swapped over to permanent 4-level kernel paging tables safely!\n");
    finalize_kernel_pml4();
}
void finalize_kernel_pml4() {
    framebuffer::update_to_hhdm();
    // Unmap the lower half securely!
    PageTable *pml4 = (PageTable *)phys_to_kvirt((uint64_t)g_kernel_pml4);
    pml4->entries[0] = 0;

    // Force the CPU to flush the Translation Lookaside Buffer (TLB) so it forgets the old low maps
    asm volatile("mov %0, %%cr3" : : "r"((uint64_t)g_kernel_pml4));
}

uint64_t virtual_to_physical(void *virtual_addr) {
    uint64_t virt = (uint64_t)virtual_addr;

    // 1. Extract the indices for all 4 levels of the active table layout
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;
    uint64_t page_offset = virt & 0xFFF;

    // 2. Read current CR3 to find the physical root of the current PML4
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    uint64_t pml4_phys = current_cr3 & PTE_FRAME_MASK;

    // Convert the physical PML4 address to a virtual pointer your code can read
    uint64_t *pml4 = (uint64_t *)phys_to_kvirt(pml4_phys);

    // --- LEVEL 4: PML4 Lookup ---
    uint64_t pml4_entry = pml4[pml4_index];
    if (!(pml4_entry & 0x1))
        return 0; // Present bit (0x1) is not set! Return 0 (unmapped)

    // --- LEVEL 3: Page Directory Pointer Table (PDPT) ---
    uint64_t pdpt_phys = pml4_entry & PTE_FRAME_MASK;
    uint64_t *pdpt = (uint64_t *)(pdpt_phys + KERNEL_VIRTUAL_OFFSET);

    uint64_t pdpt_entry = pdpt[pdpt_index];
    if (!(pdpt_entry & 0x1))
        return 0; // Not present

    // CATCH 1GB HUGE PAGES: If bit 7 (PS - Page Size) is set in the PDPT entry,
    // this address maps a 1GB huge page. No further table walking is needed!
    if (pdpt_entry & (1 << 7)) {
        uint64_t physical_frame = pdpt_entry & 0xFFFFFC0000000ULL; // 1GB boundary mask
        uint64_t large_offset = virt & 0x3FFFFFFF;                 // 1GB offset mask
        return physical_frame + large_offset;
    }

    // --- LEVEL 2: Page Directory (PD) ---
    uint64_t pd_phys = pdpt_entry & PTE_FRAME_MASK;
    uint64_t *pd = (uint64_t *)(pd_phys + KERNEL_VIRTUAL_OFFSET);

    uint64_t pd_entry = pd[pd_index];
    if (!(pd_entry & 0x1))
        return 0; // Not present

    // CATCH 2MB LARGE PAGES: If bit 7 (PS - Page Size) is set in the PD entry,
    // this address maps a 2MB large page. No further table walking is needed!
    if (pd_entry & (1 << 7)) {
        uint64_t physical_frame = pd_entry & 0xFFFFFFFFFE00000ULL; // 2MB boundary mask
        uint64_t medium_offset = virt & 0x1FFFFF;                  // 2MB offset mask
        return physical_frame + medium_offset;
    }

    // --- LEVEL 1: Page Table (PT) ---
    uint64_t pt_phys = pd_entry & PTE_FRAME_MASK;
    uint64_t *pt = (uint64_t *)(pt_phys + KERNEL_VIRTUAL_OFFSET);

    uint64_t pt_entry = pt[pt_index];
    if (!(pt_entry & 0x1))
        return 0; // Not present

    // Standard 4KB page final evaluation
    uint64_t physical_frame_address = pt_entry & PTE_FRAME_MASK;

    return physical_frame_address + page_offset;
}

void vmm_destroy_user_space(PageTable *old_pml4) {
    if (!old_pml4) return;
    
    // Iterate over the lower half (user space), indices 0-255
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (!(old_pml4->entries[pml4_idx] & VMM_PRESENT)) continue;
        
        uint64_t pdpt_phys = old_pml4->entries[pml4_idx] & PTE_FRAME_MASK;
        PageTable *pdpt = (PageTable *)phys_to_kvirt(pdpt_phys);
        
        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            if (!(pdpt->entries[pdpt_idx] & VMM_PRESENT)) continue;
            
            if (pdpt->entries[pdpt_idx] & (1ULL << 7)) {
                // 1GB huge page frame mapped directly here (unlikely in user space, but handled)
                pmm_free_page((void *)(pdpt->entries[pdpt_idx] & PTE_FRAME_MASK));
                continue;
            }
            
            uint64_t pd_phys = pdpt->entries[pdpt_idx] & PTE_FRAME_MASK;
            PageTable *pd = (PageTable *)phys_to_kvirt(pd_phys);
            
            for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                if (!(pd->entries[pd_idx] & VMM_PRESENT)) continue;
                
                if (pd->entries[pd_idx] & (1ULL << 7)) {
                    // 2MB large page frame
                    pmm_free_page((void *)(pd->entries[pd_idx] & PTE_FRAME_MASK));
                    continue;
                }
                
                uint64_t pt_phys = pd->entries[pd_idx] & PTE_FRAME_MASK;
                PageTable *pt = (PageTable *)phys_to_kvirt(pt_phys);
                
                for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
                    if (!(pt->entries[pt_idx] & VMM_PRESENT)) continue;
                    pmm_free_page((void *)(pt->entries[pt_idx] & PTE_FRAME_MASK));
                }
                pmm_free_page((void *)pt_phys);
            }
            pmm_free_page((void *)pd_phys);
        }
        pmm_free_page((void *)pdpt_phys);
        old_pml4->entries[pml4_idx] = 0;
    }
}
