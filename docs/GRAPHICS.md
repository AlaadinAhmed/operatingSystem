# Graphics Subsystem

This document describes the graphics subsystem of the OS, specifically focusing on image loading and font rendering.

## Overview

The OS uses a framebuffer-based graphics system. It supports:
- High-resolution graphics (via VBE or UEFI GOP).
- TrueType font rendering.
- Image loading (BMP, PNG, JPG, etc.).
- Double buffering (software).

## Dependencies

The graphics system relies on two single-header libraries from the [stb](https://github.com/nothings/stb) collection:
- `stb_truetype.h`: For parsing and rendering TrueType fonts.
- `stb_image.h`: For loading and decoding image files.

These libraries are located in `src/external/`.

## Isolation and Kernel Integration

Since standard C library functions (like `fopen`, `fread`, `malloc`) are not available or behave differently in the kernel, we have isolated the library usage:

1.  **Memory Management**:
    - We define `STBI_MALLOC`, `STBI_FREE`, etc., to use our kernel's `kmalloc` and `kfree`.
    - This ensures that the libraries allocate memory from the kernel heap.

2.  **File I/O**:
    - We define `STBI_NO_STDIO` to prevent `stb_image` from trying to use `stdio.h`.
    - Instead of passing file paths to the libraries, we first read the entire file into a memory buffer using `read_file_to_memory` (implemented in `src/kernel/utils.cpp` using our `lwext4` adapter).
    - We then use the `_from_memory` variants of the library functions:
        - `stbi_load_from_memory`
        - `stbtt_InitFont` (which takes a buffer)

## Font Rendering

Font rendering is handled by the `System` class in `src/system/system.cpp`.

1.  **Loading**:
    - `System::LoadFont` reads the `.ttf` file into a buffer.
    - `stbtt_InitFont` parses the font data.
    - **Important**: The font buffer must persist as long as the font is used, as `stb_truetype` reads directly from it during rendering.

2.  **Rendering**:
    - `System::DrawText` iterates over the string.
    - For each character, it calls `stbtt_GetCodepointBitmapBox` to get dimensions.
    - It uses `stbtt_MakeCodepointBitmap` to render the glyph into a temporary scratch buffer (`s_fontScratchBuffer`).
    - Finally, it blends the glyph pixels onto the framebuffer.

## Image Loading

Image loading is handled by `System::LoadImage`.

1.  **Loading**:
    - The file is read into a temporary buffer.
    - `stbi_load_from_memory` decodes the image. We request 4 channels (RGBA) to simplify rendering.
    - The temporary file buffer is freed immediately after decoding.

2.  **Rendering**:
    - `System::RenderImage` copies the decoded pixel data to the framebuffer.
    - The decoded image data is freed using `stbi_image_free` (which maps to `kfree`).

## Supported Formats

- **Fonts**: TrueType (.ttf)
- **Images**: BMP, PNG, JPG, TGA, etc. (whatever `stb_image` supports)

## Example Usage

```cpp
// Load a font
stbtt_fontinfo fontInfo;
unsigned char* fontBuffer = System::LoadFont("Roboto-Regular.ttf", &fontInfo);

// Draw text
System::DrawText(100, 100, "Hello World", 0xFFFFFF, 24.0f, &fontInfo);

// Load and draw an image
int w, h;
System::LoadImage("logo.bmp", 200, 200, w, h);
```
