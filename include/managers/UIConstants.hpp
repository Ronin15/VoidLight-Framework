/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef UI_CONSTANTS_HPP
#define UI_CONSTANTS_HPP

#include <string_view>

namespace UIConstants {
  // Standard UI Fonts
  constexpr std::string_view FONT_UI = "fonts_UI_Arial";
  constexpr std::string_view FONT_TITLE = "fonts_title_Arial";
  constexpr std::string_view FONT_TOOLTIP = "fonts_tooltip_Arial";
  constexpr std::string_view FONT_DEFAULT = "fonts_UI_Arial";

  // UI Baseline Resolution (for scaling calculations)
  // This is the reference resolution used for UI design - all UI elements are defined
  // in this coordinate space and then scaled to the actual window resolution
  constexpr int BASELINE_WIDTH = 1920;
  constexpr int BASELINE_HEIGHT = 1080;
  constexpr float BASELINE_WIDTH_F = 1920.0f;
  constexpr float BASELINE_HEIGHT_F = 1080.0f;

  // UI Component Spacing Constants (in baseline pixels)
  // These values are scaled by m_globalScale at runtime for resolution-aware spacing
  constexpr int CHECKBOX_SIZE = 24;
  constexpr int TOOLTIP_PADDING_WIDTH = 16;
  constexpr int TOOLTIP_PADDING_HEIGHT = 8;
  constexpr int TOOLTIP_MOUSE_OFFSET = 10;
  constexpr int INPUT_CURSOR_SPACE = 20;
  constexpr int LIST_ITEM_PADDING = 8;
  constexpr int SCROLLBAR_WIDTH = 20;

  // Z-Order Priority Constants
  // Controls input priority and same-family render order. UIManager submits
  // fixed render families for GPU performance: primitives, images, then text.
  constexpr int ZORDER_OVERLAY = -10;         // Lowest priority overlay backgrounds
  constexpr int ZORDER_PANEL = 0;             // Background panels
  constexpr int ZORDER_DIALOG = 2;            // Non-modal dialog priority
  constexpr int ZORDER_IMAGE = 1;             // Background images
  constexpr int ZORDER_PROGRESS_BAR = 5;      // Progress indicators
  constexpr int ZORDER_EVENT_LOG = 6;         // Event log displays
  constexpr int ZORDER_LIST = 8;              // List components
  constexpr int ZORDER_BUTTON = 10;           // Interactive buttons
  constexpr int ZORDER_SLIDER = 12;           // Slider controls
  constexpr int ZORDER_CHECKBOX = 13;         // Checkbox controls
  constexpr int ZORDER_INPUT_FIELD = 15;      // Text input fields
  constexpr int ZORDER_LABEL = 20;            // Text labels
  constexpr int ZORDER_TITLE = 25;            // Title text
  constexpr int ZORDER_MODAL_OVERLAY = 500;   // Modal overlay render/input cutoff
  constexpr int ZORDER_MODAL_DIALOG = 600;    // Modal dialog content priority
  constexpr int ZORDER_TOOLTIP = 1000;        // Highest tooltip priority

  // Border Width Constants
  constexpr int BORDER_WIDTH_NONE = 0;        // No border
  constexpr int BORDER_WIDTH_NORMAL = 1;      // Standard 1px border
  constexpr int BORDER_WIDTH_DIALOG = 2;      // Thicker dialog border
  constexpr int DEBUG_BORDER_WIDTH = 1;       // Debug mode border width

  // Font Size Constants
  constexpr int DEFAULT_FONT_SIZE = 16;       // Standard UI text size
  constexpr int TITLE_FONT_SIZE = 24;         // Title text size

  // Component Default Size Constants
  constexpr int MIN_COMPONENT_WIDTH = 32;     // Minimum component width
  constexpr int MIN_COMPONENT_HEIGHT = 16;    // Minimum component height
  constexpr int MAX_COMPONENT_WIDTH = 800;    // Maximum component width
  constexpr int MAX_COMPONENT_HEIGHT = 600;   // Maximum component height
  constexpr int DEFAULT_INPUT_MAX_LENGTH = 256; // Max input field characters

  // Dialog/Modal Size Constants (baseline pixels, auto-scaled by UIManager)
  constexpr int DEFAULT_DIALOG_WIDTH = 400;   // Default modal dialog width
  constexpr int DEFAULT_DIALOG_HEIGHT = 200;  // Default modal dialog height

  // Padding and Spacing Defaults
  constexpr int DEFAULT_COMPONENT_PADDING = 8;  // Default component internal padding
  constexpr int DEFAULT_CONTENT_PADDING = 8;    // Default content padding for auto-sizing
  constexpr int DEFAULT_MARGIN = 4;             // Default component margin
  constexpr int DEFAULT_LAYOUT_SPACING = 4;    // Default spacing between layout children
  constexpr int DEFAULT_TEXT_BG_PADDING = 4;   // Default text background padding
  constexpr int LABEL_TEXT_BG_PADDING = 6;     // Label-specific text background padding
  constexpr int TITLE_TEXT_BG_PADDING = 8;     // Title-specific text background padding

  // List Component Constants
  constexpr int DEFAULT_LIST_ITEM_HEIGHT = 32; // Default height per list item
  constexpr int FALLBACK_LIST_ITEM_HEIGHT = 29; // Fallback when font metrics unavailable (21px font + 8px padding)
  constexpr int MIN_LIST_WIDTH = 150;          // Minimum list width
  constexpr int DEFAULT_LIST_WIDTH = 200;      // Default list width
  constexpr int DEFAULT_LIST_VISIBLE_ITEMS = 3; // Default number of visible list items
  constexpr int MAX_LIST_TEXTURE_CACHE = 1000; // Maximum cached list item textures

