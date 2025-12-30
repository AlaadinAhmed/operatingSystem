#include "system/system.h"
#include "common/boot_info.h"
#include "drivers/audio/audio.h"
#include "drivers/audio/audio.h"
#include "drivers/audio/audio_player.h"
#include "drivers/mouse/mouse.h"
#include "drivers/vga.h"
#include "fs/lwext4_adapter.h"
#include "memory/kmalloc.h"
#include "print/print.h"
#include "stb_truetype.h"
#include <ext4.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- System Class Implementation ---

System::System()
    : m_robotoFontBuffer(nullptr), m_bbhbogleFontBuffer(nullptr), m_cursorX(-1),
      m_cursorY(-1), m_cursorDrawn(false), m_prevLeftButton(false),
      m_clickableRegionCount(0), m_currentEntryCount(0),
      m_filesystemAvailable(false) {
  framebuffer = vga_get_framebuffer();
  vga_clear_buffer(framebuffer, 0x000000);

  for (int i = 0; i < CURSOR_WIDTH * CURSOR_HEIGHT; i++) {
    m_cursorBackground[i] = 0x000000;
  }
  m_currentPath[0] = '\0';
}

System::~System() {
  if (m_robotoFontBuffer)
    kfree(m_robotoFontBuffer);
  if (m_bbhbogleFontBuffer)
    kfree(m_bbhbogleFontBuffer);
  if (m_jetBrainsFontBuffer)
    kfree(m_jetBrainsFontBuffer);
}

bool System::InitFilesystem() { return fs::mount_filesystem("/mp/"); }

void System::InitDrivers() {
  mouse_init(1.0f);
  find_audio_device();
  init_audio();
}

void System::Initialize() {
  kprintf("System: Starting...\n");
  kprintf("System: Screen dimensions: %dx%d, pitch=%d\n", g_efi_boot_info.width,
          g_efi_boot_info.height, g_efi_boot_info.pitch);

  vga_clear_screen(0x1a1a2e); // Dark blue background

  // Draw a test rectangle to verify graphics work
  for (int y = 50; y < 150; y++) {
    for (int x = 50; x < 250; x++) {
      vga_draw_pixel(framebuffer, x, y, 0xFF0000); // Red rectangle
    }
  }

  // Initialize filesystem
  m_filesystemAvailable = InitFilesystem();
  if (!m_filesystemAvailable) {
    kprintf("System: Filesystem not available\n");
    vga_draw_string_simple(100, 200, "FS ERROR", 0xFF0000, 2);
    return;
  }

  // Initialize drivers
  InitDrivers();

  // Load and display logo (with debug logging)
  // int lw, lh;
  // LoadImage("logo.bmp", 300, 100, lw, lh);
  //
  // // Fallback to simple font for now
  // vga_draw_string_simple(100, 400, "MyOS v0.1", 0x00FFFF, 3);
  // vga_draw_string_simple(100, 480, "System Ready", 0x00FF00, 2);
  m_robotoFontBuffer = LoadFont("Roboto-Regular.ttf", &m_robotoFontInfo);
  if (!m_robotoFontBuffer) {
    kprintf("System: Failed to load Roboto font\n");
    return;
  }
  DrawText(100, 400, "MyOS v0.1", 0xFFFFFF, 32.0f, &m_robotoFontInfo);
  
  // Play startup sound
  kprintf("System: Playing startup sound...\n");
  start_audio_file("as_it_was.wav");

  kprintf("System: Ready\n");
}

void System::Shutdown() { kprintf("System: Shutdown\n"); }

void System::Run() {
  while (true) {
    ProcessInput();
    HandleEvents();
    Update();

    MouseState mouse = mouse_update();
    audio_player_tick();
    // Only update cursor if position changed
    bool cursorMoved = (mouse.x != m_cursorX || mouse.y != m_cursorY);

    if (cursorMoved && m_cursorDrawn) {
      RestoreCursorBackground();
      m_cursorDrawn = false;
    }

    bool leftButton = mouse.buttons[0];
    if (leftButton && !m_prevLeftButton) {
      int clickedRegion = GetClickedRegion(mouse.x, mouse.y);
      if (clickedRegion >= 0) {
        OnEntryClicked(m_clickableRegions[clickedRegion].entryIndex);
      }
    }
    m_prevLeftButton = leftButton;

    if (cursorMoved || !m_cursorDrawn) {
      SaveCursorBackground(mouse.x, mouse.y);
      DrawCursor(mouse.x, mouse.y);
      m_cursorX = mouse.x;
      m_cursorY = mouse.y;
      m_cursorDrawn = true;
    }

    Render();
  }
}

