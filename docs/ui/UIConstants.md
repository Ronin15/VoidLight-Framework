# UIConstants Reference

## Overview

The `UIConstants` namespace centralizes all UI-related constants in a single location (`include/managers/UIConstants.hpp`). This approach provides several key benefits:

- **Consistency**: All UI components use the same spacing, sizing, and layering values
- **Maintainability**: Change one constant, and the entire UI system adapts
- **Scaling**: Supports resolution-aware UI that works across different screen sizes
- **Documentation**: Constants are self-documenting through descriptive names
- **Performance**: Pre-computed constants at compile-time with zero runtime overhead

## Usage Pattern

Include the constants header and use the namespace:

```cpp
#include "managers/UIConstants.hpp"

// Access constants through the namespace
int buttonWidth = UIConstants::DEFAULT_BUTTON_WIDTH;     // 120px
int buttonHeight = UIConstants::DEFAULT_BUTTON_HEIGHT;   // 40px
float scale = UIManager::Instance().calculateOptimalScale(
    logicalWidth, logicalHeight
);
```

---

## Baseline Resolution System

### The Baseline Resolution Concept

All UI constants are designed for a **baseline resolution of 1920×1080 pixels**. This is the reference resolution used for UI design—all UI elements are defined in this coordinate space, then automatically scaled to fit the actual window resolution.

```cpp
// Baseline constants
constexpr int BASELINE_WIDTH = 1920;   // Reference width
constexpr int BASELINE_HEIGHT = 1080;  // Reference height
```

### How Scaling Works

UIManager provides automatic scaling via `calculateOptimalScale()`:

```cpp
// Get optimal scale factor for current window size
float scale = ui.calculateOptimalScale(windowWidth, windowHeight);

// This scale factor is applied to:
// - Component positions and sizes
// - Spacing and padding values
// - Font sizes
// - Z-order (not affected)

// Example: On 1280×720 display
float scale = 0.667f;  // Downscaled to fit smaller screen
```

### Small Screen Support

The UI system is designed to scale down to **1280×720 (720p)** minimum, targeting PC handheld gaming devices and smaller screens:

- 720p is 66.7% of baseline (1280/1920 ≈ 0.667)
- All components scale proportionally
- No clipping or repositioning required with proper use of responsive layouts
- Optimized for PC handheld devices (Steam Deck, ROG Ally, OneXPlayer, etc.) and smaller screens
- Below 720p, consider redesigning your UI layout

### Example Scaling Scenarios

```
Baseline (1920×1080):  scale = 1.0   (no scaling)
1280×720:              scale ≈ 0.667 (67% of baseline)
2560×1440:             scale ≈ 1.333 (133% of baseline)
3840×2160:             scale ≈ 2.0   (200% of baseline)
```

---

## Standard UI Fonts

Font constants for consistent text rendering:

```cpp
// Font identifiers from FontManager
constexpr std::string_view FONT_UI = "fonts_UI_Arial";
constexpr std::string_view FONT_TITLE = "fonts_title_Arial";
constexpr std::string_view FONT_TOOLTIP = "fonts_tooltip_Arial";
constexpr std::string_view FONT_DEFAULT = "fonts_UI_Arial";  // Alias for FONT_UI
```

**Usage:**

```cpp
// Set global UI font
ui.setGlobalFont(std::string(UIConstants::FONT_UI));

// Or explicitly for a component
ui.createLabel("my_label", bounds, "Text");  // Uses FONT_DEFAULT automatically
```

---

## Z-Order Priority Constants

Controls UI component priority. Lower values are lower priority, higher values are higher priority. `UIManager` renders with fixed GPU families for performance: primitives first, images second, text last. These constants order components inside a render family and drive input/modal priority; they do not create arbitrary cross-family visual layering.

