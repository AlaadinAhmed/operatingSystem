#include <stddef.h>
#include <stdint.h>
#include "print/print.h"

// External dependencies from utils.cpp
extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);
extern "C" void* calloc(size_t nmemb, size_t size);
extern "C" void* memset(void* s, int c, size_t n);
extern "C" void* memcpy(void* dest, const void* src, size_t n);

// Simple math implementations
static double my_fabs(double x) { return x < 0 ? -x : x; }
static double my_floor(double x) { return (int)x - (x < 0 && x != (int)x); }
static double my_ceil(double x) { return (int)x + (x > 0 && x != (int)x); }
static double my_sqrt(double x) {
    if (x < 0) return 0;
    double r = x;
    if (r == 0) return 0;
    for (int i = 0; i < 10; i++) r = 0.5 * (r + x / r);
    return r;
}

static double my_pow(double base, double exp) {
    if (base == 0) return 0;
    if (exp == 0) return 1;
    if (exp == 1) return base;
    
    // Handle cube root specifically (used in cubic solver)
    if (exp > 0.33 && exp < 0.34) { // approx 1/3
        double x = base;
        double sign = 1.0;
        if (x < 0) { x = -x; sign = -1.0; }
        
        // Newton-Raphson for cbrt
        double r = (x > 1) ? x/3 : x; // Initial guess
        if (r == 0) r = 0.1;
        for(int i=0; i<20; i++) {
            r = (2.0*r + x/(r*r)) / 3.0;
        }
        return r * sign;
    }
    
    // Integer power
    if (exp == (int)exp) {
        double res = 1.0;
        int e = (int)exp;
        if (e < 0) { base = 1.0/base; e = -e; }
        while (e > 0) {
            if (e & 1) res *= base;
            base *= base;
            e >>= 1;
        }
        return res;
    }
    
    return 1.0; // Fallback
}

static double my_fmod(double x, double y) { return x - (int)(x/y) * y; }

#define PI 3.14159265358979323846

static double my_cos(double x) {
    // Reduce to [-pi, pi]
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    
    double xx = x * x;
    // Taylor series: 1 - x^2/2 + x^4/24 - x^6/720 + x^8/40320
    return 1.0 - xx / 2.0 + xx * xx / 24.0 - xx * xx * xx / 720.0 + xx * xx * xx * xx / 40320.0;
}

static double my_acos(double x) {
    // Approximation: acos(x) = pi/2 - asin(x)
    // asin(x) ~= x + x^3/6 + 3x^5/40
    if (x > 1.0) x = 1.0;
    if (x < -1.0) x = -1.0;
    
    double val = x + (x*x*x)/6.0 + (3.0*x*x*x*x*x)/40.0;
    return PI/2.0 - val;
}

static size_t my_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

// STB Truetype Configuration
#define STBTT_malloc(x,u)  ((void)(u),calloc(1,x))
#define STBTT_free(x,u)    ((void)(u),free(x))
#define STBTT_assert(x)    ((void)0)

#define STBTT_strlen(x)    my_strlen(x)
#define STBTT_memcpy       memcpy
#define STBTT_memset       memset

#define STBTT_ifloor(x)    ((int)my_floor(x))
#define STBTT_iceil(x)     ((int)my_ceil(x))
#define STBTT_sqrt(x)      my_sqrt(x)
#define STBTT_pow(x,y)     my_pow(x,y)
#define STBTT_fmod(x,y)    my_fmod(x,y)
#define STBTT_cos(x)       my_cos(x)
#define STBTT_acos(x)      my_acos(x)
#define STBTT_fabs(x)      my_fabs(x)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void init_font() {
    // print("stb_truetype included and linked.\n");
}
