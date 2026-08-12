cmake_minimum_required(VERSION 3.12)
project(MyOS)

# Build artifacts go to CMAKE_BINARY_DIR (the directory where cmake is run from)
# For out-of-source builds: cd build && cmake .. && make
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
enable_language(ASM_NASM)
enable_language(CXX)

# Compiler flags for 64-bit freestanding
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -ffreestanding -mno-red-zone -fno-stack-protector -g -msse -fno-rtti -fno-exceptions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -ffreestanding -mno-red-zone -fno-stack-protector -g")
add_definitions(-DCONFIG_EXT4_USE_XATTR=1 -DCONFIG_XATTR_ENABLE=1 -DCONFIG_META_CSUM_ENABLE=1)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# Bootloader (BIOS Custom)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/boot.bin
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}
    COMMAND nasm -f bin -o ${CMAKE_BINARY_DIR}/boot.bin ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/boot.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/boot.asm

    COMMENT "Assembling bootloader"
)

# Loader (BIOS Custom)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/loader.bin
    COMMAND nasm -f bin -o ${CMAKE_BINARY_DIR}/loader.bin ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/loader.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/loader.asm

    COMMENT "Assembling loader"
)


# LWEXT4 Library
file(GLOB LWEXT4_SOURCES "src/external/lwext4/src/*.c")
list(REMOVE_ITEM LWEXT4_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/external/lwext4/src/ext4_mkfs.c")
add_library(lwext4_lib STATIC ${LWEXT4_SOURCES})
target_compile_definitions(lwext4_lib PUBLIC CONFIG_USE_USER_MALLOC=1 CONFIG_USE_DEFAULT_CFG=1 CONFIG_DEBUG_PRINTF=0 CONFIG_DEBUG_ASSERT=0)
target_include_directories(lwext4_lib PUBLIC src/external/lwext4/include)

# Drivers Library
add_library(drivers_lib STATIC src/drivers/vga.cpp src/drivers/font.cpp src/drivers/sse.cpp src/drivers/keyboard.cpp src/drivers/mouse/mouse.cpp src/drivers/bus/pci.cpp src/drivers/audio/audio.cpp src/drivers/audio/intel_hda.cpp src/drivers/audio/ac97.cpp src/drivers/audio/audio_player.cpp src/drivers/pic.cpp src/drivers/bus/usb.cpp src/drivers/nvme/nvme.cpp src/drivers/ioapic.cpp)
target_include_directories(drivers_lib PUBLIC include/drivers src/external src)

# FS Library
add_library(fs_lib STATIC src/fs/lwext4_adapter.cpp)
target_link_libraries(fs_lib PRIVATE disk_lib)
target_include_directories(fs_lib PUBLIC include/fs src/external/lwext4/include)

# Shell Library
add_library(shell_lib STATIC src/shell/shell.cpp)
target_include_directories(shell_lib PUBLIC include/shell src/external/lwext4/include)

# Print Library
add_library(print_lib STATIC src/print/print.cpp)
target_include_directories(print_lib PUBLIC include/print)
# Gop Library
add_library(gop_lib STATIC src/drivers/gop/gop.cpp)
target_include_directories(gop_lib PUBLIC include/drivers/gop)

# Disk Library
add_library(disk_lib STATIC src/disk/disk.cpp src/disk/nvme_disk.cpp)
target_include_directories(disk_lib PUBLIC include/disk)

# Kernel Library
add_library(kernel_lib STATIC src/kernel/main.cpp src/kernel/utils.cpp src/system/system.cpp src/graphics/framebuffer.cpp src/graphics/font_renderer.cpp src/graphics/compositor.cpp src/arch/x86_64/gdt.cpp src/arch/x86_64/idt.cpp src/arch/x86_64/interrupts.cpp src/arch/x86_64/interrupts.asm src/kernel/ext4_print_adapter.cpp src/arch/x86_64/cpu/cpuid.cpp src/proc/process.cpp src/proc/scheduler.cpp src/kernel/sync/spinlock.cpp src/acpi/acpi.cpp)
target_include_directories(kernel_lib PUBLIC include/system src/external src/external/lwext4/include src include/print include/arch/x86_64/ include/proc/ include/)

#Memory
add_library(memory_lib STATIC src/mem/pmm.cpp src/mem/vmm.cpp src/mem/heap.cpp)
target_include_directories(memory_lib PUBLIC include/mem)

#Timer
add_library(timer_lib STATIC src/timer/timer.cpp src/timer/pit.cpp)
target_include_directories(timer_lib PUBLIC include/timer)


# Kernel Executable (BIOS/UEFI)
add_executable(kernel.elf src/arch/x86_64/kernel/kernel_entry.asm)

add_library(ui_lib STATIC src/ui/window.cpp src/ui/widgets/button.cpp src/ui/widgets/text.cpp)
target_include_directories(ui_lib PUBLIC include src/external)

target_link_libraries(kernel.elf PRIVATE -Wl,--start-group kernel_lib print_lib fs_lib disk_lib drivers_lib lwext4_lib gop_lib ui_lib memory_lib timer_lib gcc -Wl,--end-group)
target_include_directories(kernel.elf PRIVATE src/external)

set_target_properties(kernel.elf PROPERTIES LINK_FLAGS "-m64 -nostdlib -T ${CMAKE_SOURCE_DIR}/src/arch/x86_64/boot/linker.ld -no-pie -fno-pie -z max-page-size=0x1000")



# Extract Binary
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/kernel.bin
    COMMAND objcopy -O binary $<TARGET_FILE:kernel.elf> ${CMAKE_BINARY_DIR}/kernel.bin
    DEPENDS kernel.elf
    COMMENT "Extracting kernel binary"
)

