[org 0x7e00]
[bits 16]

KERNEL_LOCATION equ 0x100000
VBE_INFO_ADDR equ 0x5000
VBE_MODE_INFO_ADDR equ 0x5200

loader_entry:
    ; Save Boot Drive passed from boot.asm
    mov [BOOT_DISK], dl

    ; Debug: Loader started
    mov dx, 0x3f8
    mov al, 'L'
    out dx, al

    ; Check Extensions
    mov ah, 0x41
    mov bx, 0x55aa
    mov dl, [BOOT_DISK]
    int 0x13
    jc no_extensions
    
    ; Debug: Extensions supported
    mov dx, 0x3f8
    mov al, 'X'
    out dx, al

    ; 1. Read Kernel (300 sectors from LBA 2048) to 0x8000
    ; We need to read in chunks to avoid 64KB segment overflow
    ; 300 sectors = 150KB.
    ; We'll read in 10 chunks of 30 sectors (15KB each).
    
    mov ax, 0x1000          ; Start at segment 0x1000 (linear 0x10000)
    mov es, ax
    mov bx, 0               ; Offset 0
    mov ecx, 64             ; Start LBA

    mov dx, 2               ; Loop count (2 * 30 = 60 sectors = 30KB)


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
    mov dx, 0x3f8
    mov al, 'K'
    out dx, al

    ; Skip extra disk reads for now


    ; --- VBE SETUP ---
    ; 1. Get VBE Controller Info
    mov ax, 0x4F00
    mov di, VBE_INFO_ADDR
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 2. Find a suitable mode with fallback chain
    ; Try modes in order: 1920x1080, 1280x1024, 1024x768
    ; VideoModePtr is at offset 14 in VbeInfoBlock
    
    mov byte [current_mode_idx], 0  ; Start with first preferred mode

.try_next_preferred:
    ; Get current preferred resolution
    movzx bx, byte [current_mode_idx]
    shl bx, 2                       ; Each entry is 4 bytes (2 words)
    mov ax, [preferred_modes + bx]
    mov [target_width], ax
    mov ax, [preferred_modes + bx + 2]
    mov [target_height], ax
    
    ; Check if we've exhausted all modes (width == 0)
    cmp word [target_width], 0
    je vbe_error                    ; No modes worked

    ; Reset mode list pointer
    mov si, [VBE_INFO_ADDR + 14]
    mov ax, [VBE_INFO_ADDR + 16]
    mov fs, ax

.find_mode_loop:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je .next_preferred              ; End of list, try next preferred mode
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
    cmp ax, [target_width]
    jne .find_mode_loop

    mov ax, [VBE_MODE_INFO_ADDR + 20]
    cmp ax, [target_height]
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
    jne .next_preferred             ; Failed to set, try next

    ; Debug: VBE Set
    mov dx, 0x3f8
    mov al, 'V'
    out dx, al
    jmp .vbe_done

.next_preferred:
    inc byte [current_mode_idx]
    jmp .try_next_preferred

.vbe_done:

    ; ========================================
    ; A20 Enablement - ROBUST VERSION
    ; ========================================
    call enable_a20

    ; Debug: A20 enabled
    mov dx, 0x3f8
    mov al, 'A'
    out dx, al

    ; ========================================
    ; Calculate GDT Physical Address
    ; ========================================
    xor eax, eax
    mov ax, ds
    shl eax, 4
    add eax, GDT_Start
    mov [GDT_Descriptor + 2], eax

    ; Enter Protected Mode
    cli
    lgdt [GDT_Descriptor]

    ; Debug: GDT loaded
    mov dx, 0x3f8
    mov al, 'G'
    out dx, al

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; CRITICAL: Far jump to flush prefetch queue and load CS
    jmp CODE_SEG:Start_Protected_Mode

