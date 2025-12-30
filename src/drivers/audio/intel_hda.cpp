#include "drivers/audio/intel_hda.h"
#include "print/print.h"
#include "memory/kmalloc.h"

// Static Buffer Definitions
uint32_t __attribute__((aligned(128))) IntelHDA::s_corb[256];
uint64_t __attribute__((aligned(128))) IntelHDA::s_rirb[256];
BDLEntry __attribute__((aligned(4096))) IntelHDA::s_bdl[2];
int16_t __attribute__((aligned(4096))) IntelHDA::s_audio_buffer[4096];
uint64_t __attribute__((aligned(128))) IntelHDA::s_dma_pos[8];

IntelHDA::IntelHDA(uint64_t base_addr) : m_base(base_addr) {
    m_output_stream_offset = 0;
}

uint32_t IntelHDA::RegRead32(uint32_t offset) {
    return *(volatile uint32_t*)(m_base + offset);
}

void IntelHDA::RegWrite32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(m_base + offset) = value;
}

uint16_t IntelHDA::RegRead16(uint32_t offset) {
    return *(volatile uint16_t*)(m_base + offset);
}

void IntelHDA::RegWrite16(uint32_t offset, uint16_t value) {
    *(volatile uint16_t*)(m_base + offset) = value;
}

uint8_t IntelHDA::RegRead8(uint32_t offset) {
    return *(volatile uint8_t*)(m_base + offset);
}

void IntelHDA::RegWrite8(uint32_t offset, uint8_t value) {
    *(volatile uint8_t*)(m_base + offset) = value;
}

void IntelHDA::ResetController() {
    // 1. Enter Reset (CRST = 0)
    uint32_t gctl = RegRead32(HDA_REG_GCTL);
    RegWrite32(HDA_REG_GCTL, gctl & ~1);
    while (RegRead32(HDA_REG_GCTL) & 1);

    // 2. Exit Reset (CRST = 1)
    RegWrite32(HDA_REG_GCTL, 1);
    while (!(RegRead32(HDA_REG_GCTL) & 1));
    
    // 3. Wait for Codecs
    int timeout = 10000;
    while (RegRead16(HDA_REG_STATESTS) == 0 && --timeout > 0);
    
    kprintf("HDA Controller Reset Complete. Codecs: %x\n", RegRead16(HDA_REG_STATESTS));
}

void IntelHDA::InitCORBRIRB() {
    // Stop CORB/RIRB
    RegWrite8(HDA_REG_CORBCTL, 0);
    RegWrite8(HDA_REG_RIRBCTL, 0);
    while (RegRead8(HDA_REG_CORBCTL) & 2);
    while (RegRead8(HDA_REG_RIRBCTL) & 2);

    // Setup CORB
    uint64_t corb_phys = (uint64_t)s_corb;
    RegWrite32(HDA_REG_CORBLBASE, (uint32_t)corb_phys);
    RegWrite32(HDA_REG_CORBUBASE, (uint32_t)(corb_phys >> 32));
    RegWrite8(HDA_REG_CORBSIZE, 0x02); // 256 entries
    RegWrite16(HDA_REG_CORBWP, 0);
    
    // Setup RIRB
    uint64_t rirb_phys = (uint64_t)s_rirb;
    RegWrite32(HDA_REG_RIRBLBASE, (uint32_t)rirb_phys);
    RegWrite32(HDA_REG_RIRBUBASE, (uint32_t)(rirb_phys >> 32));
    RegWrite8(HDA_REG_RIRBSIZE, 0x02); // 256 entries
    RegWrite16(HDA_REG_RIRBWP, 0x8000); // Reset write pointer
    RegWrite16(HDA_REG_RINTCNT, 1);

    // Start CORB/RIRB
    RegWrite8(HDA_REG_CORBCTL, 2);
    RegWrite8(HDA_REG_RIRBCTL, 2);
    
    kprintf("CORB/RIRB Initialized.\n");
}

void IntelHDA::SendVerb(uint8_t codec, uint16_t node, uint32_t payload) {
    // Use Immediate Command Interface for simplicity
    uint32_t verb = (codec << 28) | (node << 20) | payload;
    
    while (RegRead16(HDA_REG_ICS) & 1); // Wait for busy
    RegWrite32(HDA_REG_ICO, verb);
    RegWrite16(HDA_REG_ICS, 1); // Execute
    while (RegRead16(HDA_REG_ICS) & 1); // Wait for busy
    
    // Check for valid result
    if (RegRead16(HDA_REG_ICS) & 2) {
        // Result valid
        // uint32_t result = RegRead32(HDA_REG_ICI);
    }
}

