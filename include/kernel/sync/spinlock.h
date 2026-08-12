#pragma once

typedef struct {
    // 'volatile' tells the compiler never to optimize this variable away
    // or cache it in a CPU register. It MUST be read directly from RAM every time.
    volatile int lock;
} spinlock_t;

// Initializes a lock to the unlocked state (0)
inline void spinlock_init(spinlock_t *l) { l->lock = 0; }

void spinlock_acquire(spinlock_t *l);
void spinlock_release(spinlock_t *l);
