#pragma once
#include "mem//pmm.h"
#include <cstdint>
#define VMM_PRESENT (1ULL << 0) // Page is in RAM
#define VMM_WRITE (1ULL << 1)   // Read/Write allowed (0 = Read only)
#define VMM_USER (1ULL << 2)    // User space accessible (0 = Kernel Only)
#define VMM_PWT (1ULL << 3)     // Page Write-Through
#define VMM_PCD (1ULL << 3)     // Page Cache Disable
#define VMM_LARGE (1ULL << 7)   // 1 = 2MB Page, 0 = 4KB Page
#define VMM_WRITE_COMBINING (VMM_PCD | (1ULL << 7))
struct PageTable {
    uint64_t entries[512];
};
void vmm_map_page(PageTable *plm4, uint64_t virt, uint64_t phys, uint64_t flags);
void init_vmm();
