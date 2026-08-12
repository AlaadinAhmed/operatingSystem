#pragma once
#include "mem/pmm.h"
#include <cstdint>
#define VMM_PRESENT (1ULL << 0) // Page is in RAM
#define VMM_WRITE (1ULL << 1)   // Read/Write allowed (0 = Read only)
#define VMM_USER (1ULL << 2)    // User space accessible (0 = Kernel Only)
#define VMM_PWT (1ULL << 3)     // Page Write-Through
#define VMM_PCD (1ULL << 4)     // Page Cache Disable (Bit 4)
#define VMM_LARGE (1ULL << 7)   // 1 = 2MB Page, 0 = 4KB Page
#define VMM_PAT (1ULL << 7)     // Page Attribute Table (for 4KB pages)
// We configured PAT Index 2 to be Write-Combining (WC) in init_vmm().
// Index 2 corresponds to PAT=0, PCD=1, PWT=0.
#define VMM_WRITE_COMBINING VMM_PCD
// Change this to match your kernel's direct mapping offset (HHDM)
#define KERNEL_VIRTUAL_OFFSET 0xFFFF800000000000

// Mask to strip out the lower 12 bits of flags (Present, Writable, User, etc.)
// to extract the clean physical frame base address from an entry.
#define PTE_FRAME_MASK ~0xFFFULL

struct PageTable {
    uint64_t entries[512];
};
extern PageTable *g_kernel_pml4;
void vmm_map_page(PageTable *plm4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void *vmm_map_mmio(uint64_t phys, uint64_t size_bytes, uint64_t flags = VMM_WRITE | VMM_PCD);

static inline uint64_t kvirt_to_phys(uint64_t virt) {
    if (virt >= 0xFFFFFFFF80000000) {
        return virt - 0xFFFFFFFF80000000;
    }
    if (virt >= KERNEL_VIRTUAL_OFFSET) {
        return virt - KERNEL_VIRTUAL_OFFSET;
    }
    return virt;
}

static inline void *phys_to_kvirt(uint64_t phys) {
    return (void *)(phys + KERNEL_VIRTUAL_OFFSET);
}

static inline uint64_t kvirt_to_phys(const void *ptr) { return kvirt_to_phys((uint64_t)ptr); }

void init_vmm();
void finalize_kernel_pml4();
uint64_t virtual_to_physical(void *virtual_addr);