# Concatenate and Zero Pad (Create Floppy Image)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/os-image.bin
    COMMAND cat ${CMAKE_BINARY_DIR}/boot.bin ${CMAKE_BINARY_DIR}/loader.bin ${CMAKE_BINARY_DIR}/kernel.bin > ${CMAKE_BINARY_DIR}/os-image.bin
    COMMAND truncate -s 1440k ${CMAKE_BINARY_DIR}/os-image.bin
    DEPENDS ${CMAKE_BINARY_DIR}/boot.bin ${CMAKE_BINARY_DIR}/loader.bin ${CMAKE_BINARY_DIR}/kernel.bin
    COMMENT "Creating OS image"
)

add_custom_target(MyOS ALL DEPENDS ${CMAKE_BINARY_DIR}/os-image.bin)

# Root Filesystem Image
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/rootfs.img
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/run.sh --build-only
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/run.sh
    COMMENT "Creating Root Filesystem Image"
)
add_custom_target(rootfs DEPENDS ${CMAKE_SOURCE_DIR}/build/rootfs.img)

# Update the 32-bit flags to 64-bit
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -ffreestanding -mno-red-zone -fno-stack-protector -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -ffreestanding -mno-red-zone -fno-stack-protector -g")
# =============================================================================
# UEFI BUILD (using locally compiled gnu-efi)
# =============================================================================
set(GNU_EFI_DIR ${CMAKE_SOURCE_DIR}/src/external/gnu-efi)
set(EFI_BUILD_DIR ${CMAKE_BINARY_DIR}/efi)
set(EFI_ENTRY_SRC ${CMAKE_SOURCE_DIR}/src/arch/x86_64/boot/efi_entry.cpp)

# UEFI compiler flags (must match gnu-efi)
set(EFI_CFLAGS
    -I${GNU_EFI_DIR}/inc
    -I${GNU_EFI_DIR}/inc/x86_64
    -I${GNU_EFI_DIR}/inc/protocol
    -mno-red-zone 
    -fshort-wchar           # <--- ENSURE THIS: Makes L"strings" 16-bit
    -fno-stack-protector 
    -ffreestanding 
    -fPIC 
    -mno-avx 
    -g -O2
    -Wall -Wextra 
    -DGNU_EFI_USE_MS_ABI 
    -maccumulate-outgoing-args
)
# Build gnu-efi from source (if needed)
add_custom_command(
    OUTPUT ${GNU_EFI_DIR}/x86_64/gnuefi/crt0-efi-x86_64.o
    COMMAND make ARCH=x86_64
    WORKING_DIRECTORY ${GNU_EFI_DIR}
    COMMENT "Building gnu-efi from source"
)
add_custom_target(gnuefi_lib DEPENDS ${GNU_EFI_DIR}/x86_64/gnuefi/crt0-efi-x86_64.o)

# Compile EFI entry
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/efi_entry.o
    COMMAND mkdir -p ${EFI_BUILD_DIR}
    COMMAND gcc ${EFI_CFLAGS} -c ${EFI_ENTRY_SRC} -o ${EFI_BUILD_DIR}/efi_entry.o
    DEPENDS ${EFI_ENTRY_SRC} gnuefi_lib
    COMMENT "Compiling UEFI entry"
)

# Embed kernel.bin into an object file
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/kernel_bin.o
    COMMAND mkdir -p ${EFI_BUILD_DIR}
    COMMAND cd ${CMAKE_BINARY_DIR} && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 kernel.bin ${EFI_BUILD_DIR}/kernel_bin.o
    DEPENDS ${CMAKE_BINARY_DIR}/kernel.bin
    COMMENT "Embedding kernel binary into object file"
)