| Constant | Value | Component Type | Purpose |
|----------|-------|----------------|---------|
| `ZORDER_OVERLAY` | -10 | Overlay backgrounds | Lowest priority overlay backgrounds |
| `ZORDER_PANEL` | 0 | Background panels | Container backgrounds |
| `ZORDER_IMAGE` | 1 | Background images | Visual decoration |
| `ZORDER_DIALOG` | 2 | Dialogs | Non-modal dialog priority |
| `ZORDER_PROGRESS_BAR` | 5 | Progress indicators | Status display |
| `ZORDER_EVENT_LOG` | 6 | Event logs | Information display |
| `ZORDER_LIST` | 8 | Lists | Item containers |
| `ZORDER_BUTTON` | 10 | Interactive buttons | User interaction |
| `ZORDER_SLIDER` | 12 | Slider controls | Value input |
| `ZORDER_CHECKBOX` | 13 | Checkbox controls | Boolean input |
| `ZORDER_INPUT_FIELD` | 15 | Text input fields | Text input |
| `ZORDER_LABEL` | 20 | Text labels | Information text |
| `ZORDER_TITLE` | 25 | Title text | Page headings |
| `ZORDER_MODAL_OVERLAY` | 500 | Modal overlay | Render/input cutoff for lower normal UI |
| `ZORDER_MODAL_DIALOG` | 600 | Modal dialog | Modal dialog content priority |
| `ZORDER_TOOLTIP` | 1000 | Tooltips | Highest tooltip priority |

### Priority Diagram

```
Priority -10: Dialog backgrounds
Priority  0:  Background panels
Priority  1:  Background images
Priority  5:  Progress bars
Priority  6:  Event logs
Priority  8:  Lists
Priority 10:  Buttons
Priority 12:  Sliders
Priority 13:  Checkboxes
Priority 15:  Input fields
Priority 20:  Labels
Priority 25:  Titles
Priority 500: Modal overlay cutoff
Priority 600: Modal dialog
Priority 1000:Tooltips
```

### Usage Examples

```cpp
auto& ui = UIManager::Instance();

// Components automatically get priority by type
ui.createButton("btn", bounds, "Click");           // Auto: ZORDER_BUTTON (10)
ui.createLabel("label", bounds, "Text");           // Auto: ZORDER_LABEL (20)
ui.createDialog("bg", bounds);                     // Auto: ZORDER_DIALOG (2)

// Manual override only if needed (rarely required). This changes input and
// same-family ordering, not cross-family GPU render order.
ui.setComponentZOrder("special_button", UIConstants::ZORDER_TITLE);
```

---

## Component Spacing Constants

Defines padding, margins, and internal spacing for UI components.

### Checkbox & Interactive Elements

```cpp
constexpr int CHECKBOX_SIZE = 24;              // 24×24 pixel checkbox
constexpr int INPUT_CURSOR_SPACE = 20;         // Space for input cursor
```

### Tooltip Spacing

```cpp
constexpr int TOOLTIP_PADDING_WIDTH = 16;      // Horizontal padding
constexpr int TOOLTIP_PADDING_HEIGHT = 8;      // Vertical padding
constexpr int TOOLTIP_MOUSE_OFFSET = 10;       // Offset from mouse pointer
```

### List Components

```cpp
constexpr int LIST_ITEM_PADDING = 8;           // Padding within list items
constexpr int SCROLLBAR_WIDTH = 20;            // Width of scrollbar
```

---

## Border Width Constants

Controls border thickness for different component types:

```cpp
constexpr int BORDER_WIDTH_NONE = 0;           // No border
constexpr int BORDER_WIDTH_NORMAL = 1;         // Standard 1px border
constexpr int BORDER_WIDTH_DIALOG = 2;         // Thicker 2px dialog border
constexpr int DEBUG_BORDER_WIDTH = 1;          // Debug visualization borders
```

**Usage:**

```cpp
auto& ui = UIManager::Instance();
ui.setDebugMode(true);     // Enables debug borders (uses DEBUG_BORDER_WIDTH)
ui.drawDebugBounds(true);  // Shows red component boundaries
```

---

## Font Size Constants

Standard font sizes for consistent text hierarchy:

```cpp
constexpr int DEFAULT_FONT_SIZE = 16;          // Standard UI text (body)
constexpr int TITLE_FONT_SIZE = 24;            // Title text (headings)
```

**Sizing Hierarchy:**

- Body text: 16px (default)
- Titles/headings: 24px (50% larger)
- Tooltips: 14px (slightly smaller, managed separately)

---

## Component Size Constraints

Minimum and maximum component dimensions (in baseline pixels):

```cpp
constexpr int MIN_COMPONENT_WIDTH = 32;        // Minimum width
constexpr int MIN_COMPONENT_HEIGHT = 16;       // Minimum height
constexpr int MAX_COMPONENT_WIDTH = 800;       // Maximum width
constexpr int MAX_COMPONENT_HEIGHT = 600;      // Maximum height
```