// --- Drawing Functions ---

static unsigned char s_fontScratchBuffer[128 * 128];

void System::DrawText(int x, int y, const char *text, uint32_t color,
                      float size, stbtt_fontinfo *font) {
  int cursor_x = x;
  int cursor_y = y;
  float scale = stbtt_ScaleForPixelHeight(font, size);

  while (*text) {
    char c = *text++;
    int ax, lsb;
    stbtt_GetCodepointHMetrics(font, c, &ax, &lsb);

    int c_x1, c_y1, c_x2, c_y2;
    stbtt_GetCodepointBitmapBox(font, c, scale, scale, &c_x1, &c_y1, &c_x2,
                                &c_y2);

    int bitmap_width = c_x2 - c_x1;
    int bitmap_height = c_y2 - c_y1;

    if (bitmap_width <= 0 || bitmap_height <= 0 || bitmap_width > 128 ||
        bitmap_height > 128) {
      cursor_x += (int)(ax * scale);
      continue;
    }

    stbtt_MakeCodepointBitmap(font, s_fontScratchBuffer, bitmap_width,
                              bitmap_height, 128, scale, scale, c);

    for (int by = 0; by < bitmap_height; by++) {
      for (int bx = 0; bx < bitmap_width; bx++) {
        unsigned char opacity = s_fontScratchBuffer[by * 128 + bx];
        if (opacity > 0) {
          int px = cursor_x + bx + c_x1;
          int py = cursor_y + by + c_y1;
          if (px < x || py < 0 || px >= x + 2000 || py >= y + (int)size * 2)
            continue;

          uint8_t r = ((color >> 16) & 0xFF) * opacity / 255;
          uint8_t g = ((color >> 8) & 0xFF) * opacity / 255;
          uint8_t b = (color & 0xFF) * opacity / 255;
          vga_draw_pixel(framebuffer, px, py, (r << 16) | (g << 8) | b);
        }
      }
    }
    cursor_x += (int)(ax * scale);
  }
}

void System::DrawRectangle(int x, int y, int width, int height,
                           uint32_t color) {
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      vga_draw_pixel(framebuffer, x + j, y + i, color);
    }
  }
}

void System::ClearScreen(uint32_t color) {
  vga_clear_buffer(framebuffer, color);
}

void System::RenderImage(int x, int y, const unsigned char *image_data,
                         int img_width, int img_height, int channels) {
  for (int i = 0; i < img_height; i++) {
    for (int j = 0; j < img_width; j++) {
      int index = (i * img_width + j) * channels;
      uint8_t r = 0, g = 0, b = 0;

      if (channels >= 3) {
        r = image_data[index];
        g = image_data[index + 1];
        b = image_data[index + 2];
      } else if (channels == 1) {
        r = g = b = image_data[index];
      }
      vga_draw_pixel(framebuffer, x + j, y + i, (r << 16) | (g << 8) | b);
    }
  }
}

void System::LoadImage(const char *filename, int x, int y, int &out_width,
                       int &out_height) {
  int channels_in_file;
  size_t file_size;
  int out_height_local, out_width_local;
  kprintf("LoadImage: Reading %s...\n", filename);
  unsigned char *file_buffer =
      read_file_to_memory("/mp/", filename, &file_size);
  if (!file_buffer) {
    kprintf("LoadImage: Failed to read file\n");
    return;
  }
  kprintf("LoadImage: Read %d bytes, buffer at %p\n", (int)file_size,
          file_buffer);

  kprintf("LoadImage: Calling stbi_load_from_memory...\n");
  unsigned char *image_data =
      stbi_load_from_memory(file_buffer, (int)file_size, &out_width_local,
                            &out_height_local, &channels_in_file, 4);
  kprintf("LoadImage: stbi returned %p\n", image_data);

  kfree(file_buffer);

  if (image_data) {
    kprintf("LoadImage: Rendering %dx%d image\n", out_width, out_height);
    RenderImage(x, y, image_data, out_width, out_height, 4);
    stbi_image_free(image_data);
    kprintf("LoadImage: Done\n");
  } else {
    kprintf("LoadImage: stbi decode failed\n");
  }
}

