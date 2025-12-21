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
    .bss : {
        *(.bss)
    }
}
