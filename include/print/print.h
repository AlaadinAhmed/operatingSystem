#pragma once
#include <cstdint>
#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
void print(const char *str);
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list args);
void k_putchar(char c);
void ksprintf(char *buffer, const char *fmt, ...);
void print_hex(unsigned int num);
void print_dec(int num);

#ifdef __cplusplus
}
#endif
