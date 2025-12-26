#!/bin/bash
set -e

# Clean build artifacts
echo "Cleaning build artifacts..."
rm -rf build

# Configure CMake
echo "Configuring CMake..."
cmake -S . -B build

# Build the project
echo "Building project..."
cmake --build build

# 1. Create Disk Image (32MB) - for bootloader, loader, kernel
dd if=/dev/zero of=build/disk.img bs=1k count=32768

# 2. Create Root Filesystem Image (28MB) - dedicated for ext4
dd if=/dev/zero of=build/rootfs.img bs=1k count=28672
/sbin/mke2fs -t ext4 -b 4096 -O ^64bit,^huge_file,^metadata_csum,^dir_nlink,^extra_isize,^orphan_file build/rootfs.img

# Inject your BMP into the rootfs image
echo -e "write logo.bmp logo.bmp\nwrite BBHBogle-Regular.ttf BBHBogle-Regular.ttf\nwrite Roboto-Regular.ttf Roboto-Regular.ttf\nwrite JetBrainsMono-Bold.ttf JetBrainsMono-Bold.ttf\nwrite" | /sbin/debugfs -w build/rootfs.img

# 3. Add Bootloader (First 512 bytes ONLY)
dd if=build/boot.bin of=build/disk.img conv=notrunc bs=512 count=1

# 4. Add Loader (Sector 1 - 512 bytes)
dd if=build/loader.bin of=build/disk.img conv=notrunc bs=512 count=15 seek=2000

# 5. Add Kernel
dd if=build/kernel.bin of=build/disk.img conv=notrunc bs=512 seek=2048

# Run in QEMU with two disk images
echo "Running in QEMU..."
qemu-system-x86_64 -hda build/disk.img -drive file=build/rootfs.img,if=ide,index=1,media=disk,format=raw -vga std -serial stdio
