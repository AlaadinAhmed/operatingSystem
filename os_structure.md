# OS Architecture Documentation

This document provides a detailed technical overview of the Operating System's architecture, components, and data flow.

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        Hardware Layer                           │
│  CPU (i686) │ VGA/VBE │ Keyboard │ Disk (ATA) │ Serial (COM1)  │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────┴───────────────────────────────────┐
│                        Driver Layer                              │
│   vga.cpp   │  keyboard.cpp  │  disk.cpp  │  print.cpp (serial) │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────┴───────────────────────────────────┐
│                      Subsystem Layer                             │
│    lwext4 (Filesystem)  │  stb_truetype  │  stb_image           │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────┴───────────────────────────────────┐
│                        System Layer                              │
│                    system.cpp (System class)                     │
│         Initialize() → Run() → ProcessInput() → Render()        │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────┴───────────────────────────────────┐
│                        Kernel Entry                              │
│                         kernel.cpp                               │
└─────────────────────────────────────────────────────────────────┘
```

## Boot Sequence

### Stage 1: BIOS → Bootloader (`boot.asm`)

```
BIOS loads first 512 bytes from disk to 0x7C00
     │
     ▼
boot.asm executes (Real Mode, 16-bit)
     │
     ├── Save boot drive number
     ├── Setup stack at 0x7C00
     ├── Load loader.bin using INT 13h (LBA)
     │   └── 15 sectors from LBA 2000 → 0x7E00
     │
     ▼
Jump to loader at 0x7E00
```

### Stage 2: Loader (`loader.asm`)

```
loader.asm executes (Real Mode)
     │
     ├── Load kernel (1200 sectors from LBA 2048 → 0x8000)
     ├── Load filesystem metadata
     │
     ├── VBE Graphics Setup:
     │   ├── Get VBE controller info (INT 10h, AX=4F00h)
     │   ├── Enumerate video modes
     │   ├── Find 1920×1080×32 mode with linear framebuffer
     │   └── Set video mode (INT 10h, AX=4F02h)
     │
     ├── Enable A20 line
     ├── Load GDT
     ├── Switch to Protected Mode (CR0.PE = 1)
     │
     ▼
Jump to kernel at 0x8000 (Protected Mode, 32-bit)
```

### Stage 3: Kernel Entry (`kernel_entry.asm`)

```
kernel_entry.asm executes (Protected Mode)
     │
     ├── Detect boot method (GRUB vs Custom Loader)
     │   └── Check for Multiboot magic (0x2BADB002)
     │
     ├── If GRUB:
     │   ├── Clear VBE info area at 0x5200
     │   └── Copy VBE info from Multiboot structure
     │
     ├── Clear BSS section
     ├── Setup kernel stack (256KB)
     ├── Enable SSE/FPU
     │
     ▼
Call main() in kernel.cpp
```

### Stage 4: Kernel Main (`kernel.cpp`)

```cpp
main(magic, multiboot_addr)
     │
     ├── init_memory()         // Initialize heap allocator
     ├── vga_clear_screen()    // Clear GRUB residuals
     │
     ├── Run global constructors (__init_array)
     │
     ├── Create System instance
     │   └── System::Initialize()
     │       ├── Mount EXT4 filesystem
     │       ├── Load fonts (TTF)
     │       ├── Draw text samples
     │       └── Load and render logo.bmp
     │
     ├── System::Run()         // Main event loop
     │
     └── System::Shutdown()
```

## UEFI Boot Sequence

### Stage 1: UEFI Firmware → EFI Application (`efi_entry.c`)

```
UEFI Firmware loads EFI/BOOT/BOOTX64.EFI
     │
     ▼
efi_main(ImageHandle, SystemTable) executes (64-bit)
     │
     ├── InitializeLib() → Sets up gnu-efi globals (ST, BS, RT)
     ├── ClearScreen()
     ├── Print(L"Hello from UEFI!")
     │
     └── Halt or return to firmware
```

### BIOS vs UEFI Comparison

| Aspect | BIOS Boot | UEFI Boot |
|--------|-----------|-----------|
| Mode | 16-bit Real → 32-bit | Native 64-bit |
| Graphics | VBE BIOS calls | GOP Protocol |
| Format | Raw binary | PE/COFF (.efi) |
| Entry | ASM at 0x7C00 | C `efi_main()` |
| ABI | System V | Microsoft x64 |

### UEFI Build Process
```
efi_entry.c → gcc → efi_entry.o → ld → myos.so → objcopy → myos.efi → uefi.img
```

## Memory Map

```
0x00000000 ┌─────────────────────┐
           │    Real Mode IVT    │
0x00000500 ├─────────────────────┤
           │     BIOS Data       │
0x00001000 ├─────────────────────┤
           │   FS Metadata       │ ← Loaded by loader
0x00005000 ├─────────────────────┤
           │   VBE Info Block    │
0x00005200 ├─────────────────────┤
           │  VBE Mode Info      │ ← Width, Height, Pitch, FB addr
0x00007C00 ├─────────────────────┤
           │    boot.asm         │ ← BIOS loads here
0x00007E00 ├─────────────────────┤
           │    loader.asm       │
