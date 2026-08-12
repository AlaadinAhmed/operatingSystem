#include "timer/pit.h"
#include "print/print.h" // for outb()

static volatile uint64_t g_pit_ticks = 0;
volatile bool pit_fired = false;

void init_pit(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;
    
    // Command byte: Channel 0 | Access lo/hi | Mode 2 (Rate Generator) | Binary
    // Mode 2 produces a clean periodic pulse on IRQ0; Mode 3 (square wave)
    // also works but Mode 2 is the canonical choice for OS tick interrupts.
    outb(0x43, 0x34);
    outb(0x40, (uint8_t)(divisor & 0xFF));        // Low byte
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}

uint64_t pit_get_ticks() {
    return g_pit_ticks;
}

void pit_increment_tick() {
    g_pit_ticks++;
    pit_fired = true;
}

void pit_reset_fired() {
    pit_fired = false;
}
