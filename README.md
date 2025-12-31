# MyOS - Custom Operating System

A custom 64-bit Operating System built from scratch, featuring UEFI boot support, a custom UI compositor, Intel HD Audio, and modern graphics.

## Current Status

### Completed Features

| Component | Status | Description |
|-----------|--------|-------------|
| **UEFI Boot** | Complete | Native UEFI bootloader using gnu-efi |
| **Graphics** | Complete | GOP linear framebuffer (1920x1080) with **Double Buffering** |
| **UI System** | Complete | Custom Compositor, Windowing system, Buttons, Labels |
| **Input** | Complete | PS/2 Mouse & Keyboard (Interrupt-driven) |
| **Audio** | Complete | Intel HDA & AC'97 drivers (WAV playback) |
| **Filesystem** | Complete | EXT4 read/write support via `lwext4` |
| **USB** | In Progress | XHCI Controller detection |
| **Fonts** | Complete | TrueType rendering via `stb_truetype` |
| **Images** | Complete | BMP/PNG support via `stb_image` |

## Building

### Prerequisites
- GCC 64-bit cross-compiler
- NASM assembler
- CMake 3.12+
- QEMU (for testing)
- `gnu-efi` (included)
- `mtools` (for FAT images)

### Build Commands
```bash
mkdir build
cd build
cmake ..
make run-grub-uefi        # Build and run in QEMU (UEFI mode)
```

## Running

### QEMU (Recommended)
```bash
make run-grub-uefi        # UEFI + HDA Audio + UI
```

### VMware Player
```bash
make run-vmware           # Creates config and launches VMware
```

## Project Structure

The project is organized into the following main directories:

*   **src/kernel**: Kernel entry point and main initialization logic.
*   **src/system**: Core system class managing UI, Input, and Audio orchestration.
*   **src/ui**: UI Widget implementations (Button, Text, Window).
*   **src/graphics**: Graphics subsystem including Compositor and Font Renderer.
*   **src/drivers**: Hardware drivers for Audio (HDA/AC97), Mouse, USB, and PCI.
*   **src/fs**: Filesystem abstraction and lwext4 integration.
*   **src/external**: Third-party libraries (lwext4, stb, gnu-efi).
*   **include**: Header files for all components.
*   **build**: Build artifacts and output images.

## Documentation

- [OS Architecture](os_structure.md)
- Serial debug output: COM1 @ 115200 baud

## License

Educational project. See individual library licenses for third-party code.