0x00008000 ├─────────────────────┤
           │                     │
           │    Kernel (ELF)     │ ← ~700KB
           │                     │
0x00100000 ├─────────────────────┤ (1MB mark)
           │                     │
           │    Kernel Heap      │ ← Dynamic allocations
           │                     │
           ├─────────────────────┤
           │    Kernel Stack     │ ← 256KB, grows down
0x01000000 ├─────────────────────┤ (16MB, custom loader stack)
           │                     │
           │                     │
0xFD000000 ├─────────────────────┤ (example)
           │   VBE Framebuffer   │ ← Linear, ~8MB for 1920×1080×32
           │                     │
0xFFFFFFFF └─────────────────────┘
```

## Component Details

### VGA Driver (`vga.cpp`)

Reads VBE mode info from fixed address 0x5200:

| Offset | Field           | Size   |
|--------|-----------------|--------|
| +16    | Pitch (bytes/row) | 2 bytes |
| +18    | Width (pixels)  | 2 bytes |
| +20    | Height (pixels) | 2 bytes |
| +25    | BPP             | 1 byte  |
| +40    | Framebuffer addr | 4 bytes |

**Pixel Format**: 32-bit XRGB (0x00RRGGBB)

```cpp
void vga_draw_pixel(buffer, x, y, color) {
    offset = y * pitch + x * 4;
    *(buffer + offset) = color;
}
```

### Filesystem (`lwext4_adapter.cpp`)

```cpp
// Block device interface for lwext4
struct ext4_blockdev {
    .bdif = {
        .open = disk_open,
        .bread = disk_read,     // Read blocks via ATA PIO
        .bwrite = disk_write,   // Write blocks via ATA PIO
        .close = disk_close,
        .ph_bsize = 512,        // Physical block size
        .ph_bcnt = 56K,         // ~28MB partition
    }
}
```

### System Class (`system.cpp`)

Main application logic encapsulated in a class:

```cpp
class System {
    // Framebuffer reference
    uint32_t* framebuffer;
    
    // Font data
    stbtt_fontinfo m_robotoFontInfo;
    stbtt_fontinfo m_jetBrainsFontInfo;
    stbtt_fontinfo m_bbhbogleFontInfo;
    
    // Lifecycle
    void Initialize();   // Mount FS, load fonts, render initial UI
    void Run();          // Main loop: Input → Update → Render
    void Shutdown();     // Cleanup
    
    // Rendering
    void DrawText(x, y, text, color, size, font);
    void DrawRectangle(x, y, w, h, color);
    void LoadImage(filename, x, y, &w, &h);
};
```

## Build System

### Library Structure

```
libkernel_lib.a     ← Kernel utilities, memory management
libdrivers_lib.a    ← VGA, keyboard, SSE, font drivers
libfs_lib.a         ← lwext4 adapter
liblwext4_lib.a     ← EXT4 filesystem library
libprint_lib.a      ← Serial/console printing
libshell_lib.a      ← Command shell (WIP)
libdisk_lib.a       ← ATA disk I/O
```

### Link Order
```
kernel.elf = kernel_entry.o + libraries + linker.ld
```

## Future Architecture Plans

### Planned: Interrupt Handling
```
IDT (Interrupt Descriptor Table)
     │
     ├── IRQ0: Timer → scheduler tick
     ├── IRQ1: Keyboard → input queue
     ├── IRQ14: Primary ATA
     └── Exceptions: Page fault, GPF, etc.
```

### Planned: Process Model
```
┌─────────────────────────────────────┐
│            Kernel Space             │
│  ┌─────────┐ ┌─────────┐ ┌───────┐ │
│  │Scheduler│ │MemMgr  │ │  VFS  │ │
│  └─────────┘ └─────────┘ └───────┘ │
└─────────────────────────────────────┘
              ▲ syscall
┌─────────────┴───────────────────────┐
│           User Space                │
│  ┌──────┐  ┌──────┐  ┌──────┐      │
│  │ Init │  │ Shell│  │ App  │      │
│  └──────┘  └──────┘  └──────┘      │
└─────────────────────────────────────┘
```

### Planned: Window Manager
```
┌─────────────────────────────────────┐
│           Compositor                │
│  ┌─────────┐  ┌─────────┐          │
│  │ Window1 │  │ Window2 │          │
│  │ (buffer)│  │ (buffer)│          │
│  └─────────┘  └─────────┘          │
│         ↓ composite ↓               │
│      ┌─────────────────┐           │
│      │  Framebuffer    │           │
│      └─────────────────┘           │
└─────────────────────────────────────┘
```

## Debugging

### Serial Output (COM1)
- Baud rate: 38400
- Format: 8N1
- All `kprintf()` calls go to serial

### QEMU Debug
```bash
qemu-system-i386 ... -serial stdio
# Or log to file:
qemu-system-i386 ... -serial file:serial.log
```

## References

- [OSDev Wiki](https://wiki.osdev.org)
- [Intel SDM](https://www.intel.com/sdm)
- [VBE 3.0 Specification](vbe3.pdf)
- [lwext4 Library](https://github.com/gkostka/lwext4)
- [stb Libraries](https://github.com/nothings/stb)
