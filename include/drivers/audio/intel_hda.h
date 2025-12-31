#pragma once
#include <stdint.h>

// HDA Register Offsets
#define HDA_REG_GCAP    0x00
#define HDA_REG_VMIN    0x02
#define HDA_REG_VMAJ    0x03
#define HDA_REG_OUTPAY  0x04
#define HDA_REG_INPAY   0x06
#define HDA_REG_GCTL    0x08
#define HDA_REG_WAKEEN  0x0C
#define HDA_REG_STATESTS 0x0E
#define HDA_REG_GSTS    0x10
#define HDA_REG_INTCTL  0x20
#define HDA_REG_INTSTS  0x24
#define HDA_REG_WALCLK  0x30
#define HDA_REG_SSYNC   0x34
#define HDA_REG_CORBLBASE 0x40
#define HDA_REG_CORBUBASE 0x44
#define HDA_REG_CORBWP  0x48
#define HDA_REG_CORBRP  0x4A
#define HDA_REG_CORBCTL 0x4C
#define HDA_REG_CORBSTS 0x4D
#define HDA_REG_CORBSIZE 0x4E
#define HDA_REG_RIRBLBASE 0x50
#define HDA_REG_RIRBUBASE 0x54
#define HDA_REG_RIRBWP  0x58
#define HDA_REG_RINTCNT 0x5A
#define HDA_REG_RIRBCTL 0x5C
#define HDA_REG_RIRBSTS 0x5D
#define HDA_REG_RIRBSIZE 0x5E
#define HDA_REG_ICO     0x60
#define HDA_REG_ICI     0x64
#define HDA_REG_ICS     0x68
#define HDA_REG_DPLBASE 0x70
#define HDA_REG_DPUBASE 0x74

// Stream Descriptor Offsets
#define HDA_SD_CTL      0x00
#define HDA_SD_STS      0x03
#define HDA_SD_LPIB     0x04
#define HDA_SD_CBL      0x08
#define HDA_SD_LVI      0x0C
#define HDA_SD_FMT      0x12
#define HDA_SD_BDLPL    0x18
#define HDA_SD_BDLPU    0x1C

struct BDLEntry {
    uint64_t address;
    uint32_t length;
    uint32_t flags; // Bit 0: IOC
} __attribute__((packed));

class IntelHDA {
public:
    IntelHDA(uint64_t base_addr);
    void Initialize();
    void PlayTestSound();
    
    // Audio Player API
    void StartStream(uint16_t format);
    void StopStream();
    uint32_t GetStreamPos();
    int16_t* GetBuffer(int index);

private:
    uint64_t m_base;
    uint32_t m_output_stream_offset;
    
    // CORB/RIRB Buffers (aligned to 128 bytes)
    static uint32_t s_corb[256];
    static uint64_t s_rirb[256];
    
    // Stream Buffers (aligned to 4096 bytes)
    static BDLEntry s_bdl[2];
    static int16_t s_audio_buffer[4096];
    static uint64_t s_dma_pos[8];

    // Helper functions
    void ResetController();
    void InitCORBRIRB();
    void SendVerb(uint8_t codec, uint16_t node, uint32_t payload);
    void SetupStream(uint32_t stream_id, uint8_t *buffer, uint32_t size);
    
    // Register Access
    uint32_t RegRead32(uint32_t offset);
    void RegWrite32(uint32_t offset, uint32_t value);
    uint16_t RegRead16(uint32_t offset);
    void RegWrite16(uint32_t offset, uint16_t value);
    uint8_t RegRead8(uint32_t offset);
    void RegWrite8(uint32_t offset, uint8_t value);
};
