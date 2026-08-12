#include "cpu/cpuid.h"
#include "sys/syscalls.h"
void init_syscall() {
    uint64_t efer = read_msr(IA32_EFER_MSR);
    write_msr(IA32_EFER_MSR, efer | 0x1);
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x18 << 48);
    write_msr(IA32_STAR_MSR, star);
    write_msr(IA32_LSTAR_MSR, (uint64_t)asm_syscall_entry);
    write_msr(IA32_FMASK_MSR, 0x200);
}
static inline int64_t user_syscall(uint64_t sys_num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t ret;
    register uint64_t r10_val asm("r10") = a4;
    register uint64_t r8_val asm("r8") = a5;

    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(sys_num), "D"(a1), "S"(a2), "d"(a3), "r"(r10_val), "r"(r8_val)
                 : "rcx", "r11", "memory");
    return ret;
}
