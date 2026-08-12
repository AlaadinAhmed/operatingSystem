#include "arch/x86_64/cpu/cpuid.h"
#include "mem/vmm.h"
#include "print.h"
#include "timer/pit.h"
#include "timer/timer.h"
bool check_sse_support() {
    CpuidResult regs;
    native_cpuid(1, &regs);
    bool sse1 = regs.edx & (1U << 25);
    bool sse2 = regs.edx & (1U << 26);
    // bool sse3 = regs.ecx & (1U << 0);
    // bool sse4 = regs.ecx & (1U << 9);
    if ((!sse1) || (!sse2)) {
        return false;
    }
    return true;
}
bool check_apic() {
    CpuidResult regs;
    native_cpuid(1, &regs);
    bool apic = regs.edx & CPUID_FEAT_EDX_APIC;
    return apic;
}
uint64_t g_lapic_base_virt = 0;
void init_simd() {
    uint64_t cr0, cr4;

    // 1. Read CR0 and clear the EM (Emulation) bit, and set the MP (Monitor Coprocessor) bit
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear EM bit (bit 2) -> Tells CPU we have real SIMD hardware
    cr0 |= (1ULL << 1);  // Set MP bit (bit 1)
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    // 2. Read CR4 and enable OSFXSR (FXSAVE/FXRSTOR support) and OSCEIDCPEX (Unmasked Exception Support)
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Set OSFXSR bit (bit 9) -> Enables SSE instructions
    cr4 |= (1ULL << 10); // Set OSXMMEXCPT bit (bit 10) -> Enables SSE numeric exceptions
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
}
uint64_t apic_get_physical_base() {
    uint64_t apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    if (!(apic_base_msr & IA32_APIC_BASE_MSR_ENABLE)) {
        apic_base_msr |= IA32_APIC_BASE_MSR_ENABLE;
        write_msr(IA32_APIC_BASE_MSR, apic_base_msr);
    }
    return apic_base_msr & ~0xFFFUL;
}
uint64_t apic_map_registers() {
    uint64_t lapic_phys = apic_get_physical_base();
    uint64_t lapic_virt = (uint64_t)vmm_map_mmio(lapic_phys, sizeof(lapic_phys), VMM_PRESENT | VMM_WRITE | VMM_PCD);
    return lapic_virt;
}
void apic_write_reg(uint32_t offset, uint32_t val) {
    volatile uint32_t *reg = (volatile uint32_t *)((uintptr_t)g_lapic_base_virt + offset);
    *reg = val;
}

uint32_t apic_read_reg(uint32_t offset) {
    volatile uint32_t *reg = (volatile uint32_t *)((uintptr_t)g_lapic_base_virt + offset);
    return *reg;
}

