#pragma once
#include "drivers/bus/pci.h"
#include <stdint.h>
void find_audio_device();
uint64_t get_audio_base();
void init_audio();
void play_test_sound();
void generate_sine_wave(int frequency, int duration_ms, int sample_rate, int16_t *buffer);
