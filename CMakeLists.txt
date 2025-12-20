cmake_minimum_required(VERSION 3.10)
project(MyOS ASM_NASM)

# Define the input and output paths
set(ASM_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/main.asm)
set(BIN_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main.bin)

# 1. Define the command to run NASM manually
add_custom_command(
    OUTPUT ${BIN_OUTPUT}
    COMMAND nasm -f bin -o ${BIN_OUTPUT} ${ASM_SOURCE}
    DEPENDS ${ASM_SOURCE}
    COMMENT "Assembling ${ASM_SOURCE} to raw binary..."
    VERBATIM
)

# 2. Create a "Target" so you can run 'make' or 'cmake --build'
add_custom_target(MyKernel ALL DEPENDS ${BIN_OUTPUT})
