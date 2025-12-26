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

// --- Local Filesystem Adapter (Moved from lwext4_adapter.cpp) ---
#include "disk/disk.h"
#include <ext4_blockdev.h>

static fs::Ext2Disk* g_detected_disk = nullptr;
static struct ext4_blockdev blockdev;
static struct ext4_blockdev_iface iface;
static uint8_t ph_bbuf[4096];
static struct ext4_bcache bc_static;

static int disk_open(struct ext4_blockdev *bdev) { return EOK; }
static int disk_close(struct ext4_blockdev *bdev) { return EOK; }
static int disk_lock(struct ext4_blockdev *bdev) { return EOK; }
static int disk_unlock(struct ext4_blockdev *bdev) { return EOK; }

static int disk_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    if (!g_detected_disk) return EIO;
    uint64_t part_offset_sectors = g_detected_disk->m_partition_offset / 512;
    uint64_t start_sector = part_offset_sectors + blk_id;
    if (blk_cnt == 0) return EOK;
    if (g_detected_disk->read_sectors(start_sector, (uint8_t*)buf, blk_cnt) != 0) return EIO;
    return EOK;
}

static int disk_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    if (!g_detected_disk) return EIO;
    uint64_t part_offset_sectors = g_detected_disk->m_partition_offset / 512;
    uint64_t start_sector = part_offset_sectors + blk_id;
    const uint8_t* buffer = (const uint8_t*)buf;
    for (uint32_t i = 0; i < blk_cnt; i++) {
        if (g_detected_disk->write_sector(start_sector + i, buffer + i * 512) != 0) return EIO;
    }
    return EOK;
}

static struct ext4_blockdev* get_local_blockdev() {
    // Scan drives for ext4 partition
    // Try drive 1 (secondary master) first (QEMU default for rootfs.img)
    // Then try drive 0 (primary master)
    static fs::Ext2Disk disk1(1);
    static fs::Ext2Disk disk0(0);

    kprintf("System: Scanning for ext4 partitions...\n");
    if (disk1.detect_partition()) {
        g_detected_disk = &disk1;
        kprintf("System: Using drive 1 (Secondary Master)\n");
    } else if (disk0.detect_partition()) {
        g_detected_disk = &disk0;
        kprintf("System: Using drive 0 (Primary Master)\n");
    } else {
        kprintf("System: No ext4 partition found!\n");
        return nullptr;
    }

    // Explicitly clear static structures
    memset(&blockdev, 0, sizeof(struct ext4_blockdev));
    memset(&iface, 0, sizeof(struct ext4_blockdev_iface));
    memset(ph_bbuf, 0, 4096);
    memset(&bc_static, 0, sizeof(struct ext4_bcache));

    iface.open = disk_open;
    iface.close = disk_close;
    iface.bread = disk_read;
    iface.bwrite = disk_write;
    iface.ph_bsize = 512;
    // Use partition size if detected, otherwise default to 28MB
    iface.ph_bcnt = (g_detected_disk->m_partition_size > 0) ? (g_detected_disk->m_partition_size / 512) : (28672 * 2);
    iface.ph_bbuf = ph_bbuf;
    iface.lock = disk_lock;
    iface.unlock = disk_unlock;

    blockdev.bdif = &iface;
    blockdev.part_offset = 0; // Offset is handled in disk_read/write
    blockdev.part_size = (g_detected_disk->m_partition_size > 0) ? g_detected_disk->m_partition_size : (28672 * 1024);
    
    struct ext4_bcache *bc = &bc_static;
    ext4_bcache_init_dynamic(bc, 8, 4096);
    ext4_block_bind_bcache(&blockdev, bc);

    return &blockdev;
}

// ----------------------------------------------------------------

System::System()
    : m_robotoFontBuffer(nullptr), m_bbhbogleFontBuffer(nullptr), m_cursorX(-1),
      m_cursorY(-1), m_cursorDrawn(false), m_prevLeftButton(false),
      m_clickableRegionCount(0), m_currentEntryCount(0) {
  // Clear the screen to black
  this->framebuffer = vga_get_framebuffer();
  vga_clear_buffer(this->framebuffer, 0x000000);

  // Initialize cursor background buffer
  for (int i = 0; i < CURSOR_WIDTH * CURSOR_HEIGHT; i++) {
    m_cursorBackground[i] = 0x000000;
  }

  // Initialize current path
  m_currentPath[0] = '\0';
}

