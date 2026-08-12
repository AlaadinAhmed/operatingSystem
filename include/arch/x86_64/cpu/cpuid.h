#pragma once
#include <cstdint>
#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE (1 << 11)
#define LAPIC_SPURIOUS_REG 0xF0
#define LAPIC_SOFTWARE_ENABLE (1 << 8)
#define LAPIC_EOI_REG 0x0B0
struct CpuidResult {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};
enum CpuidFunction1 {
    CPUID_FEAT_EBX_BRAND_INDEX = 0xFF << 0,
    CPUID_FEAT_EBX_CLFLUSH_LINE_SIZE = 0xFF << 8,
    CPUID_FEAT_EBX_APIC_ID_SPACE = 0xFF << 16,
    CPUID_FEAT_EBX_INITIAL_APIC_ID = 0xFF << 24,

    CPUID_FEAT_ECX_SSE3 = 1 << 0,
    CPUID_FEAT_ECX_PCLMUL = 1 << 1,
    CPUID_FEAT_ECX_DTES64 = 1 << 2,
    CPUID_FEAT_ECX_MONITOR = 1 << 3,
    CPUID_FEAT_ECX_DS_CPL = 1 << 4,
    CPUID_FEAT_ECX_VMX = 1 << 5,
    CPUID_FEAT_ECX_SMX = 1 << 6,
    CPUID_FEAT_ECX_EST = 1 << 7,
    CPUID_FEAT_ECX_TM2 = 1 << 8,
    CPUID_FEAT_ECX_SSSE3 = 1 << 9,
    CPUID_FEAT_ECX_CID = 1 << 10,
    CPUID_FEAT_ECX_SDBG = 1 << 11,
    CPUID_FEAT_ECX_FMA = 1 << 12,
    CPUID_FEAT_ECX_CX16 = 1 << 13,
    CPUID_FEAT_ECX_XTPR = 1 << 14,
    CPUID_FEAT_ECX_PDCM = 1 << 15,
    CPUID_FEAT_ECX_PCID = 1 << 17,
    CPUID_FEAT_ECX_DCA = 1 << 18,
    CPUID_FEAT_ECX_SSE4_1 = 1 << 19,
    CPUID_FEAT_ECX_SSE4_2 = 1 << 20,
    CPUID_FEAT_ECX_X2APIC = 1 << 21,
    CPUID_FEAT_ECX_MOVBE = 1 << 22,
    CPUID_FEAT_ECX_POPCNT = 1 << 23,
    CPUID_FEAT_ECX_TSC = 1 << 24,
    CPUID_FEAT_ECX_AES = 1 << 25,
    CPUID_FEAT_ECX_XSAVE = 1 << 26,
    CPUID_FEAT_ECX_OSXSAVE = 1 << 27,
    CPUID_FEAT_ECX_AVX = 1 << 28,
    CPUID_FEAT_ECX_F16C = 1 << 29,
    CPUID_FEAT_ECX_RDRAND = 1 << 30,
    CPUID_FEAT_ECX_HYPERVISOR = 1 << 31,

