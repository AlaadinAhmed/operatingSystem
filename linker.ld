ENTRY(_start)
SECTIONS {
    . = 0x8000;
    .text : {
        *(.text)
        *(.rodata)
    }
    .data : {
        *(.data)
    }
    .init_array : {
        __init_array_start = .;
        *(.init_array)
        *(.init_array.*)
        *(.ctors)
        __init_array_end = .;
    }
    .bss : {
        *(.bss)
    }
}
