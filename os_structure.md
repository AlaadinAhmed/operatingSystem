# OS Structure Documentation

This document outlines the structure of the Operating System, focusing on the Bootloader and the Kernel.

## Architectural Concepts

The OS follows a **monolithic** design where all services (filesystem, drivers, logic) run in the same address space (kernel mode). However, the build system enforces a **modular structure** by compiling subsystems into distinct static libraries.

-   **Bootloader**: A two-stage assembly loader that escapes Real Mode and sets up a 32-bit Protected Mode environment.
-   **Kernel Core**: The central C++ entry point that initializes the global system state (GDT, IDT, Memory).
-   **Driver Abstraction**: Hardware specifics are encapsulated in classes (e.g., `VGA`, `Keyboard`), providing clean interfaces to the rest of the kernel.
-   **Resources**: Fonts and images are stored in `resources/` and injected into the root filesystem image during the build process.
-   **Libraries**: Subsystems are compiled into static libraries stored in `build/lib/` before being linked into the final kernel.

## 1. Bootloader (`src/boot/boot.asm`)

The bootloader is the first code executed by the BIOS. It is responsible for loading the rest of the operating system into memory and switching the CPU to the appropriate mode.

### Structure
1.  **Initialization**: Sets up segment registers and the stack pointer.
2.  **Boot Drive Saving**: Stores the boot drive number passed by BIOS.
3.  **Loading Loader**: Reads the secondary loader (or kernel) from the disk into memory.
4.  **Transfer Control**: Jumps to the loaded code.

### Pseudocode

```assembly
Function Start:
    Jump to Main

Function Main:
    // 1. Setup Environment
    Save Boot_Drive_Number to Memory
    Initialize Segment Registers (DS, ES, SS) to 0
    Initialize Stack Pointer (SP) to 0x7C00

    // 2. Load Code from Disk
    // Use BIOS INT 0x13, AH=0x42 (Extended Read)
    Setup Disk Address Packet (DAP):
        - Number of sectors to read (e.g., 15)
        - Destination memory address (e.g., 0x7E00)
        - Start LBA (Logical Block Address) on disk
    
    Call BIOS Interrupt 0x13
    If Error (Carry Flag Set):
        Jump to Disk_Error

    // 3. Handover
    Pass Boot_Drive_Number in DL register
    Jump to 0x7E00 (Address where loader was loaded)

Function Disk_Error:
    Print "Error" message
    Hang (Infinite Loop)
```

## 2. Kernel (`src/kernel/kernel.cpp`)

The kernel is the core of the operating system. In this project, it handles hardware initialization, memory management (basic), filesystem access, and the main execution loop.

### Structure
1.  **Initialization**:
    -   `init_memory()`: Initializes memory management.
    -   `vga_clear_screen()`: Clears the display.
    -   `init_ttf()`: Initializes TrueType font rendering.
    -   `ext4_mount()`: Mounts the filesystem.
2.  **Main Loop**:
    -   Clears the screen.
    -   Draws graphical elements (e.g., debug rectangle).
    -   Renders text using TTF.
    -   Updates state (e.g., time counter).

### Pseudocode

```cpp
Function Main:
    // 1. Initialization
    Call init_memory()
    
    // Get VBE (Video Electronics Standards Association) Info
    // (Pointers to framebuffer, width, height, pitch are passed from loader/boot)
    Initialize Framebuffer Pointer
    
    Call vga_clear_screen(Black)
    
    Call init_ttf() // Initialize Font Engine
    
    // 2. Filesystem Setup
    Initialize Block Device
    Register EXT4 Device
    Mount EXT4 Filesystem to "/"
    
    // Optional: Test File Access
    Try to open "/logo.bmp"
    If Success:
        Close File

    // 3. Main Execution Loop
    Initialize timeLapsed counter = 0
    
    Loop Forever:
        // Clear Screen
        Call vga_clear_screen(Black)
        
        // Draw Graphics
        Call vga_draw_rectangle(Red)
        
        // Draw Text
        Call draw_ttf_text("Hello World", White)
        
        // Update State
        Increment timeLapsed
        
        // Dynamic Behavior (Example)
        If timeLapsed is multiple of 100:
            Draw larger text
        Else:
            Draw smaller text
            
        // Debug Output
        Format string with timeLapsed
        Call draw_ttf_text(formatted_string)
        
        // Serial Debugging
        If timeLapsed is multiple of 10:
            Print to Serial Port
```