### Input Field Constraints

```cpp
constexpr int DEFAULT_INPUT_MAX_LENGTH = 256;  // Max characters
```

---

## Dialog & Modal Size Constants

Default dimensions for modal dialogs (in baseline pixels, auto-scaled):

```cpp
constexpr int DEFAULT_DIALOG_WIDTH = 400;      // Standard dialog width
constexpr int DEFAULT_DIALOG_HEIGHT = 200;     // Standard dialog height
```

**Usage:**

```cpp
// Create centered dialog with default size
int dialogX = (windowWidth - UIConstants::DEFAULT_DIALOG_WIDTH) / 2;
int dialogY = (windowHeight - UIConstants::DEFAULT_DIALOG_HEIGHT) / 2;

ui.createModal("dialog",
    {dialogX, dialogY,
     UIConstants::DEFAULT_DIALOG_WIDTH,
     UIConstants::DEFAULT_DIALOG_HEIGHT},
    "dark", windowWidth, windowHeight);
```

---

## Padding and Spacing Defaults

Internal component padding and layout spacing:

```cpp
constexpr int DEFAULT_COMPONENT_PADDING = 8;   // Internal padding
constexpr int DEFAULT_CONTENT_PADDING = 8;     // Content padding for auto-sizing
constexpr int DEFAULT_MARGIN = 4;              // Component margin
constexpr int DEFAULT_LAYOUT_SPACING = 4;      // Spacing between layout children
```

### Text Background Padding

Padding around text backgrounds (for readability):

```cpp
constexpr int DEFAULT_TEXT_BG_PADDING = 4;     // Standard padding
constexpr int LABEL_TEXT_BG_PADDING = 6;       // Label-specific (larger)
constexpr int TITLE_TEXT_BG_PADDING = 8;       // Title-specific (largest)
```

**Example:**

```cpp
// Labels automatically get text backgrounds
ui.createLabel("info", {x, y, 0, 0}, "Health: 100%");
// Uses LABEL_TEXT_BG_PADDING (6px) automatically

// Manual override for custom padding
ui.setTextBackgroundPadding("info", UIConstants::TITLE_TEXT_BG_PADDING);  // 8px
```

---

## List Component Constants

Configuration for list components:

```cpp
// Item sizing
constexpr int DEFAULT_LIST_ITEM_HEIGHT = 32;   // Height per item
constexpr int FALLBACK_LIST_ITEM_HEIGHT = 29;  // Fallback if font unavailable

// List dimensions
constexpr int MIN_LIST_WIDTH = 150;            // Minimum width
constexpr int DEFAULT_LIST_WIDTH = 200;        // Default width
constexpr int DEFAULT_LIST_VISIBLE_ITEMS = 3;  // Visible items before scrolling

// Performance
constexpr int MAX_LIST_TEXTURE_CACHE = 1000;   // Cached item textures
```

### Grow-Only Auto-Sizing

Lists use grow-only behavior for consistency:

```cpp
// List expands as items are added
ui.addListItem("my_list", "Item 1");  // List grows
ui.addListItem("my_list", "Item 2");  // List grows again
ui.addListItem("my_list", "Item 3");  // List grows again

// Removing items does NOT shrink list (grow-only)
ui.removeListItem("my_list", 0);      // Size maintained for UI stability
```

**Benefit**: Prevents jarring UI changes when content updates.

---

## Slider Component Constants

Configuration for slider controls:

```cpp
constexpr int SLIDER_TRACK_HEIGHT = 4;         // Track thickness
constexpr int SLIDER_TRACK_OFFSET = 2;         // Vertical centering
constexpr int SLIDER_HANDLE_WIDTH = 16;        // Handle size
constexpr int DEFAULT_SLIDER_HEIGHT = 30;      // Total component height
```

---

## Tooltip Constants

Configuration for tooltips (floating help text):

```cpp
// Sizing
constexpr int TOOLTIP_FALLBACK_WIDTH = 200;    // Width if content unknown
constexpr int TOOLTIP_FALLBACK_HEIGHT = 32;    // Height if content unknown

// Timing
constexpr float DEFAULT_TOOLTIP_DELAY = 1.0f;  // Delay before showing (seconds)
```

---

## Event Log Constants

Configuration for event log displays:

