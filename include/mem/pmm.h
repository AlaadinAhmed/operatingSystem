#pragma once
#include <cstdint>
#define PAGE_SIZE 4096
struct PhysicalMemoryManager {
    uint8_t *bitmap;
    uint64_t total_pages;
    uint64_t bitmap_size;
};
enum EFI_MEMORY_TYPE {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUsableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiUnacceptedMemoryType
};
struct EFI_MEMORY_DESCRIPTOR {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

extern PhysicalMemoryManager g_pmm;
void init_pmm(struct BootInfo *bootinfo);
void *pmm_alloc_page();
inline void tlb_flush(uintptr_t virtual_address) { asm volatile("invlpg (%0)" ::"r"(virtual_address) : "memory"); }
void pmm_free_page(void *physical_addr);
