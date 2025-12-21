[org 0x7c00]
[bits 16]

start:
    jmp main

main:
    ; Save Boot Drive
    mov [BOOT_DISK], dl

    ; Debug: Boot started
    mov dx, 0xe9
    mov al, 'B'
    out dx, al

    xor ax, ax
    mov es, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Debug: Print Drive Number
    mov al, dl
    shr al, 4
    call print_hex
    mov al, [BOOT_DISK]
    and al, 0x0F
    call print_hex

    ; Read Loader (1 sector from LBA 1 -> CHS 0,0,2) to 0x7E00
    mov ah, 0x02
    mov al, 1           ; Sector count
    mov ch, 0           ; Cylinder
    mov dh, 0           ; Head
    mov cl, 2           ; Sector (1-based)
    mov dl, [BOOT_DISK]
    mov bx, 0x7E00      ; Buffer
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

times 510 - ($ - $$) db 0
db 0x55, 0xaa
