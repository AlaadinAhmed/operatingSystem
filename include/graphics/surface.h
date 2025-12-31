#pragma once
#include <cstdint>

struct Surface {
    uint32_t width;
    uint32_t height;
    uint32_t pitch; // In pixels, not bytes
    uint32_t* pixels;
};
