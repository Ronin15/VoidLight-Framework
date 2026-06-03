# Auto-Sizing System

## Overview

The Auto-Sizing System is a core feature of the UIManager that automatically calculates optimal component dimensions based on content. This system eliminates manual sizing calculations and provides consistent, content-aware layouts across all UI components.

## Baseline Resolution System

The auto-sizing system works within the UIManager's **baseline resolution** of 1920×1080 pixels. All size measurements and constants are defined for this reference resolution, then automatically scaled to fit any window size.

```cpp
// From UIConstants.hpp
constexpr int BASELINE_WIDTH = 1920;
constexpr int BASELINE_HEIGHT = 1080;
```

See [UIConstants Reference - Baseline Resolution System](UIConstants.md#baseline-resolution-system) for complete details on:
- How baseline resolution works
- Automatic scaling with `calculateOptimalScale()`
- Small screen support (1280×720 minimum)

**Key Point:** Auto-sizing calculations are performed in baseline coordinate space, then the entire UI scales to match the actual window size.

## Core Features

### Content-Aware Sizing
- **Text Measurement**: Precise calculation using FontManager with actual font metrics
- **Multi-line Detection**: Automatic detection and sizing for text containing newlines
- **Font-Based Calculations**: All measurements based on real font dimensions, not estimates
- **Padding Integration**: Automatic content padding for proper spacing (uses `UIConstants::DEFAULT_CONTENT_PADDING`)

### Smart Centering
- **Title Auto-Centering**: Titles with CENTER alignment automatically reposition to stay centered
- **Alignment Preservation**: Component alignment maintained after auto-sizing
- **Position Stability**: Auto-sizing affects dimensions only, never base positioning

### SDL3_GPU Integration
- **Coordinate Compatibility**: Uses the engine's pixel-space GPU/UI coordinate system
- **DPI Awareness**: Integrates with GameEngine's DPI detection for optimal sizing
- **Input Accuracy**: Proper mouse coordinate handling through InputManager pixel-density scaling

## How It Works

### Component Auto-Sizing Properties

```cpp
struct UIComponent {
    // Auto-sizing enabled by default
    bool autoSize{true};
    // Min/max constraints from UIConstants
    UIRect minBounds{0, 0,
        UIConstants::MIN_COMPONENT_WIDTH,
        UIConstants::MIN_COMPONENT_HEIGHT};
    UIRect maxBounds{0, 0,
        UIConstants::MAX_COMPONENT_WIDTH,
        UIConstants::MAX_COMPONENT_HEIGHT};
    int contentPadding{UIConstants::DEFAULT_CONTENT_PADDING};  // Padding around content
    bool autoWidth{true};               // Auto-size width based on content
    bool autoHeight{true};              // Auto-size height based on content

    // Callback for content changes
    std::function<void()> onContentChanged;
};
```

### FontManager Integration

```cpp
// Single-line text measurement
bool measureText(const std::string& text, const std::string& fontID, int* width, int* height);

// Multi-line text measurement (automatically detects newlines)
bool measureMultilineText(const std::string& text, const std::string& fontID, 
                         int maxWidth, int* width, int* height);

// Font metrics for spacing calculations
bool getFontMetrics(const std::string& fontID, int* lineHeight, int* ascent, int* descent);
```

### Auto-Sizing Algorithm

1. **Content Analysis**: Detect text type (single-line vs multi-line)
2. **Measurement**: Use appropriate FontManager measurement function
3. **Padding Application**: Add content padding to measured dimensions
4. **Constraint Enforcement**: Apply minimum and maximum size limits
5. **Position Adjustment**: Handle special cases like title centering
6. **Callback Execution**: Trigger onContentChanged if defined

## Usage Examples

### Basic Auto-Sizing

```cpp
// Components automatically size to fit content (width/height = 0)
ui.createLabel("info", {x, y, 0, 0}, "This text will auto-size");
ui.createButton("action", {x, y, 0, 0}, "Click Me");

// Multi-line content automatically detected and sized
ui.createLabel("multi", {x, y, 0, 0}, "Line 1\nLine 2\nLine 3");
```

### Fixed-Size Components

Some components use fixed-size design for specific purposes:

```cpp
// Event logs use fixed dimensions (industry standard)
ui.createEventLog("events", {x, y, 400, 200}, 10);
ui.addEventLogEntry("events", "Long messages automatically wrap within bounds");

// Lists use fixed dimensions with proper padding
ui.createList("options", {x, y, 220, 140});
ui.addListItem("options", "List items fit within bounds");
```

**Fixed-Size Benefits:**
- **Predictable layout**: UI elements don't shift when content changes
- **Performance**: No recalculation needed when adding content
- **Industry standard**: Follows established patterns for logs and lists
- **Word wrapping**: Long content wraps automatically within bounds

### Title Auto-Centering

```cpp
// Titles automatically center when using CENTER alignment
ui.createTitle("header", {0, y, windowWidth, 40}, "Page Title");
ui.setTitleAlignment("header", UIAlignment::CENTER_CENTER);
// Title automatically repositions to center of screen after auto-sizing
```

### Manual Control

```cpp
// Disable auto-sizing for specific components
ui.enableAutoSizing("fixed_button", false);

// Set custom size constraints
UIRect minBounds{0, 0, 100, 30};   // Minimum 100x30
UIRect maxBounds{0, 0, 400, 100};  // Maximum 400x100
ui.setAutoSizingConstraints("constrained_label", minBounds, maxBounds);

// Manually trigger recalculation
ui.calculateOptimalSize("dynamic_content");
```

## Component-Specific Behavior

### Labels and Titles
- **Single-line text**: Measured using `measureText()`
- **Multi-line text**: Automatically detected, measured using `measureMultilineText()`
- **Empty text**: Falls back to minimum bounds
- **Title centering**: Automatic repositioning for CENTER-aligned titles

### Buttons
- **Content-based sizing**: Measures button text and adds appropriate padding
- **Consistent padding**: All button types use same padding calculations
- **Type independence**: All button types (regular, success, warning, danger) behave consistently

### Lists and Event Logs
- **Font-based item heights**: Uses font line height + padding instead of hardcoded values
- **Dynamic item sizing**: List items automatically size based on font metrics
- **Content measurement**: Measures list item text for width calculations

### Input Fields
- **Placeholder consideration**: Measures placeholder text if no content present
- **Content adaptation**: Sizes to fit current text content
- **Interaction space**: Adds extra space for cursor and user interaction

### Display-Aware Font Loading

The auto-sizing system integrates with the DPI-aware font system for optimal text scaling:

```cpp
// FontManager automatically calculates DPI-scaled font sizes
// See DPI_Aware_Font_System.md for complete details

// Automatic DPI detection and font scaling
float dpiScale = GameEngine::Instance().getDPIScale();
int baseFontSize = static_cast<int>(std::round(24.0f * dpiScale / 2.0f) * 2.0f);
int uiFontSize = static_cast<int>(std::round(18.0f * dpiScale / 2.0f) * 2.0f);

// Auto-sizing uses these DPI-scaled fonts for accurate measurements
int width, height;
FontManager::Instance().measureText("Sample Text", "fonts_UI_Arial", &width, &height);
```

## Performance Considerations

### Efficient Measurement
- **FontManager Caching**: Caching for font metrics reduces recalculation
- **Single Calculation**: Auto-sizing occurs once during component creation
- **Lazy Evaluation**: Only recalculates when content actually changes

### Threading Compatibility
- **Main Thread Only**: Auto-sizing operations occur on main rendering thread
- **No Blocking**: Quick calculations don't impact frame rate
- **Background Loading**: Font loading occurs in background threads

### Memory Efficiency
- **Shared Resources**: FontManager instances shared across all components
- **Smart Pointers**: Automatic memory management for font resources
- **Minimal Overhead**: Auto-sizing properties add minimal memory footprint

## Best Practices

### Component Creation
```cpp
// Recommended: Let auto-sizing handle dimensions
ui.createLabel("dynamic", {x, y, 0, 0}, "Content");  // Width/height = 0

// Avoid: Hardcoded sizes that conflict with content
ui.createLabel("fixed", {x, y, 200, 30}, "Very long text that might not fit");
```

### Content Management
```cpp
// Good: Update text and let auto-sizing adapt
ui.setText("dynamic_label", "New longer content");
ui.calculateOptimalSize("dynamic_label");

// Good: Use callbacks for dynamic content
component->onContentChanged = [this]() {
    // Handle size changes, update layout, etc.
};
```

### Layout Integration
```cpp
// Recommended: Calculate auto-sized components before layout
ui.calculateOptimalSize("header");
ui.calculateOptimalSize("content");
ui.updateLayout("main_layout");

// Recommended: Use layout invalidation for content changes
ui.invalidateLayout("main_layout");  // Triggers recalculation of all child components
```

## Troubleshooting

### Common Issues

**Components not sizing correctly:**
- Verify font is loaded and accessible
- Check that autoSize is enabled for the component
- Ensure text content is not empty

**Text appearing cut off:**
- Check minimum/maximum size constraints
- Verify content padding settings
- Ensure container has sufficient space

**Titles not centering:**
- Confirm title alignment is set to CENTER_CENTER
- Verify auto-sizing is enabled for the title
- Check that title creation uses full window width

### Debug Techniques

```cpp
// Enable debug logging for specific components
if (component->type == UIComponentType::BUTTON) {
    std::cout << "Auto-sizing button '" << component->id << "' with text '" << component->text << 
                 "': measured=" << contentWidth << "x" << contentHeight << std::endl;
}

// Verify font availability
if (!FontManager::Instance().isFontLoaded("fonts_UI_Arial")) {
    std::cerr << "Required font not loaded" << std::endl;
}

// Check size constraints
std::cout << "Component bounds: " << component->bounds.width << "x" << component->bounds.height << 
             ", min: " << component->minBounds.width << "x" << component->minBounds.height << std::endl;
```

## API Reference

### Core Methods
```cpp
// Enable/disable auto-sizing
void enableAutoSizing(const std::string& id, bool enable = true);

// Set size constraints
void setAutoSizingConstraints(const std::string& id, const UIRect& minBounds, const UIRect& maxBounds);

// Manual size calculation
void calculateOptimalSize(const std::string& id);
void calculateOptimalSize(std::shared_ptr<UIComponent> component);

// Content measurement
bool measureComponentContent(const std::shared_ptr<UIComponent>& component, int* width, int* height);

// Layout integration
void invalidateLayout(const std::string& layoutID);
void recalculateLayout(const std::string& layoutID);
```

### FontManager Integration
```cpp
// Text measurement utilities
bool measureText(const std::string& text, const std::string& fontID, int* width, int* height);
bool measureMultilineText(const std::string& text, const std::string& fontID, int maxWidth, int* width, int* height);
bool getFontMetrics(const std::string& fontID, int* lineHeight, int* ascent, int* descent);

// Display-aware font loading
bool loadFontsForDisplay(const std::string& fontPath, int windowWidth, int windowHeight);
```

## Conclusion

The Auto-Sizing System provides a robust foundation for content-aware UI layout. By automatically calculating optimal component dimensions based on actual content and font metrics, it eliminates manual sizing calculations while ensuring consistent, professional-looking interfaces across all display types and resolutions.

The system's integration with SDL3's coordinate transformation and the engine's DPI detection ensures accurate input handling and crisp text rendering on all platforms.
