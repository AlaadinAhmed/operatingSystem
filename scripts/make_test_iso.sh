#!/bin/bash
# Create a bootable ISO for testing (BIOS + UEFI compatible)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/test"
ISO_DIR="/tmp/myos_test_iso"
ISO_FILE="$BUILD_DIR/test.iso"

echo "[*] Building Multiboot2 test kernel..."
mkdir -p "$BUILD_DIR"
nasm -f elf32 "$SCRIPT_DIR/src/test/test_kernel.asm" -o "$BUILD_DIR/test_kernel.o"
ld -m elf_i386 -T "$SCRIPT_DIR/src/test/test_linker.ld" -o "$BUILD_DIR/test-kernel.elf" "$BUILD_DIR/test_kernel.o"

# Verify multiboot2
if grub-file --is-x86-multiboot2 "$BUILD_DIR/test-kernel.elf"; then
    echo "[*] Multiboot2 header: VALID"
else
    echo "[X] Multiboot2 header: INVALID!"
    exit 1
fi

echo "[*] Entry point: $(readelf -h "$BUILD_DIR/test-kernel.elf" | grep Entry | awk '{print $4}')"

# Create ISO
echo "[*] Creating bootable ISO..."
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub"
cp "$BUILD_DIR/test-kernel.elf" "$ISO_DIR/boot/"

cat > "$ISO_DIR/boot/grub/grub.cfg" << 'EOF'
set timeout=3
set default=0

# Graphics mode for framebuffer
if loadfont /boot/grub/fonts/unicode.pf2 ; then
    set gfxmode=800x600x32
    set gfxpayload=keep
    insmod all_video
    insmod gfxterm
    terminal_output gfxterm
fi

menuentry "MyOS Test Kernel (Multiboot2)" {
    echo "Loading multiboot2 kernel..."
    multiboot2 /boot/test-kernel.elf
    boot
}
EOF

grub-mkrescue -o "$ISO_FILE" "$ISO_DIR" 2>/dev/null

echo ""
echo "[*] ISO created: $ISO_FILE"
echo "    Size: $(du -h "$ISO_FILE" | cut -f1)"
echo ""
echo "Test in VirtualBox with EFI enabled (256MB+ RAM):"
echo "  Or: qemu-system-i386 -cdrom $ISO_FILE -m 256"
echo ""
echo "Expected: Green rectangle with red square inside"
