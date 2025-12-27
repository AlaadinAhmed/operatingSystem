#include "drivers/audio/ac97.h"
#include "print/print.h"

// Static Buffer Definitions
uint32_t __attribute__((aligned(4096))) AC97::s_bdl[32 * 2];
int16_t __attribute__((aligned(4096))) AC97::s_audio_buffer[4096];

AC97::AC97(uint16_t nabmbar, uint16_t nambar) 
    : m_nabmbar(nabmbar), m_nambar(nambar) {}

void AC97::OutB(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void AC97::OutW(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

void AC97::OutL(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t AC97::InB(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint16_t AC97::InW(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t AC97::InL(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void AC97::Initialize() {
    kprintf("AC'97: Initializing...\n");
    
    // Reset the codec
    OutW(m_nambar + 0x00, 0xFFFF); // Master Volume Reset
    
    // Wait a bit
    for (volatile int i = 0; i < 100000; i++);
    
    // Set volumes
    OutW(m_nambar + 0x02, 0x0000); // Master Volume: no attenuation
    OutW(m_nambar + 0x04, 0x0000); // Aux Out Volume
    OutW(m_nambar + 0x06, 0x0000); // Mono Volume
    OutW(m_nambar + 0x18, 0x0808); // PCM Out Volume: no attenuation
    
    // Check if we can write to the PCM output register
    uint16_t pcm_vol = InW(m_nambar + 0x18);
    kprintf("AC'97: PCM Volume Register: 0x%x\n", pcm_vol);
    
    // Set sample rate to 48kHz (default, usually 0xBB80 = 48000)
    // Some codecs may not support variable rate audio
    OutW(m_nambar + 0x2C, 48000);
    
    kprintf("AC'97: Codec initialized.\n");
}

void AC97::PlayTestSound() {
    kprintf("AC'97: Playing test sound...\n");
    
    // Generate simple sawtooth wave
    for (int i = 0; i < 4096; i++) {
        s_audio_buffer[i] = (i % 100) * 200 - 10000;
    }
    
    // Setup Buffer Descriptor List
    // Each entry: 2 DWORDs
    // DWORD 0: Physical address of buffer
    // DWORD 1: bits 15:0 = length in samples (not bytes!), bit 31 = IOC, bit 30 = BUP
    uint64_t buf_phys = (uint64_t)s_audio_buffer;
    s_bdl[0] = (uint32_t)buf_phys;
    s_bdl[1] = 4096 | (1 << 31); // 4096 samples, IOC set
    
    kprintf("AC'97: BDL at 0x%lx, Buffer at 0x%lx\n", (uint64_t)s_bdl, buf_phys);
    
    // PCM Out registers are at NABMBAR + 0x10
    uint16_t pcm_out = m_nabmbar + 0x10;
    
    // Stop any current playback
    OutB(pcm_out + 0x0B, 0x00); // Control: Stop
    
    // Reset the channel
    OutB(pcm_out + 0x0B, 0x02); // Control: Reset
    for (volatile int i = 0; i < 10000; i++);
    OutB(pcm_out + 0x0B, 0x00); // Control: Clear Reset
    
    // Set Buffer Descriptor List address
    OutL(pcm_out + 0x00, (uint32_t)(uint64_t)s_bdl);
    
    // Set Last Valid Index (LVI) to 0 (one buffer)
    OutB(pcm_out + 0x05, 0);
    
    // Start playback
    OutB(pcm_out + 0x0B, 0x01); // Control: Run
    
    // Monitor
    for (int i = 0; i < 20; i++) {
        uint8_t status = InB(pcm_out + 0x06);
        uint8_t civ = InB(pcm_out + 0x04);
        uint16_t pos = InW(pcm_out + 0x08);
        kprintf("AC'97: CIV=%d, Pos=%d, Status=0x%x\n", civ, pos, status);
        for (volatile int j = 0; j < 1000000; j++);
    }
}
