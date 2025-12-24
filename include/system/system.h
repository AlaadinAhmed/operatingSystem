#include <cstdint>

// stb_image defines
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) kmalloc(sz)
#define STBI_REALLOC(p, newsz) krealloc(p, newsz)
#define STBI_FREE(p) kfree(p)

#include "stb_truetype.h"
// #include "stb_truetype.h" // Already included in font.cpp, not needed here

static const int CURSOR_SIZE = 10;

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
  uint32_t m_cursorBackground[CURSOR_SIZE * CURSOR_SIZE];
  bool m_cursorDrawn;

  void Update();
  void DrawText(int x, int y, const char *text, uint32_t color, float size,
                stbtt_fontinfo *font);
  void Render();
  void DrawRectangle(int x, int y, int width, int height, uint32_t color);
  void ProcessInput();
  void HandleEvents();
  unsigned char *LoadFont(const char *font_path, stbtt_fontinfo *font_info);
  
  void SaveCursorBackground(int x, int y);
  void RestoreCursorBackground();
  void DrawCursor(int x, int y);
};

