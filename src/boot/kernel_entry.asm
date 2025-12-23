[bits 32]
[global _start]
[extern main]

section .bss
align 16
stack_bottom:
resb 262144 ; 256KB stack
stack_top:

section .text
_start:
    jmp entry_code

    ; Multiboot Header
    align 4
    dd 0x1BADB002               ; Magic
    dd 0x00000007               ; Flags (Align | MemInfo | Video)
    dd -(0x1BADB002 + 0x00000007) ; Checksum
    dd 0, 0, 0, 0, 0            ; Unused
    dd 0                        ; Linear graphics
    dd 1024, 768, 32            ; Width, Height, Depth

entry_code:
    ; Save Multiboot registers immediately
    mov esi, eax
    mov edx, ebx

    ; Clear BSS (Always do this!)
    extern __bss_start
    extern __bss_end
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Check if booted by GRUB (Magic == 0x2BADB002)
    cmp esi, 0x2BADB002
    jne .custom_loader_setup

    ; GRUB detected. Copy VBE info to 0x5200.
    mov edi, 0x5200
    
    ; Get vbe_mode_info pointer (Multiboot offset 76 / 0x4C)
    mov ebx, edx ; Restore EBX for addressing
    mov eax, [ebx + 76]
    
    ; Check if pointer is valid
    test eax, eax
    jz .custom_loader_setup ; If 0, skip VBE setup
    
    ; Copy Framebuffer Addr (VBE Mode Info offset 40 / 0x28)
    mov ebx, [eax + 40] ; Use EBX as temp
    mov [edi + 40], ebx
    
    ; Copy Pitch (VBE Mode Info offset 16 / 0x10)
    mov bx, [eax + 16]
    mov [edi + 16], bx
    
    ; Copy Width (VBE Mode Info offset 18 / 0x12)
    mov bx, [eax + 18]
    mov [edi + 18], bx
    
    ; Copy Height (VBE Mode Info offset 20 / 0x14)
    mov bx, [eax + 20]
    mov [edi + 20], bx

    ; Copy BPP (VBE Mode Info offset 25 / 0x19)
    mov bl, [eax + 25]
    mov [edi + 25], bl

.custom_loader_setup:
    ; Set up stack for GRUB boot (same as custom loader)
    mov esp, stack_top
    mov ebp, esp

    ; Align stack to 16 bytes
    and esp, 0xFFFFFFF0
    sub esp, 8
    
    ; Push Multiboot Info (EDX) and Magic (ESI)
    push edx
    push esi
    ; Enable FPU and SSE
    mov eax, cr0
    and ax, 0xFFFB      ; Clear EM (bit 2)
    or ax, 0x2          ; Set MP (bit 1)
    mov cr0, eax

    mov eax, cr4
    or ax, 3 << 9       ; Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    mov cr4, eax

    fninit              ; Initialize FPU

    call main
    jmp $
