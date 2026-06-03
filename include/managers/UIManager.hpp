/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef UI_MANAGER_HPP
#define UI_MANAGER_HPP

#include "utils/Vector2D.hpp"
#include "gpu/UIRenderBatches.hpp"
#include "utils/TextureSource.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "managers/UIConstants.hpp" // Added for font constants

// Forward declarations
class FontManager;
class InputManager;
struct SDL_GPURenderPass;

namespace VoidLight {
class GPURenderer;
}

// UI render batch capacities (avoids per-frame reallocations)
constexpr size_t UI_TEXT_BATCH_CAPACITY = 256;
constexpr size_t UI_IMAGE_BATCH_CAPACITY = 32;

// UI Component Types
enum class UIComponentType {
  BUTTON,
  BUTTON_DANGER,  // Red colored buttons (Back, Quit, Exit, Delete, etc.)
  BUTTON_SUCCESS, // Green colored buttons (Save, Confirm, Accept, etc.)
  BUTTON_WARNING, // Orange/Yellow colored buttons (Caution, Reset, etc.)
  LABEL,
  TITLE,
  PANEL,
  PROGRESS_BAR,
  INPUT_FIELD,
  IMAGE,
  SLIDER,
  CHECKBOX,
  LIST,
  TOOLTIP,
  EVENT_LOG,
  DIALOG
};

// Layout Types
enum class UILayoutType { ABSOLUTE_POS, FLOW, GRID, STACK, ANCHOR };

// Position Modes for auto-repositioning on window resize
enum class UIPositionMode {
  ABSOLUTE,        // Fixed x,y (default, backward compatible)
  CENTERED_H,      // Horizontal center + offsetX, fixed offsetY
  CENTERED_V,      // Vertical center + offsetY, fixed offsetX
  CENTERED_BOTH,   // Center both axes + offsets
  TOP_ALIGNED,     // Top-left: x = offsetX, y = offsetY
  TOP_RIGHT,       // Top-right: x = right - width - offsetX, y = offsetY
  BOTTOM_ALIGNED,  // Bottom-left: x = offsetX, y = bottom - height - offsetY
  BOTTOM_CENTERED, // Bottom center: horizontally centered, y = bottom - height - offsetY
  BOTTOM_RIGHT,    // Bottom-right: x = right - width - offsetX, y = bottom - height - offsetY
  LEFT_ALIGNED,    // Left edge + offsetX, vertically centered
  RIGHT_ALIGNED    // Right edge - width - offsetX, vertically centered
};

// UI Positioning structure for auto-repositioning
struct UIPositioning {
  UIPositionMode mode{UIPositionMode::ABSOLUTE};
  int offsetX{0};      // Offset from positioning anchor
  int offsetY{0};      // Offset from positioning anchor
  int fixedWidth{0};   // Fixed width (0 = use current width)
  int fixedHeight{0};  // Fixed height (0 = use current height)
  float widthPercent{0.0f};   // Width as fraction of window (e.g., 0.57 = 57%), takes precedence over fixedWidth
  float heightPercent{0.0f};  // Height as fraction of window, takes precedence over fixedHeight
};

// UI States
enum class UIState { NORMAL, HOVERED, PRESSED, DISABLED, FOCUSED };

// Alignment options
enum class UIAlignment {
  LEFT,
  CENTER,
  RIGHT,
  TOP,
  BOTTOM,
  TOP_LEFT,
  TOP_CENTER,
  TOP_RIGHT,
  CENTER_LEFT,
  CENTER_CENTER,
  CENTER_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_CENTER,
  BOTTOM_RIGHT
};

// UI Rectangle structure
struct UIRect {
  int x{0};
  int y{0};
  int width{0};
  int height{0};

  UIRect() = default;
  UIRect(int x_, int y_, int w_, int h_)
      : x(x_), y(y_), width(w_), height(h_) {}

  bool contains(int px, int py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }

};

// UI Style structure
struct UIStyle {
  SDL_Color backgroundColor{50, 50, 50, 255};
  SDL_Color borderColor{100, 100, 100, 255};
  SDL_Color textColor{255, 255, 255, 255};
  SDL_Color hoverColor{70, 70, 70, 255};
  SDL_Color pressedColor{30, 30, 30, 255};
  SDL_Color disabledColor{80, 80, 80, 128};

