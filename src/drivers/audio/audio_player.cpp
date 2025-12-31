// Audio Player - dr_libs integration for WAV/MP3 playback
// Supports both blocking and non-blocking (async) playback

#include "drivers/audio/audio_player.h"
#include "dr_libs/dr_wav.h"
#include "drivers/audio/audio.h"
#include "drivers/audio/intel_hda.h"
#include "fs/lwext4_adapter.h"
#include "memory/kmalloc.h"
#include "print/print.h"

// dr_libs configuration for freestanding environment
#define DR_WAV_NO_STDIO
#define DR_WAV_NO_WCHAR
#define DRWAV_ASSERT(x) ((void)0)
#define DRWAV_MALLOC(sz) kmalloc(sz)
#define DRWAV_REALLOC(p, sz) krealloc(p, sz)
#define DRWAV_FREE(p) kfree(p)

#define DR_MP3_NO_STDIO
#define DR_MP3_NO_SIMD
#define DRMP3_ASSERT(x) ((void)0)
#define DRMP3_MALLOC(sz) kmalloc(sz)
#define DRMP3_REALLOC(p, sz) krealloc(p, sz)
#define DRMP3_FREE(p) kfree(p)

#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_libs/dr_mp3.h"

// ============================================================================
// Async Playback State
// ============================================================================

struct AudioPlayerState {
  bool playing;
  bool initialized;

  // File data (kept in memory for dr_wav)
  unsigned char *file_data;
  size_t file_size;

  // WAV decoder state
  drwav wav;
  bool wav_valid;

  // Playback tracking
  uint64_t total_frames;
  uint64_t frames_decoded;
  int last_buffer_filled; // 0 or 1 - which buffer we last filled
};

static AudioPlayerState s_state = {0};

// ============================================================================
// Helper Functions
// ============================================================================

static bool ends_with(const char *str, const char *suffix) {
  if (!str || !suffix)
    return false;
  int str_len = 0, suffix_len = 0;
  while (str[str_len])
    str_len++;
  while (suffix[suffix_len])
    suffix_len++;
  if (suffix_len > str_len)
    return false;

  for (int i = 0; i < suffix_len; i++) {
    char c1 = str[str_len - suffix_len + i];
    char c2 = suffix[i];
    if (c1 >= 'A' && c1 <= 'Z')
      c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z')
      c2 += 32;
    if (c1 != c2)
      return false;
  }
  return true;
}

// Fill a buffer with decoded audio data
static bool fill_buffer(int buffer_index) {
  if (!s_state.wav_valid || !s_state.playing) {
    return false;
  }

  int16_t *buffer = audio_get_buffer(buffer_index);
  if (!buffer)
    return false;

  // Read frames from WAV decoder
  uint64_t frames_to_read = HDA_BUFFER_FRAMES;
  uint64_t frames_read = drwav_read_pcm_frames_s16(&s_state.wav, frames_to_read, buffer);
  
  // DEBUG: Print first few samples of decoded data
  kprintf("fill_buffer(%d): read %d frames, samples: %d %d %d %d\n", 
          buffer_index, (int)frames_read, buffer[0], buffer[1], buffer[2], buffer[3]);
  
  if (frames_read < frames_to_read) {
      // End of file
      s_state.playing = false;
      // Fill rest with zeros
      for (uint64_t i = frames_read * s_state.wav.channels; i < frames_to_read * s_state.wav.channels; i++) {
          buffer[i] = 0;
      }
      frames_read = frames_to_read; // Pretend we filled it
  }
  s_state.frames_decoded += frames_read;

  // Check if we should loop or stop
  if (frames_read == 0) {
    return false; // Done playing
  }

  // Flush cache for DMA
  __asm__ volatile("wbinvd");

  return true;
}

// ============================================================================
// Public API - Initialization
// ============================================================================

void audio_player_init() {
  s_state.playing = false;
  s_state.initialized = true;
  s_state.file_data = nullptr;
  s_state.wav_valid = false;
  s_state.last_buffer_filled = -1;
  kprintf("AudioPlayer: Initialized (async mode)\n");
}

