#pragma once
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void print(const char *str);
void kprintf(const char *fmt, ...);
void putchar(char c);
void ksprintf(char *buffer, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