System::~System() {
  if (m_robotoFontBuffer) {
    kfree(m_robotoFontBuffer);
  }
  if (m_bbhbogleFontBuffer) {
    kfree(m_bbhbogleFontBuffer);
  }
  if (m_jetBrainsFontBuffer) {
    kfree(m_jetBrainsFontBuffer);
  }
}

void System::Initialize() {
  kprintf("System::Initialize starting...\n");

  vga_clear_screen(0x000000); // Clear screen to black
  
  // Test new simple font - display "HELLO WORLD" using hardcoded bitmap font
  char hello[] = {'H', 'E', 'L', 'L', 'O', ' ', 'W', 'O', 'R', 'L', 'D', '\0'};
  vga_draw_string_simple(50, 50, hello, 0xFFFFFF, 2);
  
  // Display "FS INIT..." status
  char init_msg[] = {'F', 'S', ' ', 'I', 'N', 'I', 'T', '.', '.', '.', '\0'};
  vga_draw_string_simple(50, 100, init_msg, 0x00FFFF, 1);

  // Flag to track if filesystem is available
  m_filesystemAvailable = false;

  // Initialize and mount lwext4 filesystem
  struct ext4_blockdev *bdev = get_local_blockdev();
  
  if (!bdev) {
      char err[] = {'E', 'R', 'R', ':', ' ', 'N', 'O', ' ', 'B', 'D', 'E', 'V', '\0'};
      vga_draw_string_simple(50, 120, err, 0xFF0000, 1);
      return;
  }

  // Clear BSS garbage and register device
  ext4_device_unregister_all();
  char dev_name[] = {'e', 'x', 't', '4', '_', 'f', 's', '\0'};
  int rc = ext4_device_register(bdev, dev_name);
  
  if (rc != EOK && rc != 17) {
      char err[] = {'R', 'E', 'G', ' ', 'E', 'R', 'R', ':', ' ', '\0'};
      vga_draw_string_simple(50, 120, err, 0xFF0000, 1);
      vga_draw_digit(130, 120, (rc / 10) % 10, 0xFF0000, 1);
      vga_draw_digit(150, 120, rc % 10, 0xFF0000, 1);
      return;
  }

  // Mount the filesystem
  char mount_point[] = {'/', 'm', 'p', '/', '\0'};
  kprintf("System: Mounting filesystem at /mp/...\n");
  int r = ext4_mount(dev_name, mount_point, false);

  if (r != EOK) {
      kprintf("System: Mount failed with error %d\n", r);
      char err[] = {'M', 'N', 'T', ' ', 'E', 'R', 'R', ':', ' ', '\0'};
      vga_draw_string_simple(50, 120, err, 0xFF0000, 1);
      vga_draw_digit(130, 120, (r / 10) % 10, 0xFF0000, 1);
      vga_draw_digit(150, 120, r % 10, 0xFF0000, 1);
      return;
  }

  m_filesystemAvailable = true;
  char ok_msg[] = {'F', 'S', ' ', 'M', 'O', 'U', 'N', 'T', 'E', 'D', ' ', 'O', 'K', '\0'};
  vga_draw_string_simple(50, 120, ok_msg, 0x00FF00, 1);
  kprintf("System: Filesystem mounted OK\n");

  // Load logo
  kprintf("System: Loading logo.bmp...\n");
  char logo_path[] = {'l', 'o', 'g', 'o', '.', 'b', 'm', 'p', '\0'};
  int lw, lh;
  LoadImage(logo_path, 100, 200, lw, lh);

  // Try loading a simple file first: logo.bmp
  char loading[] = {'L', 'O', 'A', 'D', 'I', 'N', 'G', ' ', 'F', 'I', 'L', 'E', '.', '.', '.', '\0'};
  vga_draw_string_simple(50, 140, loading, 0xFFFF00, 1);

  // Stack string for test file: "logo.bmp" (at root level)
  char font_path[] = {'l', 'o', 'g', 'o', '.', 'b', 'm', 'p', '\0'};
  char mp[] = {'/', 'm', 'p', '/', '\0'};
  size_t test_size;
  unsigned char* test_buffer = read_file_to_memory(mp, font_path, &test_size);
  
  if (test_buffer) {
      char file_ok[] = {'F', 'I', 'L', 'E', ' ', 'O', 'K', '!', ' ', 'S', 'Z', ':', '\0'};
      vga_draw_string_simple(50, 160, file_ok, 0x00FF00, 1);
      // Display file size
      vga_draw_digit(150, 160, (test_size / 10000) % 10, 0x00FF00, 1);
      vga_draw_digit(165, 160, (test_size / 1000) % 10, 0x00FF00, 1);
      vga_draw_digit(180, 160, (test_size / 100) % 10, 0x00FF00, 1);
      vga_draw_digit(195, 160, (test_size / 10) % 10, 0x00FF00, 1);
      vga_draw_digit(210, 160, test_size % 10, 0x00FF00, 1);
      kfree(test_buffer);
  } else {
      char file_fail[] = {'F', 'I', 'L', 'E', ' ', 'F', 'A', 'I', 'L', 'E', 'D', '\0'};
      vga_draw_string_simple(50, 160, file_fail, 0xFF0000, 1);
  }

  // Demo text with simple font
  char demo[] = {'M', 'Y', 'O', 'S', ' ', 'V', '0', '.', '1', '\0'};
  vga_draw_string_simple(50, 350, demo, 0x00FFFF, 3);

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

    // Always restore cursor background first (hide cursor)
    if (m_cursorDrawn) {
      RestoreCursorBackground();
      m_cursorDrawn = false;
    }

    // Check for click (left button rising edge)
    bool leftButton = mouse.buttons[0];
    if (leftButton && !m_prevLeftButton) {
      // Click detected - check if any region was clicked
      int clickedRegion = GetClickedRegion(mouse.x, mouse.y);
      if (clickedRegion >= 0) {
        OnEntryClicked(m_clickableRegions[clickedRegion].entryIndex);
      }
    }
    m_prevLeftButton = leftButton;

    // Save background and draw cursor at current position
    SaveCursorBackground(mouse.x, mouse.y);
    DrawCursor(mouse.x, mouse.y);
    m_cursorX = mouse.x;
    m_cursorY = mouse.y;
    m_cursorDrawn = true;

    Render();
  }
}
// Static scratch buffer for font rendering - avoids heap allocation
static unsigned char s_fontScratchBuffer[128 * 128]; // Max 128x128 per glyph

