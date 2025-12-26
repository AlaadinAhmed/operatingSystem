# Installing MyOS on Real Hardware

This guide explains how to add MyOS to your existing dual-boot GRUB menu.

## Prerequisites

- Linux system with GRUB bootloader
- `sudo` access
- ~30MB free space in `/boot/`

## Quick Installation

```bash
# Build and install MyOS to GRUB
sudo ./install_to_grub.sh
```

After reboot, you'll see "MyOS" in your GRUB menu!

## What Gets Installed

| File | Location | Size |
|------|----------|------|
| Kernel | `/boot/myos-kernel.elf` | ~500KB |
| Root FS | `/boot/myos-rootfs.img` | ~28MB |
| GRUB Entry | `/etc/grub.d/40_myos` | ~1KB |

## Boot Options

1. **MyOS** - Normal boot (1920x1080)
2. **MyOS (Safe Mode)** - Lower resolution (1024x768) for compatibility

## Troubleshooting

### Black Screen After Booting
Try "MyOS (Safe Mode)" from the GRUB menu.

### "No Multiboot Header" Error
Rebuild with GRUB support:
```bash
cmake -DUSE_GRUB=ON .
make clean && make
sudo ./install_to_grub.sh
```

### Display Issues on Real Hardware
Your graphics card may not support 1920x1080. Use Safe Mode or modify `loader.asm` to use a different resolution.

## Uninstalling

```bash
sudo ./install_to_grub.sh --remove
```

This removes all MyOS files and GRUB entries.

## Notes

- MyOS runs in 32-bit protected mode
- Requires VBE-compatible graphics (most modern hardware)
- Uses linear framebuffer for graphics
- Serial output available on COM1 for debugging
