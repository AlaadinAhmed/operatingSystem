#pragma once
#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void print(const char *str);
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list args);
void k_putchar(char c);
void ksprintf(char *buffer, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
