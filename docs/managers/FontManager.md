# FontManager

**Code:** `include/managers/FontManager.hpp`, `src/managers/FontManager.cpp`

**Singleton Access:** Use `FontManager::Instance()` to access the manager.

## Overview

`FontManager` loads and manages TTF/OTF fonts and provides GPU-accelerated text rendering via SDL3_ttf's atlas-backed draw sequences. Text rendering goes through the `prepareGPUText` / `getGPUTextDrawData` pipeline.

## Key Features

- **TTF/OTF Font Support**: TrueType and OpenType fonts with hinting and kerning
- **Cross-Platform Font Sizing**: `loadFontsForDisplay()` auto-sizes based on window dimensions and DPI scale
- **Atlas-Backed GPU Rendering**: Text is prepared into `TTF_Text` objects and rendered via `TTF_GPUAtlasDrawSequence` during the swapchain pass
- **Text Measurement**: `measureText`, `measureMultilineText`, `wrapTextToLines` for UI layout
- **Thread Safety**: Font load operations are mutex-guarded; GPU text cache is main-thread-only

## Font Loading

```cpp
// Init must be called before loading fonts
FontManager::Instance().init();

// Load fonts sized for the current window (preferred)
fontMgr.loadFontsForDisplay("res/fonts", windowWidth, windowHeight, dpiScale);
// Creates IDs: "fonts", "fonts_UI", "fonts_title", "fonts_tooltip"

// Load a single font manually
fontMgr.loadFont("res/fonts/myfont.ttf", "hud", 24);

// Reload after DPI or window size change
fontMgr.reloadFontsForDisplay("res/fonts", newWidth, newHeight, newDpi);
```

## GPU Text Rendering

Text rendering is a two-step process:

1. **Prepare** — call once per unique text string/position to create or update the `TTF_Text` object.
2. **Record** — retrieve the atlas draw sequence while recording UI vertices (before the passes) and emit it as UI vertices; the UI pass draws them.

```cpp
// 1. Prepare (during update or on content change)
int textW = 0, textH = 0;
fontMgr.prepareGPUText("score_label", "Score: 1234", "fonts_UI", &textW, &textH);
fontMgr.setGPUTextPosition("score_label", x, y);

// 2. Record (UIManager::recordGPUVertices / a state's recordGPUUIVertices)
TTF_GPUAtlasDrawSequence* seq = fontMgr.getGPUTextDrawData("score_label");
for (; seq != nullptr; seq = seq->next) {
    // Emit seq->xy / seq->uv / seq->indices as UI vertices bound to
    // seq->atlas_texture (see UIManager text recording); SDL3_ttf has no draw call
}

// Clear cache on state transitions
fontMgr.clearGPUTextCache();
```

**Important:** `getGPUTextDrawData()` must be called on the main thread during UI vertex recording (`recordGPUUIVertices()`), not from workers.

## Text Measurement

```cpp
int w, h;
fontMgr.measureText("Hello", "fonts_UI", &w, &h);

fontMgr.measureMultilineText("Line1\nLine2", "fonts", 0, &w, &h);
fontMgr.measureTextWithWrapping("Long text...", "fonts_UI", 300, &w, &h);

int lineH, ascent, descent;
fontMgr.getFontMetrics("fonts", &lineH, &ascent, &descent);

std::vector<std::string> lines = fontMgr.wrapTextToLines("Long text...", "fonts_UI", 300);
```

## API Reference

```cpp
// Lifecycle
bool init();
void clean();
bool isShutdown() const;
bool areFontsLoaded() const;

// Font loading
bool loadFont(const std::string& fontFile, const std::string& fontID, int fontSize);
bool loadFontsForDisplay(const std::string& fontPath, int w, int h, float dpiScale = 1.0f);
bool reloadFontsForDisplay(const std::string& fontPath, int w, int h, float dpiScale = 1.0f);
bool isFontLoaded(const std::string& fontID) const;
void clearFont(const std::string& fontID);

// GPU text pipeline
bool prepareGPUText(const std::string& key, const std::string& text,
                    const std::string& fontID, int* width = nullptr, int* height = nullptr);
bool setGPUTextPosition(const std::string& key, int x, int y);
TTF_GPUAtlasDrawSequence* getGPUTextDrawData(const std::string& key);
void clearGPUTextCache();

// Measurement
bool measureText(const std::string& text, const std::string& fontID, int* w, int* h);
bool measureMultilineText(const std::string& text, const std::string& fontID,
                          int maxWidth, int* w, int* h);
bool measureTextWithWrapping(const std::string& text, const std::string& fontID,
                             int maxWidth, int* w, int* h);
bool getFontMetrics(const std::string& fontID, int* lineHeight, int* ascent, int* descent);
std::vector<std::string> wrapTextToLines(const std::string& text,
                                         const std::string& fontID, int maxWidth);
```

## Integration with Other Systems

- **[UIManager](../ui/UIManager_Guide.md)**: Uses `measureText` and the GPU text pipeline for component rendering
- **[InputManager](InputManager.md)**: Window resize events trigger `reloadFontsForDisplay`
- **[GPURendering](../gpu/GPURendering.md)**: GPU text draw sequences are submitted during the swapchain pass
- **[DPI Aware Font System](../ui/DPI_Aware_Font_System.md)**: Details on DPI scaling and font size calculation
