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
