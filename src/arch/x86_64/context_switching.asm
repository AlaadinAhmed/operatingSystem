bits 64
section .text
global asm_context_switch
asm_context_switch:
  ;RDI = void** old_rsp
  ;RSI = void* new_rsp
  ;RDX = uint64_t new_cr3

  ;Push all callee-saved registers onto the current stack.
  push rbp
  push rbx
  push r12
  push r13
  push r14
  push r15

  ;Save the current stack pointer
  mov [rdi], rsp

  ;Swap cr3 but check if it's equal to avoid expensive TLB flushing
  mov rax, cr3
  cmp rax, rdx
  je .skip_cr3
  mov cr3, rdx
.skip_cr3:
  

  ;Switch to a new rsp value
  mov rsp, rsi 
  
  ;Pop the new task's calle-saved registers off its stack
  pop r15
  pop r14
  pop r13
  pop r12 
  pop rbx
  pop rbp

  ;Resume execution
  ret


