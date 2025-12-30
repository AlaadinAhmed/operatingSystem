#pragma once
#include <stdint.h>
#include <stdbool.h>

// Audio playback result codes
enum AudioResult {
    AUDIO_OK = 0,
    AUDIO_ERR_FILE_NOT_FOUND = -1,
    AUDIO_ERR_UNSUPPORTED_FORMAT = -2,
    AUDIO_ERR_DECODE_FAILED = -3,
    AUDIO_ERR_OUT_OF_MEMORY = -4,
    AUDIO_ERR_ALREADY_PLAYING = -5,
    AUDIO_ERR_NO_DRIVER = -6
};

// Audio file metadata
struct AudioFileInfo {
    uint32_t sampleRate;
    uint32_t channels;
    uint64_t totalFrames;
    uint32_t bitsPerSample;
};

// Initialize the audio player subsystem
void audio_player_init();

// === Blocking API (old) ===
int play_audio_file(const char* filename);
int play_audio_file_scaled(const char* filename, float volume);

// === Non-blocking Async API (new) ===
// Start playing a file in the background (returns immediately)
int start_audio_file(const char* filename);

// Call every frame to refill audio buffers
void audio_player_tick();

// Stop current playback
void stop_audio();

// Check if audio is currently playing
bool is_audio_playing();

// Get current playback progress (0.0 to 1.0)
float get_audio_progress();

// Get file info without playing
int get_audio_file_info(const char* filename, AudioFileInfo* info);

// Get current PCM samples for visualization (returns pointer to current buffer)
int16_t* get_current_audio_samples(uint32_t* num_samples);