void IntelHDA::Initialize() {
    ResetController();
    InitCORBRIRB();
    
    // Check 64-bit support
    uint16_t gcap = RegRead16(HDA_REG_GCAP);
    if (!(gcap & 1)) {
        kprintf("Warning: HDA Controller does not support 64-bit addressing!\n");
    }

    // Setup Traffic Class Select (TCSEL)
    // Some controllers need this to be set to allow traffic.
    // Offset 0x09 (1 byte) in GCTL area? No, TCSEL is usually at offset 0x44 in PCI config space?
    // Wait, HDA Spec says TCSEL is not in memory mapped registers, it's in PCI Config Space?
    // Let's check the spec.
    // Actually, for ICH6, TCSEL is at offset 0x44 in PCI Config Space.
    // But we don't have easy access to PCI config space here without the device struct.
    // However, QEMU usually defaults to TC0.
    
    // Setup DMA Position Buffer
    uint64_t dma_pos_phys = (uint64_t)s_dma_pos;
    RegWrite32(HDA_REG_DPLBASE, (uint32_t)(dma_pos_phys & 0xFFFFFF80) | 1); // Enable bit 0
    RegWrite32(HDA_REG_DPUBASE, (uint32_t)(dma_pos_phys >> 32));

    // Find Output Stream Offset
    uint16_t num_iss = (gcap >> 8) & 0xF;
    m_output_stream_offset = 0x80 + (num_iss * 0x20);
    
    kprintf("HDA Output Stream Offset: 0x%x\n", m_output_stream_offset);
}

void IntelHDA::PlayTestSound() {
    // Generate audio data FIRST and flush cache BEFORE touching HDA
    for (int i = 0; i < 4096; i++) {
        s_audio_buffer[i] = (i % 100) * 200 - 10000; // Sawtooth wave
    }

    // Setup BDL entries BEFORE touching HDA registers
    uint64_t buf_phys = (uint64_t)s_audio_buffer;
    s_bdl[0].address = buf_phys;
    s_bdl[0].length = 4096;
    s_bdl[0].flags = 0;
    
    s_bdl[1].address = buf_phys + 4096;
    s_bdl[1].length = 4096;
    s_bdl[1].flags = 1; // IOC

    // CRITICAL: Flush CPU cache BEFORE HDA can see the data
    __asm__ volatile("wbinvd");
    __asm__ volatile("mfence"); // Memory fence for ordering

    uint64_t bdl_phys = (uint64_t)s_bdl;
    kprintf("BDL Phys: 0x%lx, Buf Phys: 0x%lx\n", bdl_phys, buf_phys);
    
    uint32_t sd_offset = m_output_stream_offset;

    // Read current CTL (3 bytes: offset+0, +1, +2)
    // Offset +3 is STS (status)
    volatile uint8_t* ctl0 = (volatile uint8_t*)(m_base + sd_offset + 0); // Bits 7:0: SRST, RUN
    volatile uint8_t* ctl2 = (volatile uint8_t*)(m_base + sd_offset + 2); // Bits 23:16 (Stream ID)
    volatile uint8_t* sts = (volatile uint8_t*)(m_base + sd_offset + 3);

    // ============================================================
    // QEMU WORKAROUND: Stream Reset with Timeout
    // QEMU clears the reset bit immediately after it's set, so we
    // use a timeout instead of waiting indefinitely.
    // ============================================================
    
    // Step 1: Stop the stream and enter reset
    *ctl0 = 0; // Stop RUN, Clear SRST
    for (volatile int i = 0; i < 10000; i++); // Small delay
    
    // Step 2: Set SRST (Enter Reset)
    *ctl0 = 1;
    
    // Step 3: Wait for SRST to be set (with timeout - QEMU workaround)
    int timeout = 10000;
    while (!(*ctl0 & 1) && --timeout > 0);
    // Don't fail if timeout - QEMU may have already cleared it
    
    // Step 4: Clear Status
    *sts = 0xFF;

    // Step 5: Setup Registers (while in Reset, or immediately after)
    RegWrite32(sd_offset + HDA_SD_BDLPL, (uint32_t)bdl_phys);
    RegWrite32(sd_offset + HDA_SD_BDLPU, (uint32_t)(bdl_phys >> 32));
    RegWrite16(sd_offset + HDA_SD_LVI, 1); // Last Valid Index = 1
    RegWrite32(sd_offset + HDA_SD_CBL, 8192); // Total buffer length
    RegWrite16(sd_offset + HDA_SD_FMT, 0x4011); // 48kHz, 16-bit, Stereo

    // Set Stream ID 1 (bits 23:20 -> ctl2 bits 7:4)
    *ctl2 = (1 << 4); // Stream ID 1

    // Step 6: Clear SRST (Exit Reset)
    *ctl0 = 0;
    
    // Step 7: Wait for SRST to be clear (with timeout)
    timeout = 10000;
    while ((*ctl0 & 1) && --timeout > 0);

    // Step 8: Unmute Codec
    SendVerb(0, 0x2, 0x70500); // Set Power State D0
    SendVerb(0, 0x2, 0xB07F);  // Unmute DAC
    SendVerb(0, 0x2, 0x70610); // Set Converter Stream 1, Channel 0
    SendVerb(0, 0x4, 0xB07F);  // Unmute Pin
    SendVerb(0, 0x4, 0x70740); // Enable Output

    // Step 9: Run (set RUN bit)
    kprintf("Starting Playback...\n");
    *ctl0 = 2; // Set RUN only (SRST must be 0)

    // Monitor
    for (int i = 0; i < 20; i++) {
        uint32_t pos = RegRead32(sd_offset + HDA_SD_LPIB);
        uint8_t status = *sts;
        kprintf("Pos: %d, STS: %x, CTL0: %x\n", pos, status, *ctl0);
        for(volatile int j=0; j<1000000; j++);
    }
}

