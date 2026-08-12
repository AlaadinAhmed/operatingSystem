#pragma once

#include <stdint.h>

#define IOAPIC_REG_ID           0x00
#define IOAPIC_REG_VER          0x01
#define IOAPIC_REG_ARB          0x02
#define IOAPIC_REG_REDTBL       0x10

void ioapic_init();
void ioapic_set_entry(uint8_t irq, uint8_t vector, uint8_t delivery_mode, uint8_t dest_mode, uint8_t pin_polarity, uint8_t trigger_mode, uint8_t mask, uint8_t destination);
