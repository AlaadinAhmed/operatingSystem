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

# 1. Create and Format
dd if=/dev/zero of=build/floppy.img bs=1k count=1440
mkfs.ext2 -b 1024 build/floppy.img
# ... after mkfs.ext2 and dd-ing your kernel ...

# Inject your BMP into the ext2 root directory
echo "write logo.bmp logo.bmp" | debugfs -w build/floppy.img
# 2. Add Bootloader (First 512 bytes ONLY)
# Note: Use the actual path from your 'find' command
dd if=build/boot.bin of=build/floppy.img conv=notrunc bs=512 count=1

# 2.5 Add Loader (Sector 1 - 512 bytes)
# This fits in the second half of the Ext2 Boot Block (bytes 512-1023)
dd if=build/loader.bin of=build/floppy.img conv=notrunc bs=512 count=1 seek=1

# 3. Add Kernel (Skip the Superblock!)
# We seek to sector 2048 (1MB) to avoid overwriting FS metadata
dd if=build/kernel.bin of=build/floppy.img conv=notrunc bs=512 seek=2048

# Prepare ISO directory
mkdir -p build/isofiles


# Copy the OS image
cp build/floppy.img build/isofiles/floppy.img

# Create ISO using xorriso
# echo "Creating ISO..."
# xorriso -as mkisofs -quiet -o build/os.iso \
#     -b floppy.img \
#     -hide floppy.img \
#     build/isofiles
# 
# echo "ISO created at build/os.iso"

# Run in QEMU
echo "Running in QEMU..."
qemu-system-x86_64 -hda build/floppy.img -vga std -serial stdio