// ============================================================================
// Public API - Async (Non-blocking)
// ============================================================================

int start_audio_file(const char *filename) {
  if (s_state.playing) {
    return AUDIO_ERR_ALREADY_PLAYING;
  }

  kprintf("AudioPlayer: Starting '%s'...\n", filename);

  // Read file to memory
  s_state.file_data = read_file_to_memory("/mp/", filename, &s_state.file_size);
  if (!s_state.file_data) {
    kprintf("AudioPlayer: Failed to read file\n");
    return AUDIO_ERR_FILE_NOT_FOUND;
  }
  kprintf("AudioPlayer: Loaded %d bytes\n", (int)s_state.file_size);

  if (!ends_with(filename, ".wav")) {
    kprintf("AudioPlayer: Only WAV supported for async\n");
    kfree(s_state.file_data);
    s_state.file_data = nullptr;
    return AUDIO_ERR_UNSUPPORTED_FORMAT;
  }

  // Initialize WAV decoder
  if (!drwav_init_memory(&s_state.wav, s_state.file_data, s_state.file_size,
                         nullptr)) {
    kprintf("AudioPlayer: Failed to init WAV decoder\n");
    kfree(s_state.file_data);
    s_state.file_data = nullptr;
    return AUDIO_ERR_DECODE_FAILED;
  }

  s_state.wav_valid = true;
  s_state.total_frames = s_state.wav.totalPCMFrameCount;
  s_state.frames_decoded = 0;
  s_state.playing = true;
  s_state.last_buffer_filled = -1;

  kprintf("AudioPlayer: WAV %dHz, %dch, %d frames\n", s_state.wav.sampleRate,
          s_state.wav.channels, (int)s_state.total_frames);

    // Debug: Print first few samples to verify decoding
    int16_t* samples = (int16_t*)s_state.file_data; // Wait, file_data is raw file. We need to look at the decoded output.
    // drwav_init_memory just inits the decoder. We haven't decoded yet!
    // The decoding happens in fill_buffer.
    
    // Let's decode a small chunk just to check
    int16_t temp_buf[32];
    drwav_read_pcm_frames_s16(&s_state.wav, 16, temp_buf);
    drwav_seek_to_pcm_frame(&s_state.wav, 0); // Reset seek
    
    kprintf("AudioPlayer: First 16 samples: ");
    for(int i=0; i<16; i++) kprintf("%d ", temp_buf[i]);
    kprintf("\n");

  // Pre-fill both buffers
  fill_buffer(0);
  fill_buffer(1);
  s_state.last_buffer_filled = 1;

    // Calculate HDA format
    // Bit 14: Base (0=48kHz, 1=44.1kHz)
    // Bits 13-11: Multiplier (0=x1, 1=x2, 2=x3, 3=x4)
    // Bits 10-8: Divisor (0=/1, 1=/2, 2=/3, ... 7=/8)
    // Bits 6-4: Bits/Sample (0=8, 1=16, 2=20, 3=24, 4=32)
    // Bits 3-0: Channels (0=1ch, 1=2ch, ...)
    
    uint16_t format = 0;
    
    // Channels (0=1ch, 1=2ch)
    if (s_state.wav.channels == 2) format |= 1;
    
    // Bits (Assuming 16-bit for now as we decode to s16)
    format |= (1 << 4); 
    
    // Sample Rate
    if (s_state.wav.sampleRate == 48000) {
        // Base 0 (48kHz)
        format |= (0 << 14);
    } else if (s_state.wav.sampleRate == 44100) {
        // Base 1 (44.1kHz)
        format |= (1 << 14);
    } else {
        // Fallback or todo: handle other rates
        kprintf("AudioPlayer: Warning: Unsupported rate %d, defaulting to 48kHz\n", s_state.wav.sampleRate);
    }

    kprintf("AudioPlayer: HDA Format 0x%x\n", format);

    // Start HDA stream
    audio_start_stream(format);

  kprintf("AudioPlayer: Playback started (non-blocking)\n");
  return AUDIO_OK;
}

