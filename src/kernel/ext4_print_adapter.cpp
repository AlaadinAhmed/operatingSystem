#include "print/print.h"
#include <stdarg.h>

// Dummy stdout for lwext4
extern "C" {
    void* stdout = (void*)1;
}

// Dummy fflush for lwext4
extern "C" int fflush(void* stream) {
    (void)stream; // Unused parameter
    return 0;
}

// Redirect printf to vkprintf
extern "C" int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vkprintf(format, args);
    va_end(args);
    return 0; // Or return actual characters printed if we count them
}
