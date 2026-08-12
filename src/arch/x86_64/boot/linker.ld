OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 0x100000; /* Physical Load Address */
    . += 0xFFFFFFFF80000000; /* Virtual Address Offset */

    .text ALIGN(4K) : AT(ADDR(.text) - 0xFFFFFFFF80000000) {
        *(.text.entry)
        *(.multiboot2)
        *(.multiboot)
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - 0xFFFFFFFF80000000) {
        *(.data)
        *(.data.*)
    }

    .init_array ALIGN(4K) : AT(ADDR(.init_array) - 0xFFFFFFFF80000000) {
        __init_array_start = .;
        *(.init_array)
        *(.init_array.*)
        *(.ctors)
        __init_array_end = .;
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xFFFFFFFF80000000) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    .stack ALIGN(4K) : AT(ADDR(.stack) - 0xFFFFFFFF80000000) {
        *(.stack)
    }
}
