#include "arch/x86_64/interrupts.h"
#include "drivers/pic.h"
#include "drivers/keyboard.h"
#include "drivers/mouse/mouse.h"
#include "print/print.h"

extern "C" void exception_handler_cpp(uint64_t vector, InterruptFrame *frame) {
  if (vector < 32) {
      kprintf("\n------------------------------------------------------------\n");
      kprintf("EXCEPTION RECEIVED: %d\n", vector);
      kprintf("Error Code: 0x%lx\n", frame->error_code);
      kprintf("RIP: 0x%lx CS: 0x%lx RFLAGS: 0x%lx RSP: 0x%lx SS: 0x%lx\n",
              frame->rip, frame->cs, frame->rflags, frame->rsp, frame->ss);
      kprintf("RAX: 0x%lx RBX: 0x%lx RCX: 0x%lx RDX: 0x%lx\n", frame->rax,
              frame->rbx, frame->rcx, frame->rdx);
      kprintf("RSI: 0x%lx RDI: 0x%lx RBP: 0x%lx\n", frame->rsi, frame->rdi,
              frame->rbp);
      kprintf("R8:  0x%lx R9:  0x%lx R10: 0x%lx R11: 0x%lx\n", frame->r8, frame->r9,
              frame->r10, frame->r11);
      kprintf("R12: 0x%lx R13: 0x%lx R14: 0x%lx R15: 0x%lx\n", frame->r12,
              frame->r13, frame->r14, frame->r15);
      kprintf("------------------------------------------------------------\n");
      asm volatile("cli; hlt");
  }

  if (vector == 33) {
    keyboard_on_interrupt();
    pic_send_eoi(1);
    return;
  }

  if (vector == 44) {
    mouse_on_interrupt();
    pic_send_eoi(12);
    return;
  }
}
