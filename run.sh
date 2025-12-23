#!/bin/bash
set -e

# Default to custom bootloader
USE_GRUB=0

# Parse arguments
for arg in "$@"
do
    case $arg in
        --grub)
        USE_GRUB=1
        shift
        ;;
    esac
done

echo "Building project..."
if [ $USE_GRUB -eq 1 ]; then
    cmake -DUSE_GRUB=ON .
else
    cmake -DUSE_GRUB=OFF .
fi
make

echo "Creating Root Filesystem Image..."
# Create 28MB ext4 image
dd if=/dev/zero of=rootfs.img bs=1k count=28672 status=none
/sbin/mke2fs -q -t ext4 -b 4096 -O ^64bit,^huge_file,^metadata_csum,^dir_nlink,^extra_isize,^orphan_file rootfs.img

# Inject files
echo "Injecting resources..."
# Check if files exist before writing
rm -f debugfs_cmds
for file in resources/logo.bmp resources/BBHBogle-Regular.ttf resources/Roboto-Regular.ttf resources/JetBrainsMono-Bold.ttf; do
    if [ -f "$file" ]; then
        # Extract basename for the destination filename in the image
        dest_file=$(basename "$file")
        echo "write $file $dest_file" >> debugfs_cmds
    else
        echo "Warning: $file not found, skipping."
    fi
done

if [ -f debugfs_cmds ]; then
    /sbin/debugfs -w -f debugfs_cmds rootfs.img > /dev/null
    rm debugfs_cmds
fi

if [ $USE_GRUB -eq 1 ]; then
    echo "Starting QEMU (GRUB mode)..."
    if [ -f "myos.iso" ]; then
        qemu-system-i386 -hda myos.iso -drive file=rootfs.img,if=ide,index=1,media=disk,format=raw -vga std -serial stdio -no-reboot -no-shutdown
    else
        echo "ISO not found (tools missing?), booting kernel directly..."
        qemu-system-i386 -kernel kernel.elf -drive file=rootfs.img,if=ide,index=1,media=disk,format=raw -vga std -serial stdio -no-reboot -no-shutdown
    fi
else
    echo "Creating Boot Disk Image..."
    # Create 32MB disk image
    dd if=/dev/zero of=disk.img bs=1k count=32768 status=none
    
    # Add Bootloader
    dd if=boot.bin of=disk.img conv=notrunc bs=512 count=1 status=none
    
    # Add Loader (Sector 1, 15 sectors)
    dd if=loader.bin of=disk.img conv=notrunc bs=512 count=15 seek=2000 status=none
    
    # Add Kernel (Sector 2048)
    dd if=kernel.bin of=disk.img conv=notrunc bs=512 seek=2048 status=none
    
    echo "Starting QEMU (Custom Bootloader)..."
    qemu-system-i386 -hda disk.img -drive file=rootfs.img,if=ide,index=1,media=disk,format=raw -vga std -serial stdio
fi
