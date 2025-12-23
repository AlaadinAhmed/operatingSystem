cmake_minimum_required(VERSION 3.12)
project(MyOS)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
enable_language(ASM_NASM)
enable_language(CXX)

# Compiler flags for 32-bit freestanding
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -ffreestanding -fno-pie -fno-stack-protector -g -msse")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -ffreestanding -fno-pie -fno-stack-protector -g")
add_definitions(-DCONFIG_EXT4_USE_XATTR=1 -DCONFIG_XATTR_ENABLE=1 -DCONFIG_META_CSUM_ENABLE=1)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# Bootloader
add_custom_command(
    OUTPUT boot.bin
    COMMAND nasm -f bin -o boot.bin ${CMAKE_SOURCE_DIR}/src/boot/boot.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/boot/boot.asm
    COMMENT "Assembling bootloader"
)

# Loader
add_custom_command(
    OUTPUT loader.bin
    COMMAND nasm -f bin -o loader.bin ${CMAKE_SOURCE_DIR}/src/boot/loader.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/boot/loader.asm
    COMMENT "Assembling loader"
)

# LWEXT4 Library
file(GLOB LWEXT4_SOURCES "src/external/lwext4/src/*.c")
list(REMOVE_ITEM LWEXT4_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/external/lwext4/src/ext4_mkfs.c")
add_library(lwext4_lib STATIC ${LWEXT4_SOURCES})
target_compile_definitions(lwext4_lib PUBLIC CONFIG_USE_USER_MALLOC=1 CONFIG_USE_DEFAULT_CFG=1)
target_include_directories(lwext4_lib PUBLIC src/external/lwext4/include)

# Drivers Library
add_library(drivers_lib STATIC src/drivers/vga.cpp src/drivers/font.cpp src/drivers/sse.cpp src/drivers/keyboard.cpp)
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
add_library(kernel_lib STATIC src/kernel/kernel.cpp src/kernel/utils.cpp src/system/system.cpp)
target_include_directories(kernel_lib PUBLIC include/system src/external src/external/lwext4/include)

# ext4_print_adapter Library (for printf redirect)
add_library(ext4_print_adapter_lib STATIC src/kernel/ext4_print_adapter.cpp)
target_include_directories(ext4_print_adapter_lib PUBLIC include/print)

# Kernel Executable
add_executable(kernel.elf src/boot/kernel_entry.asm)
target_link_libraries(kernel.elf PRIVATE kernel_lib print_lib fs_lib disk_lib drivers_lib lwext4_lib ext4_print_adapter_lib gcc)
target_include_directories(kernel.elf PRIVATE src/external)

set_target_properties(kernel.elf PROPERTIES LINK_FLAGS "-m32 -nostdlib -T ${CMAKE_SOURCE_DIR}/linker.ld -no-pie -fno-pie")

# Extract Binary
add_custom_command(
    OUTPUT kernel.bin
    COMMAND objcopy -O binary $<TARGET_FILE:kernel.elf> kernel.bin
    DEPENDS kernel.elf
    COMMENT "Extracting kernel binary"
)

# Concatenate and Zero Pad (Create Floppy Image)
add_custom_command(
    OUTPUT os-image.bin
    COMMAND cat boot.bin loader.bin kernel.bin > os-image.bin
    COMMAND truncate -s 1440k os-image.bin
    DEPENDS boot.bin loader.bin kernel.bin
    COMMENT "Creating OS image"
)

add_custom_target(MyOS ALL DEPENDS os-image.bin)

option(USE_GRUB "Build GRUB ISO" OFF)

if(USE_GRUB)
    find_program(GRUB_MKRESCUE grub-mkrescue)
    find_program(XORRISO xorriso)
    find_program(MFORMAT mformat)

    if(GRUB_MKRESCUE AND XORRISO AND MFORMAT)
        add_custom_command(
            OUTPUT myos.iso
            COMMAND mkdir -p iso/boot/grub
            COMMAND cp kernel.elf iso/boot/kernel.elf
            COMMAND cp ${CMAKE_SOURCE_DIR}/resources/grub.cfg iso/boot/grub/grub.cfg
            COMMAND ${GRUB_MKRESCUE} -o myos.iso iso
            DEPENDS kernel.elf
            COMMENT "Creating GRUB ISO"
        )
        add_custom_target(iso ALL DEPENDS myos.iso)
    else()
        message(WARNING "Tools for GRUB ISO generation not found (grub-mkrescue, xorriso, mtools). 'make iso' will not be available.")
    endif()
endif()
