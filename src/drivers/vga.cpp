#include "common/boot_info.h"
#include "drivers/font_data.h"
#include "drivers/vga.h"
#include "drivers/sse.h"
#include "print/print.h"
#include <cstdint>
#include <cstring>

// VBE Mode Info Block is at 0x5200
#define VBE_MODE_INFO 0x5200

#include "mem/vmm.h"

// Public function to get the hardware framebuffer address
// Supports both legacy VBE (32-bit) and EFI (64-bit) framebuffer addresses
uint32_t *vga_get_framebuffer() {
    // Check if EFI framebuffer is set (non-zero)
    if (g_efi_boot_info.fb_addr != 0) {
        return (uint32_t *)phys_to_kvirt(g_efi_boot_info.fb_addr);
    }
    // Fall back to legacy VBE mode info
    return (uint32_t *)phys_to_kvirt(*(uint32_t *)(VBE_MODE_INFO + 40));
}

static uint16_t get_pitch() {
    if (g_efi_boot_info.fb_addr != 0) {
        if (g_efi_boot_info.pitch != 0) {
            return g_efi_boot_info.pitch * 4; // Convert pixels to bytes
        }
        // Fallback: assume packed pixels if pitch is not provided
        if (g_efi_boot_info.width != 0) {
            return g_efi_boot_info.width * 4;
        }
    }
    return *(uint16_t *)(VBE_MODE_INFO + 16);
}

static uint16_t get_width() {
    if (g_efi_boot_info.width != 0) {
        return g_efi_boot_info.width;
    }
    return *(uint16_t *)(VBE_MODE_INFO + 18);
}

static uint16_t get_height() {
    if (g_efi_boot_info.height != 0) {
        return g_efi_boot_info.height;
    }
    return *(uint16_t *)(VBE_MODE_INFO + 20);
}

// Helper to get font data.
// We include the data here to ensure it's available to this translation unit.
// The font_data.h defines 'font16x16_basic' as static const.
// By wrapping it in a function, we hope to fix addressing.
const uint16_t *get_font_glyph(unsigned char c) {
    if (c < 32 || c > 127)
        return font16x16_basic[0];
    return font16x16_basic[c];
}

// --- Buffer-based drawing functions ---

void vga_draw_pixel(uint32_t *buffer, int x, int y, uint32_t color) {
    if (x < 0 || x >= get_width() || y < 0 || y >= get_height())
        return;

    uint16_t pitch = get_pitch();
    uint32_t offset = y * pitch + x * 4;
    uint32_t *pixelPtr = (uint32_t *)((char *)buffer + offset);

    uint8_t alpha = (color >> 24) & 0xFF;
    if (alpha == 0)
        alpha = 255; // Default to opaque for backward compatibility

    if (alpha == 255) {
        *pixelPtr = color;
    } else {
        // Alpha blending
        uint32_t bg = *pixelPtr;
        uint8_t bgR = (bg >> 16) & 0xFF;
        uint8_t bgG = (bg >> 8) & 0xFF;
        uint8_t bgB = (bg) & 0xFF;

        uint8_t fgR = (color >> 16) & 0xFF;
        uint8_t fgG = (color >> 8) & 0xFF;
        uint8_t fgB = (color) & 0xFF;

        uint8_t outR = (alpha * fgR + (255 - alpha) * bgR) / 255;
        uint8_t outG = (alpha * fgG + (255 - alpha) * bgG) / 255;
        uint8_t outB = (alpha * fgB + (255 - alpha) * bgB) / 255;

        *pixelPtr = (outR << 16) | (outG << 8) | outB;
    }
}

void vga_clear_buffer(uint32_t *buffer, uint32_t color) {
    uint16_t height = get_height();
    uint32_t pitch_pixels = get_pitch() / 4;
    uint32_t totalpixels = pitch_pixels * height;
    
    sse_fill_buffer32(buffer, color, totalpixels);
}

void fast_clear_buffer(uint32_t *buffer) { vga_clear_buffer(buffer, 0x000000); }

void vga_draw_rectangle(uint32_t *buffer, int x, int y, int width, int height, uint32_t color) {
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            vga_draw_pixel(buffer, x + col, y + row, color);
        }
    }
}

// Helper function to draw a horizontal line, used by the optimized circle
// function.
static void vga_draw_hline(uint32_t *buffer, int x1, int x2, int y, uint32_t color) {
    if (x1 > x2) {
        int temp = x1;
        x1 = x2;
        x2 = temp;
    }
    for (int x = x1; x <= x2; x++) {
        vga_draw_pixel(buffer, x, y, color);
    }
}