# Link EFI application
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/myos.so
    COMMAND ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1
        -znocombreloc -shared -Bsymbolic
        -L${GNU_EFI_DIR}/x86_64/lib -L${GNU_EFI_DIR}/x86_64/gnuefi
        ${GNU_EFI_DIR}/x86_64/gnuefi/crt0-efi-x86_64.o
        ${EFI_BUILD_DIR}/efi_entry.o
        ${EFI_BUILD_DIR}/kernel_bin.o
        -o ${EFI_BUILD_DIR}/myos.so
        -lefi -lgnuefi
        -T ${GNU_EFI_DIR}/gnuefi/elf_x86_64_efi.lds
    DEPENDS ${EFI_BUILD_DIR}/efi_entry.o ${EFI_BUILD_DIR}/kernel_bin.o
    COMMENT "Linking UEFI application"
)

# Convert to PE format
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/myos.efi
    COMMAND objcopy -I elf64-x86-64 -j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel
        -j .rela -j .rel.* -j .rela.* -j .reloc
        -O efi-app-x86_64
        ${EFI_BUILD_DIR}/myos.so ${EFI_BUILD_DIR}/myos.efi
    DEPENDS ${EFI_BUILD_DIR}/myos.so
    COMMENT "Converting to PE/EFI format"
)

# Create bootable FAT image
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/uefi.img
    COMMAND dd if=/dev/zero of=${EFI_BUILD_DIR}/uefi.img bs=1k count=4096 status=none
    COMMAND /sbin/mkfs.vfat -F 12 ${EFI_BUILD_DIR}/uefi.img
    COMMAND mmd -i ${EFI_BUILD_DIR}/uefi.img ::/EFI
    COMMAND mmd -i ${EFI_BUILD_DIR}/uefi.img ::/EFI/BOOT
    COMMAND mcopy -i ${EFI_BUILD_DIR}/uefi.img ${EFI_BUILD_DIR}/myos.efi ::/EFI/BOOT/BOOTX64.EFI
    COMMAND mcopy -i ${EFI_BUILD_DIR}/uefi.img ${CMAKE_BINARY_DIR}/kernel.bin ::/kernel.bin
    DEPENDS ${EFI_BUILD_DIR}/myos.efi ${CMAKE_BINARY_DIR}/kernel.bin
    COMMENT "Creating UEFI boot image"
)

add_custom_target(kernel_efi DEPENDS ${EFI_BUILD_DIR}/uefi.img)

# =============================================================================
# GRUB ISO (supports both BIOS and UEFI boot via GRUB)
# =============================================================================
set(ISO_DIR ${CMAKE_BINARY_DIR}/iso)
set(GRUB_CFG ${CMAKE_SOURCE_DIR}/resources/grub.cfg)

# Create GRUB bootable ISO
# Create GRUB bootable ISO
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/myos.so
    COMMAND mkdir -p ${ISO_DIR}/boot/grub
    COMMAND mkdir -p ${ISO_DIR}/EFI/myos
    COMMAND cp $<TARGET_FILE:kernel.elf> ${ISO_DIR}/boot/kernel.elf
    COMMAND cp ${GRUB_CFG} ${ISO_DIR}/boot/grub/grub.cfg
    COMMAND cp ${EFI_BUILD_DIR}/myos.efi ${ISO_DIR}/EFI/myos/myos.efi
    COMMAND cp ${CMAKE_BINARY_DIR}/kernel.bin ${ISO_DIR}/kernel.bin
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/myos.so ${ISO_DIR}
    DEPENDS kernel.elf ${GRUB_CFG} ${EFI_BUILD_DIR}/myos.efi ${CMAKE_BINARY_DIR}/kernel.bin
    COMMENT "Creating GRUB bootable ISO (BIOS + UEFI)"
)
add_custom_target(iso DEPENDS ${CMAKE_BINARY_DIR}/myos.so)

# =============================================================================
# BUILD TARGETS (explicit targets for each boot method)
# =============================================================================
# Build BIOS custom bootloader image
add_custom_target(build-custom
    DEPENDS ${CMAKE_BINARY_DIR}/os-image.bin
    COMMENT "Building BIOS custom bootloader image"
)

# Build GRUB ISO
add_custom_target(build-grub
    DEPENDS ${CMAKE_BINARY_DIR}/myos.so
    COMMENT "Building GRUB bootable ISO"
)

# Build UEFI image
add_custom_target(build-uefi
    DEPENDS ${EFI_BUILD_DIR}/uefi.img
    COMMENT "Building UEFI boot image"
)

