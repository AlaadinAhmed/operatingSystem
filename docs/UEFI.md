# UEFI Support

This OS supports booting via UEFI (Unified Extensible Firmware Interface) in addition to the legacy BIOS/Multiboot method.

## Overview

The UEFI implementation works as follows:
1.  **Loader**: A custom EFI application (`src/boot/efi_entry.c`) serves as the kernel loader.
2.  **Kernel**: The C++ kernel (`src/kernel/kernel.cpp`) is compiled as a freestanding library and linked with the loader.
3.  **Graphics**: The loader sets up the Graphics Output Protocol (GOP) and populates a VBE-compatible mode info structure at `0x5200`, allowing the existing kernel graphics drivers to work unchanged.
4.  **Filesystem**: The kernel uses `lwext4` to access the root filesystem image (`rootfs.img`) which is attached as a secondary drive.

## Building and Running

To build and run the UEFI version, use the provided helper script:

```bash
./run_uefi.sh
```

This script performs the following steps:
1.  Compiles the kernel and loader using `build_kernel_efi.sh`.
2.  Creates a bootable FAT32 disk image (`uefi.img`) containing the kernel as `/EFI/BOOT/BOOTX64.EFI`.
3.  Creates an EXT4 root filesystem image (`rootfs.img`) and populates it with fonts and images.
4.  Launches QEMU with OVMF (UEFI firmware) and attaches both drives.

## Technical Details

- **Entry Point**: `efi_main` in `src/boot/efi_entry.c`.
- **Memory Map**: The loader retrieves the memory map and exits boot services before calling the kernel `main`.
- **Constructors**: Global C++ constructors are handled by the `.init_array` section, which is explicitly preserved during the PE/COFF conversion.
- **Isolation**: The kernel is compiled with `-ffreestanding` and does not rely on UEFI boot services after initialization.

## Troubleshooting

If you encounter "Invalid Opcode" or crashes:
- Ensure `objcopy` includes `.init_array` and `.ctors` sections (fixed in `build_kernel_efi.sh`).
- Check that `rootfs.img` is correctly attached as the secondary drive (index 1).