// Optimized algorithm for drawing a filled circle.
void vga_draw_circle(uint32_t *buffer, int x0, int y0, int radius, uint32_t color) {
    if (radius < 0)
        return;
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    while (y >= x) {
        vga_draw_hline(buffer, x0 - x, x0 + x, y0 - y, color);
        vga_draw_hline(buffer, x0 - x, x0 + x, y0 + y, color);
        vga_draw_hline(buffer, x0 - y, x0 + y, y0 - x, color);
        vga_draw_hline(buffer, x0 - y, x0 + y, y0 + x, color);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// --- Old functions updated to use the new buffer-based ones ---

void vga_clear_screen(uint32_t color) { vga_clear_buffer(vga_get_framebuffer(), color); }

void fast_clear() { fast_clear_buffer(vga_get_framebuffer()); }

void vga_draw_char(int x, int y, char c, uint32_t color, int scale) {
    uint32_t *buffer = vga_get_framebuffer();

    // Skip invalid characters
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 126)
        return;

    // Use helper function to get glyph data
    // This avoids global data relocation issues
    const uint16_t *glyph = get_font_glyph(uc);

    // Check if glyph has any data
    bool hasData = false;
    for (int i = 0; i < 16; i++) {
        if (glyph[i] != 0) {
            hasData = true;
            break;
        }
    }

    if (!hasData) {
        // Draw BLUE box for empty/invalid glyph data
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                vga_draw_pixel(buffer, x + j, y + i, 0x0000FF);
            }
        }
        return;
    }

    // Draw character normally
    for (int row = 0; row < 16; row++) {
        uint16_t rowData = glyph[row];
        for (int col = 0; col < 16; col++) {
            if ((rowData >> (15 - col)) & 1) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        vga_draw_pixel(buffer, x + col * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
}

static int console_x = 0;
static int console_y = 0;

void vga_console_putc(char c) {
    if (c == '\n') {
        console_x = 0;
        console_y += 16; // 8 * scale 2
        return;
    }
    if (c == '\b') {
        if (console_x >= 10) { // Approximate width
            console_x -= 10;
            // Erase the character
            vga_draw_rectangle(vga_get_framebuffer(), console_x, console_y, 16, 16, 0x000000);
        } else if (console_y >= 16) {
            console_y -= 16;
            console_x = get_width() - 10;
            // Erase the character
            vga_draw_rectangle(vga_get_framebuffer(), console_x, console_y, 16, 16, 0x000000);
        }
        return;
    }
    vga_draw_char(console_x, console_y, c, 0xFFFFFF, 1);
    console_x += font16x16_width[(int)c] + 1;
    if (console_x >= get_width()) {
        console_x = 0;
        console_y += 16;
    }
}

void vga_draw_string(int x, int y, const char *str, uint32_t color, int scale) {
    int cursor_x = x;
    int cursor_y = y;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cursor_x = x;
            cursor_y += 16 * scale;
            continue;
        }
        vga_draw_char(cursor_x, cursor_y, str[i], color, scale);
        cursor_x += (font16x16_width[(int)str[i]] + 1) * scale;
    }
}