# Build all boot images
add_custom_target(build-all
    DEPENDS build-custom build-grub build-uefi rootfs
    COMMENT "Building all boot images"
)

# =============================================================================
# RUN TARGETS
# =============================================================================
# Run with BIOS custom bootloader (floppy image)
add_custom_target(run
    COMMAND qemu-system-i386
        -fda ${CMAKE_BINARY_DIR}/os-image.bin
        -drive format=raw,file=${CMAKE_BINARY_DIR}/rootfs.img,index=1,media=disk,if=ide
        -m 512 -vga std -serial stdio
    DEPENDS MyOS rootfs
    COMMENT "Running MyOS (BIOS Custom) in QEMU"
)

# Run with GRUB (ISO, BIOS mode)
add_custom_target(run-grub
    COMMAND qemu-system-i386
        -cdrom ${CMAKE_BINARY_DIR}/myos.so
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -m 512 -vga std -serial stdio
    DEPENDS iso rootfs
    COMMENT "Running MyOS (GRUB BIOS) in QEMU"
)

# Run with GRUB (ISO, UEFI mode)
add_custom_target(run-grub-uefi
    COMMAND qemu-system-x86_64
        -enable-kvm
        -bios /usr/share/edk2/x64/OVMF.4m.fd
        -cdrom ${CMAKE_BINARY_DIR}/myos.so
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -audiodev pa,id=snd0
        -device intel-hda,debug=3 -device hda-duplex,audiodev=snd0
        -device qemu-xhci
        -m 512 -vga std -serial stdio
    DEPENDS iso rootfs
    COMMENT "Running MyOS (GRUB UEFI) in QEMU with HDA debug"
)

# Run with GRUB (ISO, UEFI mode) + AC'97 audio
add_custom_target(run-grub-uefi-ac97
    COMMAND qemu-system-x86_64
        -bios /usr/share/edk2/x64/OVMF.4m.fd
        -cdrom ${CMAKE_BINARY_DIR}/myos.so
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -audiodev pa,id=snd0
        -device AC97,audiodev=snd0
        -m 512 -vga std -serial stdio
    DEPENDS iso rootfs
    COMMENT "Running MyOS (GRUB UEFI + AC97) in QEMU"
)

# Run with pure UEFI (no GRUB, direct EFI app)
add_custom_target(run-uefi
    COMMAND qemu-system-x86_64
        -bios /usr/share/edk2/x64/OVMF.4m.fd
        -drive format=raw,file=${EFI_BUILD_DIR}/uefi.img,index=0,media=disk,if=ide
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -m 512 -vga std -serial stdio
    DEPENDS kernel_efi rootfs
    COMMENT "Running MyOS (Pure UEFI) in QEMU"
)
# Run with pure UEFI + Virtual NVMe PCIe Device
add_custom_target(run-nvme
    COMMAND qemu-system-x86_64
        # -enable-kvm
        -cpu qemu64,+sse,+sse2,+sse3,+ssse3,+sse4.1,+sse4.2 
        -bios /usr/share/edk2/x64/OVMF.4m.fd
        # 1. Your standard boot image (Keep as IDE or change to USB/NVMe depending on where your EFI lives)
        -drive format=raw,file=${EFI_BUILD_DIR}/uefi.img,index=0,media=disk,if=ide
        
        # 2. Define the raw file backend for your root filesystem (Bypasses automatic controller attachment)
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,if=none,id=nvme_disk0
        
        # 3. Create the physical PCIe NVMe Controller and plug the backend drive into it
        -device nvme,serial=NVME_TEST_BOOT_0,id=nvme_ctrl0
        -device nvme-ns,drive=nvme_disk0,bus=nvme_ctrl0,nsid=1
        
        # 4. System allocation resources
        -m 512 
        -vga std 
        -serial stdio
    DEPENDS kernel_efi rootfs
    COMMENT "Running MyOS with NVMe Controller in QEMU"
)
# =============================================================================
# VMWARE TARGETS
# =============================================================================
# Create VMware virtual machine configuration
add_custom_target(vmware-config
    COMMAND qemu-img convert -f raw -O vmdk ${CMAKE_BINARY_DIR}/rootfs.img ${CMAKE_BINARY_DIR}/rootfs.vmdk 2>/dev/null || true
    COMMAND ${CMAKE_COMMAND} -E echo "Creating VMware configuration..."
    COMMAND ${CMAKE_COMMAND} -E echo ".encoding = \"UTF-8\"" > ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "config.version = \"8\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "virtualHW.version = \"18\"" >> ${CMAKE_BINARY_DIR}/myos.vmx  
    COMMAND ${CMAKE_COMMAND} -E echo "memsize = \"1024\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "displayName = \"MyOS\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "guestOS = \"other-64\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "ide0:0.present = \"TRUE\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "ide0:0.deviceType = \"cdrom-image\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "ide0:0.fileName = \"myos.so\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "ide0:1.present = \"TRUE\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "ide0:1.fileName = \"rootfs.vmdk\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "firmware = \"efi\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "sound.present = \"TRUE\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "sound.virtualDev = \"hdaudio\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    COMMAND ${CMAKE_COMMAND} -E echo "sound.autoDetect = \"TRUE\"" >> ${CMAKE_BINARY_DIR}/myos.vmx
    DEPENDS iso rootfs
    COMMENT "Creating VMware configuration files"
)