  // Text background properties (for labels and titles)
  SDL_Color textBackgroundColor{0, 0, 0,
                                128}; // Semi-transparent black by default
  bool useTextBackground{false};      // Enable text background for readability
  int textBackgroundPadding{UIConstants::DEFAULT_TEXT_BG_PADDING};       // Extra padding around text background
  // Passive mouse-hover effects are opt-in. Hit testing still runs for all
  // visible/enabled components regardless of these flags.
  bool highlightOnMouseHover{false};
  bool showTooltipOnMouseHover{false};

  int borderWidth{UIConstants::BORDER_WIDTH_NORMAL};
  int padding{UIConstants::DEFAULT_COMPONENT_PADDING};
  int margin{UIConstants::DEFAULT_MARGIN};
  int listItemHeight{UIConstants::DEFAULT_LIST_ITEM_HEIGHT}; // Configurable height for list items (increased from
                          // 20 for better mouse accuracy)

  std::string fontID{UIConstants::FONT_UI};
  int fontSize{UIConstants::DEFAULT_FONT_SIZE};

  UIAlignment textAlign{UIAlignment::CENTER_CENTER};
};

// Base UI Component
struct UIComponent {
  std::string m_id{};
  UIComponentType m_type{};
  UIRect m_bounds{};
  UIState m_state{UIState::NORMAL};
  UIStyle m_style{};
  bool m_visible{true};
  bool m_enabled{true};
  int m_zOrder{0};
  // When true, this component swallows mouse hover/press for any lower-z component
  // that shares the cursor position. Used by modal overlays to block click-through
  // to UI beneath them. Non-interactive types (PANEL etc.) otherwise let input
  // fall through to whatever is underneath.
  bool m_blocksInputBelow{false};
  // Render occlusion is separate from input blocking. Modal overlays set both:
  // input uses this component as a hit barrier, while rendering skips lower
  // normal UI before fixed render-family submission begins.
  bool m_occludesRenderingBelow{false};

  // Auto-repositioning properties
  UIPositioning m_positioning{};

  // Auto-sizing properties
  bool m_autoSize{true}; // Enable content-aware auto-sizing by default
  UIRect m_minBounds{0, 0, UIConstants::MIN_COMPONENT_WIDTH,
                   UIConstants::MIN_COMPONENT_HEIGHT}; // Minimum size constraints (only width/height used)
  UIRect m_maxBounds{0, 0, UIConstants::MAX_COMPONENT_WIDTH,
                   UIConstants::MAX_COMPONENT_HEIGHT};    // Maximum size constraints (only width/height used)
  int m_contentPadding{UIConstants::DEFAULT_CONTENT_PADDING};    // Padding around content for size calculations
  bool m_autoWidth{true};     // Auto-size width based on content
  bool m_autoHeight{true};    // Auto-size height based on content
  bool m_sizeToContent{true}; // Size exactly to fit content (vs. expand to fill)

  // Component-specific data
  std::string m_text{};
  std::function<std::string()> m_textBinding{}; // For data-bound text
  bool m_bindingDirty{true}; // Skip binding callbacks when false (perf optimization)
  std::string m_textureID{};
  UIRect m_imageSourceRect{};
  bool m_useImageSourceRect{false};
  float m_value{0.0f};
  float m_minValue{0.0f};
  float m_maxValue{1.0f};
  bool m_checked{false};
  std::vector<std::string> m_listItems{};
  bool m_listItemsDirty{true}; // Flag to regenerate textures
  std::function<void(std::vector<std::string>&, std::vector<std::pair<std::string, int>>&)> m_listBinding{}; // Zero-allocation: populates reusable buffers
  mutable std::vector<std::string> m_listBindingBuffer{}; // Reusable buffer for list binding output
  mutable std::vector<std::pair<std::string, int>> m_listSortBuffer{}; // Reusable buffer for inventory-style sorting
  int m_selectedIndex{-1};
  std::string m_placeholder{};
  int m_maxLength{UIConstants::DEFAULT_INPUT_MAX_LENGTH};

  // Callbacks
  std::function<void()> m_onClick{};
  std::function<void(float)> m_onValueChanged{};
  std::function<void(const std::string &)> m_onTextChanged{};
  std::function<void()> m_onHover{};
  std::function<void()> m_onFocus{};
  std::function<void()>
      m_onContentChanged{}; // Called when content changes and resize is needed

