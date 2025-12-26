# UEFI Boot Fixes

## Issue
The kernel was crashing with `#UD` (Invalid Opcode) at address `0xB0000` (Legacy VGA memory) or other low addresses during boot.

## Root Cause
1.  **Memory Conflict**: The kernel was being loaded at a low address (e.g., `0x741E6` or `0xDEDB28`) by UEFI because the PE header specified `ImageBase` as 0. This caused the kernel code or data to overlap with legacy VGA memory (`0xA0000-0xBFFFF`) or other reserved areas.
2.  **Missing Relocations**: The `.init_array` section (containing global constructors) was not being relocated by the EFI loader because `objcopy` does not generate PE relocations for it (it's in `.data`). As a result, the constructors were being called at their link-time addresses (offsets), which pointed to invalid memory when the kernel was loaded at a different address.

## Fixes

### 1. Force High Load Address
We created a custom linker script `src/boot/efi.lds` (based on `/usr/lib/elf_x86_64_efi.lds`) that sets the base address to `0x2000000` (32MB).
This ensures:
*   The kernel is linked assuming a high base address.
*   UEFI loads the kernel at this address (since 32MB is usually free RAM), avoiding low-memory conflicts.
*   The kernel does not overlap with the heap (which ends at 16MB).

### 2. Manual Relocation Fixup
We implemented a manual relocation fixup in `src/kernel/kernel.cpp`.
*   The build script `build_kernel_efi.sh` now detects the offset of `main` in the linked ELF binary and passes it as `MAIN_OFFSET` to the compiler.
*   `kernel.cpp` calculates the actual load address (`image_base`) at runtime by comparing the address of `main` with `MAIN_OFFSET`.
*   Before calling global constructors, the code iterates through `__init_array` and adjusts the function pointers:
    *   If a pointer matches the link-time base (`0x2000000`), it is rebased to the actual load address.
    *   If a pointer is a small offset (legacy behavior), it is also rebased.

### 3. Build Script Updates
*   Updated `build_kernel_efi.sh` to use the custom linker script.
*   Added logic to extract `MAIN_OFFSET` and recompile `kernel.cpp`.
*   Ensured `-fPIC` is used to generate position-independent code (though we now prefer a fixed load address, PIC is still good practice for shared objects).

## Verification
The kernel now boots successfully without crashing. The global constructors (e.g., for `lwext4` adapter) are called correctly.
