#!/bin/bash
# MyOS GRUB Installation Script
# Adds MyOS to your existing GRUB bootloader
#
# Usage: 
#   ./install_to_grub.sh          # Build and install
#   ./install_to_grub.sh --remove  # Remove from GRUB
#
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

KERNEL_NAME="myos-kernel.elf"
ROOTFS_NAME="myos-rootfs.img"
GRUB_ENTRY="/etc/grub.d/40_myos"

print_status() { echo -e "${GREEN}[$(date +%T)]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[$(date +%T)]${NC} $1"; }
print_error() { echo -e "${RED}[$(date +%T)]${NC} $1"; }

# Check if running as root for installation steps
check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_error "This script requires root privileges for installation."
        echo "Please run: sudo $0 $@"
        exit 1
    fi
}

# Remove MyOS from GRUB
remove_myos() {
    check_root
    print_status "Removing MyOS from GRUB..."
    
    [ -f "/boot/$KERNEL_NAME" ] && rm -f "/boot/$KERNEL_NAME" && print_status "Removed /boot/$KERNEL_NAME"
    [ -f "/boot/$ROOTFS_NAME" ] && rm -f "/boot/$ROOTFS_NAME" && print_status "Removed /boot/$ROOTFS_NAME"
    [ -f "$GRUB_ENTRY" ] && rm -f "$GRUB_ENTRY" && print_status "Removed GRUB entry"
    
    print_status "Updating GRUB..."
    update_grub_config
    
    print_status "MyOS has been removed from GRUB."
    exit 0
}

# Update GRUB config (works on both Debian and Arch/Fedora)
update_grub_config() {
    if command -v update-grub &> /dev/null; then
        update-grub
    elif command -v grub-mkconfig &> /dev/null; then
        grub-mkconfig -o /boot/grub/grub.cfg
    elif command -v grub2-mkconfig &> /dev/null; then
        grub2-mkconfig -o /boot/grub2/grub.cfg
    else
        print_error "Could not find grub-mkconfig or update-grub!"
        print_warning "Please manually run: grub-mkconfig -o /boot/grub/grub.cfg"
        return 1
    fi
}

# Parse arguments
SKIP_GRUB=0
FORCE_GRUB=0
if [ "$1" == "--remove" ]; then
    remove_myos
elif [ "$1" == "--skip-grub" ]; then
    SKIP_GRUB=1
elif [ "$1" == "--force-grub" ]; then
    FORCE_GRUB=1
fi

# Step 1: Build the OS (always rebuild to catch all changes)
print_status "Building MyOS..."
cd "$(dirname "$0")"

# Build with GRUB support
if [ ! -d "build" ] || [ ! -f "build/Makefile" ]; then
    cmake -B build -DUSE_GRUB=ON .
fi
cmake --build build --target kernel.elf -j$(nproc)

if [ ! -f "build/kernel.elf" ]; then
    print_error "Build failed: kernel.elf not found"
    exit 1
fi
print_status "Build complete!"

# Step 2: Create rootfs if needed
create_rootfs() {
    print_status "Creating root filesystem (8MB)..."
    mkdir -p build
    
    # Create 8MB ext4 image (smaller is faster for FAT32 /boot)
    dd if=/dev/zero of=build/rootfs.img bs=1k count=8192 status=none
    /sbin/mke2fs -q -t ext4 -b 4096 -O ^64bit,^huge_file,^metadata_csum,^dir_nlink,^extra_isize,^orphan_file build/rootfs.img
    
    # Create directory structure
    /sbin/debugfs -w build/rootfs.img << 'EOF' > /dev/null 2>&1
mkdir fonts
mkdir images
mkdir docs
mkdir apps
mkdir config
EOF

    # Inject resources
    rm -f debugfs_cmds
    for file in resources/BBHBogle-Regular.ttf resources/Roboto-Regular.ttf resources/JetBrainsMono-Bold.ttf; do
        if [ -f "$file" ]; then
            dest_file=$(basename "$file")
            echo "write $file fonts/$dest_file" >> debugfs_cmds
            echo "write $file $dest_file" >> debugfs_cmds
        fi
    done
    if [ -f "resources/logo.bmp" ]; then
        echo "write resources/logo.bmp images/logo.bmp" >> debugfs_cmds
        echo "write resources/logo.bmp logo.bmp" >> debugfs_cmds
    fi
    
    if [ -f debugfs_cmds ]; then
        /sbin/debugfs -w -f debugfs_cmds build/rootfs.img > /dev/null 2>&1
        rm debugfs_cmds
    fi
}

