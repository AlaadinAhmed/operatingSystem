[bits 64]
global asm_syscall_entry
extern c_syscall_handler

section .text

asm_syscall_entry:
    ; 1. Swap GS base or switch to kernel stack pointer
    ; If using per-cpu GS storage: swapgs
    ; Temporary stack swap strategy:
    mov [gs:0x00], rsp         ; Save User RSP into Per-CPU scratch area
    mov rsp, [gs:0x08]         ; Load Kernel Stack RSP for current process

    ; 2. Preserve registers and user state
    push qword [gs:0x00]       ; Push User RSP
    push r11                   ; Push User RFLAGS
    push rcx                   ; Push User RIP

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; 3. Align parameters for C ABI:
    ; C function expects: sys_num(rdi), arg1(rsi), arg2(rdx), arg3(rcx), arg4(r8), arg5(r9)
    ; Inbound registers:  sys_num(rax), arg1(rdi), arg2(rsi), arg3(rdx), arg4(r10), arg5(r8)
    
    mov r9, r8                 ; Arg5 -> R9
    mov r8, r10                ; Arg4 -> R8
    mov rcx, rdx               ; Arg3 -> RCX
    mov rdx, rsi               ; Arg2 -> RDX
    mov rsi, rdi               ; Arg1 -> RSI
    mov rdi, rax               ; Syscall Number -> RDI

    ; 4. Call C++ dispatcher
    call c_syscall_handler

    ; RAX now holds the return value from c_syscall_handler

    ; 5. Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    pop rcx                    ; Restore User RIP
    pop r11                    ; Restore User RFLAGS
    pop rsp                    ; Restore User RSP

    ; If using swapgs: swapgs
    sysretq                    ; Return to User Space (Ring 3)
