/* 64-bit linker script for UEFI kernel */
ENTRY(efi_main)
SECTIONS {
    . = 0x100000;
    __load_start = .;
    
    .text : {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
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
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