unsigned char *System::LoadFont(const char *font_path,
                                stbtt_fontinfo *font_info) {
  size_t file_size;
  unsigned char *font_buffer =
      read_file_to_memory("/mp/", font_path, &file_size);
  if (!font_buffer) {
    kprintf("System: Failed to load font: %s\n", font_path);
    return nullptr;
  }

  int offset = stbtt_GetFontOffsetForIndex(font_buffer, 0);
  if (!stbtt_InitFont(font_info, font_buffer, offset)) {
    kfree(font_buffer);
    kprintf("System: Failed to init font: %s\n", font_path);
    return nullptr;
  }

  kprintf("System: Loaded font: %s\n", font_path);
  return font_buffer;
}

// --- Input & Events ---

void System::ProcessInput() {}
void System::HandleEvents() {}
void System::Update() {}
void System::Render() {}

// --- Cursor ---

void System::SaveCursorBackground(int x, int y) {
  // Use pitch from boot info (vga_get_pitch returns bytes per scanline)
  uint32_t pitch = 0;
  if (g_efi_boot_info.fb_addr != 0) {
    pitch = g_efi_boot_info.pitch * 4; // Convert pixels to bytes for UEFI
  } else {
    pitch = *(uint16_t *)(0x5200 + 16); // VBE pitch in bytes
  }

  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    for (int j = 0; j < CURSOR_WIDTH; j++) {
      int px = x + j, py = y + i;
      uint32_t offset = py * pitch + px * 4;
      m_cursorBackground[i * CURSOR_WIDTH + j] =
          *(uint32_t *)((char *)framebuffer + offset);
    }
  }
}

void System::RestoreCursorBackground() {
  // Get pitch for direct buffer access
  uint32_t pitch = 0;
  if (g_efi_boot_info.fb_addr != 0) {
    pitch = g_efi_boot_info.pitch * 4;
  } else {
    pitch = *(uint16_t *)(0x5200 + 16);
  }

  // Restore scanline by scanline (faster than pixel-by-pixel)
  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    int py = m_cursorY + i;
    char *dest = (char *)framebuffer + py * pitch + m_cursorX * 4;
    memcpy(dest, &m_cursorBackground[i * CURSOR_WIDTH], CURSOR_WIDTH * 4);
  }
}

static const uint8_t s_cursorBitmap[19][12] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0}, {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0}, {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0}, {1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
    {1, 2, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0}, {1, 2, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0},
    {1, 2, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {1, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0},
};

// Pre-rendered cursor buffer
static uint32_t s_cursorRendered[CURSOR_HEIGHT * CURSOR_WIDTH];

void System::DrawCursor(int x, int y) {
  // Get pitch for direct buffer access
  uint32_t pitch = 0;
  if (g_efi_boot_info.fb_addr != 0) {
    pitch = g_efi_boot_info.pitch * 4;
  } else {
    pitch = *(uint16_t *)(0x5200 + 16);
  }

  // Build cursor into local buffer first (blend with saved background)
  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    for (int j = 0; j < CURSOR_WIDTH; j++) {
      uint8_t pixel = s_cursorBitmap[i][j];
      if (pixel == 1) {
        s_cursorRendered[i * CURSOR_WIDTH + j] = 0x000000; // Black outline
      } else if (pixel == 2) {
        s_cursorRendered[i * CURSOR_WIDTH + j] = 0xFFFFFF; // White fill
      } else {
        s_cursorRendered[i * CURSOR_WIDTH + j] =
            m_cursorBackground[i * CURSOR_WIDTH + j];
      }
    }
  }

  // Copy to framebuffer scanline by scanline
  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    int py = y + i;
    char *dest = (char *)framebuffer + py * pitch + x * 4;
    memcpy(dest, &s_cursorRendered[i * CURSOR_WIDTH], CURSOR_WIDTH * 4);
  }
}

// --- Directory Listing ---

static DirectoryEntry s_directoryEntries[MAX_CLICKABLE_REGIONS];

DirectoryEntry *System::GetDirectoryListing(const char *path, int *count) {
  ext4_dir dir;
  *count = 0;

  if (ext4_dir_open(&dir, path) != EOK) {
    return nullptr;
  }

  const ext4_direntry *entry;
  int i = 0;
  while ((entry = ext4_dir_entry_next(&dir)) != nullptr &&
         i < MAX_CLICKABLE_REGIONS) {
    int name_len = entry->name_length;
    if (name_len >= MAX_ENTRY_NAME)
      name_len = MAX_ENTRY_NAME - 1;

    for (int j = 0; j < name_len; j++) {
      s_directoryEntries[i].name[j] = entry->name[j];
    }
    s_directoryEntries[i].name[name_len] = '\0';
    s_directoryEntries[i].type = (DirectoryEntryType)entry->inode_type;
    s_directoryEntries[i].inode = entry->inode;
    i++;
  }

  ext4_dir_close(&dir);
  *count = i;
  return (i > 0) ? s_directoryEntries : nullptr;
}