  // Parent/child relationship — parents are PANEL/DIALOG containers that
  // provide a backdrop. Children inherit that backdrop so default glyph
  // text-backgrounds (redundant over a parent backdrop) are suppressed.
  // Empty parent id means top-level component.
  std::string m_parentId{};
  std::vector<std::string> m_childIds{};
  bool m_hasBackdropAncestor{false};

  virtual ~UIComponent() = default;
};

// Layout Container
struct UILayout {
  std::string m_id{};
  UILayoutType m_type{UILayoutType::ABSOLUTE_POS};
  UIRect m_bounds{};
  std::vector<std::string> m_childComponents{};

  // Layout-specific properties
  int m_spacing{UIConstants::DEFAULT_LAYOUT_SPACING};
  int m_columns{1};
  int m_rows{1};
  UIAlignment m_alignment{UIAlignment::TOP_LEFT};
  bool m_autoSize{false};
};

// UI Theme
struct UITheme {
  std::string m_name{"default"};
  std::unordered_map<UIComponentType, UIStyle> m_componentStyles{};

  UIStyle getStyle(UIComponentType type) const {
    auto it = m_componentStyles.find(type);
    return (it != m_componentStyles.end()) ? it->second : UIStyle{};
  }
};

// Animation data
struct UIAnimation {
  std::string m_componentID{};
  float m_duration{0.0f};
  float m_elapsed{0.0f};
  bool m_active{false};

  UIRect m_startBounds{};
  UIRect m_targetBounds{};
  SDL_Color m_startColor{};
  SDL_Color m_targetColor{};

  std::function<void()> m_onComplete{};
};

// Event log state for auto-updating
struct EventLogState {
  float m_timer{0.0f};
  int m_messageIndex{0};
  float m_updateInterval{2.0f};
  bool m_autoUpdate{false};
};

class UIManager {
public:
  ~UIManager() {
    if (!m_isShutdown) {
      clean();
    }
  }

  static UIManager &Instance() {
    static UIManager instance;
    return instance;
  }

  // Core system methods
  bool init();
  void update(float deltaTime);
  void clean();
  bool isShutdown() const { return m_isShutdown; }