void System::DrawText(int x, int y, const char *text, uint32_t color,
                      float size, stbtt_fontinfo *font) {
  int cursor_x = x;
  int cursor_y = y;
  float scale = stbtt_ScaleForPixelHeight(font, size);

  while (*text) {
    char c = *text++;
    int ax;  // advance width
    int lsb; // left side bearing
    stbtt_GetCodepointHMetrics(font, c, &ax, &lsb);

    int c_x1, c_y1, c_x2, c_y2;
    stbtt_GetCodepointBitmapBox(font, c, scale, scale, &c_x1, &c_y1, &c_x2,
                                &c_y2);

    int bitmap_width = c_x2 - c_x1;
    int bitmap_height = c_y2 - c_y1;

    // Skip if bitmap is too large for our scratch buffer or invalid
    if (bitmap_width <= 0 || bitmap_height <= 0 || bitmap_width > 128 ||
        bitmap_height > 128) {
      cursor_x += (int)(ax * scale); // Still move cursor forward
      continue;
    }

    // Render directly into scratch buffer (no allocation!)
    stbtt_MakeCodepointBitmap(font, s_fontScratchBuffer, bitmap_width,
                              bitmap_height, 128, scale, scale, c);

    for (int by = 0; by < bitmap_height; by++) {
      for (int bx = 0; bx < bitmap_width; bx++) {
        unsigned char opacity = s_fontScratchBuffer[by * 128 + bx];
        if (opacity > 0) {
          int px = cursor_x + bx + c_x1;
          int py = cursor_y + by + c_y1;

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

    cursor_x += (int)(ax * scale); // Move cursor forward
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

  // 1. Read the raw file content into a memory buffer.
  // This is crucial for kernel environments where we don't have standard C I/O (FILE*).
  unsigned char *file_buffer =
      read_file_to_memory("/mp/", filename, &file_size);
  if (file_buffer == NULL) {
    kprintf("Failed to read image file: %s\n", filename);
    return;
  }

  // 2. Decode the image from memory using stb_image.
  // Request STB_IMAGE to output 4 channels (RGBA) for consistency.
  // 'channels_in_file' will store the number of components actually found in
  // the file.
  unsigned char *image_data =
      stbi_load_from_memory(file_buffer, (int)file_size, &out_width,
                            &out_height, &channels_in_file, 4);

  kfree(file_buffer); // Free the file buffer as it's no longer needed

  if (image_data) {
    // 3. Render the decoded image data to the framebuffer.
    // We pass 4 as the number of channels because we requested RGBA.
    System::RenderImage(x, y, image_data, out_width, out_height, 4);
    
    // 4. Free the decoded image data.
    stbi_image_free(image_data);
  } else {
    kprintf("Failed to decode image: %s\n", filename);
  }
}

unsigned char *System::LoadFont(const char *font_path,
                                stbtt_fontinfo *font_info) {
  size_t file_size;
  // CHECKPOINT 3.1.1: BROWN (Start LoadFont)
  vga_draw_rectangle(vga_get_framebuffer(), 110, 220, 10, 10, 0xA52A2A);

  // Read the font file into a memory buffer.
  // The buffer must persist as long as the font is used, as stb_truetype reads directly from it.
  char mp[] = {'/', 'm', 'p', '/', '\0'};
  unsigned char *font_buffer =
      read_file_to_memory(mp, font_path, &file_size);
      
  // CHECKPOINT 3.1.2: GRAY (Post-ReadFile)
  vga_draw_rectangle(vga_get_framebuffer(), 130, 220, 10, 10, 0x808080);

  if (font_buffer == NULL) {
    return NULL;
  }

  // Initialize the font info structure.
  // This parses the TrueType header in the buffer.
  int offset = stbtt_GetFontOffsetForIndex(font_buffer, 0);
  
  // CHECKPOINT 3.1.3: TEAL (Pre-InitFont)
  vga_draw_rectangle(vga_get_framebuffer(), 150, 220, 10, 10, 0x008080);
  
  if (!stbtt_InitFont(font_info, font_buffer, offset)) {
    kfree(font_buffer); // Free if initialization fails
    return NULL;
  }
  
  // CHECKPOINT 3.1.4: GOLD (Post-InitFont)
  vga_draw_rectangle(vga_get_framebuffer(), 170, 220, 10, 10, 0xFFD700);
  
  return font_buffer; // Return the buffer, caller is responsible for kfree'ing
}

void System::ProcessInput() {}

void System::HandleEvents() {}

void System::SaveCursorBackground(int x, int y) {
  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    for (int j = 0; j < CURSOR_WIDTH; j++) {
      int px = x + j;
      int py = y + i;
      // Read pixel from framebuffer
      uint16_t pitch = *(uint16_t *)(0x5200 + 16);
      uint32_t offset = py * pitch + px * 4;
      m_cursorBackground[i * CURSOR_WIDTH + j] =
          *(uint32_t *)((char *)framebuffer + offset);
    }
  }
}

void System::RestoreCursorBackground() {
  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    for (int j = 0; j < CURSOR_WIDTH; j++) {
      vga_draw_pixel(framebuffer, m_cursorX + j, m_cursorY + i,
                     m_cursorBackground[i * CURSOR_WIDTH + j]);
    }
  }
}

// Arrow cursor bitmap (12x19 pixels)
// 0 = transparent, 1 = black outline, 2 = white fill
static const uint8_t s_cursorBitmap[19][12] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
    {1, 2, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0},
    {1, 2, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {1, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0},
};

void System::DrawCursor(int x, int y) {
  for (int i = 0; i < CURSOR_HEIGHT; i++) {
    for (int j = 0; j < CURSOR_WIDTH; j++) {
      uint8_t pixel = s_cursorBitmap[i][j];
      if (pixel == 1) {
        // Black outline
        vga_draw_pixel(this->framebuffer, x + j, y + i, 0x000000);
      } else if (pixel == 2) {
        // White fill
        vga_draw_pixel(this->framebuffer, x + j, y + i, 0xFFFFFF);
      }
      // pixel == 0 is transparent, skip
    }
  }
}

// Static buffer for directory entries - avoids heap allocation
static DirectoryEntry s_directoryEntries[MAX_CLICKABLE_REGIONS];

DirectoryEntry *System::GetDirectoryListing(const char *path, int *count) {
  ext4_dir dir;
  *count = 0;

  if (ext4_dir_open(&dir, path) != EOK) {
    kprintf("Failed to open directory: %s\n", path);
    return nullptr;
  }

  // Single pass: populate entries directly into static buffer
  const ext4_direntry *entry;
  int i = 0;
  while ((entry = ext4_dir_entry_next(&dir)) != nullptr &&
         i < MAX_CLICKABLE_REGIONS) {
    // Copy name (ensure null termination)
    int name_len = entry->name_length;
    if (name_len >= MAX_ENTRY_NAME) {
      name_len = MAX_ENTRY_NAME - 1;
    }
    for (int j = 0; j < name_len; j++) {
      s_directoryEntries[i].name[j] = entry->name[j];
    }
    s_directoryEntries[i].name[name_len] = '\0';

    // Copy type and inode
    s_directoryEntries[i].type = (DirectoryEntryType)entry->inode_type;
    s_directoryEntries[i].inode = entry->inode;
    i++;
  }

  ext4_dir_close(&dir);
  *count = i;
  return (i > 0) ? s_directoryEntries : nullptr;
}

void System::Update() {}

void System::Render() {}

// Click detection helpers
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
  return -1; // No region clicked
}

void System::AddClickableRegion(int x, int y, int width, int height,
                                int entryIndex) {
  if (m_clickableRegionCount < MAX_CLICKABLE_REGIONS) {
    m_clickableRegions[m_clickableRegionCount].x = x;
    m_clickableRegions[m_clickableRegionCount].y = y;
    m_clickableRegions[m_clickableRegionCount].width = width;
    m_clickableRegions[m_clickableRegionCount].height = height;
    m_clickableRegions[m_clickableRegionCount].entryIndex = entryIndex;
    m_clickableRegionCount++;
  }
}

void System::ClearClickableRegions() { m_clickableRegionCount = 0; }

// Directory UI functions
void System::DisplayDirectory(const char *path) {
  ClearClickableRegions();

  // Copy path
  int i = 0;
  while (path[i] && i < 511) {
    m_currentPath[i] = path[i];
    i++;
  }
  m_currentPath[i] = '\0';

  // Get directory listing (uses static buffer s_directoryEntries)
  GetDirectoryListing(path, &m_currentEntryCount);

  // Clear the directory listing area (including path header at y=280)
  DrawRectangle(50, 260, 500, 450, 0x000000);

  if (m_currentEntryCount == 0) {
    DrawText(50, 280, m_currentPath, 0x00AAFF, 14.0f, &m_robotoFontInfo);
    DrawText(50, 300, "Empty directory", 0x888888, 16.0f, &m_robotoFontInfo);
    return;
  }

  // Draw path header
  DrawText(50, 280, m_currentPath, 0x00AAFF, 14.0f, &m_robotoFontInfo);

  // Draw each entry and register clickable regions
  const int ENTRY_HEIGHT = 22;
  const int ENTRY_WIDTH = 300;
  const int START_Y = 300;

  for (int i = 0; i < m_currentEntryCount; i++) {
    int y = START_Y + i * ENTRY_HEIGHT;

    // Determine color based on type
    uint32_t color = 0xFFFFFF; // Default white for files
    if (s_directoryEntries[i].type == ENTRY_DIR) {
      color = 0x00AAFF; // Blue for directories
    } else if (s_directoryEntries[i].type == ENTRY_SYMLINK) {
      color = 0x00FF00; // Green for symlinks
    }

    // Draw the entry name
    DrawText(50, y, s_directoryEntries[i].name, color, 16.0f, &m_robotoFontInfo);

    // Register clickable region
    AddClickableRegion(50, y - 4, ENTRY_WIDTH, ENTRY_HEIGHT, i);
  }

  kprintf("Displayed %d entries from %s\n", m_currentEntryCount, path);
}

void System::OnEntryClicked(int entryIndex) {
  if (entryIndex < 0 || entryIndex >= m_currentEntryCount) {
    return;
  }

  DirectoryEntry &entry = s_directoryEntries[entryIndex];
  kprintf("Clicked on: %s (type: %d)\n", entry.name, entry.type);

  if (entry.type == ENTRY_DIR) {
    // Navigate into directory
    // Build new path
    char newPath[512];
    int i = 0;

    // Handle ".." (go up)
    if (entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == '\0') {
      // Find last '/' before the trailing one
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
        for (int j = 0; j <= lastSlash; j++) {
          newPath[j] = m_currentPath[j];
        }
        newPath[lastSlash + 1] = '\0';
      } else {
        // Already at root
        newPath[0] = '/';
        newPath[1] = 'm';
        newPath[2] = 'p';
        newPath[3] = '/';
        newPath[4] = '\0';
      }
    } else if (entry.name[0] == '.' && entry.name[1] == '\0') {
      // "." - stay in current directory, do nothing
      return;
    } else {
      // Normal directory - append to path
      i = 0;
      while (m_currentPath[i]) {
        newPath[i] = m_currentPath[i];
        i++;
      }
      // Append entry name
      int j = 0;
      while (entry.name[j] && i < 510) {
        newPath[i++] = entry.name[j++];
      }
      // Ensure trailing slash
      if (i > 0 && newPath[i - 1] != '/') {
        newPath[i++] = '/';
      }
      newPath[i] = '\0';
    }

    DisplayDirectory(newPath);
  } else {
    // File clicked - just log for now
    kprintf("File selected: %s\n", entry.name);
  }
}
