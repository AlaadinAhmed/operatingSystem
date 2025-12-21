cmake_minimum_required(VERSION 3.12)
project(MyOS)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
enable_language(ASM_NASM)
enable_language(CXX)

# Compiler flags for 32-bit freestanding
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -ffreestanding -fno-pie -fno-stack-protector -g")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/lib/lwext4/include )
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

# Kernel
add_executable(kernel.elf src/boot/kernel_entry.asm src/kernel/kernel.cpp src/disk/disk.cpp src/print/print.cpp src/drivers/vga.cpp)
target_include_directories(kernel.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
