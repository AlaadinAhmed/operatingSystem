#include "common/boot_info.h"
#include "mem/pmm.h"
#include "memory/kmalloc.h"
#include "print/print.h"
void init_pmm(struct BootInfo *boot_info) {
    EFI_MEMORY_DESCRIPTOR *mmap = (EFI_MEMORY_DESCRIPTOR *)boot_info->mmap;
    uint64_t mmap_size = boot_info->mmap_size;
    uint64_t desc_size = boot_info->desc_size;
    uint64_t total_descriptors = mmap_size / desc_size;
    uint64_t highest_address = 0;
    for (uint64_t i = 0; i < total_descriptors; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint64_t)mmap + (i * desc_size));
        uint64_t end_address = desc->physical_start + (desc->number_of_pages * 4096);
        if (desc->type == EfiConventionalMemory || desc->type == EfiBootServicesCode ||
            desc->type == EfiBootServicesData || desc->type == EfiLoaderCode || desc->type == EfiLoaderData ||
            desc->type == EfiRuntimeServicesCode || desc->type == EfiRuntimeServicesData) {
            uint64_t end_address = desc->physical_start + (desc->number_of_pages * 4096);
            if (end_address > highest_address) {
                highest_address = end_address;
            }
        }
    }
    g_pmm.total_pages = highest_address / 4096;
    g_pmm.bitmap_size = g_pmm.total_pages / 8;
    g_pmm.bitmap = (uint8_t *)kmalloc(g_pmm.bitmap_size);
    if (g_pmm.bitmap == nullptr) {
        kprintf("PMM CRITICAL ERROR: kmalloc failed to allocate memory for the tracking bitmap!\n");
        return;
    }
    memset(g_pmm.bitmap, 0xFF, g_pmm.bitmap_size);
    for (uint64_t i = 0; i < total_descriptors; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint64_t)mmap + (i * desc_size));

        if (desc->type == EfiConventionalMemory || desc->type == EfiBootServicesCode ||
            desc->type == EfiBootServicesData) {
            uint64_t start_page = desc->physical_start / PAGE_SIZE;
            for (uint64_t p = 0; p < desc->number_of_pages; p++) {
                uint64_t target_page = start_page + p;
                g_pmm.bitmap[target_page / 8] &= ~(1 << (target_page % 8));
            }
        }
    }
    g_pmm.bitmap[0] |= 0x01;
    kprintf("PMM: Dynamic Bitmap initialized at virtual address %lx\n", (uint64_t)g_pmm.bitmap);
    kprintf("PMM: Number of Pages found %d\n", g_pmm.total_pages);
    kprintf("PMM: Tracking %d MB of physical RAM.\n", (g_pmm.total_pages * 4096) / 1024 / 1024);
}
void *pmm_alloc_page() {
    for (uint64_t i = 0; i < g_pmm.bitmap_size; i++) {
        if (g_pmm.bitmap[i] != 0xFF) {
            for (int b = 0; b < 8; b++) {
                if ((g_pmm.bitmap[i] & (1 << b)) == 0) {
                    uint64_t page_index = (i * 8) + b;
                    g_pmm.bitmap[i] |= (1 << b);
                    return (void *)(page_index * 4096);
                }
            }
        }
    }
    return nullptr;
}
void pmm_free_page(void *physical_addr) {
    uint64_t page_index = (uint64_t)physical_addr / 4096;
    uint64_t byte_index = page_index / 8;
    uint64_t bit_offset = page_index % 8;
    g_pmm.bitmap[byte_index] &= ~(1 << bit_offset);
}
