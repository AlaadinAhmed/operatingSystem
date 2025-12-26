; ============================================================================
; UEFI Multiboot2 Test - With Serial Output for Confirmation
; ============================================================================
[bits 32]
[global _start]

MULTIBOOT2_MAGIC        equ 0xE85250D6
MULTIBOOT2_ARCH         equ 0
SERIAL_PORT             equ 0x3F8

section .multiboot2
align 8
mb2_header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd mb2_header_end - mb2_header_start
    dd -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + (mb2_header_end - mb2_header_start))

    ; Optional framebuffer tag
    align 8
    dw 5                    ; type = framebuffer
    dw 1                    ; flags = OPTIONAL
    dd 20                   ; size
    dd 800
    dd 600
    dd 32

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8
mb2_header_end:

section .bss
align 16
stack_bottom:
    resb 8192
stack_top:

section .text
_start:
    mov esp, stack_top
    
    ; Initialize serial port for debug output
    mov dx, SERIAL_PORT + 1
    xor al, al
    out dx, al              ; Disable interrupts
    mov dx, SERIAL_PORT + 3
    mov al, 0x80
    out dx, al              ; Enable DLAB
    mov dx, SERIAL_PORT
    mov al, 3
    out dx, al              ; 38400 baud
    mov dx, SERIAL_PORT + 1
    xor al, al
    out dx, al
    mov dx, SERIAL_PORT + 3
    mov al, 0x03
    out dx, al              ; 8N1
    
    ; Print "MULTIBOOT2 KERNEL RUNNING!" to serial
    mov esi, msg_running
    call print_serial
    
    ; Check multiboot2 magic
    cmp eax, 0x36d76289
    jne .bad_magic
    
    mov esi, msg_magic_ok
    call print_serial
    jmp .parse_fb

.bad_magic:
    mov esi, msg_bad_magic
    call print_serial
    jmp .halt

.parse_fb:
    ; Parse multiboot2 info for framebuffer
    mov esi, ebx
    add esi, 8
    
.parse:
    mov eax, [esi]
    test eax, eax
    jz .no_fb
    cmp eax, 8              ; Framebuffer tag
    je .found_fb
    mov eax, [esi + 4]
    add eax, 7
    and eax, ~7
    add esi, eax
    jmp .parse

.found_fb:
    mov esi, msg_fb_found
    call print_serial
    
    mov edi, [esi + 8]      ; framebuffer address
    mov ebp, [esi + 16]     ; pitch
    mov ecx, [esi + 24]     ; height
    mov edx, [esi + 20]     ; width
    
    test edi, edi
    jz .no_fb
    
    ; Print framebuffer address
    push eax
    push edx
    mov eax, edi
    call print_hex
    pop edx
    pop eax
    
    ; Fill with MAGENTA
.row:
    push ecx
    push edi
    mov ecx, edx
.pixel:
    mov dword [edi], 0x00FF00FF     ; Magenta
    add edi, 4
    dec ecx
    jnz .pixel
    pop edi
    add edi, ebp
    pop ecx
    dec ecx
    jnz .row
    
    mov esi, msg_done
    call print_serial
    jmp .halt

.no_fb:
    mov esi, msg_no_fb
    call print_serial

.halt:
    mov esi, msg_halt
    call print_serial
    cli
    hlt
    jmp .halt

; Print string to serial port (ESI = string pointer)
print_serial:
    push eax
    push edx
.loop:
    lodsb
    test al, al
    jz .done
    mov dx, SERIAL_PORT + 5
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    mov dx, SERIAL_PORT
    mov al, [esi-1]
    out dx, al
    jmp .loop
.done:
    pop edx
    pop eax
    ret

; Print hex value (EAX = value)
print_hex:
    push eax
    push ebx
    push ecx
    push edx
    mov ecx, 8
.digit:
    rol eax, 4
    mov bl, al
    and bl, 0x0F
    add bl, '0'
    cmp bl, '9'
    jle .print
    add bl, 7
.print:
    mov dx, SERIAL_PORT + 5
.wait2:
    in al, dx
    test al, 0x20
    jz .wait2
    mov dx, SERIAL_PORT
    mov al, bl
    out dx, al
    dec ecx
    jnz .digit
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

section .data
msg_running:  db 13, 10, "=== MULTIBOOT2 KERNEL RUNNING! ===", 13, 10, 0
msg_magic_ok: db "Magic check: OK (0x36d76289)", 13, 10, 0
msg_bad_magic: db "Magic check: FAILED!", 13, 10, 0
msg_fb_found: db "Framebuffer found! Address: 0x", 0
msg_no_fb:    db "No framebuffer available", 13, 10, 0
msg_done:     db 13, 10, "Drawing complete!", 13, 10, 0
msg_halt:     db "System halted.", 13, 10, 0
