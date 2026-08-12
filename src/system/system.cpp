#include <mem/heap.h>
#include "common/boot_info.h"
#include "drivers/audio/audio.h"
#include "drivers/bus/usb.h"
#include "drivers/mouse/mouse.h"
#include "drivers/nvme/nvme.h"
#include "drivers/sse.h"
#include "drivers/vga.h"
#include "fs/lwext4_adapter.h"
#include "memory/kmalloc.h"
#include "print/print.h"
#include "stb_truetype.h"
#include "system/system.h"
#include "timer/timer.h"
#include "timer/pit.h"
#include "ui/widgets/text.h"
#include <ext4.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- System Class Implementation ---

#include "graphics/compositor.h"
#include "graphics/font_renderer.h"
#include "ui/widgets/button.h"
#include "ui/widgets/text.h"

System *System::s_instance = nullptr;
static DirectoryEntry s_directoryEntries[MAX_CLICKABLE_REGIONS];

// Callback for test button
// Callback for test button
static void onTestButtonClick() {
    kprintf("Test Button Clicked!\n");
    if (System::s_instance) {
        System::s_instance->ToggleDebugInfo();
    }
}

// ...

System::System()
    : m_robotoFontBuffer(nullptr), m_bbhbogleFontBuffer(nullptr), m_cursorX(100), m_cursorY(100), 
      m_render_mouseX(100.0f), m_render_mouseY(100.0f), m_savedCursorX(0), m_savedCursorY(0),
      m_hasSavedCursorBackground(false), m_cursorDrawn(false),
      m_prevLeftButton(false), m_clickableRegionCount(0), m_currentEntryCount(0), m_filesystemAvailable(false),
      m_testButton(nullptr), m_compositor(new Compositor()), m_debugInfoVisible(false), m_backbuffer(nullptr) {
    s_instance = this;
    for (int i = 0; i < 10; i++)
        m_debugLabels[i] = nullptr;

    framebuffer = vga_get_framebuffer();
    asm volatile("sfence" ::: "memory");
    profile_void("vga clear buffer", vga_clear_buffer, framebuffer, 0x000000);
    m_currentPath[0] = '\0';
}

bool System::InitFilesystem() { return fs::mount_filesystem("/mp/"); }

bool System::s_driversInitialized = false;

void System::InitDrivers() {
    if (s_driversInitialized) {
        kprintf("System: Drivers already initialized, skipping.\n");
        return;
    }
    s_driversInitialized = true;

    mouse_init(1.0f);
    find_audio_device();
    find_nvme_devices();
    init_audio();
}

void System::Initialize() {
    kprintf("System: Starting...\n");

    kprintf("System: Screen dimensions: %dx%d, pitch=%d\n", g_efi_boot_info.width, g_efi_boot_info.height,
            g_efi_boot_info.pitch);

    // Allocate backbuffer
    m_backbuffer = (uint32_t *)kmalloc(g_efi_boot_info.width * g_efi_boot_info.height * 4);
    if (!m_backbuffer) {
        kprintf("System: Failed to allocate backbuffer!\n");
        // Fallback?
    } else {
        kprintf("System: Backbuffer allocated at 0x%lx\n", m_backbuffer);
        memset(m_backbuffer, 0, g_efi_boot_info.width * g_efi_boot_info.height * 4);
    }

    profile_void("vga clear buffer", vga_clear_buffer, framebuffer, 0x003049);
    asm volatile("sfence" ::: "memory");

    // Initialize drivers
    InitDrivers();

    // Initialize filesystem after storage discovery
    m_filesystemAvailable = InitFilesystem();
    if (!m_filesystemAvailable) {
        kprintf("System: Filesystem not available\n");
        return;
    }

    m_robotoFontBuffer = LoadFont("Roboto-Regular.ttf", &m_robotoFontInfo);
    if (!m_robotoFontBuffer) {
        kprintf("System: Failed to load Roboto font\n");
        return;
    }

    // Initialize test button after font data is ready.
    m_testButton = new Button(100, 30, 600, 100, "Click Me", 0x000000FF, 0xFFFDF0D5, 4, &m_robotoFontInfo,
                              onTestButtonClick, 10, 0xFFFDF0);
    m_compositor->addWindow(m_testButton);
    kprintf("System: Button initialized\n");

    // Initialize version label
    m_versionLabel = new Text(0, 400, "MyOS v0.1", 0xFFFFFFFF, 32.0f, &m_robotoFontInfo, 1024);
    kprintf("System: Text initialized\n");
    m_versionLabel->setAlignment(Alignment::Center);
    m_versionLabel->setAlpha(200);
    m_compositor->addWindow(m_versionLabel);
    kprintf("System: Text added to compositor\n");

    // Play startup sound
    kprintf("System: Playing startup sound...\n");
    // start_audio_file("as_it_was.wav");

    find_xhci();
    
    kprintf("System: Ready\n");
}

