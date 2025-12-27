#include <cstdint>

// stb_image defines - must be before including stb_image.h
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) kmalloc(sz)
#define STBI_REALLOC(p, newsz) krealloc(p, newsz)
#define STBI_FREE(p) kfree(p)

#include "stb_truetype.h"
// #include "stb_truetype.h" // Already included in font.cpp, not needed here

static const int CURSOR_WIDTH = 12;
static const int CURSOR_HEIGHT = 19;
static const int MAX_ENTRY_NAME = 256;

// Directory entry types (from lwext4)
enum DirectoryEntryType {
  ENTRY_UNKNOWN = 0,
  ENTRY_REG_FILE = 1,
  ENTRY_DIR = 2,
  ENTRY_CHRDEV = 3,
  ENTRY_BLKDEV = 4,
  ENTRY_FIFO = 5,
  ENTRY_SOCK = 6,
  ENTRY_SYMLINK = 7
};

struct DirectoryEntry {
  char name[MAX_ENTRY_NAME];
  DirectoryEntryType type;
  uint32_t inode;
};

// Clickable region for UI elements
struct ClickableRegion {
  int x, y, width, height;
  int entryIndex; // Index into directory entries array
};

static const int MAX_CLICKABLE_REGIONS = 64;

class System {

public:
  System();
  ~System();
  void Initialize();
  void Shutdown();
  void Run();
  void ClearScreen(uint32_t color);
  void RenderImage(int x, int y, const unsigned char *image_data, int img_width,
                   int img_height, int channels);
  void LoadImage(const char *filename, int x, int y, int &out_width,
                 int &out_height);

private:
  uint32_t *framebuffer;
  stbtt_fontinfo m_robotoFontInfo;
  unsigned char *m_robotoFontBuffer;
  stbtt_fontinfo m_bbhbogleFontInfo;
  unsigned char *m_bbhbogleFontBuffer;
  stbtt_fontinfo m_jetBrainsFontInfo;
  unsigned char *m_jetBrainsFontBuffer;

  // Cursor state
  int m_cursorX, m_cursorY;
  uint32_t m_cursorBackground[CURSOR_WIDTH * CURSOR_HEIGHT];
  bool m_cursorDrawn;
  bool m_prevLeftButton; // For click detection (edge trigger)

  // Clickable regions for directory listing
  ClickableRegion m_clickableRegions[MAX_CLICKABLE_REGIONS];
  int m_clickableRegionCount;

  // Current directory state
  int m_currentEntryCount;
  char m_currentPath[512];
  bool m_filesystemAvailable;

  void Update();
  void DrawText(int x, int y, const char *text, uint32_t color, float size,
                stbtt_fontinfo *font);
  void Render();
  void DrawRectangle(int x, int y, int width, int height, uint32_t color);
  void ProcessInput();
  void HandleEvents();
  unsigned char *LoadFont(const char *font_path, stbtt_fontinfo *font_info);
  
  // Subsystem initialization
  bool InitFilesystem();
  void InitDrivers();

  void SaveCursorBackground(int x, int y);
  void RestoreCursorBackground();
  void DrawCursor(int x, int y);
  DirectoryEntry *GetDirectoryListing(const char *path, int *count);

  // Click detection helpers
  bool IsPointInRect(int px, int py, int rx, int ry, int rw, int rh);
  int GetClickedRegion(int x, int y);
  void AddClickableRegion(int x, int y, int width, int height, int entryIndex);
  void ClearClickableRegions();

  // Directory UI
  void DisplayDirectory(const char *path);
  void OnEntryClicked(int entryIndex);
};
