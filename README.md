# Custom Operating System

This project is a custom Operating System built from scratch, featuring a custom bootloader and a C++ kernel.

## Features
-   **Custom Bootloader**: Written in Assembly, handles switching to Protected Mode and loading the kernel.
-   **C++ Kernel**: Supports basic hardware initialization, VGA graphics, and TrueType font rendering.
-   **Filesystem**: Integration with `lwext4` for EXT4 filesystem support.
-   **Graphics**: VBE-based graphics mode with custom drawing primitives.

## Structure
-   `src/boot/`: Bootloader source code.
-   `src/kernel/`: Kernel source code.
-   `src/drivers/`: Hardware drivers (VGA, Font, etc.).
-   `src/fs/`: Filesystem adapters.
-   `resources/`: Assets (Fonts, Images, GRUB config).
-   `build/lib/`: Compiled static libraries.

## Documentation
For a detailed overview of the OS structure and pseudocode, please refer to [os_structure.md](os_structure.md).

## Building
(Add build instructions here, e.g., using CMake and Make)
```bash
mkdir build
cd build
cmake ..
make
```

## Running
Use the provided script to run in QEMU:
```bash
./run_iso.sh
```

## Design Philosophy

### Modern C++ on Bare Metal
This OS demonstrates how to leverage C++ features (classes, inheritance, templates) in a freestanding environment. By stripping away the standard library and implementing a minimal runtime, we achieve high-level code organization with low-level hardware access.

### Component-Based Monolith
The kernel is built as a "static library monolith." Subsystems like the filesystem (`libfs`), drivers (`libdrivers`), and shell (`libshell`) are compiled as separate static libraries and linked together. This enforces modularity during development but results in a single, efficient binary at runtime.

### Graphics-First Approach
Instead of relying on the legacy VGA text mode (0xB8000), the bootloader sets up a VESA Linear Framebuffer immediately. This allows the kernel to own the entire rendering pipeline, using `stb_truetype` for high-quality text and `stb_image` for graphics, providing a modern visual experience from the first boot.
