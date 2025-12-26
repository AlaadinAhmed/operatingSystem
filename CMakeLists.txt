cmake_minimum_required(VERSION 3.12)
project(MyOS)

# Redirect build artifacts to build/ folder
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
enable_language(ASM_NASM)
enable_language(CXX)

# Compiler flags for 32-bit freestanding
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -ffreestanding -fno-pie -fno-stack-protector -g -msse")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -ffreestanding -fno-pie -fno-stack-protector -g")
add_definitions(-DCONFIG_EXT4_USE_XATTR=1 -DCONFIG_XATTR_ENABLE=1 -DCONFIG_META_CSUM_ENABLE=1)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# Bootloader (BIOS Custom)
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/boot.bin
    COMMAND mkdir -p ${CMAKE_SOURCE_DIR}/build
    COMMAND nasm -f bin -o ${CMAKE_SOURCE_DIR}/build/boot.bin ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/boot.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/boot.asm

    COMMENT "Assembling bootloader"
)

# Loader (BIOS Custom)
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/loader.bin
    COMMAND nasm -f bin -o ${CMAKE_SOURCE_DIR}/build/loader.bin ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/loader.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/loader.asm

    COMMENT "Assembling loader"
)


# LWEXT4 Library
file(GLOB LWEXT4_SOURCES "src/external/lwext4/src/*.c")
list(REMOVE_ITEM LWEXT4_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/external/lwext4/src/ext4_mkfs.c")
add_library(lwext4_lib STATIC ${LWEXT4_SOURCES})
target_compile_definitions(lwext4_lib PUBLIC CONFIG_USE_USER_MALLOC=1 CONFIG_USE_DEFAULT_CFG=1)
target_include_directories(lwext4_lib PUBLIC src/external/lwext4/include)

# Drivers Library
add_library(drivers_lib STATIC src/drivers/vga.cpp src/drivers/font.cpp src/drivers/sse.cpp src/drivers/keyboard.cpp src/drivers/mouse/mouse.cpp)
target_include_directories(drivers_lib PUBLIC include/drivers src/external)

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

# Disk Library
add_library(disk_lib STATIC src/disk/disk.cpp)
target_include_directories(disk_lib PUBLIC include/disk)

# Kernel Library
add_library(kernel_lib STATIC src/kernel/main.cpp src/kernel/utils.cpp src/system/system.cpp)
target_include_directories(kernel_lib PUBLIC include/system src/external src/external/lwext4/include)


# ext4_print_adapter Library (for printf redirect)
add_library(ext4_print_adapter_lib STATIC src/kernel/ext4_print_adapter.cpp)
target_include_directories(ext4_print_adapter_lib PUBLIC include/print)

# Kernel Executable (BIOS)
add_executable(kernel.elf src/arch/i386/kernel/kernel_entry.asm)

target_link_libraries(kernel.elf PRIVATE kernel_lib print_lib fs_lib disk_lib drivers_lib lwext4_lib ext4_print_adapter_lib gcc)
target_include_directories(kernel.elf PRIVATE src/external)

set_target_properties(kernel.elf PROPERTIES LINK_FLAGS "-m32 -nostdlib -T ${CMAKE_SOURCE_DIR}/src/arch/i386/boot/linker.ld -no-pie -fno-pie")



# Extract Binary
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/kernel.bin
    COMMAND objcopy -O binary $<TARGET_FILE:kernel.elf> ${CMAKE_SOURCE_DIR}/build/kernel.bin
    DEPENDS kernel.elf
    COMMENT "Extracting kernel binary"
)

# Concatenate and Zero Pad (Create Floppy Image)
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/os-image.bin
    COMMAND cat ${CMAKE_SOURCE_DIR}/build/boot.bin ${CMAKE_SOURCE_DIR}/build/loader.bin ${CMAKE_SOURCE_DIR}/build/kernel.bin > ${CMAKE_SOURCE_DIR}/build/os-image.bin
    COMMAND truncate -s 1440k ${CMAKE_SOURCE_DIR}/build/os-image.bin
    DEPENDS ${CMAKE_SOURCE_DIR}/build/boot.bin ${CMAKE_SOURCE_DIR}/build/loader.bin ${CMAKE_SOURCE_DIR}/build/kernel.bin
    COMMENT "Creating OS image"
)

add_custom_target(MyOS ALL DEPENDS ${CMAKE_SOURCE_DIR}/build/os-image.bin)

# Root Filesystem Image
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/rootfs.img
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/run.sh --build-only
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/run.sh
    COMMENT "Creating Root Filesystem Image"
)
add_custom_target(rootfs DEPENDS ${CMAKE_SOURCE_DIR}/build/rootfs.img)

# =============================================================================
# UEFI BUILD (using locally compiled gnu-efi)
# =============================================================================
set(GNU_EFI_DIR ${CMAKE_SOURCE_DIR}/src/external/gnu-efi)
set(EFI_BUILD_DIR ${CMAKE_SOURCE_DIR}/build/efi)
set(EFI_ENTRY_SRC ${CMAKE_SOURCE_DIR}/src/arch/x86_64/boot/efi_entry.c)

