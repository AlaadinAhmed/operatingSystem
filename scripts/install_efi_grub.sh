#!/bin/bash
# ============================================================================
# Install EFI Kernel to GRUB
# Creates a GRUB menu entry that chainloads the EFI application
# ============================================================================
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[*]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[!]${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EFI_SRC="$SCRIPT_DIR/build/test/BOOTX64.EFI"
EFI_DEST="/boot/EFI/myos/myos.efi"

# Check if EFI file exists
if [ ! -f "$EFI_SRC" ]; then
    echo "Error: EFI application not found. Run ./build_efi.sh first."
    exit 1
fi

# Install requires root
if [ "$EUID" -ne 0 ]; then
    print_warning "Run with sudo: sudo $0"
    exit 1
fi

# Copy EFI application to ESP
print_status "Installing EFI application..."
mkdir -p /boot/EFI/myos
cp "$EFI_SRC" "$EFI_DEST"
chmod 644 "$EFI_DEST"
print_status "Installed: $EFI_DEST ($(du -h "$EFI_DEST" | cut -f1))"

# Create GRUB entry that chainloads the EFI app
GRUB_ENTRY="/etc/grub.d/42_myos_efi"
print_status "Creating GRUB menu entry..."

cat > "$GRUB_ENTRY" << 'EOF'
#!/bin/sh
exec tail -n +3 $0

menuentry "MyOS EFI Kernel" --class os {
    insmod part_gpt
    insmod fat
    insmod chain
    
    # Find the EFI System Partition
    search --no-floppy --fs-uuid --set=root --hint-efi=hd0,gpt1 $(grub-probe --target=fs_uuid /boot/EFI)
    
    # Chainload our EFI application
    chainloader /EFI/myos/myos.efi
}

menuentry "MyOS EFI Kernel (fallback search)" --class os {
    insmod part_gpt
    insmod fat
    insmod chain
    
    # Alternative: search by file
    search --no-floppy --file --set=root /EFI/myos/myos.efi
    
    chainloader /EFI/myos/myos.efi
}
EOF

chmod +x "$GRUB_ENTRY"

# Update GRUB configuration
print_status "Updating GRUB configuration..."
if command -v update-grub &> /dev/null; then
    update-grub 2>&1 | grep -i "myos\|done" || true
elif command -v grub-mkconfig &> /dev/null; then
    grub-mkconfig -o /boot/grub/grub.cfg 2>&1 | grep -i "myos\|done" || true
fi

echo ""
print_status "============================================"
print_status "  GRUB INSTALLATION COMPLETE!"
print_status "============================================"
echo ""
echo "On next boot, GRUB will show 'MyOS EFI Kernel' option."
echo "Select it to boot your EFI kernel!"
echo ""
echo "Files installed:"
echo "  - $EFI_DEST"
echo "  - $GRUB_ENTRY"
