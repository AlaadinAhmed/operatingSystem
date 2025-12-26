# EFI Kernel Build Guide

This document explains how to build and test the EFI kernel.

## Quick Start

```bash
# Build EFI application
./build_efi.sh

# Test in QEMU
qemu-system-x86_64 -bios /usr/share/edk2/x64/OVMF.4m.fd \
    -drive format=raw,file=build/test/uefi.img -m 512

# Install to real hardware (via GRUB)
sudo ./install_efi_grub.sh
```

## Source Files

| File | Description |
|------|-------------|
| `src/test/efi_kernel.c` | EFI application source code |
| `build_efi.sh` | Build script using gnu-efi |
| `install_efi_grub.sh` | Installs EFI app and creates GRUB entry |

## Build Output

| File | Description |
|------|-------------|
| `build/test/BOOTX64.EFI` | Compiled EFI application (~50KB) |
| `build/test/uefi.img` | Bootable FAT32 image for QEMU |

## EFI Application Features

The test EFI kernel demonstrates:

1. **EFI Console Output** - Print text to UEFI console
2. **GOP (Graphics Output Protocol)** - Get framebuffer access
3. **Framebuffer Drawing** - Draw pixels directly to screen

### Visual Test Pattern

When the kernel runs, it displays:
- **Cyan background** - Fills entire screen
- **Red square** - Top-left corner
- **Green square** - Top-right corner
- **Blue square** - Bottom-left corner
- **White square** - Bottom-right corner

## How It Works

### 1. Entry Point

```c
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
```

UEFI calls `efi_main` with handles to the image and system table.

### 2. Graphics Access

```c
// Get Graphics Output Protocol
Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GopGuid, NULL, (VOID**)&Gop);

// Access framebuffer
FrameBuffer = (UINT32*)Gop->Mode->FrameBufferBase;
Width = Gop->Mode->Info->HorizontalResolution;
Height = Gop->Mode->Info->VerticalResolution;
```

### 3. Drawing

```c
// Draw a pixel at (x, y) with color
FrameBuffer[y * Pitch + x] = color;  // 0x00RRGGBB format
```

## Compilation Details

The build process:

1. **Compile** with gcc (freestanding mode)
2. **Link** with gnu-efi libraries (`libefi.a`, `libgnuefi.a`)
3. **Convert** to PE format with objcopy

Key compiler flags:
- `-ffreestanding` - No standard library
- `-fno-stack-protector` - No stack canary
- `-mno-red-zone` - Required for x86_64 EFI

## Debugging

### QEMU Debug Console

Add `-serial stdio` to see debug output:

```bash
qemu-system-x86_64 -bios /usr/share/edk2/x64/OVMF.4m.fd \
    -drive format=raw,file=build/test/uefi.img -m 512 \
    -serial stdio
```

### EFI Shell

If boot fails, enter EFI Shell and run manually:

```
FS0:
cd EFI\BOOT
BOOTX64.EFI
```