```cpp
// Capacity
constexpr int DEFAULT_EVENT_LOG_MAX_ENTRIES = 5;           // Max visible entries
constexpr float DEFAULT_EVENT_LOG_UPDATE_INTERVAL = 2.0f;  // Update interval (seconds)
```

**Usage:**

```cpp
// Create event log with default capacity
ui.createEventLog("game_log", bounds, UIConstants::DEFAULT_EVENT_LOG_MAX_ENTRIES);

// Add entries (oldest scrolls out when exceeding max)
ui.addEventLogEntry("game_log", "Player spawned");
ui.addEventLogEntry("game_log", "Enemy detected");
```

---

## Timing and Animation Constants

Animation and interaction timing:

```cpp
// Tooltips
constexpr float DEFAULT_TOOLTIP_DELAY = 1.0f;  // Hover delay (seconds)

// Scaling limit
constexpr float MAX_UI_SCALE = 3.0f;           // Allows high-DPI scaling up to 3x baseline
```

**Note:** `MAX_UI_SCALE = 3.0f` caps the UI scale factor while still allowing high-DPI displays to scale beyond baseline resolution when needed.

---

## Positioning Constants

Standard offsets for common positioning patterns:

```cpp
// Titles
constexpr int TITLE_TOP_OFFSET = 10;           // Distance from top
constexpr int DEFAULT_TITLE_HEIGHT = 40;       // Standard title height

// Buttons
constexpr int BUTTON_BOTTOM_OFFSET = 20;       // Distance from bottom
constexpr int DEFAULT_BUTTON_WIDTH = 120;      // Standard button width
constexpr int DEFAULT_BUTTON_HEIGHT = 40;      // Standard button height
```

### Example Usage

```cpp
auto& ui = UIManager::Instance();
int windowWidth = gameEngine.getWidthInPixels();
int windowHeight = gameEngine.getHeightInPixels();

// Positioned title (top with offset)
ui.createTitle("header",
    {0, UIConstants::TITLE_TOP_OFFSET, windowWidth, UIConstants::DEFAULT_TITLE_HEIGHT},
    "My Page");

// Bottom-aligned button
int buttonX = UIConstants::TITLE_TOP_OFFSET;
int buttonY = windowHeight - UIConstants::DEFAULT_BUTTON_HEIGHT - UIConstants::BUTTON_BOTTOM_OFFSET;
ui.createButton("back", {buttonX, buttonY, UIConstants::DEFAULT_BUTTON_WIDTH,
                        UIConstants::DEFAULT_BUTTON_HEIGHT}, "Back");
```

---

## Info/Status Label Sizing Constants

Standardized heights for info and status labels:

```cpp
// General info labels
constexpr int INFO_LABEL_HEIGHT = 36;          // Standard (for demo states)

// Variants
constexpr int INFO_LABEL_HEIGHT_STANDARD = 28; // Standard (increased for readability)
constexpr int INFO_LABEL_HEIGHT_COMPACT = 24;  // Compact (tighter layouts)
```

### Positioning

```cpp
// Vertical positioning for info lines
constexpr int INFO_FIRST_LINE_Y = 62;          // First info line Y position
constexpr int INFO_LINE_SPACING = 8;           // Gap between lines
constexpr int INFO_STATUS_SPACING = 4;         // Extra gap before status section

// Horizontal positioning
constexpr int INFO_LABEL_MARGIN_X = 10;        // Left margin for all info labels
```

**Example Layout:**

```
Y=10:   [Title]
        (gap of 5px)
Y=62:   [Info Line 1]
Y=70:   [Info Line 2]  (8px spacing)
        (gap of 4px for status)
Y=82:   [Status Line]
Y=90:   [Status Line 2] (8px spacing)
```

---

## Form/Settings Layout Constants

Standardized dimensions for form-based interfaces:

```cpp
// Vertical structure
constexpr int CONTENT_START_Y_AFTER_TABS = 160;    // Y where content begins
constexpr int FORM_ROW_HEIGHT = 60;                // Height per form row

// Horizontal structure
constexpr int FORM_LABEL_WIDTH = 250;              // Width of form labels
constexpr int FORM_CONTROL_WIDTH = 300;            // Width of form controls (sliders, inputs)
constexpr int FORM_LABEL_CONTROL_GAP = 20;         // Gap between label and control

// Vertical structure
constexpr int BOTTOM_BUTTON_MARGIN = 80;           // Distance from screen bottom
```

**Example Form Layout:**

