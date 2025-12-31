#pragma once

#include <stdint.h>

// Initializes the PIC and remaps IRQs
// IRQ 0-7  -> IDT 32-39
// IRQ 8-15 -> IDT 40-47
void init_pic();

// Sends End of Interrupt (EOI) signal to the PIC
// irq: The IRQ number (0-15)
void pic_send_eoi(uint8_t irq);

void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
