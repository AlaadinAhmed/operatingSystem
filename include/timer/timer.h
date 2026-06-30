#pragma once
#include "print/print.h"
#include <stdint.h>

// 1. Keep the TSC reader inline
inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// 2. The FULL body of the template must live here in the header!
template <typename Func, typename... Args> inline void profile_void(const char *name, Func func, Args... args) {
    uint64_t start = rdtsc();

    func(args...);

    uint64_t end = rdtsc();
    kprintf("[PROFILE] %s took %lx cycles\n", name, end - start);
}

// 3. Do the same for your return-value wrapper
template <typename ReturnType, typename Func, typename... Args>
inline ReturnType profile_return(const char *name, Func func, Args... args) {
    uint64_t start = rdtsc();

    ReturnType result = func(args...);

    uint64_t end = rdtsc();
    kprintf("[PROFILE] %s took %lx cycles\n", name, end - start);
    return result;
}
