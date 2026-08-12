#pragma once
#include "kernel/sync/spinlock.h"
#include <cstddef>

#define HEAP_START_ADDR 0xFFFFFFFF90000000ULL
#define PAGE_SIZE 4096
struct HeapBlockHeader {
    size_t size;
    bool is_free;
    HeapBlockHeader *next_block;
};
#include "memory/kmalloc.h"
void *kernel_alloc_virtual_pages(size_t page_count);
void *operator new(size_t size);
void operator delete(void *p) noexcept;
void operator delete(void *p, size_t) noexcept;
