#!/bin/bash
# MyOS Custom Bootloader Installation Script
# Installs the raw disk.img to test custom bootloader on real hardware
#
# Usage: 
#   sudo ./install_bootloader.sh /dev/sdXN  # Install to partition
#   sudo ./install_bootloader.sh --info     # Show available partitions
#
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[*]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[!]${NC} $1"; }
print_error() { echo -e "${RED}[X]${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DISK_IMG="$SCRIPT_DIR/build/disk.img"

show_info() {
    echo ""
    print_status "Available partitions for installation:"
    echo ""
    lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT | grep -E "part|disk"
    echo ""
    print_warning "Choose an UNMOUNTED partition with NO important data!"
    print_warning "The partition will be COMPLETELY OVERWRITTEN."
    echo ""
    echo "Example usage:"
    echo "  sudo $0 /dev/sda3    # Install to sda3"
    echo ""
    exit 0
}

# Check arguments
if [ "$1" == "--info" ] || [ -z "$1" ]; then
    show_info
fi

TARGET_PARTITION="$1"

# Safety checks
if [ "$EUID" -ne 0 ]; then
    print_error "This script requires root privileges."
    echo "Run: sudo $0 $1"
    exit 1
fi

if [ ! -b "$TARGET_PARTITION" ]; then
    print_error "$TARGET_PARTITION is not a valid block device!"
    exit 1
fi

if [ ! -f "$DISK_IMG" ]; then
    print_error "disk.img not found! Run ./run.sh first to build."
    exit 1
fi

# Check if mounted
if mount | grep -q "$TARGET_PARTITION"; then
    print_error "$TARGET_PARTITION is currently mounted!"
    print_warning "Please unmount it first: sudo umount $TARGET_PARTITION"
    exit 1
fi

# Final confirmation
print_warning "============================================"
print_warning "  WARNING: THIS WILL DESTROY ALL DATA ON"
print_warning "  $TARGET_PARTITION"
print_warning "============================================"
echo ""
echo "Disk image: $DISK_IMG ($(du -h "$DISK_IMG" | cut -f1))"
echo "Target:     $TARGET_PARTITION"
echo ""
read -p "Type 'YES' to continue: " confirm
if [ "$confirm" != "YES" ]; then
    print_status "Cancelled."
    exit 0
fi

# Write the image
print_status "Writing disk image to $TARGET_PARTITION..."
dd if="$DISK_IMG" of="$TARGET_PARTITION" bs=4M status=progress conv=fsync

print_status "Syncing..."
sync

print_status "Done!"
echo ""
print_status "============================================"
print_status "  Installation complete!"
print_status "============================================"
echo ""
echo "To boot:"
echo "  1. Add a GRUB chainload entry (see below)"
echo "  2. Or boot directly from BIOS if it's a full disk"
echo ""
echo "GRUB chainload entry (add to /etc/grub.d/40_custom):"
echo ""
echo "menuentry 'MyOS Custom Bootloader' {"
echo "    set root='(hdX,msdosY)'  # Replace with your partition"
echo "    chainloader +1"
echo "}"
echo ""
print_warning "Then run: sudo update-grub"