  // GPU rendering methods
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer);
  void renderGPU(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* pass);

  // Window resize notification (called by InputManager on SDL_EVENT_WINDOW_RESIZED)
  void onWindowResize(int newWidthInPixels, int newHeightInPixels);

  // UI Component creation methods. The optional `parentId` attaches the new
  // component to a parent container (typically a PANEL or DIALOG) so that
  // visibility cascades and the child inherits the parent's backdrop —
  // suppressing redundant text-backgrounds that would otherwise bleed past
  // the parent's edges at small scale.
  void createButton(const std::string &id, const UIRect &bounds,
                    const std::string &text = "",
                    const std::string &parentId = "");
  void createButtonDanger(const std::string &id, const UIRect &bounds,
                          const std::string &text = "",
                          const std::string &parentId = "");
  void createButtonSuccess(const std::string &id, const UIRect &bounds,
                           const std::string &text = "",
                           const std::string &parentId = "");
  void createButtonWarning(const std::string &id, const UIRect &bounds,
                           const std::string &text = "",
                           const std::string &parentId = "");
  void createLabel(const std::string &id, const UIRect &bounds,
                   const std::string &text = "",
                   const std::string &parentId = "");
  void createTitle(const std::string &id, const UIRect &bounds,
                   const std::string &text,
                   const std::string &parentId = "");
  void createPanel(const std::string &id, const UIRect &bounds,
                   const std::string &parentId = "");
  void createProgressBar(const std::string &id, const UIRect &bounds,
                         float minVal = 0.0f, float maxVal = 1.0f,
                         const std::string &parentId = "");
  void createInputField(const std::string &id, const UIRect &bounds,
                        const std::string &placeholder = "",
                        const std::string &parentId = "");
  void createImage(const std::string &id, const UIRect &bounds,
                   const std::string &textureID = "",
                   const std::string &parentId = "");
  void createAtlasImage(const std::string &id, const UIRect &bounds,
                        const std::string &textureID,
                        const UIRect &sourceRect,
                        const std::string &parentId = "");
  void createSlider(const std::string &id, const UIRect &bounds,
                    float minVal = 0.0f, float maxVal = 1.0f,
                    const std::string &parentId = "");
  void createCheckbox(const std::string &id, const UIRect &bounds,
                      const std::string &text = "",
                      const std::string &parentId = "");
  void createList(const std::string &id, const UIRect &bounds,
                  const std::string &parentId = "");
  void createTooltip(const std::string &id, const std::string &text = "");
  void createEventLog(const std::string &id, const UIRect &bounds,
                      int maxEntries = UIConstants::DEFAULT_EVENT_LOG_MAX_ENTRIES);
  void createDialog(const std::string &id, const UIRect &bounds,
                    const std::string &parentId = "");

  // Modal creation helper - combines theme + overlay + dialog
  void createModal(const std::string &dialogId, const UIRect &bounds,
                   const std::string &theme, int windowWidth, int windowHeight);

  // Theme management
  void refreshAllComponentThemes() const;

  // Component manipulation
  void removeComponent(const std::string &id);
  void clearAllComponents();
  bool hasComponent(const std::string &id) const;
  void setComponentVisible(const std::string &id, bool visible);
  void setComponentEnabled(const std::string &id, bool enabled);
  void setComponentBounds(const std::string &id, const UIRect &bounds);
  void setComponentZOrder(const std::string &id, int zOrder);
  void setComponentPositioning(const std::string &id, const UIPositioning &positioning);

  // Component property setters
  void setText(const std::string &id, const std::string &text);
  void setTexture(const std::string &id, const std::string &textureID);
  void setImageSource(const std::string &id, const TextureSource &source);
  void setImageSourceRect(const std::string &id, const UIRect &sourceRect);
  void clearImageSourceRect(const std::string &id);
  void setValue(const std::string &id, float value);
  void setChecked(const std::string &id, bool checked);
  void setStyle(const std::string &id, const UIStyle &style);

  // Data binding methods
  void bindText(const std::string &id, std::function<std::string()> binding);
  void bindList(const std::string &id,
                std::function<void(std::vector<std::string>&, std::vector<std::pair<std::string, int>>&)> binding);
  void markBindingDirty(const std::string &id);
  void markAllBindingsDirty();

  // Component property getters
  std::string getText(const std::string &id) const;
  std::string getTexture(const std::string &id) const;
  UIRect getImageSourceRect(const std::string &id) const;
  float getValue(const std::string &id) const;
  bool getChecked(const std::string &id) const;
  UIRect getBounds(const std::string &id) const;
  UIState getComponentState(const std::string &id) const;

  // Event handling
  bool isButtonClicked(const std::string &id) const;
  bool isButtonPressed(const std::string &id) const;
  bool isButtonHovered(const std::string &id) const;
  bool isComponentFocused(const std::string &id) const;

  // Keyboard/gamepad selection — game states use this to highlight a button
  // when driving menus with MenuUp/MenuDown instead of the mouse. When set,
  // the selected component renders in HOVERED state provided the mouse is not
  // over any other interactive component. Mouse hover always wins.
  void setKeyboardSelection(const std::string &id);
  void clearKeyboardSelection();
  const std::string &getKeyboardSelection() const { return m_keyboardSelection; }

  // Synthesizes a click on the named component — queues its onClick callback
  // just like a mouse click would. Used by MenuConfirm keyboard/gamepad input.
  void simulateClick(const std::string &id);

  // Callback setters
  void setOnClick(const std::string &id, std::function<void()> callback);
  void setOnValueChanged(const std::string &id,
                         std::function<void(float)> callback);
  void setOnTextChanged(const std::string &id,
                        std::function<void(const std::string &)> callback);
  void setOnHover(const std::string &id, std::function<void()> callback);
  void setOnFocus(const std::string &id, std::function<void()> callback);

  // Layout management
  void createLayout(const std::string &id, UILayoutType type,
                    const UIRect &bounds);
  void addComponentToLayout(const std::string &layoutID,
                            const std::string &componentID);
  void removeComponentFromLayout(const std::string &layoutID,
                                 const std::string &componentID);
  void updateLayout(const std::string &layoutID);
  void setLayoutSpacing(const std::string &layoutID, int spacing);
  void setLayoutColumns(const std::string &layoutID, int columns);
  void setLayoutAlignment(const std::string &layoutID, UIAlignment alignment);

  // Progress bar specific methods
  void updateProgressBar(const std::string &id, float value);
  void setProgressBarRange(const std::string &id, float minVal, float maxVal);

  // List specific methods
  void addListItem(const std::string &listID, const std::string &item);
  void removeListItem(const std::string &listID, int index);
  void clearList(const std::string &listID);
  int getSelectedListItem(const std::string &listID) const;
  void setSelectedListItem(const std::string &listID, int index);

  // Enhanced list methods for auto-scrolling and management
  void setListMaxItems(const std::string &listID, int maxItems);
  void addListItemWithAutoScroll(const std::string &listID,
                                 const std::string &item);
  void clearListItems(const std::string &listID);

  // Event log specific methods
  // Event log management
  void addEventLogEntry(const std::string &logID, const std::string &entry);
  void clearEventLog(const std::string &logID);
  void setEventLogMaxEntries(const std::string &logID, int maxEntries);
  void enableEventLogAutoUpdate(const std::string &logID,
                                float interval = UIConstants::DEFAULT_EVENT_LOG_UPDATE_INTERVAL);
  void disableEventLogAutoUpdate(const std::string &logID);

  // Title specific methods
  void setTitleAlignment(const std::string &titleID, UIAlignment alignment);
  void
  centerTitleInContainer(const std::string &titleID, int containerX,
                         int containerWidth); // Center title after auto-sizing

  // Label specific methods
  void setLabelAlignment(const std::string &labelID, UIAlignment alignment);

  // Input field specific methods
  void setInputFieldPlaceholder(const std::string &id,
                                const std::string &placeholder);
  void setInputFieldMaxLength(const std::string &id, int maxLength);
  bool isInputFieldFocused(const std::string &id) const;

  // Animation system
  void animateMove(const std::string &id, const UIRect &targetBounds,
                   float duration, std::function<void()> onComplete = nullptr);
  void animateColor(const std::string &id, const SDL_Color &targetColor,
                    float duration, std::function<void()> onComplete = nullptr);
  void stopAnimation(const std::string &id);
  bool isAnimating(const std::string &id) const;

  // Theme management
  void loadTheme(const UITheme &theme);
  void setDefaultTheme();
  void setLightTheme();
  void setDarkTheme();
  void setThemeMode(const std::string &mode);
  const std::string &getCurrentThemeMode() const;
  void applyThemeToComponent(const std::string &id, UIComponentType type);
  void setGlobalStyle(const UIStyle &style);

  // Overlay management - creates/removes semi-transparent background overlays
  void
  createOverlay(int windowWidth,
                int windowHeight); // Creates overlay using specified dimensions
  void
  createOverlay(); // Creates overlay using auto-detected logical dimensions
  void removeOverlay(); // Removes the overlay background

  // Text background methods (for labels and titles readability)
  void enableTextBackground(const std::string &id, bool enable = true);
  void setTextBackgroundColor(const std::string &id, SDL_Color color);
  void setTextBackgroundPadding(const std::string &id, int padding);

  // Component cleanup utilities
  void removeComponentsWithPrefix(const std::string &prefix);
  void resetToDefaultTheme();
  void cleanupForStateTransition();

  // Simplified state transition method
  void prepareForStateTransition();

  // Auto-sizing core methods
  void calculateOptimalSize(
      const std::string &id); // Calculate and apply optimal size for component
  void calculateOptimalSize(
      std::shared_ptr<UIComponent>
          component); // Calculate and apply optimal size for component
  bool measureComponentContent(const std::shared_ptr<UIComponent> &component,
                               int *width,
                               int *height); // Measure content dimensions
  void invalidateLayout(
      const std::string &layoutID); // Mark layout for recalculation
  void recalculateLayout(
      const std::string
          &layoutID); // Recalculate layout with new component sizes
  void enableAutoSizing(
      const std::string &id,
      bool enable = true); // Enable/disable auto-sizing for component
  void
  setAutoSizingConstraints(const std::string &id, const UIRect &minBounds,
                           const UIRect &maxBounds); // Set size constraints

  // Auto-detection and convenience methods
  int getWidthInPixels() const;   // Auto-detect width in pixels from GameEngine
  int getHeightInPixels() const;  // Auto-detect height in pixels from GameEngine
  void createTitleAtTop(const std::string &id, const std::string &text,
                        int height = UIConstants::DEFAULT_TITLE_HEIGHT);
  void createButtonAtBottom(const std::string &id, const std::string &text,
                            int width = UIConstants::DEFAULT_BUTTON_WIDTH, int height = UIConstants::DEFAULT_BUTTON_HEIGHT);
  void createCenteredDialog(const std::string &id, int width, int height,
                            const std::string &theme = "dark");
  void createCenteredButton(const std::string &id, int offsetY,
                           int width, int height, const std::string &text);

  /**
   * @brief Creates a panel positioned at bottom-right corner
   * @param id Panel component ID
   * @param width Panel width
   * @param height Panel height
   * @param offsetX Offset from right edge (default: BOTTOM_RIGHT_OFFSET_X)
   * @param offsetY Offset from bottom edge (default: BOTTOM_RIGHT_OFFSET_Y)
   */
  void createPanelAtBottomRight(const std::string &id, int width, int height,
                                int offsetX = UIConstants::BOTTOM_RIGHT_OFFSET_X,
                                int offsetY = UIConstants::BOTTOM_RIGHT_OFFSET_Y);

  /**
   * @brief Creates a label positioned at bottom-right corner
   * @param id Label component ID
   * @param text Initial label text
   * @param width Label width
   * @param height Label height
   * @param offsetX Offset from right edge (default: BOTTOM_RIGHT_OFFSET_X)
   * @param offsetY Offset from bottom edge (default: BOTTOM_RIGHT_OFFSET_Y)
   */
  void createLabelAtBottomRight(const std::string &id, const std::string &text,
                                int width, int height,
                                int offsetX = UIConstants::BOTTOM_RIGHT_OFFSET_X,
                                int offsetY = UIConstants::BOTTOM_RIGHT_OFFSET_Y);

  // Combat HUD helpers
  void createCombatHUD();
  void updateCombatHUD(float playerHealth, float playerStamina,
                       bool hasTarget, const std::string& targetName,
                       float targetHealth);
  void destroyCombatHUD();

  // Utility methods
  void setGlobalFont(const std::string &fontID);
  void setGlobalScale(float scale);
  float getGlobalScale() const { return m_globalScale; }

  // Helper to scale UIRect by global scale factor (eliminates redundant per-component multiplication)
  inline UIRect scaleRect(const UIRect& bounds) const {
    return {
      static_cast<int>(bounds.x * m_globalScale),
      static_cast<int>(bounds.y * m_globalScale),
      static_cast<int>(bounds.width * m_globalScale),
      static_cast<int>(bounds.height * m_globalScale)
    };
  }

  float calculateOptimalScale(int width, int height) const;  // Calculate resolution-aware scale
  void enableTooltips(bool enable) { m_tooltipsEnabled = enable; }
  void setTooltipDelay(float delay) { m_tooltipDelay = delay; }

  // Debug methods
  void setDebugMode(bool enable) { m_debugMode = enable; }
   void drawDebugBounds(bool enable) { m_drawDebugBounds = enable; }
  bool isClickOnUI(const Vector2D& screenPos) const;

