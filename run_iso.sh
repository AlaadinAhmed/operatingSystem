#!/bin/bash
set -e

# Ensure build directory exists
if [ ! -d "build" ]; then
    echo "Configuring CMake..."
    cmake -S . -B build
fi

# Build the project
echo "Building project..."
cmake --build build

# Prepare ISO directory
mkdir -p build/isofiles

# Create floppy image (1.44MB)
echo "Creating floppy image..."
# Create empty 1.44MB file
dd if=/dev/zero of=build/isofiles/floppy.img bs=1024 count=1440 2>/dev/null
# Write the kernel binary to the beginning of the floppy image
dd if=build/main.bin of=build/isofiles/floppy.img conv=notrunc 2>/dev/null

# Create ISO using xorriso
echo "Creating ISO..."
xorriso -as mkisofs -quiet -o build/os.iso \
    -b floppy.img \
    -hide floppy.img \
    build/isofiles

echo "ISO created at build/os.iso"

# Run in QEMU
echo "Running in QEMU..."
qemu-system-x86_64 -cdrom build/os.iso
