#include "system/system.h" // This now brings in all STBI_ configuration macros
#include "drivers/vga.h"
#include "stb_truetype.h" // The implementation is pulled in here.
// #define STB_TRUETYPE_IMPLEMENTATION
#include "drivers/mouse/mouse.h"
#include "fs/lwext4_adapter.h" // For ext4_blockdev
#include "memory/kmalloc.h"    // For read_file_to_memory, kfree
#include "print/print.h"       // For kprintf
#include <ext4.h>              // For ext4_device_register, ext4_mount, EOK

// This MUST be defined here, and ONLY here, before including stb_image.h
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h" // The implementation is pulled in here.

System::System() : m_robotoFontBuffer(nullptr), m_bbhbogleFontBuffer(nullptr),
                   m_cursorX(-1), m_cursorY(-1), m_cursorDrawn(false) {
  // Clear the screen to black
  this->framebuffer = vga_get_framebuffer();
  vga_clear_buffer(this->framebuffer, 0x000000);
  
  // Initialize cursor background buffer
  for (int i = 0; i < CURSOR_SIZE * CURSOR_SIZE; i++) {
    m_cursorBackground[i] = 0x000000;
  }
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
  struct ext4_blockdev *bdev = fs::get_lwext4_blockdev();
  int rc = ext4_device_register(bdev, "ext4_fs");
  if (rc != EOK) {
    return; // Failed to register ext4 device
  }

  rc = ext4_mount("ext4_fs", "/mp/", false); // Mount as read-write
  if (rc != EOK) {
    return; // Failed to mount ext4 filesystem
  }

  // Load BBHBogle-Regular.ttf font
  m_bbhbogleFontBuffer =
      System::LoadFont("BBHBogle-Regular.ttf", &m_bbhbogleFontInfo);
  if (m_bbhbogleFontBuffer) {
    System::DrawText(50, 50, "Hello BBHBogle!", 0xFFFFFF, 50.0f,
                     &m_bbhbogleFontInfo);
  }

  // Load Roboto-Regular.ttf font
  m_robotoFontBuffer =
      System::LoadFont("Roboto-Regular.ttf", &m_robotoFontInfo);
  if (m_robotoFontBuffer) {
    System::DrawText(50, 150, "Hello Roboto!", 0xFFFFFF, 50.0f,
                     &m_robotoFontInfo);
  }

  // Load JetBrainsMono-Bold.ttf font
  m_jetBrainsFontBuffer =
      System::LoadFont("JetBrainsMono-Bold.ttf", &m_jetBrainsFontInfo);
  if (m_jetBrainsFontBuffer) {
    System::DrawText(200, 200, "Hello, World!", 0xFFFFFF, 50.0f,
                     &m_jetBrainsFontInfo);
  }

  int img_w, img_h;
  LoadImage("logo.bmp", 100, 100, img_w, img_h);

  // Initialize mouse driver
  mouse_init(1.0f);
}

void System::Shutdown() {}

void System::Run() {
  bool running = true;
  while (running) {
    ProcessInput();
    HandleEvents();
    Update();
    
    // Update mouse state
    MouseState mouse = mouse_update();
    
    // Only update if cursor moved
    if (mouse.x != m_cursorX || mouse.y != m_cursorY) {
      // Restore previous background
      if (m_cursorDrawn) {
        RestoreCursorBackground();
      }
      
      // Save new background and draw cursor
      SaveCursorBackground(mouse.x, mouse.y);
      DrawCursor(mouse.x, mouse.y);
      
      m_cursorX = mouse.x;
      m_cursorY = mouse.y;
      m_cursorDrawn = true;
    }
    
    Render();
  }
}