private:
  // Core data
  std::unordered_map<std::string, std::shared_ptr<UIComponent>> m_components{};
  std::unordered_map<std::string, std::shared_ptr<UILayout>> m_layouts{};
  std::vector<std::shared_ptr<UIAnimation>> m_animations{};

  // State tracking
  std::vector<std::string> m_clickedButtons{};
  std::vector<std::string> m_hoveredComponents{};
  std::string m_focusedComponent{};
  std::string m_keyboardSelection{};
  std::string m_hoveredTooltip{};
  std::string m_hoveredTooltipCandidate{};
  float m_tooltipTimer{0.0f};

  // Theme and styling
  UITheme m_currentTheme{};
  UIStyle m_globalStyle{};
  std::string m_globalFontID{UIConstants::FONT_DEFAULT};
  std::string m_titleFontID{UIConstants::FONT_TITLE};
  std::string m_uiFontID{UIConstants::FONT_UI};
  float m_globalScale{1.0f};
  std::string m_currentThemeMode{"light"};

  // Settings
  bool m_tooltipsEnabled{true};
  float m_tooltipDelay{1.0f};
  bool m_debugMode{false};
  bool m_drawDebugBounds{false};

  // Event log state tracking
  std::unordered_map<std::string, EventLogState> m_eventLogStates{};
  bool m_isShutdown{false};

  // Window resize tracking for auto-repositioning
  int m_currentWidthInPixels{0};
  int m_currentHeightInPixels{0};

  // Input state
  Vector2D m_lastMousePosition{};
  bool m_mousePressed{false};
  bool m_mouseReleased{false};

  // Performance optimization: Cached sorted components to avoid per-frame allocation + sorting
  mutable std::vector<std::shared_ptr<UIComponent>> m_sortedComponentsCache{};
  mutable bool m_sortedComponentsDirty{true};

  // Performance optimization: Value caches to avoid hash lookup when values unchanged
  std::unordered_map<std::string, float> m_valueCache{};
  std::unordered_map<std::string, std::string> m_textCache{};

  // Private helper methods
  std::shared_ptr<UIComponent> getComponent(const std::string &id);
  std::shared_ptr<const UIComponent> getComponent(const std::string &id) const;
  std::shared_ptr<UILayout> getLayout(const std::string &id);
  void registerComponent(const std::shared_ptr<UIComponent> &component,
                         const std::string &parentId,
                         bool autoSize = false);

  // Parent/child linkage — called from every create* method after the
  // component is fully initialized. Registers the child with the parent,
  // computes backdrop inheritance, and suppresses the child's default
  // text-background when the parent already provides one (PANEL/DIALOG or
  // any descendant of one).
  void linkToParent(const std::shared_ptr<UIComponent> &component,
                    const std::string &parentId);
  void applyThemeStyle(const std::shared_ptr<UIComponent> &component,
                       UIComponentType type) const;
  void applyCurrentThemeToComponents() const;

  // Auto-repositioning system (private helpers)
  void repositionAllComponents(int width, int height);
  void applyPositioning(std::shared_ptr<UIComponent> component, int width, int height);
  int calculateListItemHeight(const std::shared_ptr<UIComponent> &component) const;

  void handleInput();
  void updateAnimations(float deltaTime);
  void updateTooltips(float deltaTime);
  void updateEventLogs(float deltaTime);
  // PERFORMANCE: Return const reference to avoid vector copy every frame
  const std::vector<std::shared_ptr<UIComponent>>& getSortedComponents() const;

  // Performance optimization helper
  void invalidateComponentCache();
  void clearFrameRenderBatches();

  // Layout helpers
  void applyAbsoluteLayout(const std::shared_ptr<UILayout> &layout);
  void applyFlowLayout(const std::shared_ptr<UILayout> &layout);
  void applyGridLayout(const std::shared_ptr<UILayout> &layout);
  void applyStackLayout(const std::shared_ptr<UILayout> &layout);
  void applyAnchorLayout(const std::shared_ptr<UILayout> &layout);

  // Utility helpers
  SDL_Color interpolateColor(const SDL_Color &start, const SDL_Color &end,
                             float t);
  UIRect interpolateRect(const UIRect &start, const UIRect &end, float t);
  void executeDeferredCallbacks();

  // Deferred execution queue to prevent iterator invalidation
  std::vector<std::function<void()>> m_deferredCallbacks{};

  // PERFORMANCE: Track active bindings to skip iteration when none exist
  size_t m_activeBindingCount{0};

  // Frame-local render batch state. Render order is fixed by family: primitives, images,
  // then text. Component z-order controls input priority and ordering inside
  // each family, not cross-family visual interleaving.
  uint32_t m_uiPrimitiveVertexCount{0};                  // Filled rects, borders, text backgrounds
  std::vector<VoidLight::UITextureDrawBatch> m_imageRenderBatches{}; // Images/textures
  std::vector<VoidLight::UITextDrawBatch> m_textRenderBatches{};     // SDL3_ttf atlas-backed text

  // Delete copy constructor and assignment operator
  UIManager(const UIManager &) = delete;
  UIManager &operator=(const UIManager &) = delete;

  UIManager() = default;
};

#endif // UI_MANAGER_HPP