void audio_player_tick() {
  if (!s_state.playing) {
    return;
  }

  // Check which buffer is currently playing
  int current_buffer = audio_get_current_buffer();
  
  // Debug: Print state on change
  static int last_seen_buffer = -1;
  if (current_buffer != last_seen_buffer) {
      kprintf("AudioPlayer: Buffer switch %d -> %d (Last filled: %d)\n", 
              last_seen_buffer, current_buffer, s_state.last_buffer_filled);
      last_seen_buffer = current_buffer;
  }

  // Fill the OTHER buffer if we haven't already
  int buffer_to_fill = 1 - current_buffer;

  if (buffer_to_fill != s_state.last_buffer_filled) {
    kprintf("AudioPlayer: Refilling buffer %d\n", buffer_to_fill);
    if (!fill_buffer(buffer_to_fill)) {
      // EOF reached
      kprintf("AudioPlayer: Playback complete\n");
      stop_audio();
      return;
    }
    s_state.last_buffer_filled = buffer_to_fill;
  }
}

void stop_audio() {
  if (!s_state.playing)
    return;

  audio_stop_stream();

  if (s_state.wav_valid) {
    drwav_uninit(&s_state.wav);
    s_state.wav_valid = false;
  }

  if (s_state.file_data) {
    kfree(s_state.file_data);
    s_state.file_data = nullptr;
  }

  s_state.playing = false;
  kprintf("AudioPlayer: Stopped\n");
}

bool is_audio_playing() { return s_state.playing; }

float get_audio_progress() {
  if (!s_state.playing || s_state.total_frames == 0) {
    return 0.0f;
  }
  return (float)s_state.frames_decoded / (float)s_state.total_frames;
}

int16_t *get_current_audio_samples(uint32_t *num_samples) {
  if (!s_state.playing) {
    if (num_samples)
      *num_samples = 0;
    return nullptr;
  }

  int current = audio_get_current_buffer();
  if (num_samples)
    *num_samples = HDA_BUFFER_FRAMES * 2; // stereo samples
  return audio_get_buffer(current);
}

// ============================================================================
// Public API - Blocking (Legacy compatibility)
// ============================================================================

int play_audio_file(const char *filename) {
  return play_audio_file_scaled(filename, 1.0f);
}

int play_audio_file_scaled(const char *filename, float volume) {
  // For blocking playback, use start_audio_file then poll until done
  int result = start_audio_file(filename);
  if (result != AUDIO_OK) {
    return result;
  }

  // Poll until playback completes
  while (is_audio_playing()) {
    audio_player_tick();
    // Small delay to avoid busy-spinning too aggressively
    for (volatile int i = 0; i < 1000; i++)
      ;
  }

  return AUDIO_OK;
}

int get_audio_file_info(const char *filename, AudioFileInfo *info) {
  if (!info)
    return AUDIO_ERR_DECODE_FAILED;

  size_t file_size;
  unsigned char *file_data = read_file_to_memory("/mp/", filename, &file_size);
  if (!file_data) {
    return AUDIO_ERR_FILE_NOT_FOUND;
  }

  if (ends_with(filename, ".wav")) {
    drwav wav;
    if (!drwav_init_memory(&wav, file_data, file_size, nullptr)) {
      kfree(file_data);
      return AUDIO_ERR_DECODE_FAILED;
    }

    info->sampleRate = wav.sampleRate;
    info->channels = wav.channels;
    info->totalFrames = wav.totalPCMFrameCount;
    info->bitsPerSample = wav.bitsPerSample;

    drwav_uninit(&wav);
  } else {
    kfree(file_data);
    return AUDIO_ERR_UNSUPPORTED_FORMAT;
  }

  kfree(file_data);
  return AUDIO_OK;
}
