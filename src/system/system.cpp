#include "system/system.h" // This now brings in all STBI_ configuration macros
#include "drivers/vga.h"
#include "fonts/roboto_regular.h"
#include "stb_truetype.h"
#include "fonts/bbhbogle_font.h"
#include "fs/lwext4_adapter.h" // For ext4_blockdev
#include <ext4.h>             // For ext4_device_register, ext4_mount, EOK
#include "print/print.h"      // For kprintf
#include "memory/kmalloc.h"   // For read_file_to_memory, kfree

// This MUST be defined here, and ONLY here, before including stb_image.h
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h" // The implementation is pulled in here. 

System::System() {
  // Clear the screen to black
  this->framebuffer = vga_get_framebuffer();
  vga_clear_buffer(this->framebuffer, 0x000000);
}

System::~System() {}

void System::Initialize() {
  vga_clear_screen(0x000000); // Clear screen to black

  // Initialize and mount lwext4 filesystem
  struct ext4_blockdev* bdev = fs::get_lwext4_blockdev();
  int rc = ext4_device_register(bdev, "ext4_fs");
  if (rc != EOK) {
      kprintf("Error registering ext4 device: %d\n", rc);
      // Handle error, maybe panic or halt
  }

  rc = ext4_mount("ext4_fs", "/mp/", false); // Mount as read-write
  if (rc != EOK) {
      kprintf("Error mounting ext4 filesystem: %d\n", rc);
      // Handle error, maybe panic or halt
  } else {
      kprintf("Ext4 filesystem mounted at /mp/\n");
  }

  // Load fonts
  const unsigned char* roboto_regular_ttf = bbhbogle_font;
  stbtt_fontinfo font;
  int offset = stbtt_GetFontOffsetForIndex(roboto_regular_ttf, 0);
  stbtt_InitFont(&font, roboto_regular_ttf, offset);
  // Draw "Hello, World!" at position (50, 50) in white color with size 24
  System::DrawText(50, 50, "Hello, World!", 0xFFFFFF, 50.0f, &font);
}

void System::Shutdown() {}

void System::Run() {}

void System::DrawText( int x, int y, const char* text, uint32_t color, float size, stbtt_fontinfo* font){
    int cursor_x = x;
    int cursor_y = y;
    float scale = stbtt_ScaleForPixelHeight(font, size);
    while (*text) {
        char c = *text++;
        int ax; // advance width
        int lsb; // left side bearing
        stbtt_GetCodepointHMetrics(font, c, &ax, &lsb);

        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(font, c, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

        int bitmap_width, bitmap_height, x_off, y_off;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(font, scale, scale, c, &bitmap_width, &bitmap_height, &x_off, &y_off);
        for (int by = 0; by < bitmap_height; by++) {
            for (int bx = 0; bx < bitmap_width; bx++) {
                unsigned char opacity = bitmap[by * bitmap_width + bx];
                if (opacity > 0) {
                    uint32_t final_color = 0;
                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;

                    r = (r * opacity) / 255;
                    g = (g * opacity) / 255;
                    b = (b * opacity) / 255;

                    final_color = (r << 16) | (g << 8) | b;

                    vga_draw_pixel(this->framebuffer, cursor_x + bx + x_off, cursor_y + by + y_off, final_color);
                }
            }
        }

        stbtt_FreeBitmap(bitmap, nullptr);

        cursor_x += (ax * scale); // Move cursor forward
    }
}

void System::DrawRectangle(int x, int y, int width, int height, uint32_t color) {
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      vga_draw_pixel(this->framebuffer, x + j, y + i, color);
    }
  }
}

void System::ClearScreen(uint32_t color) {
  vga_clear_buffer(this->framebuffer, color);
}

void System::RenderImage(int x, int y, const unsigned char* image_data, int img_width, int img_height) {
  for (int i = 0; i < img_height; i++) {
    for (int j = 0; j < img_width; j++) {
      int index = (i * img_width + j) * 4; // Assuming RGBA format
      uint8_t r = image_data[index];
      uint8_t g = image_data[index + 1];
      uint8_t b = image_data[index + 2];
      uint32_t color = (r << 16) | (g << 8) | b;
      vga_draw_pixel(this->framebuffer, x + j, y + i, color);
    }
  }
}

void System::LoadImage(const char* filename, int x, int y, int& out_width, int& out_height) {
    int channels;
    size_t file_size;

    // Use the new helper to read the file into memory
    // Assume filename already contains the mount point prefix, e.g., "/mp/logo.bmp"
    unsigned char* file_buffer = read_file_to_memory("/mp/", filename, &file_size);
    if (file_buffer == NULL) {
        kprintf("Failed to read image file: %s\n", filename);
        return;
    }

    // Decode it into a pixel buffer using stbi_load_from_memory
    unsigned char* image_data = stbi_load_from_memory(file_buffer, (int)file_size, &out_width, &out_height, &channels, 4);

    // Free the file buffer as it's no longer needed
    kfree(file_buffer);

    if (image_data) {
        // Render the image
        System::RenderImage(x, y, image_data, out_width, out_height);
        // Free the image data after rendering
        stbi_image_free(image_data);
    } else {
        kprintf("Failed to decode image: %s\n", filename);
    }
}

void System::ProcessInput() {}

void System::HandleEvents() {}

void System::Update() {}

void System::Render() {}
