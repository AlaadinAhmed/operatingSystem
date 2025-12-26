[bits 32]
[global _start]
[extern main]

section .text.entry
global _start
_start:
    jmp kernel_entry

section .multiboot2
    align 8
    multiboot2_header_start:
        dd 0xe85250d6                ; magic
        dd 0                         ; architecture (i386)
        dd multiboot2_header_end - multiboot2_header_start ; header length
        dd -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start)) ; checksum
        
        ; End tag
        dw 0 ; type
        dw 0 ; flags
        dd 8 ; size
    multiboot2_header_end:

section .multiboot
    ; ============================================
    ; MULTIBOOT HEADER - Minimal format
    ; ============================================
    align 4
    MULTIBOOT_MAGIC     equ 0x1BADB002
    MULTIBOOT_FLAGS     equ 0x00000003  ; Align modules (bit 0) + Memory info (bit 1)
    MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
    
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM


section .stack
align 16
stack_bottom:
    resb 262144 ; 256KB stack
stack_top:

section .text
kernel_entry:
    ; ============================================
    ; STEP 1: Set up stack IMMEDIATELY
    ; ============================================
    mov esp, stack_top

    ; ============================================
    ; STEP 2: Write test pattern to VGA text buffer
    ; VGA text mode memory is at 0xB8000
    ; Each character is 2 bytes: [char][attr]
    ; attr: high nibble = background, low nibble = foreground
    ; ============================================
    
    ; Write 'H' at position 0 (white on black)
    mov byte [0xB8000], 'H'
    mov byte [0xB8001], 0x0F
    
    ; Write 'I' at position 1
    mov byte [0xB8002], 'I'
    mov byte [0xB8003], 0x0F
    
    ; Write '!' at position 2
    mov byte [0xB8004], '!'
    mov byte [0xB8005], 0x4F  ; White on red

    ; ============================================
    ; STEP 3: Call Kernel main()
    ; ============================================
    ; Push arguments for main(uint32_t magic, uint64_t addr)
    ; For multiboot, EAX = magic, EBX = info addr
    push 0          ; High 32 bits of addr
    push ebx        ; Low 32 bits of addr
    push eax        ; magic
    call main
    
    ; Should never return
    cli
.hang:
    hlt
    jmp .hang