  // Slider Component Constants
  constexpr int SLIDER_TRACK_HEIGHT = 4;       // Height of slider track
  constexpr int SLIDER_TRACK_OFFSET = 2;       // Vertical offset for slider track centering
  constexpr int SLIDER_HANDLE_WIDTH = 16;      // Width of slider handle

  // Tooltip Constants
  constexpr int TOOLTIP_FALLBACK_WIDTH = 200;  // Fallback tooltip width
  constexpr int TOOLTIP_FALLBACK_HEIGHT = 32;  // Fallback tooltip height

  // Event Log Constants
  constexpr int DEFAULT_EVENT_LOG_MAX_ENTRIES = 5; // Default max event log entries
  constexpr float DEFAULT_EVENT_LOG_UPDATE_INTERVAL = 2.0f; // Default update interval in seconds
  constexpr float EVENT_LOG_WIDTH_PERCENT = 0.30f; // Event log spans 30% of window width

  // Time Status Bar Constants
  constexpr int TIME_STATUS_WIDTH = 280;        // Width of time status panel
  constexpr int TIME_STATUS_HEIGHT = 32;        // Height of time status panel
  constexpr int TIME_STATUS_TOP_OFFSET = 10;    // Offset from top of screen
  constexpr int TIME_STATUS_RIGHT_OFFSET = 10;  // Offset from right edge

  // Full-Width Status Bar Constants
  constexpr int STATUS_BAR_HEIGHT = 40;           // Height of full-width status bar
  constexpr int STATUS_BAR_LABEL_PADDING = 12;    // Label text inset from panel edges
  constexpr int FPS_COUNTER_WIDTH = 160;           // Width for high-DPI FPS badge

  // Timing and Animation Constants
  constexpr float DEFAULT_TOOLTIP_DELAY = 1.0f; // Default tooltip hover delay in seconds
  constexpr float MAX_UI_SCALE = 3.0f;          // Maximum UI scale factor (allows high-DPI scaling)

  // Positioning Constants
  constexpr int TITLE_TOP_OFFSET = 10;          // Default top offset for titles
  constexpr int BUTTON_BOTTOM_OFFSET = 20;      // Default bottom offset for buttons
  constexpr int BOTTOM_RIGHT_OFFSET_X = 10;     // Default offset from right edge for bottom-right elements
  constexpr int BOTTOM_RIGHT_OFFSET_Y = 10;     // Default offset from bottom edge for bottom-right elements
  constexpr int DEFAULT_TITLE_HEIGHT = 40;      // Default title component height
  constexpr int DEFAULT_BUTTON_WIDTH = 120;     // Default button width
  constexpr int DEFAULT_BUTTON_HEIGHT = 40;     // Default button height

  // Info/Status Label Sizing Constants (baseline pixels, auto-scaled by UIManager)
  constexpr int INFO_LABEL_HEIGHT = 36;            // Standard info label height for demo states (balanced for windowed and fullscreen)
  constexpr int INFO_LABEL_HEIGHT_STANDARD = 28;   // Standard info/instruction labels (increased for readability)
  constexpr int INFO_LABEL_HEIGHT_COMPACT = 24;    // Compact info labels (tighter layouts)

  // Info Label Positioning Constants (baseline pixels)
  constexpr int INFO_FIRST_LINE_Y = 62;            // Y position of first info line after title (12px gap from title end)
  constexpr int INFO_LINE_SPACING = 8;             // Vertical gap between consecutive info lines
  constexpr int INFO_STATUS_SPACING = 4;           // Extra gap before status line
  constexpr int INFO_LABEL_MARGIN_X = 10;          // Left margin for info labels

  // Form/Settings Layout Constants (reusable for any form-like interface)
  constexpr int CONTENT_START_Y_AFTER_TABS = 160;    // Y position where content starts below tabs
  constexpr int FORM_ROW_HEIGHT = 60;                // Vertical spacing between form rows
  constexpr int FORM_LABEL_WIDTH = 250;              // Standard width for form labels
  constexpr int FORM_CONTROL_WIDTH = 300;            // Standard width for form controls (sliders, inputs)
  constexpr int FORM_LABEL_CONTROL_GAP = 20;         // Horizontal gap between label and control
  constexpr int DEFAULT_SLIDER_HEIGHT = 30;          // Standard slider component height
  constexpr int BOTTOM_BUTTON_MARGIN = 80;           // Distance from screen bottom for bottom-aligned buttons

  // Text Measurement Estimates
  constexpr int CHAR_WIDTH_ESTIMATE = 12;       // Estimated character width for fallback calculations
  constexpr int INPUT_CURSOR_CHAR_WIDTH = 8;    // Character width estimate for input cursor positioning

  // Performance/Memory Constants
  constexpr int DEFAULT_COMPONENT_BATCH_SIZE = 32;  // Reserve size for component removal operations
  constexpr int MAX_COMPONENT_BATCH_SIZE = 64;      // Reserve size for bulk component clearing

  // Debug Profiler Overlay Constants (Debug builds only)
  constexpr int PROFILER_OVERLAY_WIDTH = 500;       // Width of profiler overlay panel
  constexpr int PROFILER_OVERLAY_HEIGHT = 210;      // Height of profiler overlay panel (7 lines)
  constexpr int PROFILER_OVERLAY_MARGIN = 10;       // Margin from screen edge
  constexpr int PROFILER_LINE_HEIGHT = 27;          // Height per profiler text line
  constexpr int PROFILER_LABEL_COUNT = 7;           // Number of profiler labels
  constexpr int PROFILER_ZORDER_PANEL = 9000;       // Z-order for profiler panel (high priority)
  constexpr int PROFILER_ZORDER_LABEL = 9001;       // Z-order for profiler labels

} // namespace UIConstants

#endif // UI_CONSTANTS_HPP