vbe_error:
    ; Fallback to text mode if VBE fails
    mov ax, 0x03
    int 0x10
    ; Debug: VBE Error
    mov dx, 0x3f8
    mov al, 'v'
    out dx, al
    
    ; Still need A20 and protected mode
    call enable_a20
    
    xor eax, eax
    mov ax, ds
    shl eax, 4
    add eax, GDT_Start
    mov [GDT_Descriptor + 2], eax

    cli
    lgdt [GDT_Descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:Start_Protected_Mode

no_extensions:
    mov dx, 0x3f8
    mov al, 'N'
    out dx, al
    jmp disk_error

; ============================================================
; ROBUST A20 ENABLEMENT - Tries multiple methods
; ============================================================
enable_a20:
    call check_a20
    jnz .a20_done           ; Already enabled

    ; Method 1: BIOS INT 0x15, AX=0x2401
    mov ax, 0x2401
    int 0x15
    call check_a20
    jnz .a20_done

    ; Method 2: Keyboard Controller (most compatible)
    call .a20_keyboard
    call check_a20
    jnz .a20_done

    ; Method 3: Fast A20 (Port 0x92)
    in al, 0x92
    test al, 2
    jnz .a20_fast_skip
    or al, 2
    and al, 0xFE            ; Don't reset CPU!
    out 0x92, al
.a20_fast_skip:
    call check_a20
    jnz .a20_done

    ; A20 failed - critical error
    mov dx, 0x3f8
    mov al, '!'             ; A20 failure marker
    out dx, al
    jmp $

.a20_done:
    ret

.a20_keyboard:
    cli
    call .a20_wait_input
    mov al, 0xAD            ; Disable keyboard
    out 0x64, al
    
    call .a20_wait_input
    mov al, 0xD0            ; Read output port
    out 0x64, al
    
    call .a20_wait_output
    in al, 0x60
    push ax
    
    call .a20_wait_input
    mov al, 0xD1            ; Write output port
    out 0x64, al
    
    call .a20_wait_input
    pop ax
    or al, 2                ; Set A20 bit
    out 0x60, al
    
    call .a20_wait_input
    mov al, 0xAE            ; Enable keyboard
    out 0x64, al
    
    call .a20_wait_input
    ret

.a20_wait_input:
    in al, 0x64
    test al, 2
    jnz .a20_wait_input
    ret

.a20_wait_output:
    in al, 0x64
    test al, 1
    jz .a20_wait_output
    ret

; Returns ZF=0 if A20 is enabled, ZF=1 if disabled
check_a20:
    pushf
    push ds
    push es
    push di
    push si
    
    xor ax, ax
    mov es, ax
    mov di, 0x0500
    
    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x0510          ; 0xFFFF:0x0510 = 0x100500 (wraps to 0x0500 if A20 off)
    
    mov al, [es:di]         ; Save original values
    push ax
    mov al, [ds:si]
    push ax
    
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
    cmp byte [es:di], 0xFF  ; If equal, A20 is OFF (wrapped)
    
    pop ax
    mov [ds:si], al         ; Restore
    pop ax
    mov [es:di], al
    
    mov ax, 0               ; ZF=1 means A20 is OFF
    je .a20_off
    mov ax, 1               ; ZF=0 means A20 is ON
.a20_off:
    pop si
    pop di
    pop es
    pop ds
    popf
    test ax, ax
    ret

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
    mov dx, 0x3f8
    mov al, '2'
    out dx, al
    mov ah, 0x0e
    mov al, '2'
    int 0x10
    jmp $

align 4

BOOT_DISK: db 0

; VBE Mode fallback table (width, height pairs)
; Tries modes in order until one works
preferred_modes:
    dw 1920, 1080   ; First choice: Full HD
    dw 1680, 1050   ; Fallback 1: WSXGA+
    dw 1280, 1024   ; Fallback 2: SXGA
    dw 1024, 768    ; Fallback 3: XGA (most compatible)
    dw 0, 0         ; End marker

current_mode_idx: db 0
target_width: dw 0
target_height: dw 0

[bits 32]
Start_Protected_Mode:
    ; Set up data segments
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x1000000      ; 16MB stack base
    mov esp, ebp

    ; Debug: Protected Mode entered
    mov dx, 0x3f8
    mov al, 'P'
    out dx, al

    ; ========================================
    ; Move Kernel to 1MB and Jump
    ; ========================================
    mov esi, 0x10000        ; Source: where we loaded the kernel
    mov edi, 0x100000       ; Destination: 1MB
    mov ecx, 8000           ; Move 32KB (enough for now)
    cld
    rep movsd               ; Move dwords

    ; Debug: Move done
    mov dx, 0x3f8
    mov al, 'M'
    out dx, al

    ; Pass magic value to kernel in EAX
    ; 0x1337B007 as a custom magic for our custom bootloader
    mov eax, 0x1337B007
    mov ebx, 0              ; No extra info for now
    
    ; Jump to kernel
    jmp KERNEL_LOCATION


    ; Should never reach here
    cli
.halt_loop:
    hlt
    jmp .halt_loop


; GDT
align 8
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
    dd 0                ; Will be filled at runtime with physical address

CODE_SEG equ Code_descriptor - GDT_Start
DATA_SEG equ Data_descriptor - GDT_Start
