# UEFI Boot Guide

This document explains how to boot MyOS on UEFI systems.

## The Problem

GRUB's `multiboot` and `multiboot2` commands **crash on 64-bit UEFI** with a Page Fault exception. This is a known limitation of GRUB's multiboot implementation on modern UEFI systems.

## Solutions

### Option 1: EFI Application (Recommended)

Build the kernel as a native EFI application using `gnu-efi`:

```bash
# Build
./build_efi.sh

# Install to GRUB
sudo ./install_efi_grub.sh
```

GRUB uses `chainloader` instead of `multiboot` to load EFI apps.

### Option 2: Direct UEFI Boot Entry

Add a direct boot entry bypassing GRUB:

```bash
sudo efibootmgr --create --disk /dev/nvme0n1 --part 1 \
    --loader '\EFI\myos\myos.efi' --label "MyOS" --unicode
```

## File Structure

```
/boot/EFI/myos/
└── myos.efi          # EFI application (50KB)

/etc/grub.d/
└── 42_myos_efi       # GRUB menu entry
```

## GRUB Configuration

The GRUB entry uses `chainloader`:

```grub
menuentry "MyOS EFI Kernel" {
    insmod chain
    search --file --set=root /EFI/myos/myos.efi
    chainloader /EFI/myos/myos.efi
}
```

## Why Multiboot Fails on UEFI

1. GRUB EFI runs in 64-bit long mode
2. Multiboot spec requires switching to 32-bit protected mode
3. This mode switch causes Page Fault on some UEFI implementations
4. The crash occurs in GRUB, not in the kernel code

## Dependencies

```bash
# Arch Linux
sudo pacman -S gnu-efi dosfstools mtools

# Ubuntu/Debian
sudo apt install gnu-efi dosfstools mtools
```
