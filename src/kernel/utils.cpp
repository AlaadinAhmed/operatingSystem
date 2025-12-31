#include "drivers/vga.h"       // For debug checkpoints
#include "fs/lwext4_adapter.h" // For ext4_blockdev
#include "memory/kmalloc.h"    // Include kmalloc.h for declarations
#include "print/print.h"
#include <ext4.h> // For ext4_fopen, ext4_fsize, ext4_fread, ext4_fclose, EOK
#include <stddef.h>
#include <stdint.h>

extern "C" {

// For EFI builds, gnu-efi provides these functions
#ifndef __EFI__

void *memset(void *s, int c, size_t n) noexcept {
  uint8_t *p = (uint8_t *)s;

  // Fill small buffers or unaligned start byte-by-byte
  while (n > 0 && ((uintptr_t)p & 7)) {
    *p++ = (uint8_t)c;
    n--;
  }

  // Fill 64-bit chunks
  if (n >= 8) {
    uint64_t c64 = (uint8_t)c;
    c64 |= c64 << 8;
    c64 |= c64 << 16;
    c64 |= c64 << 32;

    uint64_t *p64 = (uint64_t *)p;
    while (n >= 8) {
      *p64++ = c64;
      n -= 8;
    }
    p = (uint8_t *)p64;
  }

  // Fill remaining bytes
  while (n > 0) {
    *p++ = (uint8_t)c;
    n--;
  }

  return s;
}

void *memcpy(void *dest, const void *src, size_t n) noexcept {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;

  // Copy small buffers or unaligned start byte-by-byte
  while (n > 0 && (((uintptr_t)d & 7) || ((uintptr_t)s & 7))) {
    *d++ = *s++;
    n--;
  }

  // Copy 64-bit chunks
  if (n >= 8) {
    uint64_t *d64 = (uint64_t *)d;
    const uint64_t *s64 = (const uint64_t *)s;
    while (n >= 8) {
      *d64++ = *s64++;
      n -= 8;
    }
    d = (uint8_t *)d64;
    s = (const uint8_t *)s64;
  }

  // Copy remaining bytes
  while (n > 0) {
    *d++ = *s++;
    n--;
  }

  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  while (n--) {
    if (*p1 != *p2)
      return *p1 - *p2;
    p1++;
    p2++;
  }
  return 0;
}

#endif /* __EFI__ */

// ============================================================================
// Simple Free-List Memory Allocator
// ============================================================================
// Each block has a header containing size and free status.
// Free blocks are linked together in a free list.
// ============================================================================

struct BlockHeader {
  size_t size;       // Size of the data portion (not including header)
  bool is_free;      // Whether this block is free
  BlockHeader *next; // Next block in memory (for coalescing)
};

static const size_t HEADER_SIZE = sizeof(BlockHeader);
static const size_t MIN_BLOCK_SIZE = 16;   // Minimum allocation size
static const size_t HEAP_START = 0x200000; // 2MB
static const size_t HEAP_END = 0x10000000; // 256MB (increased from 32MB)

static BlockHeader *heap_start = nullptr;
static bool heap_initialized = false;

// Align size to 8 bytes
static inline size_t align8(size_t size) { return (size + 7) & ~7; }

void init_memory() {
  if (heap_initialized)
    return;

  // Initialize the heap with one big free block
  heap_start = (BlockHeader *)HEAP_START;
  heap_start->size = (HEAP_END - HEAP_START) - HEADER_SIZE;
  heap_start->is_free = true;
  heap_start->next = nullptr;
  heap_initialized = true;
}

// Find a free block that fits the requested size
static BlockHeader *find_free_block(size_t size) {
  BlockHeader *current = heap_start;
  while (current != nullptr) {
    if (current->is_free && current->size >= size) {
      return current;
    }
    current = current->next;
  }
  return nullptr;
}

// Split a block if it's significantly larger than needed
static void split_block(BlockHeader *block, size_t size) {
  // Check if there's enough space to split (avoid underflow)
  size_t min_split_size = size + HEADER_SIZE + MIN_BLOCK_SIZE;
  if (block->size < min_split_size) {
    return; // Block is too small to split
  }

  size_t remaining = block->size - size;
  BlockHeader *new_block =
      (BlockHeader *)((uint8_t *)block + HEADER_SIZE + size);
  new_block->size = remaining - HEADER_SIZE;
  new_block->is_free = true;
  new_block->next = block->next;

  block->size = size;
  block->next = new_block;
}

// Coalesce adjacent free blocks
static void coalesce() {
  BlockHeader *current = heap_start;
  while (current != nullptr && current->next != nullptr) {
    if (current->is_free && current->next->is_free) {
      // Merge with next block
      current->size += HEADER_SIZE + current->next->size;
      current->next = current->next->next;
      // Don't advance - check if we can merge more
    } else {
      current = current->next;
    }
  }
}

void *kmalloc(size_t size) {
  if (size == 0)
    return nullptr;

  // Ensure heap is initialized
  if (!heap_initialized) {
    init_memory();
  }

  // Align size
  size = align8(size);
  if (size < MIN_BLOCK_SIZE)
    size = MIN_BLOCK_SIZE;

  // Find a free block
  BlockHeader *block = find_free_block(size);
  if (block == nullptr) {
    // Try coalescing and searching again
    coalesce();
    block = find_free_block(size);
    if (block == nullptr) {
      kprintf("kmalloc failed: OOM (Stack Collision)\n");
      return nullptr;
    }
  }

  // Split if the block is too large
  split_block(block, size);

  // Mark as used
  block->is_free = false;

  // Return pointer to data (after header)
  void *ptr = (void *)((uint8_t *)block + HEADER_SIZE);

  // Zero-initialize
  memset(ptr, 0, size);

  return ptr;
}

void kfree(void *ptr) {
  if (ptr == nullptr)
    return;

  // Get the block header
  BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);

  // Sanity check - make sure this looks like a valid block
  if ((uint8_t *)block < (uint8_t *)HEAP_START ||
      (uint8_t *)block >= (uint8_t *)HEAP_END) {
    return; // Invalid pointer
  }

  // Mark as free
  block->is_free = true;

  // Coalesce adjacent free blocks
  coalesce();
}

