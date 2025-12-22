#include "system/system.h" // This now brings in all STBI_ configuration macros
#include "drivers/vga.h"
#include "fonts/roboto_regular.h"
#include "stb_truetype.h" // The implementation is pulled in here.
#define STB_TRUETYPE_IMPLEMENTATION
#include "fonts/bbhbogle_font.h"
#include "fs/lwext4_adapter.h" // For ext4_blockdev
#include <ext4.h>             // For ext4_device_register, ext4_mount, EOK
#include "print/print.h"      // For kprintf
#include "memory/kmalloc.h"   // For read_file_to_memory, kfree
#include "fonts/jetbrainsmono_bold.h"

// This MUST be defined here, and ONLY here, before including stb_image.h
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h" // The implementation is pulled in here. 

System::System() : m_robotoFontBuffer(nullptr), m_bbhbogleFontBuffer(nullptr) {
  // Clear the screen to black
  this->framebuffer = vga_get_framebuffer();
  vga_clear_buffer(this->framebuffer, 0x000000);
}

System::~System() {
  if (m_robotoFontBuffer) {
    kfree(m_robotoFontBuffer);
  }
  if (m_bbhbogleFontBuffer) {
    kfree(m_bbhbogleFontBuffer);
  }
}

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

  // Load BBHBogle-Regular.ttf font
  m_bbhbogleFontBuffer = System::LoadFont("BBHBogle-Regular.ttf", &m_bbhbogleFontInfo);
  if (m_bbhbogleFontBuffer) {
    System::DrawText(50, 50, "Hello BBHBogle!", 0xFFFFFF, 50.0f, &m_bbhbogleFontInfo);
  } else {
    kprintf("Failed to load BBHBogle-Regular.ttf\n");
  }

  // Load Roboto-Regular.ttf font
  m_robotoFontBuffer = System::LoadFont("Roboto-Regular.ttf", &m_robotoFontInfo);
  if (m_robotoFontBuffer) {
    System::DrawText(50, 150, "Hello Roboto!", 0xFFFFFF, 50.0f, &m_robotoFontInfo);
  } else {
    kprintf("Failed to load Roboto-Regular.ttf\n");
  }

  // Example of drawing with one of the fonts
  if (m_bbhbogleFontBuffer) {
    System::DrawText(200, 200, "Hello, World!", 0xFFFFFF, 50.0f, &m_bbhbogleFontInfo);
  } else {
    kprintf("BBHBogle font not available for 'Hello, World!'\n");
  }
  
  int img_w, img_h;
  LoadImage("logo.bmp", 100, 100, img_w, img_h);
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

void System::RenderImage(int x, int y, const unsigned char* image_data, int img_width, int img_height, int channels) {
  for (int i = 0; i < img_height; i++) {
    for (int j = 0; j < img_width; j++) {
      int index = (i * img_width + j) * channels; // Use 'channels' for stride
      uint8_t r = 0, g = 0, b = 0;

      if (channels >= 3) {
        // Assume RGB or RGBA. STB_IMAGE loads as RGB if 3 channels, RGBA if 4.
        // It's usually R, G, B order internally once decoded.
        r = image_data[index];
        g = image_data[index + 1];
        b = image_data[index + 2];
      } else if (channels == 1) {
        // Grayscale
        r = g = b = image_data[index];
      }
      
      uint32_t color = (r << 16) | (g << 8) | b;
      vga_draw_pixel(this->framebuffer, x + j, y + i, color);
    }
  }
}

void System::LoadImage(const char* filename, int x, int y, int& out_width, int& out_height) {
    int channels_in_file; // Variable to store actual channels found in file
    size_t file_size;

    unsigned char* file_buffer = read_file_to_memory("/mp/", filename, &file_size);
    if (file_buffer == NULL) {
        kprintf("Failed to read image file: %s\n", filename);
        return;
    }

    // Request STB_IMAGE to output 4 channels (RGBA) for consistency.
    // 'channels_in_file' will store the number of components actually found in the file.
    unsigned char* image_data = stbi_load_from_memory(file_buffer, (int)file_size, &out_width, &out_height, &channels_in_file, 4);

    kfree(file_buffer); // Free the file buffer

    if (image_data) {
        // Render the image, passing 4 as the number of channels in the decoded data (RGBA).
        System::RenderImage(x, y, image_data, out_width, out_height, 4); 
        stbi_image_free(image_data);
    } else {
        kprintf("Failed to decode image: %s\n", filename);
    }
}

unsigned char* System::LoadFont(const char* font_path, stbtt_fontinfo* font_info) {
    size_t file_size;
    unsigned char* font_buffer = read_file_to_memory("/mp/", font_path, &file_size);
    if (font_buffer == NULL) {
        kprintf("Failed to read font file: %s\n", font_path);
        return NULL;
    }

    int offset = stbtt_GetFontOffsetForIndex(font_buffer, 0);
    if (!stbtt_InitFont(font_info, font_buffer, offset)) {
        kprintf("Failed to initialize font: %s\n", font_path);
        kfree(font_buffer); // Free if initialization fails
        return NULL;
    }
    return font_buffer; // Return the buffer, caller is responsible for kfree'ing
}

void System::ProcessInput() {}

void System::HandleEvents() {}

void System::Update() {}

void System::Render() {}
