#!/bin/bash
set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

# Parse arguments
BUILD_ONLY=0
USE_GRUB=0
for arg in "$@"

do
    case $arg in
        --grub)
        USE_GRUB=1
        shift
        ;;
        --build-only)
        BUILD_ONLY=1
        shift
        ;;
    esac
done

# Create build directory
mkdir -p "$BUILD_DIR"

if [ "$BUILD_ONLY" -eq 0 ]; then
    echo "Building project..."
    cd "$ROOT_DIR"
    if [ "$USE_GRUB" -eq 1 ]; then
        cmake -DUSE_GRUB=ON .
    else
        cmake -DUSE_GRUB=OFF .
    fi
    make
fi



echo "Creating Root Filesystem Image..."
# Create 28MB ext4 image in build folder
dd if=/dev/zero of="$BUILD_DIR/rootfs.img" bs=1k count=28672 status=none
/sbin/mke2fs -q -t ext4 -b 4096 -O ^64bit,^huge_file,^metadata_csum,^dir_nlink,^extra_isize,^orphan_file "$BUILD_DIR/rootfs.img"

# Create directory structure in the filesystem
echo "Creating filesystem directories..."
/sbin/debugfs -w "$BUILD_DIR/rootfs.img" << 'EOF' > /dev/null 2>&1
mkdir fonts
mkdir images
mkdir docs
mkdir apps
mkdir config
EOF

# Inject files into filesystem
echo "Injecting resources..."
rm -f "$ROOT_DIR/debugfs_cmds"

# Add fonts to /fonts directory
for file in "$ROOT_DIR/resources/BBHBogle-Regular.ttf" "$ROOT_DIR/resources/Roboto-Regular.ttf" "$ROOT_DIR/resources/JetBrainsMono-Bold.ttf"; do
    if [ -f "$file" ]; then
        dest_file=$(basename "$file")
        echo "write $file fonts/$dest_file" >> "$ROOT_DIR/debugfs_cmds"
    fi
done

# Add images to /images directory  
for file in "$ROOT_DIR/resources/logo.bmp" "$ROOT_DIR/resources/blackbuck.bmp"; do
    if [ -f "$file" ]; then
        dest_file=$(basename "$file")
        echo "write $file images/$dest_file" >> "$ROOT_DIR/debugfs_cmds"
    fi
done

# Also copy fonts to root for backwards compatibility (temporary)
for file in "$ROOT_DIR/resources/BBHBogle-Regular.ttf" "$ROOT_DIR/resources/Roboto-Regular.ttf" "$ROOT_DIR/resources/JetBrainsMono-Bold.ttf"; do
    if [ -f "$file" ]; then
        dest_file=$(basename "$file")
        echo "write $file $dest_file" >> "$ROOT_DIR/debugfs_cmds"
    fi
done

# Copy logo to root for backwards compatibility
if [ -f "$ROOT_DIR/resources/logo.bmp" ]; then
    echo "write $ROOT_DIR/resources/logo.bmp logo.bmp" >> "$ROOT_DIR/debugfs_cmds"
fi

if [ -f "$ROOT_DIR/debugfs_cmds" ]; then
    /sbin/debugfs -w -f "$ROOT_DIR/debugfs_cmds" "$BUILD_DIR/rootfs.img" > /dev/null 2>&1
    rm "$ROOT_DIR/debugfs_cmds"
fi

if [ $BUILD_ONLY -eq 1 ]; then
    exit 0
fi


if [ "$USE_GRUB" -eq 1 ]; then
    echo "Creating GRUB ISO..."
    ISO_DIR="$BUILD_DIR/isodir"
    rm -rf "$ISO_DIR"
    mkdir -p "$ISO_DIR/boot/grub"
    cp "$BUILD_DIR/kernel.elf" "$ISO_DIR/boot/"
    cp "$ROOT_DIR/resources/grub.cfg" "$ISO_DIR/boot/grub/"
    grub-mkrescue -o "$BUILD_DIR/myos.iso" "$ISO_DIR" 2>/dev/null || echo "Warning: grub-mkrescue failed"
    
    echo "Starting QEMU (GRUB mode)..."
    if [ -f "$BUILD_DIR/myos.iso" ]; then
        qemu-system-i386 -hda "$BUILD_DIR/myos.iso" -drive file="$BUILD_DIR/rootfs.img",if=ide,index=1,media=disk,format=raw -vga std -serial stdio -no-reboot -no-shutdown
    else
        echo "ISO not found, booting kernel directly..."
        qemu-system-i386 -kernel "$BUILD_DIR/kernel.elf" -drive file="$BUILD_DIR/rootfs.img",if=ide,index=1,media=disk,format=raw -vga std -serial stdio -no-reboot -no-shutdown
    fi

else
    echo "Creating Boot Disk Image..."
    # Create 32MB disk image
    dd if=/dev/zero of="$BUILD_DIR/disk.img" bs=1k count=32768 status=none
    
    # Add Bootloader
    dd if="$BUILD_DIR/boot.bin" of="$BUILD_DIR/disk.img" conv=notrunc bs=512 count=1 status=none
    
    # Add Loader (Sector 1, 15 sectors)
    dd if="$BUILD_DIR/loader.bin" of="$BUILD_DIR/disk.img" conv=notrunc bs=512 count=15 seek=1 status=none

    
    # Add Kernel (Sector 64)
    dd if="$BUILD_DIR/kernel.bin" of="$BUILD_DIR/disk.img" conv=notrunc bs=512 seek=64 status=none

    
    echo "Starting QEMU (Custom Bootloader)..."
    qemu-system-i386 -hda "$BUILD_DIR/disk.img" -drive file="$BUILD_DIR/rootfs.img",if=ide,index=1,media=disk,format=raw -vga std -serial stdio
fi

