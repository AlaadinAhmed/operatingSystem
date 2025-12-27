# MyOS - Custom Operating System

A custom 64-bit Operating System built from scratch, featuring UEFI boot support, Intel HD Audio, PS/2 mouse, EXT4 filesystem, and modern graphics.

## 🎯 Current Status

### ✅ Completed Features

| Component | Status | Description |
|-----------|--------|-------------|
| **UEFI Boot** | ✅ Complete | Pure UEFI bootloader using gnu-efi |
| **GRUB Support** | ✅ Complete | Multiboot-compliant with hybrid BIOS/UEFI ISO |
| **64-bit Kernel** | ✅ Complete | C++ kernel with freestanding runtime |
| **Memory Management** | ✅ Basic | Simple heap allocator (`kmalloc`/`kfree`) |
| **VGA Graphics** | ✅ Complete | GOP/VBE linear framebuffer (1920x1080) |
| **TrueType Fonts** | ✅ Complete | TTF rendering via `stb_truetype` |
| **Image Loading** | ✅ Complete | BMP/PNG support via `stb_image` |
| **EXT4 Filesystem** | ✅ Complete | Full read/write support via `lwext4` |
| **Intel HD Audio** | ✅ Complete | HDA driver (works in VMware) |
| **AC'97 Audio** | ✅ Complete | AC'97 driver (works in QEMU) |
| **PS/2 Mouse** | ✅ Complete | Polling-based mouse driver with cursor |
| **Serial Debugging** | ✅ Complete | COM1 serial output |

## 🔧 Building

### Prerequisites
- GCC 64-bit cross-compiler
- NASM assembler
- CMake 3.12+
- QEMU and/or VMware Player
- `grub-mkrescue` (for ISO)
- `gnu-efi` (included)
- `mtools` (for FAT images)

### Build Commands
```bash
cmake .
make              # Build kernel and ISO
make iso          # Build GRUB bootable ISO
make rootfs       # Build root filesystem
make kernel_efi   # Build UEFI application
```

## 🚀 Running

### QEMU (AC'97 Audio - Recommended)
```bash
make run-grub-uefi-ac97   # UEFI + AC'97 audio (working)
make run-grub-uefi        # UEFI + HDA (DMA issues in QEMU)
make run-grub             # BIOS mode
```

### VMware Player (Intel HD Audio - Recommended)
```bash
make run-vmware           # Creates config and launches VMware
make vmware-config        # Just create config files
```

VMware provides better Intel HD Audio emulation than QEMU.

### Output Files
| File | Description |
|------|-------------|
| `build/myos.iso` | GRUB bootable ISO (BIOS + UEFI) |
| `build/efi/myos.efi` | UEFI application |
| `build/rootfs.img` | EXT4 filesystem |
| `build/myos.vmx` | VMware configuration |

## 📁 Project Structure

```
├── src/
│   ├── kernel/         # Kernel main and entry
│   ├── drivers/
│   │   ├── audio/      # Intel HDA, AC'97 drivers
│   │   ├── mouse/      # PS/2 mouse driver
│   │   ├── gop/        # GOP graphics driver
│   │   └── bus/        # PCI driver
│   ├── fs/             # Filesystem (lwext4)
│   ├── print/          # Serial/screen output
│   └── external/
│       ├── lwext4/     # EXT4 library
│       ├── gnu-efi/    # UEFI library
│       └── dr_libs/    # Audio file parsers (WAV, MP3, FLAC)
├── include/            # Header files
├── resources/          # Fonts, images, GRUB config
└── build/              # Build output
```

## 🎵 Audio Support

| Driver | Emulator | Status |
|--------|----------|--------|
| Intel HDA | VMware | ✅ Working |
| Intel HDA | QEMU | ❌ DMA issues |
| AC'97 | QEMU | ✅ Working |
| AC'97 | VMware | ✅ Working |

**Recommendation:** Use VMware for HDA testing, QEMU with AC'97 for quick testing.

## 📖 Documentation

- [OS Structure & Architecture](os_structure.md)
- Serial debug output: COM1 @ 115200 baud

## 📄 License

Educational project. See individual library licenses for third-party code.
