extern "C" void kprintf(const char *format, ...);
#include "mem/heap.h"
#include "pmm.h"
#include "vmm.h"
#include "memory/kmalloc.h"
#include <cstdint>
spinlock_t g_heap_lock = {0};
uint64_t g_heap_current_top = HEAP_START_ADDR;
HeapBlockHeader *g_heap_first_entry = nullptr;
void *kernel_alloc_virtual_pages(size_t page_count) {
    void *allocation_start_addr = (void *)g_heap_current_top;
    for (size_t i = 0; i < page_count; i++) {
        uint64_t phy_frame = (uint64_t)pmm_alloc_page();
        vmm_map_page(g_heap_current_top, phy_frame, VMM_PRESENT | VMM_WRITE);
        g_heap_current_top += PAGE_SIZE;
    }
    return allocation_start_addr;
}

void *kmalloc(size_t size) {
    spinlock_acquire(&g_heap_lock);
    if (size == 0) {
        spinlock_release(&g_heap_lock);
        return nullptr;
    }
    size = (size + 7) & ~7;

    if (g_heap_first_entry == nullptr) {
        g_heap_first_entry = (HeapBlockHeader *)kernel_alloc_virtual_pages(4);
        g_heap_first_entry->size = (4 * PAGE_SIZE) - sizeof(HeapBlockHeader);
        g_heap_first_entry->is_free = true;
        g_heap_first_entry->next_block = nullptr;
    }
    HeapBlockHeader *current = g_heap_first_entry;
    while (current != nullptr) {
        if (current->is_free && (current->size >= size)) {
            if (current->size >= size + sizeof(HeapBlockHeader) + 8) {
                uintptr_t split_addr = (uintptr_t)current + sizeof(HeapBlockHeader) + size;
                HeapBlockHeader *new_block = (HeapBlockHeader *)split_addr;
                new_block->size = current->size - size - sizeof(HeapBlockHeader);
                new_block->is_free = true;
                new_block->next_block = current->next_block;
                current->next_block = new_block;
                current->size = size;
            }
            current->is_free = false;
            spinlock_release(&g_heap_lock);
            void *ptr = (void *)((uintptr_t)current + sizeof(HeapBlockHeader));
            memset(ptr, 0, size);
            return ptr;
        }
        current = current->next_block;
    }
    size_t pages_needed = (size + sizeof(HeapBlockHeader) + PAGE_SIZE - 1) / PAGE_SIZE;
    HeapBlockHeader *fresh_expansion_block = (HeapBlockHeader *)kernel_alloc_virtual_pages(pages_needed);
    fresh_expansion_block->size = (pages_needed * PAGE_SIZE) - sizeof(HeapBlockHeader);
    fresh_expansion_block->is_free = false;
    fresh_expansion_block->next_block = nullptr;

    // Append our fresh expansion block to the end of our heap linked list
    current = g_heap_first_entry;
    while (current->next_block != nullptr) {
        current = current->next_block;
    }
    current->next_block = fresh_expansion_block;

    spinlock_release(&g_heap_lock);
    void *ptr = (void *)((uintptr_t)fresh_expansion_block + sizeof(HeapBlockHeader));
    memset(ptr, 0, size);
    return ptr;
}
void kfree(void *ptr) {
    if (ptr == nullptr)
        return;

    spinlock_acquire(&g_heap_lock);
    // Roll backward to inspect the hidden block metadata header
    HeapBlockHeader *block = (HeapBlockHeader *)((uintptr_t)ptr - sizeof(HeapBlockHeader));
    block->is_free = true;

    // Coalescing Phase: Merge contiguous free blocks
    HeapBlockHeader *current = g_heap_first_entry;
    while (current != nullptr && current->next_block != nullptr) {
        if (current->is_free && current->next_block->is_free) {
            // Combine both data capacities into a single structural entry
            current->size += sizeof(HeapBlockHeader) + current->next_block->size;
            current->next_block = current->next_block->next_block;
            // Don't advance the loop pointer yet; check if the newly expanded block can merge again
            continue;
        }
        current = current->next_block;
    }
    spinlock_release(&g_heap_lock);
}
void *krealloc(void *ptr, size_t size) {
    if (size == 0) {
        kfree(ptr);
        return nullptr;
    }
    if (ptr == nullptr) {
        return kmalloc(size);
    }

    HeapBlockHeader *block = (HeapBlockHeader *)((uintptr_t)ptr - sizeof(HeapBlockHeader));
    size_t old_size = block->size;

    size = (size + 7) & ~7;
    if (old_size >= size) {
        return ptr;
    }

    void *new_ptr = kmalloc(size);
    if (new_ptr != nullptr) {
        memcpy(new_ptr, ptr, old_size);
        kfree(ptr);
    }
    return new_ptr;
}

void *kcalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = kmalloc(total);
    // kmalloc already zeroes memory, so we don't need to do it here
    return ptr;
}

void *operator new(size_t size) { return kmalloc(size); }
void *operator new[](size_t size) { return kmalloc(size); }
void operator delete(void *p) noexcept { kfree(p); }
void operator delete(void *p, size_t) noexcept { kfree(p); }
void operator delete[](void *p) noexcept { kfree(p); }
void operator delete[](void *p, size_t) noexcept { kfree(p); }