void apic_software_enable() {
    // Read the current state of the Spurious register
    uint32_t spurious_val = apic_read_reg(LAPIC_SPURIOUS_REG);

    // Set Spurious Vector number to 255 (0xFF) and activate software bit
    spurious_val |= LAPIC_SOFTWARE_ENABLE;
    spurious_val |= 0xFF;

    apic_write_reg(LAPIC_SPURIOUS_REG, spurious_val);
}
void disable_legacy_8259_pic() {
    // Write 0xFF (mask all interrupts) to PIC1 and PIC2 Data Ports
    asm volatile("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}
void apic_init() {
    if (!check_apic()) {
        return;
    }
    // 1. Disable the old 8259 PIC training wheels
    disable_legacy_8259_pic();

    // 2. Map the APIC MMIO registers to virtual addresses
    g_lapic_base_virt = apic_map_registers();

    // 3. Flip the local software switch on the core pipeline
    apic_software_enable();

    // Your APIC is now alive and ready to clear out tasks!
}

void apic_send_eoi() { apic_write_reg(LAPIC_EOI_REG, 0); }
bool check_cpuid_leaf_16() {
    CpuidResult regs;
    native_cpuid(0x0, &regs);
    // EAX from leaf 0 = max supported leaf number.
    // Leaf 0x16 is available if max_leaf >= 0x16.
    return (regs.eax >= 0x16);
}
uint32_t get_bus_freq_mhz() {
    CpuidResult regs;
    native_cpuid(0x16, &regs);

    // CPUID leaf 0x16 returns:
    //   EAX = Processor Base Frequency (MHz)
    //   EBX = Maximum Frequency (MHz)
    //   ECX = Bus/Reference Frequency (MHz)
    // Some emulators (QEMU) may return 0 or garbage in ECX.
    // A valid bus frequency is typically 100, 133, or 200 MHz.
    if (regs.ecx > 0 && regs.ecx <= 500) {
        kprintf("Bus freq from CPUID 0x16: base=%d max=%d bus=%d MHz\n",
                regs.eax, regs.ebx, regs.ecx);
        return regs.ecx;
    }

    // Fallback: Measure TSC frequency using PIT Channel 2 (no IRQ needed,
    // works with interrupts disabled). Channel 2 is wired to the PC speaker
    // gate — its OUT pin status is readable at port 0x61 bit 5.
    uint8_t port61_saved = inb(0x61);
    outb(0x61, (port61_saved & 0xFD) | 0x01); // Gate high, speaker output off
    outb(0x43, 0xB0);                          // Ch2, lo/hi, Mode 0 (one-shot), binary
    uint16_t pit_count = 11931;                 // ~10ms at 1.193182 MHz
    outb(0x42, pit_count & 0xFF);               // Low byte
    outb(0x42, (pit_count >> 8) & 0xFF);        // High byte

    uint64_t tsc_start = rdtsc();
    while (!(inb(0x61) & 0x20)) {               // Poll OUT2 (bit 5) until countdown hits 0
        asm volatile("pause");
    }
    uint64_t tsc_end = rdtsc();

    outb(0x61, port61_saved);                   // Restore port 0x61

    // tsc_delta = cycles in ~10ms → multiply by 100 for cycles/second
    uint64_t tsc_freq = (tsc_end - tsc_start) * 100;

    // Derive bus frequency: bus_freq = tsc_freq / max_non_turbo_ratio
    // MSR_PLATFORM_INFO (0xCE) bits 15:8 = max non-turbo ratio
    uint64_t platform_info = read_msr(0xCE);
    uint32_t ratio = (platform_info >> 8) & 0xFF;

    kprintf("Bus freq fallback: TSC=%ld Hz, MSR_PLATFORM_INFO=0x%lx, ratio=%d\n",
            tsc_freq, platform_info, ratio);

    if (ratio >= 4 && ratio <= 80) {
        uint32_t bus = (uint32_t)(tsc_freq / ratio / 1000000);
        if (bus > 0 && bus <= 500) {
            return bus;
        }
    }

    // Neither CPUID nor MSR gave a valid result (common in QEMU/emulators)
    kprintf("Bus freq: using 100 MHz default (CPUID ECX=%d, ratio=%d)\n",
            regs.ecx, ratio);
    return 100;
}
void prepare_pit(uint16_t divisor) {
    // 0x36 = 00 (Channel 0) | 11 (Lo/Hi Byte) | 011 (Mode 3) | 0 (Binary)
    // Mode 3 is a Square Wave, often used for stable periodic interrupts
    outb(0x43, 0x36);

    // Send low byte
    outb(0x40, (uint8_t)(divisor & 0xFF));

    // Send high byte
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
/*
 * =============================================================================
 * PIT → APIC Timer Transition Path
 * =============================================================================
 *
 * Phase 1: PIT (Legacy Mode) — active during early boot
 *   • PIC remaps IRQ0 → vector 32 (init_pic)
 *   • PIT programmed at 1000 Hz, Mode 2 rate generator (init_pit)
 *   • Each tick: pit_increment_tick() + pic_send_eoi(0)
 *
 * Phase 2: APIC Takeover — happens in main() after VMM is ready
 *   • disable_legacy_8259_pic() masks all PIC lines (0xFF to both)
 *   • Local APIC enabled via IA32_APIC_BASE MSR + software enable
 *   • I/O APIC programmed: IRQ0 → GSI → vector 32 (ioapic_init)
 *   • PIT still generates the signal, but I/O APIC routes it now
 *   • EOI goes to Local APIC (apic_send_eoi), not PIC
 *
 * Phase 3: APIC Timer (Future)
 *   • Use PIT to calibrate APIC timer / TSC frequency:
 *     1. Program PIT for a known 10ms interval (divisor 11931)
 *     2. Read TSC before/after → derive TSC frequency
 *     3. Program APIC LVT Timer (offset 0x320) in periodic mode
 *     4. Mask PIT in I/O APIC → PIT goes silent
 *   • Result: Per-core timers, no shared I/O APIC routing needed
 *
 * calibrate_tsc_frequency() below implements the Phase 3 calibration step.
 * =============================================================================
 */
uint64_t calibrate_tsc_frequency() {
    // Clear the flag before starting the measurement window
    pit_reset_fired();

    // Program PIT for a ~10ms one-shot (1193182 Hz / 11931 ≈ 100 Hz → 10ms)
    prepare_pit(11931);

    // Capture TSC start, then busy-wait for the PIT interrupt
    uint64_t start = rdtsc();

    while (!pit_fired) {
        asm volatile("pause");
    }

    uint64_t end = rdtsc();

    // (end - start) cycles in ~10ms → multiply by 100 for cycles/second
    return (end - start) * 100;
}

void apic_timer_init(uint32_t frequency) {
    if (!check_apic()) {
        kprintf("APIC Timer: APIC not supported/enabled!\n");
        return;
    }

    // 1. Set the divider to 16
    // Write 0x03 to APIC_TMRDIV
    apic_write_reg(APIC_TMRDIV, 0x03);

    // 2. Calibrate using PIT Channel 2
    // Save current port 0x61 state
    uint8_t port61_saved = inb(0x61);
    
    // Gate high (enable PIT Ch2), speaker output off
    outb(0x61, (port61_saved & 0xFD) | 0x01);
    
    // Mode 0 (one-shot), write low/high bytes, binary
    outb(0x43, 0xB0);
    
    // Divisor 11931 (~10ms)
    uint16_t pit_count = 11931;
    outb(0x42, pit_count & 0xFF);        // Low byte
    outb(0x42, (pit_count >> 8) & 0xFF); // High byte

    // Start APIC timer counting down from Max (0xFFFFFFFF)
    apic_write_reg(APIC_TMRINITCNT, 0xFFFFFFFF);

    // Wait for PIT to finish (when bit 5 of port 0x61 becomes 1)
    while (!(inb(0x61) & 0x20)) {
        asm volatile("pause");
    }

    // Read the current count
    uint32_t ticks_after = apic_read_reg(APIC_TMRCURRCNT);

    // Restore port 0x61
    outb(0x61, port61_saved);

    // Calculate ticks elapsed in 10ms
    uint32_t elapsed_ticks = 0xFFFFFFFF - ticks_after;

    // Stop the timer
    apic_write_reg(APIC_TMRINITCNT, 0);

    if (elapsed_ticks == 0) {
        kprintf("APIC Timer: Calibration failed (ticks = 0)!\n");
        return;
    }

    // Calculate ticks per interval for the requested frequency
    // (elapsed_ticks is for 10ms, i.e., 1/100 sec. So elapsed_ticks * 100 is ticks per second.
    // Ticks per interval = ticks per second / frequency)
    uint32_t ticks_per_interval = (elapsed_ticks * 100) / frequency;

    kprintf("APIC Timer: Calibrated. 10ms = %d ticks. Initial count for %d Hz = %d ticks.\n",
            elapsed_ticks, frequency, ticks_per_interval);

    // 3. Configure the APIC Timer LVT register
    // Periodic mode: bit 17 set (0x20000)
    // Vector: 32
    // Not masked: bit 16 clear (0)
    apic_write_reg(APIC_LVT_TMR, 0x20000 | 32);

    // 4. Start the timer by writing the initial count
    apic_write_reg(APIC_TMRINITCNT, ticks_per_interval);
}