void vga_draw_digit(int x, int y, int digit, uint32_t color, int scale) {
    if (digit < 0 || digit > 9)
        return;

    // 5x7 bitmaps for digits 0-9 (using 3 bits width for simplicity: 0-7)
    // 0x7 = 111, 0x5 = 101, 0x2 = 010, etc.
    // Local array to avoid relocation issues
    uint8_t bitmap[7];

    switch (digit) {
    case 0:
        bitmap[0] = 7;
        bitmap[1] = 5;
        bitmap[2] = 5;
        bitmap[3] = 5;
        bitmap[4] = 5;
        bitmap[5] = 5;
        bitmap[6] = 7;
        break;
    case 1:
        bitmap[0] = 2;
        bitmap[1] = 6;
        bitmap[2] = 2;
        bitmap[3] = 2;
        bitmap[4] = 2;
        bitmap[5] = 2;
        bitmap[6] = 7;
        break;
    case 2:
        bitmap[0] = 7;
        bitmap[1] = 1;
        bitmap[2] = 1;
        bitmap[3] = 7;
        bitmap[4] = 4;
        bitmap[5] = 4;
        bitmap[6] = 7;
        break;
    case 3:
        bitmap[0] = 7;
        bitmap[1] = 1;
        bitmap[2] = 1;
        bitmap[3] = 7;
        bitmap[4] = 1;
        bitmap[5] = 1;
        bitmap[6] = 7;
        break;
    case 4:
        bitmap[0] = 5;
        bitmap[1] = 5;
        bitmap[2] = 5;
        bitmap[3] = 7;
        bitmap[4] = 1;
        bitmap[5] = 1;
        bitmap[6] = 1;
        break;
    case 5:
        bitmap[0] = 7;
        bitmap[1] = 4;
        bitmap[2] = 4;
        bitmap[3] = 7;
        bitmap[4] = 1;
        bitmap[5] = 1;
        bitmap[6] = 7;
        break;
    case 6:
        bitmap[0] = 7;
        bitmap[1] = 4;
        bitmap[2] = 4;
        bitmap[3] = 7;
        bitmap[4] = 5;
        bitmap[5] = 5;
        bitmap[6] = 7;
        break;
    case 7:
        bitmap[0] = 7;
        bitmap[1] = 1;
        bitmap[2] = 1;
        bitmap[3] = 1;
        bitmap[4] = 1;
        bitmap[5] = 1;
        bitmap[6] = 1;
        break;
    case 8:
        bitmap[0] = 7;
        bitmap[1] = 5;
        bitmap[2] = 5;
        bitmap[3] = 7;
        bitmap[4] = 5;
        bitmap[5] = 5;
        bitmap[6] = 7;
        break;
    case 9:
        bitmap[0] = 7;
        bitmap[1] = 5;
        bitmap[2] = 5;
        bitmap[3] = 7;
        bitmap[4] = 1;
        bitmap[5] = 1;
        bitmap[6] = 7;
        break;
    }

    uint32_t *buffer = vga_get_framebuffer();

    for (int row = 0; row < 7; row++) {
        uint8_t rowData = bitmap[row];
        for (int col = 0; col < 3; col++) {
            if ((rowData >> (2 - col)) & 1) {
                // Draw 4x4 pixel blocks for visibility
                for (int sy = 0; sy < 4 * scale; sy++) {
                    for (int sx = 0; sx < 4 * scale; sx++) {
                        vga_draw_pixel(buffer, x + col * 4 * scale + sx, y + row * 4 * scale + sy, color);
                    }
                }
            }
        }
    }
}

