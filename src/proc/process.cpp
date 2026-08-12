#include "mem/heap.h"
#include "mem/vmm.h"
#include "print.h"
#include "proc/process.h"
#include "proc/scheduler.h"
ProcessControlBlock *ready_queue_head = nullptr;
ProcessControlBlock *ready_queue_tail = nullptr;
void *preemptive_prepare_stack(void *stack_alloc_page, void (*entry_point)()) {
    uintptr_t page_top = (uintptr_t)stack_alloc_page + 4096;
    uintptr_t frame_address = page_top - sizeof(RegisterState);

    RegisterState *frame = (RegisterState *)frame_address;

    // Zero out general purpose execution tracking registers
    frame->r15 = 0;
    frame->r14 = 0;
    frame->r13 = 0;
    frame->r12 = 0;
    frame->r11 = 0;
    frame->r10 = 0;
    frame->r9 = 0;
    frame->r8 = 0;
    frame->rdi = 0;
    frame->rsi = 0;
    frame->rdx = 0;
    frame->rcx = 0;
    frame->rbx = 0;
    frame->rax = 0;
    frame->rbp = 0;

    // Initialize dummy vector and error code
    frame->vector = 0;
    frame->error_code = 0;

    // Setup CPU execution jump details
    frame->rip = (uint64_t)entry_point;
    frame->cs = 0x08; // Your GDT Kernel Code Segment Selector
    frame->ss = 0x10; // Your GDT Kernel Data Segment Selector

    // 0x202 keeps CPU interrupts ENABLED when this task spawns.
    // If you leave this 0, the process starts with interrupts disabled and locks up the CPU.
    frame->rflags = 0x202;

    // The task's initial running RSP points back to its clean page boundary top
    frame->rsp = page_top;

    // Return the lowest address of the struct to write into your PCB tracking block
    return (void *)frame_address;
}
ProcessControlBlock *create_process(void (*entry_point)()) {
    ProcessControlBlock *pcb = new ProcessControlBlock;
    void *stack_page = kernel_alloc_virtual_pages(1);
    pcb->context = (RegisterState *)preemptive_prepare_stack(stack_page, entry_point);
    pcb->pml4_physical = create_process_pml4();
    pcb->pid = generate_process_id();
    pcb->state = ProcessState::READY;
    return pcb;
}
PageTable *create_process_pml4() {
    // 1. Allocate a fresh physical page for the new process's PML4 root
    PageTable *new_pml4_phys = (PageTable *)pmm_alloc_page();
    
    // Convert to virtual address so we can read/write it
    PageTable *new_pml4_virt = (PageTable *)phys_to_kvirt((uint64_t)new_pml4_phys);

    // 2. Clear out indices 0 to 255 (The Lower half / User Space)
    for (int i = 0; i < 256; i++) {
        new_pml4_virt->entries[i] = 0;
    }

    // 3. Clone ONLY the Kernel spaces from the master kernel PML4 (Indices 256 to 511)
    PageTable *kernel_pml4_virt = (PageTable *)phys_to_kvirt((uint64_t)g_kernel_pml4);
    for (int i = 256; i < 512; i++) {
        new_pml4_virt->entries[i] = kernel_pml4_virt->entries[i];
    }

    // Return the physical pointer! CR3 needs the physical address.
    return new_pml4_phys;
}
uint64_t generate_process_id() {
    static uint64_t g_next_pid = 1;
    return __atomic_fetch_add(&g_next_pid, 1, 5);
}
void idle_process_entry() {
    while (true) {
        asm volatile("hlt");
    }
}
void worker_thread_1() {
    while (true) {
        kprintf("Hello from Process 1!\n");
    }
}

ProcessControlBlock *pick_next_ready_task() {
    if (!ready_queue_head) {
        return g_idle_task; // Nothing ready? Fall back to the power-saving idle loop
    }

    // Pull the task off the front of our FIFO queue
    ProcessControlBlock *next_task = ready_queue_head;
    ready_queue_head = ready_queue_head->next;

    if (!ready_queue_head) {
        ready_queue_tail = nullptr;
    }

    next_task->next = nullptr;
    return next_task;
}