void System::DrawText(int x, int y, const char *text, uint32_t color,
                      float size, stbtt_fontinfo *font) {
  int cursor_x = x;
  int cursor_y = y;
  float scale = stbtt_ScaleForPixelHeight(font, size);
  // kprintf("DrawText: x=%d y=%d text='%s' scale_x1000=%d\n", x, y, text,
  // (int)(scale * 1000));
  while (*text) {
    char c = *text++;
    int ax;  // advance width
    int lsb; // left side bearing
    stbtt_GetCodepointHMetrics(font, c, &ax, &lsb);

    int c_x1, c_y1, c_x2, c_y2;
    stbtt_GetCodepointBitmapBox(font, c, scale, scale, &c_x1, &c_y1, &c_x2,
                                &c_y2);

    int bitmap_width, bitmap_height, x_off, y_off;
    unsigned char *bitmap = stbtt_GetCodepointBitmap(
        font, scale, scale, c, &bitmap_width, &bitmap_height, &x_off, &y_off);

    // Skip if bitmap is null or invalid
    if (!bitmap || bitmap_width <= 0 || bitmap_height <= 0) {
      cursor_x += (ax * scale); // Still move cursor forward
      continue;
    }

    for (int by = 0; by < bitmap_height; by++) {
      for (int bx = 0; bx < bitmap_width; bx++) {
        unsigned char opacity = bitmap[by * bitmap_width + bx];
        if (opacity > 0) {
          int px = cursor_x + bx + x_off;
          int py = cursor_y + by + y_off;

          // Skip pixels outside expected text region (prevent stray pixels)
          if (px < x || py < 0 || px >= x + 2000 || py >= y + (int)size * 2)
            continue;

          uint8_t r = (color >> 16) & 0xFF;
          uint8_t g = (color >> 8) & 0xFF;
          uint8_t b = color & 0xFF;

          r = (r * opacity) / 255;
          g = (g * opacity) / 255;
          b = (b * opacity) / 255;

          uint32_t final_color = (r << 16) | (g << 8) | b;
          vga_draw_pixel(this->framebuffer, px, py, final_color);
        }
      }
    }

    stbtt_FreeBitmap(bitmap, nullptr);

    cursor_x += (ax * scale); // Move cursor forward
  }
}

void System::DrawRectangle(int x, int y, int width, int height,
                           uint32_t color) {
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      vga_draw_pixel(this->framebuffer, x + j, y + i, color);
    }
  }
}

void System::ClearScreen(uint32_t color) {
  vga_clear_buffer(this->framebuffer, color);
}

void System::RenderImage(int x, int y, const unsigned char *image_data,
                         int img_width, int img_height, int channels) {
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

void System::LoadImage(const char *filename, int x, int y, int &out_width,
                       int &out_height) {
  int channels_in_file; // Variable to store actual channels found in file
  size_t file_size;

  unsigned char *file_buffer =
      read_file_to_memory("/mp/", filename, &file_size);
  if (file_buffer == NULL) {
    kprintf("Failed to read image file: %s\n", filename);
    return;
  }

  // Request STB_IMAGE to output 4 channels (RGBA) for consistency.
  // 'channels_in_file' will store the number of components actually found in
  // the file.
  unsigned char *image_data =
      stbi_load_from_memory(file_buffer, (int)file_size, &out_width,
                            &out_height, &channels_in_file, 4);

  kfree(file_buffer); // Free the file buffer

  if (image_data) {
    // Render the image, passing 4 as the number of channels in the decoded data
    // (RGBA).
    System::RenderImage(x, y, image_data, out_width, out_height, 4);
    stbi_image_free(image_data);
  } else {
    kprintf("Failed to decode image: %s\n", filename);
  }
}

unsigned char *System::LoadFont(const char *font_path,
                                stbtt_fontinfo *font_info) {
  size_t file_size;
  unsigned char *font_buffer =
      read_file_to_memory("/mp/", font_path, &file_size);
  if (font_buffer == NULL) {
    return NULL;
  }

  int offset = stbtt_GetFontOffsetForIndex(font_buffer, 0);
  if (!stbtt_InitFont(font_info, font_buffer, offset)) {
    kfree(font_buffer); // Free if initialization fails
    return NULL;
  }
  return font_buffer; // Return the buffer, caller is responsible for kfree'ing
}

void System::ProcessInput() {}

void System::HandleEvents() {}

void System::SaveCursorBackground(int x, int y) {
  for (int i = 0; i < CURSOR_SIZE; i++) {
    for (int j = 0; j < CURSOR_SIZE; j++) {
      int px = x + j;
      int py = y + i;
      // Read pixel from framebuffer
      uint16_t pitch = *(uint16_t *)(0x5200 + 16);
      uint32_t offset = py * pitch + px * 4;
      m_cursorBackground[i * CURSOR_SIZE + j] = *(uint32_t *)((char *)framebuffer + offset);
    }
  }
}

void System::RestoreCursorBackground() {
  for (int i = 0; i < CURSOR_SIZE; i++) {
    for (int j = 0; j < CURSOR_SIZE; j++) {
      vga_draw_pixel(framebuffer, m_cursorX + j, m_cursorY + i, 
                     m_cursorBackground[i * CURSOR_SIZE + j]);
    }
  }
}

void System::DrawCursor(int x, int y) {
  for (int i = 0; i < CURSOR_SIZE; i++) {
    for (int j = 0; j < CURSOR_SIZE; j++) {
      vga_draw_pixel(this->framebuffer, x + j, y + i, 0xFFFFFF);
    }
  }
}

void System::Update() {}

void System::Render() {}
