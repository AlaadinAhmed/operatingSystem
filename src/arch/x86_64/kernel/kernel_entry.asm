[bits 64]
[global _start]
[extern main]

section .text.entry
global _start
_start:
    ; ============================================
    ; STEP 1: Set up stack
    ; ============================================
    mov rsp, stack_top
    and rsp, -16 ; Force 16-byte alignment

    ; ============================================
    ; STEP 2: Call Kernel main()
    ; ============================================
    ; Arguments:
    ; RDI = magic (if passed)
    ; RSI = addr (if passed)
    ; For now we assume the loader sets these up or we don't care.
    
    call main
    
    ; Should never return
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 262144 ; 256KB stack
stack_top:
