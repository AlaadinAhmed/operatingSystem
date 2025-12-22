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

# 1. Create and Format (32MB)
dd if=/dev/zero of=build/disk.img bs=1k count=32768
mkfs.ext4 -O ^has_journal,^64bit,^metadata_csum -b 1024 build/disk.img
# ... after mkfs.ext2 and dd-ing your kernel ...

# Inject your BMP into the ext2 root directory
echo "write logo.bmp logo.bmp" | debugfs -w build/disk.img
# 2. Add Bootloader (First 512 bytes ONLY)
# Note: Use the actual path from your 'find' command
dd if=build/boot.bin of=build/disk.img conv=notrunc bs=512 count=1

# 2.5 Add Loader (Sector 1 - 512 bytes)
# This fits in the second half of the Ext2 Boot Block (bytes 512-1023)
dd if=build/loader.bin of=build/disk.img conv=notrunc bs=512 count=15 seek=2000

# 3. Add Kernel (Skip the Superblock!)
# We seek to sector 2048 (1MB) to avoid overwriting FS metadata
dd if=build/kernel.bin of=build/disk.img conv=notrunc bs=512 seek=2048

# Run in QEMU
echo "Running in QEMU..."
qemu-system-x86_64 -hda build/disk.img -vga std -serial stdio
