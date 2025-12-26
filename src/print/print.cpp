#include "print/print.h"
#include "drivers/vga.h"
#include <stdarg.h>
#include <stdint.h>

static int offset = 0;

void init_serial() {
  outb(0x3f8 + 1, 0x00); // Disable all interrupts
  outb(0x3f8 + 3, 0x80); // Enable DLAB (set baud rate divisor)
  outb(0x3f8 + 0, 0x01); // Set divisor to 1 (lo byte) 115200 baud
  outb(0x3f8 + 1, 0x00); //                  (hi byte)
  outb(0x3f8 + 3, 0x03); // 8 bits, no parity, one stop bit
  outb(0x3f8 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
  outb(0x3f8 + 4, 0x0B); // IRQs enabled, RTS/DSR set
  outb(0x3f8 + 1, 0x01); // Re-enable interrupts
}


int is_transmit_empty() { return inb(0x3f8 + 5) & 0x20; }

void write_serial(char a) {
  while (is_transmit_empty() == 0);
  outb(0x3f8, a);
}


void k_putchar(char c) {
  write_serial(c);
  // vga_console_putc(c);
}

void print(const char *str) {
  static int initialized = 0;
  if (!initialized) {
    init_serial();
    offset = 0; // Explicitly initialize offset to 0 at runtime
    initialized = 1;
  }
  for (int i = 0; str[i] != '\0'; i++) {
    k_putchar(str[i]);
  }
}

void print_dec(int num) {
  if (num == 0) {
    k_putchar('0');
    return;
  }
  if (num < 0) {
    k_putchar('-');
    num = -num;
  }
  char buffer[12];
  int i = 0;
  while (num > 0) {
    buffer[i++] = (num % 10) + '0';
    num /= 10;
  }
  while (--i >= 0) {
    k_putchar(buffer[i]);
  }
}

void print_hex(unsigned int num) {
  const char *hex = "0123456789ABCDEF";
  k_putchar('0');
  k_putchar('x');
  for (int i = 28; i >= 0; i -= 4) {
    k_putchar(hex[(num >> i) & 0xF]);
  }
}

void vkprintf(const char *fmt, va_list args) {
  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      i++;
      if (fmt[i] == 'd') {
        int val = va_arg(args, int);
        print_dec(val);
      } else if (fmt[i] == 's') {
        const char *s = va_arg(args, const char *);
        print(s);
      } else if (fmt[i] == 'c') {
        int val = va_arg(args, int);
        k_putchar((char)val);
      } else if (fmt[i] == 'x') {
        int val = va_arg(args, int);
        print_hex(val);
      } else {
        k_putchar(fmt[i]);
      }
    } else {
      k_putchar(fmt[i]);
    }
  }
}

void kprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vkprintf(fmt, args);
  va_end(args);
}

void ksprintf(char *buffer, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int buf_idx = 0;
  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      i++;
      if (fmt[i] == 'd') {
        int num = va_arg(args, int);
        if (num == 0) {
          buffer[buf_idx++] = '0';
        } else {
          if (num < 0) {
            buffer[buf_idx++] = '-';
            num = -num;
          }
          char temp[12];
          int t_idx = 0;
          while (num > 0) {
            temp[t_idx++] = (num % 10) + '0';
            num /= 10;
          }
          while (--t_idx >= 0) {
            buffer[buf_idx++] = temp[t_idx];
          }
        }
      } else if (fmt[i] == 's') {
        const char *s = va_arg(args, const char *);
        while (*s) {
          buffer[buf_idx++] = *s++;
        }
      } else if (fmt[i] == 'c') {
        int val = va_arg(args, int);
        buffer[buf_idx++] = (char)val;
      } else {
        buffer[buf_idx++] = fmt[i];
      }
    } else {
      buffer[buf_idx++] = fmt[i];
    }
  }
  buffer[buf_idx] = '\0';
  va_end(args);
}