// --- Click Detection ---

bool System::IsPointInRect(int px, int py, int rx, int ry, int rw, int rh) {
  return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

int System::GetClickedRegion(int x, int y) {
  for (int i = 0; i < m_clickableRegionCount; i++) {
    ClickableRegion &r = m_clickableRegions[i];
    if (IsPointInRect(x, y, r.x, r.y, r.width, r.height)) {
      return i;
    }
  }
  return -1;
}

void System::AddClickableRegion(int x, int y, int width, int height,
                                int entryIndex) {
  if (m_clickableRegionCount < MAX_CLICKABLE_REGIONS) {
    m_clickableRegions[m_clickableRegionCount] = {x, y, width, height,
                                                  entryIndex};
    m_clickableRegionCount++;
  }
}

void System::ClearClickableRegions() { m_clickableRegionCount = 0; }

// --- Directory UI ---

void System::DisplayDirectory(const char *path) {
  ClearClickableRegions();

  int i = 0;
  while (path[i] && i < 511) {
    m_currentPath[i] = path[i];
    i++;
  }
  m_currentPath[i] = '\0';

  GetDirectoryListing(path, &m_currentEntryCount);
  DrawRectangle(50, 260, 500, 450, 0x000000);

  if (m_currentEntryCount == 0) {
    DrawText(50, 280, m_currentPath, 0x00AAFF, 14.0f, &m_robotoFontInfo);
    DrawText(50, 300, "Empty directory", 0x888888, 16.0f, &m_robotoFontInfo);
    return;
  }

  DrawText(50, 280, m_currentPath, 0x00AAFF, 14.0f, &m_robotoFontInfo);

  const int ENTRY_HEIGHT = 22;
  const int ENTRY_WIDTH = 300;
  const int START_Y = 300;

  for (int i = 0; i < m_currentEntryCount; i++) {
    int y = START_Y + i * ENTRY_HEIGHT;
    uint32_t color = 0xFFFFFF;

    if (s_directoryEntries[i].type == ENTRY_DIR) {
      color = 0x00AAFF;
    } else if (s_directoryEntries[i].type == ENTRY_SYMLINK) {
      color = 0x00FF00;
    }

    DrawText(50, y, s_directoryEntries[i].name, color, 16.0f,
             &m_robotoFontInfo);
    AddClickableRegion(50, y - 4, ENTRY_WIDTH, ENTRY_HEIGHT, i);
  }
}

void System::OnEntryClicked(int entryIndex) {
  if (entryIndex < 0 || entryIndex >= m_currentEntryCount)
    return;

  DirectoryEntry &entry = s_directoryEntries[entryIndex];

  if (entry.type == ENTRY_DIR) {
    char newPath[512];
    int i = 0;

    if (entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == '\0') {
      int len = 0;
      while (m_currentPath[len])
        len++;
      if (len > 1 && m_currentPath[len - 1] == '/')
        len--;

      int lastSlash = -1;
      for (int j = len - 1; j >= 0; j--) {
        if (m_currentPath[j] == '/') {
          lastSlash = j;
          break;
        }
      }

      if (lastSlash > 0) {
        for (int j = 0; j <= lastSlash; j++)
          newPath[j] = m_currentPath[j];
        newPath[lastSlash + 1] = '\0';
      } else {
        newPath[0] = '/';
        newPath[1] = 'm';
        newPath[2] = 'p';
        newPath[3] = '/';
        newPath[4] = '\0';
      }
    } else if (entry.name[0] == '.' && entry.name[1] == '\0') {
      return;
    } else {
      i = 0;
      while (m_currentPath[i]) {
        newPath[i] = m_currentPath[i];
        i++;
      }
      int j = 0;
      while (entry.name[j] && i < 510) {
        newPath[i++] = entry.name[j++];
      }
      if (i > 0 && newPath[i - 1] != '/')
        newPath[i++] = '/';
      newPath[i] = '\0';
    }

    DisplayDirectory(newPath);
  }
}
