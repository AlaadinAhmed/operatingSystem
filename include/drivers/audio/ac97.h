#pragma once
#include <stdint.h>

// AC'97 is much simpler than HDA
// It uses PIO (Port I/O) instead of MMIO for most operations

class AC97 {
public:
    AC97(uint16_t nabmbar, uint16_t nambar);
    void Initialize();
    void PlayTestSound();

private:
    uint16_t m_nabmbar;  // Native Audio Bus Mastering BAR (BAR1)
    uint16_t m_nambar;   // Native Audio Mixer BAR (BAR0)
    
    // Buffer Descriptor List
    static uint32_t s_bdl[32 * 2]; // 32 entries, 2 DWORDs each
    static int16_t s_audio_buffer[4096];
    
    // Port I/O
    void OutB(uint16_t port, uint8_t val);
    void OutW(uint16_t port, uint16_t val);
    void OutL(uint16_t port, uint32_t val);
    uint8_t InB(uint16_t port);
    uint16_t InW(uint16_t port);
    uint32_t InL(uint16_t port);
};