# Run in VMware (with XKEYBOARD fix for Linux)
add_custom_target(run-vmware
    COMMAND setxkbmap -option ""
    COMMAND vmware -x ${CMAKE_BINARY_DIR}/myos.vmx
    DEPENDS vmware-config
    COMMENT "Running MyOS in VMware Workstation (Intel HD Audio)"
)

# =============================================================================
# INSTALL TARGETS
# Install to /boot/EFI/myos/ for real hardware UEFI boot
add_custom_target(install-uefi
    COMMAND echo "Installing MyOS UEFI to /boot/EFI/myos/..."
    COMMAND sudo mkdir -p /boot/EFI/myos
    COMMAND sudo cp ${EFI_BUILD_DIR}/myos.efi /boot/EFI/myos/myos.efi
    COMMAND echo "Installed: /boot/EFI/myos/myos.efi"
    COMMAND echo ""
    COMMAND echo "To boot, add an entry to your EFI bootloader or use efibootmgr:"
    COMMAND echo "  sudo efibootmgr -c -d /dev/sda -p 1 -L MyOS -l \\\\EFI\\\\myos\\\\myos.efi"
    DEPENDS kernel_efi
    COMMENT "Installing UEFI bootloader to system"
)

# Install to grub EFI directory for real hardware UEFI boot
add_custom_target(install-grub-uefi
    COMMAND echo "Installing MyOS To GRUB..."
    COMMAND sudo mkdir -p /boot/myos
    COMMAND sudo cp ${EFI_BUILD_DIR}/myos.efi /boot/myos/myos.efi
    COMMAND sudo cp ${CMAKE_BINARY_DIR}/kernel.bin /boot/myos/kernel.bin
    COMMAND echo "Installed: /boot/myos/myos.efi"
    COMMAND echo "Installed: /boot/myos/kernel.bin"
    COMMAND echo ""
    COMMAND echo "To boot, add the following to /etc/grub.d/40_custom and run update-grub:"
    COMMAND echo "menuentry 'MyOS UEFI' {"
    COMMAND echo "    insmod part_gpt"
    COMMAND echo "    insmod fat"
    COMMAND echo "    search --no-floppy --fs-uuid --set=root UUID_OF_PARTITION"
    COMMAND echo "    chainloader /boot/myos/myos.efi"
    COMMAND echo "}"
    DEPENDS kernel_efi kernel.elf
    COMMENT "Installing UEFI bootloader to system"
)

# Create bootable USB image
add_custom_target(install-usb
    COMMAND echo "Creating bootable USB image at build/myos-uefi-usb.img..."
    COMMAND dd if=/dev/zero of=${CMAKE_BINARY_DIR}/myos-uefi-usb.img bs=1M count=64 status=progress
    COMMAND /sbin/mkfs.vfat -F 32 ${CMAKE_BINARY_DIR}/myos-uefi-usb.img
    COMMAND mmd -i ${CMAKE_BINARY_DIR}/myos-uefi-usb.img ::/EFI
    COMMAND mmd -i ${CMAKE_BINARY_DIR}/myos-uefi-usb.img ::/EFI/BOOT
    COMMAND mcopy -i ${CMAKE_BINARY_DIR}/myos-uefi-usb.img ${EFI_BUILD_DIR}/myos.efi ::/EFI/BOOT/BOOTX64.EFI
    COMMAND echo ""
    COMMAND echo "USB image created: build/myos-uefi-usb.img"
    COMMAND echo "Write to USB: sudo dd if=build/myos-uefi-usb.img of=/dev/sdX bs=4M status=progress"
    DEPENDS kernel_efi
    COMMENT "Creating bootable USB image"
)

# Clean UEFI build
add_custom_target(clean-uefi
    COMMAND rm -rf ${EFI_BUILD_DIR}
    COMMENT "Cleaning UEFI build directory"
)
