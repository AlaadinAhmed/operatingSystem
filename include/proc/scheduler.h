#pragma once

#include "proc/process.h"
#include <cstdint>
struct InterruptFrame;

extern "C" {
void asm_context_switch(void **old_rsp, void *new_rsp, uint64_t cr3);
InterruptFrame *schedule_preempt(InterruptFrame *frame);
}
extern ProcessControlBlock *g_idle_task;
extern ProcessControlBlock *current_task;
extern ProcessControlBlock *ready_queue_head;
extern ProcessControlBlock *ready_queue_tail;
void scheduler_init();
void queue_ready_process(ProcessControlBlock *pcb);
