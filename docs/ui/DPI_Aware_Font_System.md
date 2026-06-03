# DPI-Aware Font System

## Overview

The font system uses `GameEngine`, `FontManager`, and `UIManager` to keep text
readable and crisp across standard and high-density displays. Rendering is
SDL3_GPU based: UI text draw data comes from SDL3_ttf and is recorded into the
UI GPU batch path.

## Responsibilities

- `GameEngine` reads SDL logical window size, pixel size, and pixel density.
- `GameEngine` calculates the font DPI scale where needed and refreshes it on
  display/window changes.
- `FontManager` loads TTF/OTF files at display-appropriate sizes and exposes
  measurement APIs for UI layout.
- `UIManager` records text vertices from SDL3_ttf GPU draw data, snaps integer
  UI placement to whole pixels, and submits text through `GPURenderer`.

## Display Metric Flow

```cpp
int logicalWidth = 0;
int logicalHeight = 0;
SDL_GetWindowSize(window, &logicalWidth, &logicalHeight);

int pixelWidth = logicalWidth;
int pixelHeight = logicalHeight;
SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);

float dpiScale = calculateFontDPIScale(
    logicalWidth, logicalHeight, pixelWidth, pixelHeight);
```

The engine stores pixel dimensions separately from logical window dimensions.
Use `getWidthInPixels()` / `getHeightInPixels()` for UI placement and GPU
viewport work.

## Font Sizing

`FontManager::loadFontsForDisplay(...)` calculates logical font sizes from the
window height, applies minimum readable sizes, then multiplies by the effective
DPI scale for high-density displays.

Font IDs follow these families:

- `fonts_<name>` for base text
- `fonts_UI_<name>` for UI text
- `fonts_title_<name>` for titles
- `fonts_tooltip_<name>` for tooltips

## Text Measurement

UI layout uses `FontManager` measurement APIs:

```cpp
bool measureText(const std::string& text,
                 const std::string& fontID,
                 int* width,
                 int* height);

bool measureMultilineText(const std::string& text,
                          const std::string& fontID,
                          int maxWidth,
                          int* width,
                          int* height);

bool getFontMetrics(const std::string& fontID,
                    int* lineHeight,
                    int* ascent,
                    int* descent);
```

Components that update every frame should use fixed bounds when possible to
avoid repeated font-metrics work.

## GPU Text Rendering

The GPU path uses SDL3_ttf draw data:

- `UIManager` requests text draw sequences from SDL3_ttf
- text quads are recorded into UI render batches
- `GPURenderer::renderUIBatches()` submits primitives, images, then text during
  the swapchain UI pass

Do not add SDL_Renderer text paths or per-label GPU texture ownership to UI
code.

## Related Docs

- [UIManager Guide](UIManager_Guide.md)
- [Auto-Sizing System](Auto_Sizing_System.md)
- [SDL3 GPU Display Coordinates](SDL3_Logical_Presentation_Modes.md)
- [GPU Rendering](../gpu/GPURendering.md)
