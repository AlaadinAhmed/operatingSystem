#include "mem/heap.h"
#include "mem/vmm.h"
#include "print.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "sys/syscalls.h"
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

    // The task's initial running RSP points back to its clean page boundary top (adjusted for ABI alignment)
    frame->rsp = page_top - 8;

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
    kprintf("Process 1: Testing Syscall / VFS File Descriptor implementation...\n");
    
    int fd = sys_open("/mp/Roboto-Regular.ttf", 0);
    if (fd < 0) {
        kprintf("Process 1: Error opening file via sys_open!\n");
    } else {
        kprintf("Process 1: Successfully opened file with fd = %d\n", fd);
        
        uint8_t buf[16];
        int64_t bytes = sys_read(fd, buf, 16);
        if (bytes < 0) {
            kprintf("Process 1: Error reading file via sys_read!\n");
        } else {
            kprintf("Process 1: Read %d bytes from file. First 4 bytes (hex): %x %x %x %x\n",
                    (int)bytes, buf[0], buf[1], buf[2], buf[3]);
        }
        
        sys_close(fd);
        kprintf("Process 1: Closed fd = %d\n", fd);
    }

    while (true) {
        asm volatile("hlt");
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