    CPUID_FEAT_EDX_FPU = 1 << 0,
    CPUID_FEAT_EDX_VME = 1 << 1,
    CPUID_FEAT_EDX_DE = 1 << 2,
    CPUID_FEAT_EDX_PSE = 1 << 3,
    CPUID_FEAT_EDX_TSC = 1 << 4,
    CPUID_FEAT_EDX_MSR = 1 << 5,
    CPUID_FEAT_EDX_PAE = 1 << 6,
    CPUID_FEAT_EDX_MCE = 1 << 7,
    CPUID_FEAT_EDX_CX8 = 1 << 8,
    CPUID_FEAT_EDX_APIC = 1 << 9,
    CPUID_FEAT_EDX_SEP = 1 << 11,
    CPUID_FEAT_EDX_MTRR = 1 << 12,
    CPUID_FEAT_EDX_PGE = 1 << 13,
    CPUID_FEAT_EDX_MCA = 1 << 14,
    CPUID_FEAT_EDX_CMOV = 1 << 15,
    CPUID_FEAT_EDX_PAT = 1 << 16,
    CPUID_FEAT_EDX_PSE36 = 1 << 17,
    CPUID_FEAT_EDX_PSN = 1 << 18,
    CPUID_FEAT_EDX_CLFLUSH = 1 << 19,
    CPUID_FEAT_EDX_DS = 1 << 21,
    CPUID_FEAT_EDX_ACPI = 1 << 22,
    CPUID_FEAT_EDX_MMX = 1 << 23,
    CPUID_FEAT_EDX_FXSR = 1 << 24,
    CPUID_FEAT_EDX_SSE = 1 << 25,
    CPUID_FEAT_EDX_SSE2 = 1 << 26,
    CPUID_FEAT_EDX_SS = 1 << 27,
    CPUID_FEAT_EDX_HTT = 1 << 28,
    CPUID_FEAT_EDX_TM = 1 << 29,
    CPUID_FEAT_EDX_IA64 = 1 << 30,
    CPUID_FEAT_EDX_PBE = 1 << 31
};
enum APIC {
    APIC_APICID = 0x20,
    APIC_APICVER = 0x30,
    APIC_TASKPRIOR = 0x80,
    APIC_EOI = 0x0B0,
    APIC_LDR = 0x0D0,
    APIC_DFR = 0x0E0,
    APIC_SPURIOUS = 0x0F0,
    APIC_ESR = 0x280,
    APIC_ICRL = 0x300,
    APIC_ICRH = 0x310,
    APIC_LVT_TMR = 0x320,
    APIC_LVT_PERF = 0x340,
    APIC_LVT_LINT0 = 0x350,
    APIC_LVT_LINT1 = 0x360,
    APIC_LVT_ERR = 0x370,
    APIC_TMRINITCNT = 0x380,
    APIC_TMRCURRCNT = 0x390,
    APIC_TMRDIV = 0x3E0,
    APIC_LAST = 0x38F,
    APIC_DISABLE = 0x10000,
    APIC_SW_ENABLE = 0x100,
    APIC_CPUFOCUS = 0x200,
    APIC_NMI = (4 << 8),
    TMR_PERIODIC = 0x20000,
    TMR_BASEDIV = (1 << 20)
};
extern uint64_t g_lapic_base_virt;
inline void native_cpuid(uint32_t code, CpuidResult *res) {
    // We manually preserve RBX/EBX in rbx_backup to prevent compiler optimization glitches.
    // The "=&r" (early-clobber) constraint on rbx_backup guarantees the compiler
    // picks a DIFFERENT register than rbx — without it, GCC could assign rbx_backup
    // to rbx itself, making the save a no-op.
    uint64_t rbx_backup;

    asm volatile("mov %%rbx, %4\n\t" // 1. Save the compiler's RBX register
                 "cpuid\n\t"         // 2. Run raw CPUID (clobbers eax, ebx, ecx, edx)
                 "mov %%ebx, %1\n\t" // 3. Move the hardware answer out of ebx into our variable
                 "mov %4, %%rbx\n\t" // 4. Restore the compiler's original RBX
                 : "=a"(res->eax),   // %0 -> maps to EAX
                   "=r"(res->ebx),   // %1 -> maps to an arbitrary register holding our EBX answer
                   "=c"(res->ecx),   // %2 -> maps to ECX
                   "=d"(res->edx),   // %3 -> maps to EDX
                   "=&r"(rbx_backup) // %4 -> early-clobber: must differ from rbx
                 : "a"(code),        // Input: Load function code into EAX before starting
                   "c"(0)            // Input: Sub-leaf ECX = 0 (safe default for all leaves)
                 : "memory"          // Tell compiler we are modifying memory state
    );
}
inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}
bool check_sse_support();
bool check_apic();
void init_simd();
uint64_t apic_get_physical_base();
uint64_t apic_map_registers();
void disable_legacy_8259_pic();
void apic_init();
void apic_timer_init(uint32_t frequency);
void apic_send_eoi();
bool check_cpuid_leaf_16();
uint32_t get_bus_freq_mhz();
void prepare_pit(uint16_t divisor);
