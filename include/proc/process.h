#pragma once
#include "fs/vfs.h"
#include "mem/vmm.h"
#include <cstdint>
constexpr int MAX_OPEN_FILES = 32;
enum class ProcessState { RUNNING, READY, BLOCKED, ZOMBIE };
struct RegisterState {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rbp;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));
struct ProcessControlBlock {
    uint64_t pid;
    ProcessState state;
    uint8_t priority;
    void *program_counter;
    PageTable *pml4_physical;
    RegisterState *context;
    FileDescriptor *filedescriptors[MAX_OPEN_FILES];
    uint64_t cpu_cycles_used;
    ProcessControlBlock *next;

    ProcessControlBlock() {
        pid = 0;
        state = ProcessState::READY;
        priority = 0;
        program_counter = nullptr;
        pml4_physical = nullptr;
        context = nullptr;
        cpu_cycles_used = 0;
        next = nullptr;
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            filedescriptors[i] = nullptr;
        }
    }
};
ProcessControlBlock *create_process(void (*entry_point)());
PageTable *create_process_pml4();
uint64_t generate_process_id();
void idle_process_entry();
void worker_thread_1();
ProcessControlBlock *pick_next_ready_task();