void IntelHDA::StartStream(uint16_t format) {
    uint32_t sd_offset = m_output_stream_offset;
    
    // Setup BDL entries
    uint64_t buf_phys = (uint64_t)s_audio_buffer;
    s_bdl[0].address = buf_phys;
    s_bdl[0].length = 4096; // 2048 samples * 2 bytes
    s_bdl[0].flags = 1; // IOC - Interrupt on Completion
    
    s_bdl[1].address = buf_phys + 4096;
    s_bdl[1].length = 4096;
    s_bdl[1].flags = 1; // IOC
    
    // Flush cache
    __asm__ volatile("wbinvd");
    __asm__ volatile("mfence"); // Memory fence for ordering

    uint64_t bdl_phys = (uint64_t)s_bdl;

    volatile uint8_t* ctl0 = (volatile uint8_t*)(m_base + sd_offset + 0); // Bits 7:0: SRST, RUN
    volatile uint8_t* ctl2 = (volatile uint8_t*)(m_base + sd_offset + 2); // Bits 23:16 (Stream ID)
    volatile uint8_t* sts = (volatile uint8_t*)(m_base + sd_offset + 3);

    // Step 1: Stop the stream and enter reset
    *ctl0 = 0; // Stop RUN, Clear SRST
    for (volatile int i = 0; i < 10000; i++); // Small delay
    
    // Step 2: Set SRST (Enter Reset)
    *ctl0 = 1;
    
    // Step 3: Wait for SRST to be set (with timeout - QEMU workaround)
    int timeout = 10000;
    while (!(*ctl0 & 1) && --timeout > 0);
    
    // Step 4: Clear Status
    *sts = 0x1C; // Clear status

    // Step 5: Setup Registers
    RegWrite32(sd_offset + HDA_SD_BDLPL, (uint32_t)bdl_phys);
    RegWrite32(sd_offset + HDA_SD_BDLPU, (uint32_t)(bdl_phys >> 32));
    RegWrite16(sd_offset + HDA_SD_LVI, 1); // Last Valid Index = 1
    RegWrite32(sd_offset + HDA_SD_CBL, 8192); // Total buffer length
    RegWrite16(sd_offset + HDA_SD_FMT, format); // Use provided format
    *ctl2 = (1 << 4); // Stream ID 1

    // Step 6: Clear SRST (Exit Reset)
    *ctl0 = 0;
    
    // Step 7: Wait for SRST to be clear (with timeout)
    timeout = 10000;
    while ((*ctl0 & 1) && --timeout > 0);
    
    // Unmute (Assuming already done in Init or PlayTestSound, but good to ensure)
    SendVerb(0, 0x2, 0x70500); // D0
    SendVerb(0, 0x2, 0xB07F);  // Unmute DAC
    SendVerb(0, 0x2, 0x70610); // Stream 1
    SendVerb(0, 0x4, 0xB07F);  // Unmute Pin
    SendVerb(0, 0x4, 0x70740); // Enable Out
    
    RegWrite8(sd_offset + HDA_SD_CTL, 2); // RUN
}

void IntelHDA::StopStream() {
    uint32_t sd_offset = m_output_stream_offset;
    RegWrite8(sd_offset + HDA_SD_CTL, 0); // Stop
}

uint32_t IntelHDA::GetStreamPos() {
    uint32_t sd_offset = m_output_stream_offset;
    return RegRead32(sd_offset + HDA_SD_LPIB);
}

int16_t* IntelHDA::GetBuffer(int index) {
    if (index == 0) return s_audio_buffer;
    if (index == 1) return s_audio_buffer + 2048; // 2048 samples offset (4096 bytes)
    return nullptr;
}