void *krealloc(void *ptr, size_t size) {
  if (size == 0) {
    kfree(ptr);
    return nullptr;
  }
  if (ptr == nullptr) {
    return kmalloc(size);
  }

  // Get the block header
  BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);
  size_t old_size = block->size;

  // If the block is already big enough, just return the same pointer
  size = align8(size);
  if (old_size >= size) {
    return ptr;
  }

  // Allocate new block
  void *new_ptr = kmalloc(size);
  if (new_ptr != nullptr) {
    // Copy old data
    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);
  }
  return new_ptr;
}

void *kcalloc(size_t nmemb, size_t size) { // Renamed from calloc to kcalloc
  // printf("kcalloc(%d, %d)\n", nmemb, size);
  size_t total = nmemb * size;
  void *ptr = kmalloc(total);
  // if (ptr)
  //     memset(ptr, 0, total); // kmalloc now zeroes memory
  return ptr;
}

void __stack_chk_fail(void) {
  // Panic
  while (1)
    ;
}

size_t strlen(const char *s) {
  size_t len = 0;
  while (s[len])
    len++;
  return len;
}

void *memmove(void *dest, const void *src, size_t n) {
  char *d = (char *)dest;
  const char *s = (const char *)src;
  if (d < s) {
    while (n--)
      *d++ = *s++;
  } else {
    const char *lasts = s + (n - 1);
    char *lastd = d + (n - 1);
    while (n--)
      *lastd-- = *lasts--;
  }
  return dest;
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
  char *b = (char *)base;
  for (size_t i = 1; i < nmemb; i++) {
    for (size_t j = i; j > 0; j--) {
      char *p1 = b + (j - 1) * size;
      char *p2 = b + j * size;
      if (compar(p1, p2) > 0) {
        // Swap
        for (size_t k = 0; k < size; k++) {
          char tmp = p1[k];
          p1[k] = p2[k];
          p2[k] = tmp;
        }
      } else {
        break;
      }
    }
  }
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n--) {
    if (*s1 != *s2)
      return *(const unsigned char *)s1 - *(const unsigned char *)s2;
    if (*s1 == 0)
      break;
    s1++;
    s2++;
  }
  return 0;
}

int abs(int x) { return (x < 0) ? -x : x; }

// Returns a pointer to the allocated buffer and sets file_size.
// Returns NULL on error. The caller is responsible for kfree'ing the buffer.
unsigned char *read_file_to_memory(const char *mount_point,
                                   const char *filename, size_t *file_size) {
  ext4_file file;
  int rc;

  char full_path[256];
  strcpy(full_path, mount_point);
  int len = strlen(full_path);
  if (len > 0 && full_path[len - 1] != '/') {
    full_path[len] = '/';
    full_path[len + 1] = '\0';
  }
  strcpy(full_path + strlen(full_path), filename);

  rc = ext4_fopen(&file, full_path, "rb");
  if (rc != EOK) {
    kprintf("read_file_to_memory: Failed to open %s (error %d)\n", full_path,
            rc);
    return NULL;
  }

  uint64_t size = ext4_fsize(&file);
  if (size == 0) {
    ext4_fclose(&file);
    return NULL;
  }

  unsigned char *buffer = (unsigned char *)kmalloc(size);
  if (buffer == NULL) {
    kprintf("read_file_to_memory: OOM for %s\n", full_path);
    ext4_fclose(&file);
    return NULL;
  }

  size_t bytes_read;
  rc = ext4_fread(&file, buffer, size, &bytes_read);

  if (rc != EOK || bytes_read != size) {
    kfree(buffer);
    ext4_fclose(&file);
    return NULL;
  }

  ext4_fclose(&file);
  *file_size = (size_t)size;
  return buffer;
}

// Implementations for lwext4 user-provided memory functions
void *ext4_user_malloc(size_t size) { return kmalloc(size); }

void *ext4_user_calloc(size_t nmemb, size_t size) {
  return kcalloc(nmemb, size);
}

void *ext4_user_realloc(void *ptr, size_t size) { return krealloc(ptr, size); }

void ext4_user_free(void *ptr) { kfree(ptr); }

} // End of extern "C"

// Renamed to avoid conflicts with standard library functions
void *operator new(size_t size) { return kmalloc(size); }

void *operator new[](size_t size) { return kmalloc(size); }

void operator delete(void *p) { kfree(p); }

void operator delete(void *p, size_t size) { kfree(p); }

void operator delete[](void *p) { kfree(p); }

void operator delete[](void *p, size_t size) { kfree(p); }

extern "C" void __gxx_personality_v0() {}

extern "C" void _Unwind_Resume() {
  while (1)
    ;
}

// C++ static local variable guard stubs
// These are used by the compiler for thread-safe initialization of static
// locals
extern "C" int __cxa_guard_acquire(uint64_t *guard) {
  if (*guard == 0) {
    *guard = 1; // Mark as initializing
    return 1;   // Return 1 to indicate initialization should proceed
  }
  return 0; // Already initialized
}

extern "C" void __cxa_guard_release(uint64_t *guard) {
  *guard = 2; // Mark as fully initialized
}

extern "C" void __cxa_guard_abort(uint64_t *guard) {
  *guard = 0; // Reset on failure
}
