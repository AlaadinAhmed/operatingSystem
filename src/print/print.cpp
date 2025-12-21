#include "print/print.h"
#include <cstddef>
#include <stdarg.h>
#include <stdint.h>

static int offset = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

void init_serial() {
   outb(0x3f8 + 1, 0x00);    // Disable all interrupts
   outb(0x3f8 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(0x3f8 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb(0x3f8 + 1, 0x00);    //                  (hi byte)
   outb(0x3f8 + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(0x3f8 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(0x3f8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int is_transmit_empty() {
   return inb(0x3f8 + 5) & 0x20;
}

void write_serial(char a) {
   // while (is_transmit_empty() == 0);
   outb(0x3f8, a);
}

void putchar(char c) {
  write_serial(c);
  char *video_memory = (char *)0xb8000;
  if (c == '\n') {
    offset += 160 - (offset % 160);
    return;
  }
  video_memory[offset] = c;
  video_memory[offset + 1] = 0x07;
  offset += 2;
}

void print(const char *str) {
  static int initialized = 0;
  if (!initialized) {
      init_serial();
      offset = 0; // Explicitly initialize offset to 0 at runtime
      initialized = 1;
  }
  for (int i = 0; str[i] != '\0'; i++) {
    putchar(str[i]);
  }
}

void print_dec(int num) {
  if (num == 0) {
    putchar('0');
    return;
  }
  if (num < 0) {
    putchar('-');
    num = -num;
  }
  char buffer[12];
  int i = 0;
  while (num > 0) {
    buffer[i++] = (num % 10) + '0';
    num /= 10;
  }
  while (--i >= 0) {
    putchar(buffer[i]);
  }
}

void printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      i++;
      if (fmt[i] == 'd') {
        int val = va_arg(args, int);
        print_dec(val);
      } else if (fmt[i] == 's') {
        const char *s = va_arg(args, const char *);
        print(s);
      } else {
        putchar(fmt[i]);
      }
    } else {
      putchar(fmt[i]);
    }
  }
  va_end(args);
}
bool strncmp(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i]) {
      return false;
    }
    if (s1[i] == '\0') {
      return true;
    }
  }
  return true;
}