```
┌─────────────────────────────────────────┐
│ Settings                                │  Y=10 (title)
├─────────────────────────────────────────┤
│ [Tabs: Graphics | Audio | Gameplay]     │  Y=40-100 (tabs)
├─────────────────────────────────────────┤
│                                         │  Y=160 (CONTENT_START_Y_AFTER_TABS)
│ Volume Label [────●──] 50%             │  (FORM_ROW_HEIGHT=60)
│                                         │
│ Brightness Label [────●──] 75%         │  (Y=160+60=220, then 280, etc)
│                                         │
│ Quality: [Dropdown: Ultra    ]         │
│                                         │
├─────────────────────────────────────────┤
│ [Save]  [Cancel]                        │  Y=Height-BOTTOM_BUTTON_MARGIN
└─────────────────────────────────────────┘
```

---

## Text Measurement Estimates

Fallback values for text measurement when FontManager metrics unavailable:

```cpp
// Character dimensions
constexpr int CHAR_WIDTH_ESTIMATE = 12;        // Estimated character width
constexpr int INPUT_CURSOR_CHAR_WIDTH = 8;     // Character width for cursor positioning
```

These are used as fallbacks when precise font metrics can't be determined.

---

## Performance/Memory Constants

Constants for optimization and memory management:

```cpp
// Component removal batching
constexpr int DEFAULT_COMPONENT_BATCH_SIZE = 32;  // Typical batch size
constexpr int MAX_COMPONENT_BATCH_SIZE = 64;      // Maximum batch size

// Usage:
// - Reserve size for temporary vectors during component removal
// - Reduces reallocations during bulk operations
```

---

## Best Practices

### ✅ DO: Use Constants

```cpp
// Good: Self-documenting, easy to maintain
int buttonX = UIConstants::TITLE_TOP_OFFSET;
int buttonY = windowHeight - UIConstants::DEFAULT_BUTTON_HEIGHT -
              UIConstants::BUTTON_BOTTOM_OFFSET;

ui.createButton("back_btn", {buttonX, buttonY,
                            UIConstants::DEFAULT_BUTTON_WIDTH,
                            UIConstants::DEFAULT_BUTTON_HEIGHT}, "Back");
```

### ❌ DON'T: Magic Numbers

```cpp
// Bad: Hard to maintain, unclear intent
ui.createButton("back_btn", {10, 550, 120, 40}, "Back");

// Unclear: What do 550, 120, 40 mean?
// Hard to change: If you want to adjust all button heights, you must search for "40"
```

### ✅ DO: Reference Constants for Scaling

```cpp
// Good: Scale-aware positioning
float scale = ui.calculateOptimalScale(windowWidth, windowHeight);
int adjustedPadding = static_cast<int>(UIConstants::DEFAULT_COMPONENT_PADDING * scale);
```

### ✅ DO: Use Semantic Positioning

```cpp
// Good: Clear intent with positioning modes
ui.setComponentPositioning("my_button", {
    UIPositionMode::BOTTOM_ALIGNED,
    UIConstants::TITLE_TOP_OFFSET,  // offsetX
    UIConstants::BUTTON_BOTTOM_OFFSET,  // offsetY
    UIConstants::DEFAULT_BUTTON_WIDTH,
    UIConstants::DEFAULT_BUTTON_HEIGHT
});
```

---

## Integration with UIManager

See [UIManager Guide](UIManager_Guide.md) for usage patterns and integration examples.

Key sections that reference UIConstants:
- [Basic Pattern](UIManager_Guide.md#basic-pattern) - State integration
- [Positioning](UIManager_Guide.md#positioning) - Resize-aware helper usage
- [Rendering Notes](UIManager_Guide.md#rendering-notes) - Fixed UI render-family behavior
- [Auto-Repositioning System](UIManager_Guide.md#auto-repositioning-system) - Responsive positioning
- [Best Practices](UIManager_Guide.md#best-practices) - When to use constants

---

## Related Files

- **Implementation**: `include/managers/UIConstants.hpp`
- **UI System**: [UIManager Guide](UIManager_Guide.md)
- **Automatic Sizing**: [Auto-Sizing System](Auto_Sizing_System.md)
- **DPI Awareness**: [DPI-Aware Font System](DPI_Aware_Font_System.md)
- **Responsive Positioning**: [UIManager Guide - Auto-Repositioning](UIManager_Guide.md#auto-repositioning-system)
