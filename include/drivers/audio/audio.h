#pragma once
#include "drivers/bus/pci.h"
#include <stdint.h>
void find_audio_device();
uint64_t get_audio_base();
void init_audio();
void play_test_sound();
void generate_sine_wave(int frequency, int duration_ms, int sample_rate, int16_t *buffer);

#define HDA_BUFFER_FRAMES 1024

// Global Audio API (for AudioPlayer)
int16_t* audio_get_buffer(int index);
void audio_start_stream(uint16_t format);
void audio_stop_stream();
int audio_get_current_buffer();