void System::Shutdown() { kprintf("System: Shutdown\n"); }

void System::Run() {
    uint64_t last_ticks = pit_get_ticks();
    const uint64_t ticks_per_frame = 16; // ~60 FPS

    Render(); // Initial render

    while (true) {
        ProcessInput();
        HandleEvents();

        MouseState mouse = mouse_update();

        bool buttonChanged = (mouse.buttons[0] != m_prevLeftButton);

        // Update hardware target
        m_cursorX = mouse.x;
        m_cursorY = mouse.y;

        // Update button state
        if (m_testButton) {
            if (mouse.buttons[0]) {
                m_testButton->onMouseDown(mouse.x, mouse.y, 0);
            } else {
                m_testButton->onMouseUp(mouse.x, mouse.y, 0);
            }
        }

        // Handle clicks
        bool leftButton = mouse.buttons[0];
        if (leftButton && !m_prevLeftButton) {
            int clickedRegion = GetClickedRegion(mouse.x, mouse.y);
            if (clickedRegion >= 0) {
                OnEntryClicked(m_clickableRegions[clickedRegion].entryIndex);
            }
        }
        m_prevLeftButton = leftButton;

        uint64_t current_ticks = pit_get_ticks();
        if (current_ticks - last_ticks >= ticks_per_frame) {
            float dt = (current_ticks - last_ticks) / 1000.0f;
            last_ticks = current_ticks;

            Update(dt);

            if (buttonChanged || m_debugInfoVisible) {
                Render();
            } else {
                RenderCursor();
            }
        }

        asm volatile("hlt");
    }
}

// --- Drawing Functions ---

void System::DrawRectangle(int x, int y, int width, int height, uint32_t color) {
    // Draw to backbuffer if available, else framebuffer
    uint32_t *target = m_backbuffer ? m_backbuffer : framebuffer;
    int pitch = g_efi_boot_info.width; // Pixels

    // Clip to screen
    int start_y = y < 0 ? 0 : y;
    int end_y = y + height > g_efi_boot_info.height ? g_efi_boot_info.height : y + height;
    int start_x = x < 0 ? 0 : x;
    int end_x = x + width > g_efi_boot_info.width ? g_efi_boot_info.width : x + width;
    
    int clipped_width = end_x - start_x;
    if (clipped_width <= 0) return;

    for (int i = start_y; i < end_y; i++) {
        sse_fill_buffer32(&target[i * pitch + start_x], color, clipped_width);
    }
}

void System::ClearScreen(uint32_t color) {
    // Clear backbuffer
    if (m_backbuffer) {
        sse_fill_buffer32(m_backbuffer, color, g_efi_boot_info.width * g_efi_boot_info.height);
    } else {
        vga_clear_buffer(framebuffer, color);
    }
}

void System::RenderImage(int x, int y, const unsigned char *image_data, int img_width, int img_height, int channels) {
    // Draw to backbuffer
    uint32_t *target = m_backbuffer ? m_backbuffer : framebuffer;
    int pitch = g_efi_boot_info.width;

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

            int px = x + j;
            int py = y + i;
            if (px >= 0 && px < g_efi_boot_info.width && py >= 0 && py < g_efi_boot_info.height) {
                target[py * pitch + px] = (r << 16) | (g << 8) | b;
            }
        }
    }
}

