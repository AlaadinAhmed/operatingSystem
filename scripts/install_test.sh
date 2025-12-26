#!/bin/bash
# ============================================================================
# UEFI Test Kernel Installation Script
# ============================================================================
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[*]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[!]${NC} $1"; }
print_error() { echo -e "${RED}[X]${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_DIR="$SCRIPT_DIR/src/test"
BUILD_DIR="$SCRIPT_DIR/build/test"
KERNEL_NAME="test-kernel.elf"

mkdir -p "$BUILD_DIR"

# Build
print_status "Assembling test kernel..."
nasm -f elf32 "$TEST_DIR/test_kernel.asm" -o "$BUILD_DIR/test_kernel.o"

print_status "Linking..."
ld -m elf_i386 -T "$TEST_DIR/test_linker.ld" -o "$BUILD_DIR/$KERNEL_NAME" "$BUILD_DIR/test_kernel.o"

# Verify multiboot2
print_status "Verifying multiboot2 header..."
if grub-file --is-x86-multiboot2 "$BUILD_DIR/$KERNEL_NAME"; then
    print_status "Multiboot2 header: VALID"
else
    print_error "Multiboot2 header: INVALID!"
    exit 1
fi

print_status "Kernel info:"
echo "  File: $BUILD_DIR/$KERNEL_NAME"
echo "  Size: $(du -h "$BUILD_DIR/$KERNEL_NAME" | cut -f1)"
readelf -h "$BUILD_DIR/$KERNEL_NAME" | grep "Entry point"
echo ""

# Check if running as root for install
if [ "$EUID" -ne 0 ]; then
    print_warning "To install, run: sudo $0"
    exit 0
fi

# Install
print_status "Copying to /boot..."
cp "$BUILD_DIR/$KERNEL_NAME" "/boot/$KERNEL_NAME"
chmod 644 "/boot/$KERNEL_NAME"

# Verify it's there
if [ -f "/boot/$KERNEL_NAME" ]; then
    print_status "Verified: /boot/$KERNEL_NAME exists ($(ls -lh /boot/$KERNEL_NAME | awk '{print $5}'))"
else
    print_error "Failed to copy kernel to /boot!"
    exit 1
fi

# Create GRUB entry
GRUB_ENTRY="/etc/grub.d/41_myos_test"
print_status "Creating GRUB entry..."

cat > "$GRUB_ENTRY" << 'GRUBEOF'
#!/bin/sh
exec tail -n +3 $0

menuentry "MyOS Test (UEFI)" --class os {
    insmod part_gpt
    insmod part_msdos
    insmod ext2
    insmod all_video
    
    search --no-floppy --file --set=root /test-kernel.elf
    
    # Set graphics mode for framebuffer
    set gfxpayload=800x600x32,1024x768x32,auto
    terminal_output gfxterm
    
    echo "Loading UEFI test kernel..."
    multiboot2 /test-kernel.elf
    boot
}
GRUBEOF

chmod +x "$GRUB_ENTRY"

# Update GRUB
print_status "Updating GRUB..."
if command -v update-grub &> /dev/null; then
    update-grub 2>&1 | grep -i "myos\|test\|found\|done" || true
elif command -v grub-mkconfig &> /dev/null; then
    grub-mkconfig -o /boot/grub/grub.cfg 2>&1 | grep -i "myos\|test\|found\|done" || true
fi

echo ""
print_status "============================================"
print_status "  INSTALLED SUCCESSFULLY"
print_status "============================================"
echo ""
echo "Reboot and select 'MyOS Test (Beep Test)' from GRUB."
echo ""
echo "If you hear a BEEP -> kernel is working!"
echo "If NO beep -> kernel isn't executing"
