[org 0x7c00]
[bits 16]
%define NULL_T 0x00 
%define ENDL 0x0D, 0x0A, 0x00
start:
    jmp main

main:
    ; Save Boot Drive
    mov [BOOT_DISK], dl

    ; Debug: Boot started
    mov si, START_BOOT
    call log_message

    xor ax, ax
    mov es, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Debug: Print Drive Number
    mov si, DRIVE_MSG
    call log_message
    mov al, dl
    shr al, 4
    call print_hex
    mov al, [BOOT_DISK]
    and al, 0x0F
    call print_hex

    ; Read Loader using LBA Packet
    ; Check LBA Extensions
    mov ah, 0x41
    mov bx, 0x55aa
    mov dl, [BOOT_DISK]
    int 0x13
    jc disk_error

    ; Read 15 sectors from LBA 2000 to 0x7E00
    mov ah, 0x42
    mov dl, [BOOT_DISK]
    mov si, dap
    int 0x13
    jc disk_error

    ; Debug: Loader Loaded
    mov dx, 0xe9
    mov al, 'L'
    out dx, al

    ; Pass Boot Drive to Loader
    mov dl, [BOOT_DISK]
    
    ; Jump to Loader
    jmp 0x7E00

disk_error:
    mov dx, 0xe9
    mov al, 'E'
    out dx, al
    mov ah, 0x0e
    mov al, 'E'
    int 0x10
    jmp $

print_hex:
    cmp al, 9
    jg .letter
    add al, '0'
    jmp .print
.letter:
    add al, 'A' - 10
.print:
    mov dx, 0xe9
    out dx, al
    ret

BOOT_DISK: db 0

log_message:
    lodsb
    cmp al, 0
    je .done
    mov dx, 0xe9
    out dx, al
    jmp log_message
.done:
    ret
dap:
    db 0x10     ; Size
    db 0        ; Reserved
    dw 15       ; Count
    dw 0x7E00   ; Offset
    dw 0        ; Segment
    dd 2000     ; LBA Low
    dd 0        ; LBA High

START_BOOT:
    db "Boot Started", ENDL
DRIVE_MSG:
    db "Boot Drive: 0x", ENDL
times 510 - ($ - $$) db 0
db 0x55, 0xaa
