# OS Architecture Documentation

This document provides a detailed technical overview of the Operating System's architecture, components, and data flow.

## System Overview

[Insert System Architecture Diagram Here]

## Boot Sequence (UEFI)

### Stage 1: UEFI Firmware → EFI Application (`efi_entry.c`)

[Insert UEFI Boot Flow Diagram Here]

### Stage 2: Kernel Main (`kernel.cpp`)

[Insert Kernel Initialization Flow Diagram Here]

## Memory Map

[Insert Memory Map Diagram Here]

## Component Details

### Graphics Pipeline
1.  **Rendering**: UI elements draw to a software `Surface` (wrapping the `m_backbuffer`).
2.  **Compositing**: The `Compositor` manages a list of `Window` objects and calls their `onDraw`.
3.  **Presentation**: `System::Render` copies the `m_backbuffer` to the GOP Framebuffer (VRAM) using an optimized 64-bit `memcpy`.

### Input Handling
-   **Mouse**: PS/2 mouse driver handles interrupts/polling, updates global state.
-   **Events**: `System::Run` polls mouse state and dispatches `onMouseDown`/`onMouseUp` events to the active `Window` or `Button`.

### Filesystem
-   **lwext4**: Used for EXT4 filesystem operations.
-   **VFS**: `fs/lwext4_adapter.cpp` provides the glue between the raw disk/ramdisk and lwext4.

## Build System

### Library Structure

The build system compiles the following static libraries before linking the final kernel:

*   `libkernel_lib.a`: Kernel utilities, memory management
*   `libdrivers_lib.a`: VGA, keyboard, mouse, audio, usb
*   `libfs_lib.a`: lwext4 adapter
*   `libui_lib.a`: UI widgets (Button, Text, Window)
*   `libgraphics_lib.a`: Font renderer, Compositor
*   `libprint_lib.a`: Serial/console printing

## Debugging

### Serial Output (COM1)
- Baud rate: 115200
- Format: 8N1
- All `kprintf()` calls go to serial.

### QEMU Debug
```bash
make run-grub-uefi
# Check serial.log for output
```
