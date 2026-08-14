#pragma once
#include <cstdint>
#define IA32_EFER_MSR 0xC0000080
#define IA32_STAR_MSR 0xC0000081
#define IA32_LSTAR_MSR 0xC0000082
#define IA32_FMASK_MSR 0xC0000084
extern "C" void asm_syscall_entry();
void init_syscall();
static inline int64_t user_syscall(uint64_t sys_num, uint64_t a1 = 0, uint64_t a2 = 0, uint64_t a3 = 0, uint64_t a4 = 0,
                                   uint64_t a5 = 0);
extern "C" int64_t sys_open(const char *path, int flags);
extern "C" int64_t sys_read(int fd, uint8_t *buffer, std::size_t count);
extern "C" int64_t sys_write(int fd, uint8_t *buffer, std::size_t count);
extern "C" int64_t sys_close(int fd);
