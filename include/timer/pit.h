#pragma once
#include <stdint.h>

// Flag set by the ISR when the PIT fires (used for TSC calibration)
extern volatile bool pit_fired;

void init_pit(uint32_t frequency);
uint64_t pit_get_ticks();
void pit_increment_tick();
void pit_reset_fired();
