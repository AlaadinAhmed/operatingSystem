# Custom Operating System

A custom Operating System built from scratch, featuring dual bootloader support (custom + GRUB), a C++ kernel with modern graphics capabilities, and EXT4 filesystem support.

## 🎯 Current Status

### ✅ Completed Features

| Component | Status | Description |
|-----------|--------|-------------|
| **Custom Bootloader** | ✅ Complete | Two-stage assembly bootloader with VBE graphics mode setup |
| **GRUB Support** | ✅ Complete | Multiboot-compliant kernel entry with automatic VBE detection |
| **Protected Mode** | ✅ Complete | 32-bit protected mode with GDT setup |
| **Memory Management** | ✅ Basic | Simple heap allocator (`kmalloc`/`kfree`) |
| **VGA Graphics** | ✅ Complete | 1920x1080 linear framebuffer with drawing primitives |
| **TrueType Fonts** | ✅ Complete | TTF rendering via `stb_truetype` (Roboto, JetBrains Mono, BBHBogle) |
| **Image Loading** | ✅ Complete | BMP/PNG image support via `stb_image` |
| **EXT4 Filesystem** | ✅ Complete | Full read/write support via `lwext4` library |
| **Serial Debugging** | ✅ Complete | COM1 serial output for debugging |
| **SSE/FPU** | ✅ Complete | Hardware floating-point and SIMD enabled |

### 🔄 In Progress

- System class architecture for modular OS components
- Event-driven main loop structure

## 🗺️ Roadmap

### Phase 1: Core Infrastructure ✅
- [x] Bootloader (Real Mode → Protected Mode)
- [x] VBE graphics initialization
- [x] Basic memory management
- [x] Kernel entry and C++ runtime
- [x] Serial debugging output

### Phase 2: Graphics & Filesystem ✅
- [x] Linear framebuffer graphics
- [x] TTF font rendering
- [x] Image loading (BMP, PNG)
- [x] EXT4 filesystem integration
- [x] File reading from disk

### Phase 3: User Interface (Next)
- [ ] Window manager / compositor
- [ ] Mouse cursor and input handling
- [ ] Keyboard input (PS/2)
- [ ] Basic widget system (buttons, text fields)
- [ ] Desktop environment shell

### Phase 4: System Services
- [ ] Process/task management
- [ ] Virtual memory (paging)
- [ ] Interrupt handling (IDT)
- [ ] System calls
- [ ] Timer/RTC support

### Phase 5: Networking & Advanced Features
- [ ] Network stack (TCP/IP)
- [ ] Sound driver
- [ ] USB support
- [ ] Multi-core support (SMP)

## 📁 Project Structure

```
├── src/
│   ├── boot/           # Bootloader (boot.asm, loader.asm, kernel_entry.asm)
│   ├── kernel/         # Kernel core (kernel.cpp, utils.cpp)
│   ├── drivers/        # Hardware drivers (VGA, keyboard, SSE)
│   ├── system/         # System class and main logic
│   ├── fs/             # Filesystem adapters (lwext4)
│   ├── print/          # Serial and screen printing
│   ├── shell/          # Command shell (WIP)
│   ├── disk/           # Disk I/O operations
│   └── external/       # Third-party libraries (lwext4)
├── include/            # Header files
├── resources/          # Fonts, images, GRUB config
├── scripts/            # Build helper scripts
└── iso/                # ISO build output directory
```

## 🔧 Building

### Prerequisites
- GCC cross-compiler (i686-elf-gcc) or system GCC with `-m32`
- NASM assembler
- CMake 3.10+
- QEMU (for testing)
- `grub-mkrescue` (for ISO creation)

### Build Commands
```bash
# Configure
cmake -B build

# Build
cmake --build build

# Or using the project Makefile (after cmake)
make
```

### Output Files
- `kernel.elf` - The kernel binary
- `os-image.bin` - Bootable disk image (custom bootloader)
- `myos.iso` - Bootable ISO (GRUB)
- `rootfs.img` - EXT4 filesystem image with resources

## 🚀 Running

### Custom Bootloader (Floppy/Disk Image)
```bash
./run.sh
```

### GRUB ISO
```bash
./run_iso.sh
```

### Manual QEMU
```bash
# Disk image
qemu-system-i386 -drive format=raw,file=os-image.bin -serial stdio

# ISO with GRUB
qemu-system-i386 -cdrom myos.iso -drive format=raw,file=rootfs.img -serial stdio
```

## 🎨 Design Philosophy

### Modern C++ on Bare Metal
This OS leverages C++ features (classes, inheritance, templates) in a freestanding environment. A minimal runtime enables high-level code organization with direct hardware access.

### Component-Based Monolith
Subsystems (`libfs`, `libdrivers`, `libshell`) are compiled as separate static libraries, enforcing modularity during development while producing a single efficient binary at runtime.

### Graphics-First Approach
Instead of legacy VGA text mode, the bootloader immediately sets up a VESA Linear Framebuffer (1920x1080x32). This enables:
- High-quality TrueType font rendering
- Image display from boot
- Foundation for a modern GUI

## 📖 Documentation

- [OS Structure & Pseudocode](os_structure.md) - Detailed architectural overview
- Serial output available on COM1 (115200 baud) for debugging

## 📄 License

This project is for educational purposes. See individual library licenses for third-party code (lwext4, stb_truetype, stb_image).