if [ ! -f "build/rootfs.img" ]; then
    create_rootfs
fi

# Verify files exist
if [ ! -f "build/kernel.elf" ] || [ ! -f "build/rootfs.img" ]; then
    print_error "Required files not found in build/"
    exit 1
fi

print_status "Files ready:"
echo "  - build/kernel.elf ($(du -h build/kernel.elf | cut -f1))"
echo "  - build/rootfs.img ($(du -h build/rootfs.img | cut -f1))"

# Step 3: Install to /boot (requires root)
check_root

print_status "Installing to /boot (FAT32 detected)..."
print_status "  -> Copying kernel..."
rm -f "/boot/$KERNEL_NAME"
cp build/kernel.elf "/boot/$KERNEL_NAME"

print_status "  -> Removing old rootfs..."
rm -f "/boot/$ROOTFS_NAME"

print_status "  -> Copying rootfs (using dd for progress)..."
dd if=build/rootfs.img of="/boot/$ROOTFS_NAME" bs=4M status=progress oflag=sync

print_status "  -> Setting permissions..."
chmod 644 "/boot/$KERNEL_NAME" "/boot/$ROOTFS_NAME"

print_status "Installed successfully to /boot."

# Step 4: Create GRUB entry
# Check if we need to update GRUB entry
NEEDS_GRUB_UPDATE=1
if [ -f "$GRUB_ENTRY" ] && [ -f "/boot/$KERNEL_NAME" ] && [ $FORCE_GRUB -eq 0 ]; then
    NEEDS_GRUB_UPDATE=0
    print_status "Smart Update: GRUB entry already exists. Skipping slow menu refresh."
    print_status "  (Your kernel and filesystem were updated, but the GRUB menu was not changed.)"
else
    if [ $FORCE_GRUB -eq 1 ]; then
        print_status "Force Update: Refreshing GRUB menu as requested..."
    else
        print_status "New Installation: Refreshing GRUB menu for the first time..."
    fi
fi

if [ $NEEDS_GRUB_UPDATE -eq 1 ]; then
    print_status "Creating GRUB menu entry..."

    cat > "$GRUB_ENTRY" << 'GRUBEOF'
#!/bin/sh
exec tail -n +3 $0

menuentry "MyOS" --class os {
    insmod part_msdos
    insmod part_gpt
    insmod ext2
    insmod all_video
    
    # Set video mode before loading kernel
    set gfxpayload=1920x1080x32,1280x1024x32,1024x768x32,auto
    
    # Use graphical terminal to avoid "no console" warning
    terminal_output gfxterm
    
    # Find the boot partition
    search --no-floppy --file --set=root /myos-kernel.elf
    
    echo "Loading MyOS kernel..."
    multiboot /myos-kernel.elf
    
    echo "Loading root filesystem..."
    module /myos-rootfs.img
    
    boot
}

menuentry "MyOS (Safe Mode - 1024x768)" --class os {
    insmod part_msdos
    insmod part_gpt
    insmod ext2
    insmod all_video
    
    search --no-floppy --file --set=root /myos-kernel.elf
    
    set gfxpayload=1024x768x32
    terminal_output gfxterm
    
    echo "Loading MyOS kernel (Safe Mode)..."
    multiboot /myos-kernel.elf
    module /myos-rootfs.img
    boot
}
GRUBEOF

    chmod +x "$GRUB_ENTRY"

    # Step 5: Update GRUB
    if [ $SKIP_GRUB -eq 0 ]; then
        print_warning "Updating GRUB configuration (this can take up to 60 seconds)..."
        print_warning "Please do not interrupt this process."
        update_grub_config
    else
        print_warning "Skipping GRUB update as requested."
        print_status "You will need to manually run 'sudo update-grub' or equivalent."
    fi
fi

echo ""
print_status "============================================"
print_status "  MyOS has been installed successfully!    "
print_status "============================================"
echo ""
echo "On next reboot, you'll see 'MyOS' in your GRUB menu."
echo ""
echo "To remove MyOS from GRUB later, run:"
echo "  sudo ./install_to_grub.sh --remove"
echo ""
print_warning "Note: If your display doesn't work, try 'MyOS (Safe Mode - 1024x768)'"
