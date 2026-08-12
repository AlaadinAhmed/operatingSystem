#include "proc/scheduler.h"
#include "arch/x86_64/interrupts.h"
#include "mem/vmm.h"

ProcessControlBlock *g_idle_task = nullptr;
ProcessControlBlock *current_task = nullptr;

void scheduler_init() {
    // 1. Create the true power-saving idle process
    ProcessControlBlock *idle_pcb = create_process(idle_process_entry);
    idle_pcb->pid = 0;
    idle_pcb->priority = 0;
    g_idle_task = idle_pcb;

    // 2. Create the main thread process representing the boot context
    ProcessControlBlock *main_pcb = new ProcessControlBlock;
    main_pcb->pid = generate_process_id();
    main_pcb->state = ProcessState::RUNNING;
    main_pcb->priority = 1;
    main_pcb->pml4_physical = g_kernel_pml4;
    main_pcb->context = nullptr; // Will be saved on first interrupt
    main_pcb->next = nullptr;

    current_task = main_pcb;
}

extern "C" InterruptFrame *schedule_preempt(InterruptFrame *frame) {
    if (!current_task) {
        return frame;
    }

    // Save the interrupted task's stack pointer pointing to its InterruptFrame
    current_task->context = (RegisterState *)frame;

    // If the current task is not the idle task, put it back on the ready queue
    if (current_task != g_idle_task) {
        queue_ready_process(current_task);
    } else {
        current_task->state = ProcessState::READY;
    }

    // Pick the next ready task to run
    ProcessControlBlock *next_task = pick_next_ready_task();

    current_task = next_task;
    current_task->state = ProcessState::RUNNING;

    // Switch CR3 if the page table changes
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    uint64_t target_cr3 = (uint64_t)current_task->pml4_physical;
    if (current_cr3 != target_cr3) {
        asm volatile("mov %0, %%cr3" : : "r"(target_cr3));
    }

    // Return the next task's saved stack pointer pointing to its stack frame
    return (InterruptFrame *)current_task->context;
}

void queue_ready_process(ProcessControlBlock *pcb) {
    pcb->state = ProcessState::READY;
    pcb->next = nullptr;

    if (!ready_queue_head) {
        ready_queue_head = pcb;
        ready_queue_tail = pcb;
    } else {
        ready_queue_tail->next = pcb;
        ready_queue_tail = pcb;
    }
}