# UEFI compiler flags (must match gnu-efi)
set(EFI_CFLAGS
    -I${GNU_EFI_DIR}/inc
    -I${GNU_EFI_DIR}/inc/x86_64
    -I${GNU_EFI_DIR}/inc/protocol
    -mno-red-zone -mno-avx -fPIE -g -O2
    -Wall -Wextra -Wno-pointer-sign -funsigned-char -fshort-wchar
    -fno-strict-aliasing -ffreestanding -fno-stack-protector -fno-stack-check
    -fno-merge-all-constants -DCONFIG_x86_64 -std=c11
    -DGNU_EFI_USE_MS_ABI -maccumulate-outgoing-args
)
string(REPLACE ";" " " EFI_CFLAGS_STR "${EFI_CFLAGS}")

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
    COMMAND gcc ${EFI_CFLAGS_STR} -c ${EFI_ENTRY_SRC} -o ${EFI_BUILD_DIR}/efi_entry.o
    DEPENDS ${EFI_ENTRY_SRC} gnuefi_lib
    COMMENT "Compiling UEFI entry"
)

# Link EFI application
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/myos.so
    COMMAND ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1
        -znocombreloc -shared -Bsymbolic
        -L${GNU_EFI_DIR}/x86_64/lib -L${GNU_EFI_DIR}/x86_64/gnuefi
        ${GNU_EFI_DIR}/x86_64/gnuefi/crt0-efi-x86_64.o
        ${EFI_BUILD_DIR}/efi_entry.o
        -o ${EFI_BUILD_DIR}/myos.so
        -lefi -lgnuefi
        -T ${GNU_EFI_DIR}/gnuefi/elf_x86_64_efi.lds
    DEPENDS ${EFI_BUILD_DIR}/efi_entry.o
    COMMENT "Linking UEFI application"
)

# Convert to PE format
add_custom_command(
    OUTPUT ${EFI_BUILD_DIR}/myos.efi
    COMMAND objcopy -j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel
        -j .rela -j .rel.* -j .rela.* -j .reloc
        --target=efi-app-x86_64
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
    DEPENDS ${EFI_BUILD_DIR}/myos.efi
    COMMENT "Creating UEFI boot image"
)

add_custom_target(kernel_efi DEPENDS ${EFI_BUILD_DIR}/uefi.img)

# =============================================================================
# GRUB ISO (supports both BIOS and UEFI boot via GRUB)
# =============================================================================
set(ISO_DIR ${CMAKE_SOURCE_DIR}/build/iso)
set(GRUB_CFG ${CMAKE_SOURCE_DIR}/resources/grub.cfg)

# Create GRUB bootable ISO
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/build/myos.iso
    COMMAND mkdir -p ${ISO_DIR}/boot/grub
    COMMAND cp $<TARGET_FILE:kernel.elf> ${ISO_DIR}/boot/kernel.elf
    COMMAND cp ${GRUB_CFG} ${ISO_DIR}/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_SOURCE_DIR}/build/myos.iso ${ISO_DIR}
    DEPENDS kernel.elf ${GRUB_CFG}
    COMMENT "Creating GRUB bootable ISO (BIOS + UEFI)"
)
add_custom_target(iso DEPENDS ${CMAKE_SOURCE_DIR}/build/myos.iso)

# =============================================================================
# BUILD TARGETS (explicit targets for each boot method)
# =============================================================================
# Build BIOS custom bootloader image
add_custom_target(build-custom
    DEPENDS ${CMAKE_SOURCE_DIR}/build/os-image.bin
    COMMENT "Building BIOS custom bootloader image"
)

# Build GRUB ISO
add_custom_target(build-grub
    DEPENDS ${CMAKE_SOURCE_DIR}/build/myos.iso
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
        -fda ${CMAKE_SOURCE_DIR}/build/os-image.bin
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -m 512 -vga std -serial stdio
    DEPENDS MyOS rootfs
    COMMENT "Running MyOS (BIOS Custom) in QEMU"
)

# Run with GRUB (ISO, BIOS mode)
add_custom_target(run-grub
    COMMAND qemu-system-i386
        -cdrom ${CMAKE_SOURCE_DIR}/build/myos.iso
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -m 512 -vga std -serial stdio
    DEPENDS iso rootfs
    COMMENT "Running MyOS (GRUB BIOS) in QEMU"
)

# Run with GRUB (ISO, UEFI mode via OVMF)
add_custom_target(run-grub-uefi
    COMMAND qemu-system-x86_64
        -bios /usr/share/edk2/x64/OVMF.4m.fd
        -cdrom ${CMAKE_SOURCE_DIR}/build/myos.iso
        -drive format=raw,file=${CMAKE_SOURCE_DIR}/build/rootfs.img,index=1,media=disk,if=ide
        -m 512 -vga std -serial stdio
    DEPENDS iso rootfs
    COMMENT "Running MyOS (GRUB UEFI) in QEMU"
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

# =============================================================================
# INSTALL TARGETS
# =============================================================================
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

# Create bootable USB image
add_custom_target(install-usb
    COMMAND echo "Creating bootable USB image at build/myos-uefi-usb.img..."
    COMMAND dd if=/dev/zero of=${CMAKE_SOURCE_DIR}/build/myos-uefi-usb.img bs=1M count=64 status=progress
    COMMAND /sbin/mkfs.vfat -F 32 ${CMAKE_SOURCE_DIR}/build/myos-uefi-usb.img
    COMMAND mmd -i ${CMAKE_SOURCE_DIR}/build/myos-uefi-usb.img ::/EFI
    COMMAND mmd -i ${CMAKE_SOURCE_DIR}/build/myos-uefi-usb.img ::/EFI/BOOT
    COMMAND mcopy -i ${CMAKE_SOURCE_DIR}/build/myos-uefi-usb.img ${EFI_BUILD_DIR}/myos.efi ::/EFI/BOOT/BOOTX64.EFI
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