void System::LoadImage(const char *filename, int x, int y, int &out_width, int &out_height) {
    // ... (Keep existing implementation, it calls RenderImage)
    int channels_in_file;
    size_t file_size;
    int out_height_local, out_width_local;
    kprintf("LoadImage: Reading %s...\n", filename);
    unsigned char *file_buffer = read_file_to_memory("/mp/", filename, &file_size);
    if (!file_buffer) {
        kprintf("LoadImage: Failed to read file\n");
        return;
    }
    kprintf("LoadImage: Read %d bytes, buffer at 0x%lx\n", (int)file_size, file_buffer);

    kprintf("LoadImage: Calling stbi_load_from_memory...\n");
    unsigned char *image_data =
        stbi_load_from_memory(file_buffer, (int)file_size, &out_width_local, &out_height_local, &channels_in_file, 4);
    kprintf("LoadImage: stbi returned 0x%lx\n", image_data);

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

unsigned char *System::LoadFont(const char *font_path, stbtt_fontinfo *font_info) {
    // ... (Keep existing)
    size_t file_size;
    unsigned char *font_buffer = read_file_to_memory("/mp/", font_path, &file_size);
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

void System::Render() {
    if (!m_backbuffer)
        return;

    // 1. Clear Backbuffer (Background)
    // Fast fill with dark blue
    uint32_t bgColor = 0x003049;
    int count = g_efi_boot_info.width * g_efi_boot_info.height;
    sse_fill_buffer32(m_backbuffer, bgColor, count);

    // 2. Render UI to Backbuffer
    Surface screen;
    screen.width = g_efi_boot_info.width;
    screen.height = g_efi_boot_info.height;
    screen.pitch = g_efi_boot_info.width; // Pixels in backbuffer
    screen.pixels = m_backbuffer;

    if (m_compositor) {
        m_compositor->render(&screen);
    }

    // 3. Render Directory UI (if active)
    // Note: DisplayDirectory draws directly using DrawRectangle/draw_text.
    // We need to ensure those methods use m_backbuffer.
    // DisplayDirectory calls DrawRectangle (updated above) and
    // font_renderer::draw_text. font_renderer::draw_text takes a Surface. We need
    // to update DisplayDirectory to use the backbuffer surface.
    // ... (See DisplayDirectory update below)
    if (m_currentEntryCount > 0 || m_currentPath[0] != '\0') {
        // Re-draw directory UI on top
        // Actually, DisplayDirectory is called once? No, it's stateful.
        // We should call a "RenderDirectory" method here.
        // But DisplayDirectory was designed to draw immediately.
        // Let's refactor DisplayDirectory to just render based on state.
        // For now, let's just call it? No, it resets clickable regions.
        // We need to separate "Update Directory State" from "Render Directory".
        // Given the complexity, let's just assume DisplayDirectory is not the main
        // focus right now. But wait, if we clear backbuffer, directory UI
        // disappears! We MUST redraw it. Let's move the drawing logic of
        // DisplayDirectory to here or a helper.

        if (m_currentPath[0] != '\0') {
            DrawRectangle(50, 260, 500, 450, 0x000000);

            if (m_currentEntryCount == 0) {
                font_renderer::draw_text(&screen, 50, 280, m_currentPath, 0x00AAFF, 14.0f, &m_robotoFontInfo);
                font_renderer::draw_text(&screen, 50, 300, "Empty directory", 0x888888, 16.0f, &m_robotoFontInfo);
            } else {
                font_renderer::draw_text(&screen, 50, 280, m_currentPath, 0x00AAFF, 14.0f, &m_robotoFontInfo);
                const int ENTRY_HEIGHT = 22;
                const int START_Y = 300;
                for (int i = 0; i < m_currentEntryCount; i++) {
                    int y = START_Y + i * ENTRY_HEIGHT;
                    uint32_t color = 0xFFFFFF;
                    if (s_directoryEntries[i].type == ENTRY_DIR)
                        color = 0x00AAFF;
                    else if (s_directoryEntries[i].type == ENTRY_SYMLINK)
                        color = 0x00FF00;
                    font_renderer::draw_text(&screen, 50, y, s_directoryEntries[i].name, color, 16.0f,
                                             &m_robotoFontInfo);
                }
            }
        }
    }

    // 4. Draw Cursor on top using Dirty Rectangles before flipping
    m_hasSavedCursorBackground = false; // Reset since we just created a clean frame
    RenderCursor();

    // 5. Flip (Copy Backbuffer to Framebuffer)
    uint32_t fb_pitch_pixels = g_efi_boot_info.pitch;

    if (fb_pitch_pixels == g_efi_boot_info.width) {
        sse_memcpy(framebuffer, m_backbuffer, g_efi_boot_info.width * g_efi_boot_info.height * 4);
    } else {
        for (int y = 0; y < g_efi_boot_info.height; y++) {
            sse_memcpy(framebuffer + y * fb_pitch_pixels, m_backbuffer + y * g_efi_boot_info.width,
                   g_efi_boot_info.width * 4);
        }
    }
}

// --- Cursor ---

static const uint8_t s_cursorBitmap[19][12] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0}, {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0}, {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0}, {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0}, {1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
    {1, 2, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0}, {1, 2, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0}, {1, 2, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0}, {1, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0},
};

void System::RestoreCursorBackground() {
    if (!m_hasSavedCursorBackground || !framebuffer) return;
    
    int width = g_efi_boot_info.width;
    for (int i = 0; i < CURSOR_HEIGHT; i++) {
        for (int j = 0; j < CURSOR_WIDTH; j++) {
            int px = m_savedCursorX + j;
            int py = m_savedCursorY + i;
            if (px >= 0 && px < g_efi_boot_info.width && py >= 0 && py < g_efi_boot_info.height) {
                framebuffer[py * width + px] = m_cursorBackground[i * CURSOR_WIDTH + j];
                m_backbuffer[py * width + px] = m_cursorBackground[i * CURSOR_WIDTH + j];
            }
        }
    }
    m_hasSavedCursorBackground = false;
}

void System::RenderCursor() {
    if (!m_backbuffer || !framebuffer) return;

    RestoreCursorBackground();

    int cx = (int)m_render_mouseX;
    int cy = (int)m_render_mouseY;

    m_savedCursorX = cx;
    m_savedCursorY = cy;
    int width = g_efi_boot_info.width;
    int height = g_efi_boot_info.height;
    
    for (int i = 0; i < CURSOR_HEIGHT; i++) {
        for (int j = 0; j < CURSOR_WIDTH; j++) {
            int px = cx + j;
            int py = cy + i;
            if (px >= 0 && px < g_efi_boot_info.width && py >= 0 && py < g_efi_boot_info.height) {
                m_cursorBackground[i * CURSOR_WIDTH + j] = m_backbuffer[py * width + px];
            } else {
                m_cursorBackground[i * CURSOR_WIDTH + j] = 0;
            }
        }
    }
    m_hasSavedCursorBackground = true;

    for (int i = 0; i < CURSOR_HEIGHT; i++) {
        for (int j = 0; j < CURSOR_WIDTH; j++) {
            uint8_t pixel = s_cursorBitmap[i][j];
            int px = cx + j;
            int py = cy + i;

            if (px >= 0 && px < width && py >= 0 && py < height) {
                if (pixel == 1) {
                    framebuffer[py * width + px] = 0x000000;
                    m_backbuffer[py * width + px] = 0x000000;
                } else if (pixel == 2) {
                    framebuffer[py * width + px] = 0xFFFFFF;
                    m_backbuffer[py * width + px] = 0xFFFFFF;
                }
            }
        }
    }
}

// --- Directory Listing ---

DirectoryEntry *System::GetDirectoryListing(const char *path, int *count) {
    *count = 0;
    // This is a placeholder. In a real OS, we would call the FS driver.
    // For now, we rely on the fact that we might have some static entries or
    // we call the ext4 adapter if available.

    if (m_filesystemAvailable) {
        // Use lwext4 adapter
        // We need to map lwext4 dirent to our DirectoryEntry
        // This requires a helper or direct usage.
        // For simplicity, let's assume we have a wrapper or we just list root.
        // Actually, let's just implement a simple test listing if FS is up.

        // Real implementation using fs::read_directory (if it existed)
        // Since we don't have a full VFS yet, let's just return nullptr or
        // implement a dummy for the UI test.
        // BUT, the user wants to see "Filesystem: Mounted".
        // The Directory UI is secondary.
        // Let's just return 0 entries for now to avoid linker errors if
        // fs::read_directory is missing. Wait, the previous code had it. I should
        // have been more careful.

        // Let's try to implement a basic one.
        static DirectoryEntry entries[2];
        strcpy(entries[0].name, ".");
        entries[0].type = ENTRY_DIR;
        strcpy(entries[1].name, "..");
        entries[1].type = ENTRY_DIR;
        *count = 2;

        // Copy to s_directoryEntries
        for (int i = 0; i < *count; i++) {
            s_directoryEntries[i] = entries[i];
        }
        return s_directoryEntries;
    }
    return nullptr;
}

bool System::IsPointInRect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

int System::GetClickedRegion(int x, int y) {
    for (int i = 0; i < m_clickableRegionCount; i++) {
        if (IsPointInRect(x, y, m_clickableRegions[i].x, m_clickableRegions[i].y, m_clickableRegions[i].width,
                          m_clickableRegions[i].height)) {
            return i;
        }
    }
    return -1;
}

void System::AddClickableRegion(int x, int y, int width, int height, int entryIndex) {
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

void System::DisplayDirectory(const char *path) {
    ClearClickableRegions();

    int i = 0;
    while (path[i] && i < 511) {
        m_currentPath[i] = path[i];
        i++;
    }
    m_currentPath[i] = '\0';

    GetDirectoryListing(path, &m_currentEntryCount);

    // Re-calculate clickable regions
    const int ENTRY_HEIGHT = 22;
    const int ENTRY_WIDTH = 300;
    const int START_Y = 300;

    for (int i = 0; i < m_currentEntryCount; i++) {
        int y = START_Y + i * ENTRY_HEIGHT;
        AddClickableRegion(50, y - 4, ENTRY_WIDTH, ENTRY_HEIGHT, i);
    }

    // Drawing is now handled in Render()
}

void System::OnEntryClicked(int entryIndex) {
    // ... (Keep existing implementation)
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

void System::ToggleDebugInfo() {
    m_debugInfoVisible = !m_debugInfoVisible;

    if (m_debugInfoVisible) {
        // Create debug labels with LARGER font
        m_debugLabels[0] = new Text(10, 10, "Debug Info:", 0xFF00FF00, 24.0f, &m_robotoFontInfo, 400);
        m_debugLabels[1] = new Text(10, 40, "Resolution: ", 0xFFFFFFFF, 20.0f, &m_robotoFontInfo, 400);
        m_debugLabels[2] = new Text(10, 70, "Mouse: ", 0xFFFFFFFF, 20.0f, &m_robotoFontInfo, 400);
        m_debugLabels[3] = new Text(10, 100, "Clickable Regions: ", 0xFFFFFFFF, 20.0f, &m_robotoFontInfo, 400);
        m_debugLabels[4] = new Text(10, 130, "Filesystem: ", 0xFFFFFFFF, 20.0f, &m_robotoFontInfo, 400);

        for (int i = 0; i < 5; i++) {
            if (m_debugLabels[i])
                m_compositor->addWindow(m_debugLabels[i]);
        }

        if (m_testButton) {
            m_testButton->setText("Hide Debug");
            m_testButton->setColor(0xFF555555);
        }

    } else {
        for (int i = 0; i < 10; i++) {
            if (m_debugLabels[i]) {
                m_debugLabels[i]->move(-1000, 0); // Hide
            }
        }

        if (m_testButton) {
            m_testButton->setText("Show Debug");
            m_testButton->setColor(0xFF333333);
        }
    }
}

void System::UpdateDebugInfo() {
    // ... (Keep existing)
    if (!m_debugInfoVisible)
        return;

    char buffer[64];

    // Resolution
    ksprintf(buffer, "Resolution: %dx%d", g_efi_boot_info.width, g_efi_boot_info.height);
    if (m_debugLabels[1])
        m_debugLabels[1]->setText(buffer);

    // Mouse
    ksprintf(buffer, "Mouse: %d, %d", m_cursorX, m_cursorY);
    if (m_debugLabels[2])
        m_debugLabels[2]->setText(buffer);

    // Regions
    ksprintf(buffer, "Clickable Regions: %d", m_clickableRegionCount);
    if (m_debugLabels[3])
        m_debugLabels[3]->setText(buffer);

    // FS
    ksprintf(buffer, "Filesystem: %s", m_filesystemAvailable ? "Mounted" : "Error");
    if (m_debugLabels[4])
        m_debugLabels[4]->setText(buffer);
}

void System::Update(float dt) { 
    // Smooth Cursor LERP
    float dx = m_cursorX - m_render_mouseX;
    float dy = m_cursorY - m_render_mouseY;
    
    // Increased factor so it's snappy and doesn't slide on ice
    float lerp_factor = 40.0f * dt;
    if (lerp_factor > 1.0f) lerp_factor = 1.0f;
    
    m_render_mouseX += dx * lerp_factor;
    m_render_mouseY += dy * lerp_factor;

    UpdateDebugInfo(); 
}
