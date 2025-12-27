OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 0x100000; /* Load at 1MB */

    .text : {
        *(.text.entry)
        *(.multiboot2)
        *(.multiboot)
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .init_array : {
        __init_array_start = .;
        *(.init_array)
        *(.init_array.*)
        *(.ctors)
        __init_array_end = .;
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    .stack : {
        *(.stack)
    }
}
