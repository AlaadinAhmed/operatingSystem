ENTRY(_start)
SECTIONS {
    . = 0x100000;
    __load_start = .;
    .text : {
        *(.text.entry)
        *(.multiboot2)
        *(.multiboot)
        *(.text)
        *(.rodata)
    }


    .data : {
        *(.data)
    }
    __load_end = .;
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
        __bss_end = .;
    }
    .stack : {
        *(.stack)
    }
}

