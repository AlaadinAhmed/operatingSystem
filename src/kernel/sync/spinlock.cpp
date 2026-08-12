#include "kernel/sync/spinlock.h"

void spinlock_acquire(spinlock_t *l) {
    // __atomic_test_and_set is a built-in compiler function.
    // It forces the CPU to execute a hardware-level atomic "LOCK" instruction.
    // It sets the lock to 1 and returns what the value WAS before.
    while (__atomic_test_and_set(&(l->lock), __ATOMIC_ACQUIRE)) {

#if defined(__x86_64__)
        // This tells an x86 CPU: "I am in a spin loop."
        // It stops the CPU from overheating and optimizes pipeline execution.
        asm volatile("pause");
#endif
    }
}

void spinlock_release(spinlock_t *l) {
    // Atomically clears the lock back to 0
    __atomic_clear(&(l->lock), __ATOMIC_RELEASE);
}