// Simple hardcoded 5x7 font for debugging - bypasses all relocation issues
void vga_draw_char_simple(int x, int y, char c, uint32_t color, int scale) {
    uint8_t bitmap[7] = {0};

    // Only handle printable ASCII (space to ~)
    if (c >= '0' && c <= '9') {
        // Reuse digit logic
        switch (c) {
        case '0':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case '1':
            bitmap[0] = 2;
            bitmap[1] = 6;
            bitmap[2] = 2;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 2;
            bitmap[6] = 7;
            break;
        case '2':
            bitmap[0] = 7;
            bitmap[1] = 1;
            bitmap[2] = 1;
            bitmap[3] = 7;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 7;
            break;
        case '3':
            bitmap[0] = 7;
            bitmap[1] = 1;
            bitmap[2] = 1;
            bitmap[3] = 7;
            bitmap[4] = 1;
            bitmap[5] = 1;
            bitmap[6] = 7;
            break;
        case '4':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 7;
            bitmap[4] = 1;
            bitmap[5] = 1;
            bitmap[6] = 1;
            break;
        case '5':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 7;
            bitmap[4] = 1;
            bitmap[5] = 1;
            bitmap[6] = 7;
            break;
        case '6':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 7;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case '7':
            bitmap[0] = 7;
            bitmap[1] = 1;
            bitmap[2] = 1;
            bitmap[3] = 1;
            bitmap[4] = 1;
            bitmap[5] = 1;
            bitmap[6] = 1;
            break;
        case '8':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 7;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case '9':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 7;
            bitmap[4] = 1;
            bitmap[5] = 1;
            bitmap[6] = 7;
            break;
        }
    } else if (c >= 'A' && c <= 'Z') {
        switch (c) {
        case 'A':
            bitmap[0] = 2;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 7;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'B':
            bitmap[0] = 6;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 6;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 6;
            break;
        case 'C':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 4;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 7;
            break;
        case 'D':
            bitmap[0] = 6;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 6;
            break;
        case 'E':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 6;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 7;
            break;
        case 'F':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 6;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 4;
            break;
        case 'G':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 4;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case 'H':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 7;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'I':
            bitmap[0] = 7;
            bitmap[1] = 2;
            bitmap[2] = 2;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 2;
            bitmap[6] = 7;
            break;
        case 'J':
            bitmap[0] = 7;
            bitmap[1] = 1;
            bitmap[2] = 1;
            bitmap[3] = 1;
            bitmap[4] = 1;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case 'K':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 6;
            bitmap[3] = 4;
            bitmap[4] = 6;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'L':
            bitmap[0] = 4;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 4;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 7;
            break;
        case 'M':
            bitmap[0] = 5;
            bitmap[1] = 7;
            bitmap[2] = 7;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'N':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 7;
            bitmap[3] = 7;
            bitmap[4] = 7;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'O':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case 'P':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 7;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 4;
            break;
        case 'Q':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 7;
            bitmap[6] = 3;
            break;
        case 'R':
            bitmap[0] = 7;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 6;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'S':
            bitmap[0] = 7;
            bitmap[1] = 4;
            bitmap[2] = 4;
            bitmap[3] = 7;
            bitmap[4] = 1;
            bitmap[5] = 1;
            bitmap[6] = 7;
            break;
        case 'T':
            bitmap[0] = 7;
            bitmap[1] = 2;
            bitmap[2] = 2;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 2;
            bitmap[6] = 2;
            break;
        case 'U':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 5;
            bitmap[6] = 7;
            break;
        case 'V':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 5;
            bitmap[5] = 2;
            bitmap[6] = 2;
            break;
        case 'W':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 5;
            bitmap[4] = 7;
            bitmap[5] = 7;
            bitmap[6] = 5;
            break;
        case 'X':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 2;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 5;
            bitmap[6] = 5;
            break;
        case 'Y':
            bitmap[0] = 5;
            bitmap[1] = 5;
            bitmap[2] = 5;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 2;
            bitmap[6] = 2;
            break;
        case 'Z':
            bitmap[0] = 7;
            bitmap[1] = 1;
            bitmap[2] = 1;
            bitmap[3] = 2;
            bitmap[4] = 4;
            bitmap[5] = 4;
            bitmap[6] = 7;
            break;
        }
    } else if (c >= 'a' && c <= 'z') {
        // Lowercase - use uppercase bitmaps
        return vga_draw_char_simple(x, y, c - 32, color, scale);
    } else {
        // Special chars
        switch (c) {
        case ' ':
            return; // Space - just advance
        case '/':
            bitmap[0] = 1;
            bitmap[1] = 1;
            bitmap[2] = 2;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 4;
            bitmap[6] = 4;
            break;
        case '.':
            bitmap[0] = 0;
            bitmap[1] = 0;
            bitmap[2] = 0;
            bitmap[3] = 0;
            bitmap[4] = 0;
            bitmap[5] = 0;
            bitmap[6] = 2;
            break;
        case ':':
            bitmap[0] = 0;
            bitmap[1] = 2;
            bitmap[2] = 0;
            bitmap[3] = 0;
            bitmap[4] = 0;
            bitmap[5] = 2;
            bitmap[6] = 0;
            break;
        case '-':
            bitmap[0] = 0;
            bitmap[1] = 0;
            bitmap[2] = 0;
            bitmap[3] = 7;
            bitmap[4] = 0;
            bitmap[5] = 0;
            bitmap[6] = 0;
            break;
        case '_':
            bitmap[0] = 0;
            bitmap[1] = 0;
            bitmap[2] = 0;
            bitmap[3] = 0;
            bitmap[4] = 0;
            bitmap[5] = 0;
            bitmap[6] = 7;
            break;
        case '!':
            bitmap[0] = 2;
            bitmap[1] = 2;
            bitmap[2] = 2;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 0;
            bitmap[6] = 2;
            break;
        case '?':
            bitmap[0] = 6;
            bitmap[1] = 1;
            bitmap[2] = 1;
            bitmap[3] = 2;
            bitmap[4] = 2;
            bitmap[5] = 0;
            bitmap[6] = 2;
            break;
        case '=':
            bitmap[0] = 0;
            bitmap[1] = 7;
            bitmap[2] = 0;
            bitmap[3] = 7;
            bitmap[4] = 0;
            bitmap[5] = 0;
            bitmap[6] = 0;
            break;
        default:
            return; // Unknown char
        }
    }

    uint32_t *buffer = vga_get_framebuffer();
    for (int row = 0; row < 7; row++) {
        uint8_t rowData = bitmap[row];
        for (int col = 0; col < 3; col++) {
            if ((rowData >> (2 - col)) & 1) {
                for (int sy = 0; sy < 2 * scale; sy++) {
                    for (int sx = 0; sx < 2 * scale; sx++) {
                        vga_draw_pixel(buffer, x + col * 2 * scale + sx, y + row * 2 * scale + sy, color);
                    }
                }
            }
        }
    }
}

void vga_draw_string_simple(int x, int y, const char *str, uint32_t color, int scale) {
    int cursor_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cursor_x = x;
            y += 16 * scale;
            continue;
        }
        vga_draw_char_simple(cursor_x, y, str[i], color, scale);
        cursor_x += 8 * scale; // Fixed width
    }
}
