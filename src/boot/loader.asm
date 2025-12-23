[org 0x7e00]
[bits 16]

KERNEL_LOCATION equ 0x8000
VBE_INFO_ADDR equ 0x5000
VBE_MODE_INFO_ADDR equ 0x5200

loader_entry:
    ; Save Boot Drive passed from boot.asm
    mov [BOOT_DISK], dl

    ; Debug: Loader started
    mov dx, 0xe9
    mov al, 'L'
    out dx, al

    ; Check Extensions
    mov ah, 0x41
    mov bx, 0x55aa
    mov dl, [BOOT_DISK]
    int 0x13
    jc no_extensions
    
    ; Debug: Extensions supported
    mov dx, 0xe9
    mov al, 'X'
    out dx, al

    ; 1. Read Kernel (300 sectors from LBA 2048) to 0x8000
    ; We need to read in chunks to avoid 64KB segment overflow
    ; 300 sectors = 150KB.
    ; We'll read in 10 chunks of 30 sectors (15KB each).
    
    mov ax, 0
    mov es, ax
    mov bx, KERNEL_LOCATION ; 0x8000
    mov ecx, 2048           ; Start LBA
    mov dx, 40              ; Loop count (40 * 30 = 1200 sectors = 600KB)

.kernel_load_loop:
    push dx
    mov ax, 30              ; Read 30 sectors
    call read_disk_lba
    
    add ecx, 30             ; Next LBA
    
    ; Advance Segment by 30 sectors * 512 bytes = 15360 bytes = 0x3C00
    ; 0x3C00 / 16 = 0x3C0 paragraphs
    mov dx, es
    add dx, 0x3C0
    mov es, dx
    
    pop dx
    dec dx
    jnz .kernel_load_loop

    ; Restore ES to 0 for the rest of the loader
    mov ax, 0
    mov es, ax

    ; Debug: Kernel loaded
    mov dx, 0xe9
    mov al, 'K'
    out dx, al

    ; 2. Read Superblock + GDT (4 sectors from LBA 8194) to 0x1000
    mov ax, 4
    mov bx, 0x1000
    mov ecx, 8194
    call read_disk_lba

    ; Debug: FS Metadata loaded
    mov dx, 0xe9
    mov al, 'F'
    out dx, al

    ; --- VBE SETUP ---
    ; 1. Get VBE Controller Info
    mov ax, 0x4F00
    mov di, VBE_INFO_ADDR
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 2. Find a suitable mode (e.g., 1024x768x32)
    ; VideoModePtr is at offset 14 in VbeInfoBlock
    mov si, [VBE_INFO_ADDR + 14]
    mov ax, [VBE_INFO_ADDR + 16]
    mov fs, ax

.find_mode_loop:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je vbe_error            ; End of list, mode not found
    add si, 2

    ; Get Mode Info
    push es
    mov ax, 0
    mov es, ax
    mov ax, 0x4F01
    mov di, VBE_MODE_INFO_ADDR
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .find_mode_loop

    ; Check properties
    ; ModeAttributes at offset 0. Bit 7 must be set (Linear Framebuffer).
    mov ax, [VBE_MODE_INFO_ADDR]
    and ax, 0x0080
    jz .find_mode_loop

    ; Width at offset 18, Height at 20, BPP at 25
    mov ax, [VBE_MODE_INFO_ADDR + 18]
    cmp ax, 1920
    jne .find_mode_loop

    mov ax, [VBE_MODE_INFO_ADDR + 20]
    cmp ax, 1080
    jne .find_mode_loop

    mov al, [VBE_MODE_INFO_ADDR + 25]
    cmp al, 32
    jne .find_mode_loop

    ; Found mode! Set it.
    ; CX holds the mode number.
    ; Bit 14 (0x4000) enables linear framebuffer
    or cx, 0x4000
    mov ax, 0x4F02
    mov bx, cx
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; Debug: VBE Set
    mov dx, 0xe9
    mov al, 'V'
    out dx, al

    ; Enable A20 Line
    mov ax, 0x2401
    int 0x15

    ; Enter Protected Mode
    cli
    lgdt [GDT_Descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:Start_Protected_Mode

vbe_error:
    ; Fallback to text mode if VBE fails
    mov ax, 0x03
    int 0x10
    ; Debug: VBE Error
    mov dx, 0xe9
    mov al, 'v'
    out dx, al
    
    cli
    lgdt [GDT_Descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:Start_Protected_Mode

no_extensions:
    mov dx, 0xe9
    mov al, 'N'
    out dx, al
    jmp disk_error

read_disk_lba:
    ; Input:
    ; AX = Sectors to read
    ; BX = Buffer Offset (ES:BX)
    ; ECX = Start LBA
    
    pusha
    
    ; Construct DAP on stack
    push dword 0    ; LBA High
    push ecx        ; LBA Low
    push es         ; Segment
    push bx         ; Offset
    push ax         ; Count
    push word 0x0010 ; Size=0x10, Reserved=0x00
    
    mov ah, 0x42
    mov dl, [BOOT_DISK]
    mov si, sp      ; DS:SI -> DAP on stack
    int 0x13
    jc disk_error
    
    add sp, 16      ; Clean up stack
    
    popa
    ret

disk_error:
    mov dx, 0xe9
    mov al, '2'
    out dx, al
    mov ah, 0x0e
    mov al, '2'
    int 0x10
    jmp $

align 4

BOOT_DISK: db 0

[bits 32]
Start_Protected_Mode:
    ; Debug: Protected Mode
    mov dx, 0xe9
    mov al, 'P'
    out dx, al

    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x1000000 ; 16MB
    mov esp, ebp

    ; Enable SSE
    mov eax, cr0
    and ax, 0xFFFB      ; Clear EM
    or ax, 0x2          ; Set MP
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9       ; Set OSFXSR and OSXMMEXCPT
    mov cr4, eax

    jmp KERNEL_LOCATION

; GDT
GDT_Start:
    dq 0x0
Code_descriptor:
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
Data_descriptor:
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
GDT_End:
GDT_Descriptor:
    dw GDT_End - GDT_Start - 1
    dd GDT_Start

CODE_SEG equ Code_descriptor - GDT_Start
DATA_SEG equ Data_descriptor - GDT_Start
