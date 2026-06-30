/* 64-bit linker script for UEFI kernel */
ENTRY(efi_main)
SECTIONS {
    . = 0xC0000000;
    __load_start = .;
    
    .text : AT(ADDR(.text) - 0xC0000000) {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : AT(ADDR(.data) - 0xC0000000) {
        *(.data)
        *(.data.*)
    }
    
    __load_end = .;
    
    .init_array : AT(ADDR(.init_array) - 0xC0000000){
        __init_array_start = .;
        *(.init_array)
        *(.init_array.*)
        *(.ctors)
        __init_array_end = .;
    }
    
    .bss : AT(ADDR(.bss) - 0xC0000000){
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
