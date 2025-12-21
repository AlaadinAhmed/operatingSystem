[org 0x7c00]
bits 16
%define ENDL 0x0D, 0x0A, 0
KERNEL_LOCATION equ 0x1000
BOOT_DISK db 0
start:
    ;Read the Kernel
    mov [BOOT_DISK], dl 
    xor ax, ax
    mov es, ax 
    mov ds, ax 
    mov bp, 0x8000
    mov sp, bp
    mov bx, KERNEL_LOCATION
    mov dh, 20
    mov ax, 0x02
    mov al, dh
    mov ch, x00
    mov dh, x00 
    mov cl, 0x02
    mov dl, [BOOT_DISK]
    int 0x13
    ; Clear the Screen
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    mov ah, 0x0e
    mov al, 'A'
    int 0x10
    cli
    lgdt [GDT_Descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:Start_Protected_Mode
[bits 32]
Start_Protected_Mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000
    mov esp, ebp
    jmp KERNEL_LOCATION

    mov al, 'A'
    mov ah, 0x0f
    mov [0xb8000], ax
    mov al, 'B'
    mov ah, 0x0f
    mov [0xb8002], ax
halt:
    jmp halt

times 510 - ($ - $$) db 0
db 0x55, 0xaa
GDT_Start:
    ; Null Descriptor
    Null_descriptor:
    dq 0x0000000000000000
    ; Code Segment Descriptor
    Code_descriptor:
    dw 0xFFFF          ; Limit Low
    dw 0x0000          ; Base Low
    db 0x00            ; Base Middle
    db 10011010b            ; Access
    db 11001111b ; Granularity
    db 0x00            ; Base Copyright (c) 2025 Author. All Rights Reserved.
    ; Data Segment Descriptor
    Data_descriptor:
    dw 0xFFFF          ; Limit Low
    dw 0x0000          ; Base Low
    db 0x00            ; Base Middle
    db 10010010b            ; Access
    db 11001111b ; Granularity
    db 0x00            ; Base Copyright (c) 2025 Author. All Rights Reserved.
    GDT_End:
    GDT_Descriptor:
       dw GDT_End - GDT_Start - 1
       dd GDT_Start

    CODE_SEG equ Code_descriptor - GDT_Start
    DATA_SEG equ Data_descriptor - GDT_Start
