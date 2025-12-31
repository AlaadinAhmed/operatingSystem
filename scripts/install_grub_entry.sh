#!/bin/bash

GRUB_FILE="/etc/grub.d/40_custom"
ENTRY_NAME="MyOS Audio"

if grep -q "$ENTRY_NAME" "$GRUB_FILE"; then
    echo "GRUB entry '$ENTRY_NAME' already exists in $GRUB_FILE"
else
    echo "Adding '$ENTRY_NAME' to $GRUB_FILE..."
    cat <<EOF | sudo tee -a "$GRUB_FILE" > /dev/null

menuentry '$ENTRY_NAME' {
    insmod part_gpt
    insmod ext2
    search --no-floppy --file --set=root /boot/myos/kernel.bin
    multiboot /boot/myos/kernel.bin
    module /boot/myos/rootfs.img
    boot
}
EOF
    echo "Entry added."
fi

echo "Updating GRUB configuration..."
if command -v update-grub > /dev/null; then
    sudo update-grub
elif command -v grub-mkconfig > /dev/null; then
    sudo grub-mkconfig -o /boot/grub/grub.cfg
else
    echo "Error: Could not find update-grub or grub-mkconfig"
    exit 1
fi

echo "Done! You can now reboot and select '$ENTRY_NAME'."
