/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/UIManager.hpp"
#include "managers/UIConstants.hpp"
#include "utils/TextureSource.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "managers/FontManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/TextureManager.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include "gpu/GPURenderer.hpp"
#include "gpu/GPUVertexPool.hpp"
#include "gpu/GPUTypes.hpp"

bool UIManager::init() {
  if (m_isShutdown) {
    return false;
  }

  // Initialize with enhanced dark theme
  setDarkTheme();

  // Set global fonts to match what's loaded by FontManager's
  // loadFontsForDisplay
  m_globalFontID = UIConstants::FONT_DEFAULT;
  m_titleFontID = UIConstants::FONT_TITLE;
  m_uiFontID = UIConstants::FONT_UI;

  // Clear any existing data and reserve capacity for performance
  m_components.clear();
  m_layouts.clear();
  m_animations.clear();
  constexpr size_t ANIMATIONS_CAPACITY = 16;
  m_animations.reserve(
      ANIMATIONS_CAPACITY); // Reserve for typical UI animations
  m_clickedButtons.clear();
  constexpr size_t INTERACTIONS_CAPACITY = 8;
  m_clickedButtons.reserve(
      INTERACTIONS_CAPACITY); // Reserve for typical button interactions
  m_hoveredComponents.clear();
  m_hoveredComponents.reserve(
      INTERACTIONS_CAPACITY); // Reserve for typical hover states
  m_focusedComponent.clear();
  m_hoveredTooltip.clear();
  m_hoveredTooltipCandidate.clear();

  m_tooltipTimer = 0.0f;
  m_mousePressed = false;
  m_mouseReleased = false;

  // Initialize current pixel dimensions from GameEngine
  const auto& gameEngine = GameEngine::Instance();
  m_currentWidthInPixels = gameEngine.getWidthInPixels();
  m_currentHeightInPixels = gameEngine.getHeightInPixels();
  UI_INFO(std::format("Initialized pixel dimensions: {}x{}",
                      m_currentWidthInPixels, m_currentHeightInPixels));

  // Calculate and set resolution-aware UI scale (1920x1080 baseline, capped at 1.0)
  m_globalScale = calculateOptimalScale(m_currentWidthInPixels, m_currentHeightInPixels);
  UI_INFO(std::format("UI scale set to {} for resolution {}x{}",
                      m_globalScale, m_currentWidthInPixels, m_currentHeightInPixels));

  // Reserve GPU batch buffers to avoid per-frame reallocations.
  m_textRenderBatches.reserve(UI_TEXT_BATCH_CAPACITY);
  m_imageRenderBatches.reserve(UI_IMAGE_BATCH_CAPACITY);

  // Note: UIManager::onWindowResize() is called directly by InputManager when window resizes

  return true;
}

void UIManager::update(float deltaTime) {
  if (m_isShutdown) {
    return;
  }


  // Note: Window resize is now event-driven via onWindowResize(), not polled every frame

  // PERFORMANCE: Only iterate components if there are active bindings
  // This skips the O(n) loop entirely when no bindings exist
  if (m_activeBindingCount > 0) {
    // Process data bindings - ONLY for visible AND dirty components
    for (auto &[id, component] : m_components) {
        if (component && component->m_visible && component->m_bindingDirty) {
            // Handle text bindings
            if (component->m_textBinding) {
                setText(id, component->m_textBinding());
            }
            // Handle list bindings (zero-allocation: uses reusable buffers)
            if (component->m_listBinding) {
                component->m_listBindingBuffer.clear();  // Reuse capacity, no deallocation
                component->m_listSortBuffer.clear();     // Reuse sort buffer capacity
                component->m_listBinding(component->m_listBindingBuffer, component->m_listSortBuffer);
                if (component->m_listItems.size() != component->m_listBindingBuffer.size() ||
                    !std::equal(component->m_listItems.begin(), component->m_listItems.end(),
                                component->m_listBindingBuffer.begin())) {
                    // Copy (not move): reuses m_listItems' existing capacity and
                    // preserves m_listBindingBuffer's storage for reuse next frame.
                    component->m_listItems = component->m_listBindingBuffer;
                    component->m_listItemsDirty = true;
                }
            }
            // Reset dirty flag after processing bindings
            component->m_bindingDirty = false;
        }
    }
  }

  // Clear frame-specific state
  m_clickedButtons.clear();
  m_mouseReleased = false;

  // Handle input (may need component access)
  handleInput();

  // PERFORMANCE: Skip animation update if no animations exist
  if (!m_animations.empty()) {
    updateAnimations(deltaTime);
  }

  // Update tooltips
  updateTooltips(deltaTime);

  // PERFORMANCE: Skip event log update if no event logs exist
  if (!m_eventLogStates.empty()) {
    updateEventLogs(deltaTime);
  }

  // PERFORMANCE: Skip callback execution if no callbacks queued
  if (!m_deferredCallbacks.empty()) {
    executeDeferredCallbacks();
  }
}

void UIManager::executeDeferredCallbacks() {
    // Swap out before invoking anything: a callback can synchronously trigger
    // a state transition (prepareForStateTransition() clears
    // m_deferredCallbacks), which would otherwise invalidate this loop
    // mid-iteration. A local vector is safe against that same reentrant
    // clear happening again while this drain is still running.
    std::vector<std::function<void()>> callbacksToRun;
    callbacksToRun.swap(m_deferredCallbacks);

    for (const auto& callback : callbacksToRun) {
        if (callback) {
            callback();
        }
    }
}

void UIManager::clean() {
  if (m_isShutdown) {
    return;
  }
  
  // Perform comprehensive cleanup to clear all cached textures
  cleanupForStateTransition();
  
  // Mark as shutdown
  m_isShutdown = true;
}

const std::vector<std::shared_ptr<UIComponent>>& UIManager::getSortedComponents() const {
  // Performance optimization: Only rebuild sorted list when components added/removed/z-order changed
  if (m_sortedComponentsDirty) {
    m_sortedComponentsCache.clear();
    m_sortedComponentsCache.reserve(m_components.size());

    for (const auto &[id, component] : m_components) {
      if (component) {
        m_sortedComponentsCache.push_back(component);
      }
    }

    std::sort(m_sortedComponentsCache.begin(), m_sortedComponentsCache.end(),
              [](const std::shared_ptr<UIComponent> &a,
                 const std::shared_ptr<UIComponent> &b) {
                return a->m_zOrder < b->m_zOrder;
              });

    m_sortedComponentsDirty = false;
  }

  return m_sortedComponentsCache;
}

// Performance optimization helper - centralized cache invalidation
void UIManager::invalidateComponentCache() {
  m_sortedComponentsDirty = true;
}

void UIManager::clearFrameRenderBatches() {
  m_uiPrimitiveVertexCount = 0;
  m_imageRenderBatches.clear();
  m_textRenderBatches.clear();
}

// Parent/child linkage — called from every create* method after the
// component is in m_components. Registers with parent, computes backdrop
// inheritance, and suppresses the child's default text-background when the
// parent already provides one (PANEL/DIALOG or any descendant of one).
void UIManager::linkToParent(const std::shared_ptr<UIComponent> &component,
                             const std::string &parentId) {
  if (!component || parentId.empty()) {
    return;
  }

  auto parent = getComponent(parentId);
  if (!parent) {
    UI_WARN(std::format("linkToParent: parent '{}' not found for child '{}'",
                        parentId, component->m_id));
    return;
  }

  component->m_parentId = parentId;
  parent->m_childIds.push_back(component->m_id);

  const bool parentProvidesBackdrop =
      parent->m_type == UIComponentType::PANEL ||
      parent->m_type == UIComponentType::DIALOG ||
      parent->m_hasBackdropAncestor;

  component->m_hasBackdropAncestor = parentProvidesBackdrop;
  if (parentProvidesBackdrop) {
    component->m_style.useTextBackground = false;
  }

  // Ensure child renders on top of its parent regardless of theme defaults.
  // Additive bump preserves relative ordering between siblings (e.g. a button at z=10 and
  // a title at z=25 inside a modal dialog at z=600 end up at 611 and 626 respectively).
  if (component->m_zOrder <= parent->m_zOrder) {
    component->m_zOrder += parent->m_zOrder + 1;
  }
}

// Component creation methods
void UIManager::createButton(const std::string &id, const UIRect &bounds,
                             const std::string &text,
                             const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::BUTTON;
  // Apply global scale for resolution-aware sizing (1920x1080 baseline, capped at 1.0)
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_style = m_currentTheme.getStyle(UIComponentType::BUTTON);
  component->m_zOrder = UIConstants::ZORDER_BUTTON; // Interactive elements on top

  // Caller-supplied size is the floor; auto-sizing only grows the button to fit text
  component->m_minBounds.width = component->m_bounds.width;
  component->m_minBounds.height = component->m_bounds.height;

  // Grow to fit text so long labels never overflow the button bounds
  registerComponent(component, parentId, true);
}

void UIManager::createButtonDanger(const std::string &id, const UIRect &bounds,
                                   const std::string &text,
                                   const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::BUTTON_DANGER;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_style = m_currentTheme.getStyle(UIComponentType::BUTTON_DANGER);
  component->m_zOrder = UIConstants::ZORDER_BUTTON; // Interactive elements on top

  component->m_minBounds.width = component->m_bounds.width;
  component->m_minBounds.height = component->m_bounds.height;

  registerComponent(component, parentId, true);
}

void UIManager::createButtonSuccess(const std::string &id, const UIRect &bounds,
                                    const std::string &text,
                                    const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::BUTTON_SUCCESS;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_style = m_currentTheme.getStyle(UIComponentType::BUTTON_SUCCESS);
  component->m_zOrder = UIConstants::ZORDER_BUTTON; // Interactive elements on top

  component->m_minBounds.width = component->m_bounds.width;
  component->m_minBounds.height = component->m_bounds.height;

  registerComponent(component, parentId, true);
}

void UIManager::createButtonWarning(const std::string &id, const UIRect &bounds,
                                    const std::string &text,
                                    const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::BUTTON_WARNING;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_style = m_currentTheme.getStyle(UIComponentType::BUTTON_WARNING);
  component->m_zOrder = UIConstants::ZORDER_BUTTON; // Interactive elements on top

  component->m_minBounds.width = component->m_bounds.width;
  component->m_minBounds.height = component->m_bounds.height;

  registerComponent(component, parentId, true);
}

void UIManager::createLabel(const std::string &id, const UIRect &bounds,
                            const std::string &text,
                            const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::LABEL;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_style = m_currentTheme.getStyle(UIComponentType::LABEL);
  component->m_zOrder = UIConstants::ZORDER_LABEL; // Text on top

  // Apply auto-sizing after creation
  registerComponent(component, parentId, true);
}

void UIManager::createTitle(const std::string &id, const UIRect &bounds,
                            const std::string &text,
                            const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::TITLE;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_style = m_currentTheme.getStyle(UIComponentType::TITLE);
  component->m_zOrder = UIConstants::ZORDER_TITLE; // Titles on top

  // Apply auto-sizing after creation
  registerComponent(component, parentId, true);
}

void UIManager::createPanel(const std::string &id, const UIRect &bounds,
                            const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::PANEL;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_style = m_currentTheme.getStyle(UIComponentType::PANEL);
  component->m_zOrder = UIConstants::ZORDER_PANEL; // Background panels

  registerComponent(component, parentId);
}

void UIManager::createProgressBar(const std::string &id, const UIRect &bounds,
                                  float minVal, float maxVal,
                                  const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::PROGRESS_BAR;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_minValue = minVal;
  component->m_maxValue = maxVal;
  component->m_value = minVal;
  component->m_style = m_currentTheme.getStyle(UIComponentType::PROGRESS_BAR);
  component->m_zOrder = UIConstants::ZORDER_PROGRESS_BAR; // UI elements

  registerComponent(component, parentId);
}

void UIManager::createInputField(const std::string &id, const UIRect &bounds,
                                 const std::string &placeholder,
                                 const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::INPUT_FIELD;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_placeholder = placeholder;
  component->m_style = m_currentTheme.getStyle(UIComponentType::INPUT_FIELD);
  component->m_zOrder = UIConstants::ZORDER_INPUT_FIELD; // Interactive elements

  registerComponent(component, parentId);
}

void UIManager::createImage(const std::string &id, const UIRect &bounds,
                            const std::string &textureID,
                            const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::IMAGE;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_textureID = textureID;
  component->m_style = m_currentTheme.getStyle(UIComponentType::IMAGE);
  component->m_zOrder = UIConstants::ZORDER_IMAGE; // Background images

  registerComponent(component, parentId);
}

void UIManager::createAtlasImage(const std::string &id, const UIRect &bounds,
                                 const std::string &textureID,
                                 const UIRect &sourceRect,
                                 const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::IMAGE;
  component->m_bounds = scaleRect(bounds);
  component->m_textureID = textureID;
  component->m_imageSourceRect = sourceRect;
  component->m_useImageSourceRect = true;
  component->m_style = m_currentTheme.getStyle(UIComponentType::IMAGE);
  component->m_zOrder = UIConstants::ZORDER_IMAGE;

  registerComponent(component, parentId);
}

void UIManager::createSlider(const std::string &id, const UIRect &bounds,
                             float minVal, float maxVal,
                             const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::SLIDER;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_minValue = minVal;
  component->m_maxValue = maxVal;
  component->m_value = minVal;
  component->m_style = m_currentTheme.getStyle(UIComponentType::SLIDER);
  component->m_zOrder = UIConstants::ZORDER_SLIDER; // Interactive elements

  registerComponent(component, parentId);
}

void UIManager::createCheckbox(const std::string &id, const UIRect &bounds,
                               const std::string &text,
                               const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::CHECKBOX;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_text = text;
  component->m_checked = false;
  component->m_style = m_currentTheme.getStyle(UIComponentType::CHECKBOX);
  component->m_zOrder = UIConstants::ZORDER_CHECKBOX; // Interactive elements

  registerComponent(component, parentId);
}

void UIManager::createList(const std::string &id, const UIRect &bounds,
                           const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::LIST;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_selectedIndex = -1;
  component->m_style = m_currentTheme.getStyle(UIComponentType::LIST);
  component->m_zOrder = UIConstants::ZORDER_LIST; // UI elements

  // Enable auto-sizing for dynamic content-based sizing
  registerComponent(component, parentId, true);
}

void UIManager::createTooltip(const std::string &id, const std::string &text) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::TOOLTIP;
  component->m_text = text;
  component->m_visible = false;
  component->m_style = m_currentTheme.getStyle(UIComponentType::TOOLTIP);
  component->m_zOrder = UIConstants::ZORDER_TOOLTIP; // Always on top

  registerComponent(component, "");
}

void UIManager::createEventLog(const std::string &id, const UIRect &bounds,
                               int maxEntries) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::EVENT_LOG;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_maxLength = maxEntries; // Store max entries in maxLength field
  component->m_style = m_currentTheme.getStyle(
      UIComponentType::EVENT_LOG); // Use event log styling
  component->m_zOrder = UIConstants::ZORDER_EVENT_LOG;           // UI elements

  registerComponent(component, "");
}

void UIManager::createDialog(const std::string &id, const UIRect &bounds,
                             const std::string &parentId) {
  auto component = std::make_shared<UIComponent>();
  component->m_id = id;
  component->m_type = UIComponentType::DIALOG;
  // Apply global scale for resolution-aware sizing
  component->m_bounds = scaleRect(bounds);
  component->m_style = m_currentTheme.getStyle(UIComponentType::DIALOG);
  component->m_zOrder = UIConstants::ZORDER_DIALOG; // Default z; createModal() bumps to ZORDER_MODAL_DIALOG for modal use

  registerComponent(component, parentId);
}

void UIManager::createModal(const std::string &dialogId, const UIRect &bounds,
                            const std::string &theme, int windowWidth,
                            int windowHeight) {
  // Set theme first
  if (!theme.empty()) {
    setThemeMode(theme);
    // Refresh existing components to use new theme
    refreshAllComponentThemes();
  }

  // Create overlay to dim background, then elevate it to the modal layer so it
  // renders above all regular UI (buttons/labels/titles) and dims them correctly.
  createOverlay(windowWidth, windowHeight);
  if (auto overlay = getComponent("__overlay")) {
    overlay->m_zOrder = UIConstants::ZORDER_MODAL_OVERLAY;
    // The modal overlay must swallow input for everything beneath it; otherwise
    // clicks pass through the overlay onto the buttons underneath (PANEL hits
    // do not set mouseHandled in handleInput).
    overlay->m_blocksInputBelow = true;
    overlay->m_occludesRenderingBelow = true;
    invalidateComponentCache();
  }

  // Create dialog box and elevate it above the modal overlay so children added
  // via linkToParent (createLabel/createButton with parentId) auto-elevate above it.
  createDialog(dialogId, bounds);
  if (auto dialog = getComponent(dialogId)) {
    dialog->m_zOrder = UIConstants::ZORDER_MODAL_DIALOG;
    invalidateComponentCache();
  }
}

void UIManager::refreshAllComponentThemes() const {
  applyCurrentThemeToComponents();
}

// Component manipulation
void UIManager::removeComponent(const std::string &id) {

  auto it = m_components.find(id);
  if (it == m_components.end() || !it->second) {
    return;
  }

  // Snapshot parent/children before erasing — iterators invalidate on erase
  // and recursive removeComponent() calls below will modify the same map.
  const std::string parentId = it->second->m_parentId;
  const std::vector<std::string> childIds = it->second->m_childIds;

  // Decrement binding count if component has active bindings. Prevents
  // m_activeBindingCount from drifting when bound components are removed.
  if (it->second->m_textBinding) {
    --m_activeBindingCount;
  }
  if (it->second->m_listBinding) {
    --m_activeBindingCount;
  }

  m_components.erase(it);
  invalidateComponentCache();

  // Cascade removal to children so parent destruction takes its subtree
  // with it (no dangling child ids referring to a removed parent).
  for (const auto &childId : childIds) {
    removeComponent(childId);
  }

  // Unlink from parent's child list
  if (!parentId.empty()) {
    if (auto parent = getComponent(parentId)) {
      auto &siblings = parent->m_childIds;
      siblings.erase(std::remove(siblings.begin(), siblings.end(), id),
                     siblings.end());
    }
  }

  // Clear from value/text caches
  m_valueCache.erase(id);
  m_textCache.erase(id);

  // Remove from any layouts
  for (auto &[layoutId, layout] : m_layouts) {
    auto &children = layout->m_childComponents;
    children.erase(std::remove(children.begin(), children.end(), id),
                   children.end());
  }

  // Clear focus if this component was focused
  if (m_focusedComponent == id) {
    m_focusedComponent.clear();
  }
}

bool UIManager::hasComponent(const std::string &id) const {
  return m_components.find(id) != m_components.end();
}

void UIManager::setComponentVisible(const std::string &id, bool visible) {
  auto component = getComponent(id);
  if (!component) {
    return;
  }
  component->m_visible = visible;
  // Cascade to children — callers toggle a single container and all child
  // labels/lists/bars follow. Eliminates per-component visibility boilerplate.
  for (const auto &childId : component->m_childIds) {
    setComponentVisible(childId, visible);
  }
}

void UIManager::setComponentEnabled(const std::string &id, bool enabled) {
  auto component = getComponent(id);
  if (component) {
    component->m_enabled = enabled;
    if (!enabled && component->m_state != UIState::DISABLED) {
      component->m_state = UIState::DISABLED;
    } else if (enabled && component->m_state == UIState::DISABLED) {
      component->m_state = UIState::NORMAL;
    }
  }
}

void UIManager::setComponentBounds(const std::string &id,
                                   const UIRect &bounds) {
  auto component = getComponent(id);
  if (component) {
    // Apply global scale for resolution-aware sizing
    component->m_bounds = scaleRect(bounds);
  }
}

void UIManager::setComponentZOrder(const std::string &id, int zOrder) {
  auto component = getComponent(id);
  if (component) {
    component->m_zOrder = zOrder;
    invalidateComponentCache();
  }
}

// Component property setters
void UIManager::setText(const std::string &id, const std::string &text) {
  // Performance optimization: Check cache first to avoid mutex lock + hash lookup when text unchanged
  auto cacheIt = m_textCache.find(id);
  if (cacheIt != m_textCache.end() && cacheIt->second == text) {
    return; // Text unchanged, skip expensive getComponent() call
  }

  auto component = getComponent(id);
  if (component) {
    const bool textChanged = component->m_text != text;
    component->m_text = text;
    m_textCache[id] = text; // Update cache
    // Re-run auto-sizing so the component grows/shrinks to fit the new text.
    // calculateOptimalSize() is a no-op when m_autoSize is false.
    calculateOptimalSize(component);
    if (textChanged && component->m_onTextChanged) {
      component->m_onTextChanged(text);
    }
  }
}

void UIManager::setTexture(const std::string &id,
                           const std::string &textureID) {
  auto component = getComponent(id);
  if (component) {
    component->m_textureID = textureID;
  }
}

void UIManager::setImageSource(const std::string &id,
                               const TextureSource &source) {
  auto component = getComponent(id);
  if (!component) {
    return;
  }

  component->m_textureID = source.textureId;
  if (source.isEmpty() || !source.useSourceRect) {
    component->m_imageSourceRect = UIRect{};
    component->m_useImageSourceRect = false;
    return;
  }

  component->m_imageSourceRect =
      UIRect{source.sourceX, source.sourceY, source.sourceW, source.sourceH};
  component->m_useImageSourceRect = true;
}

void UIManager::setImageSourceRect(const std::string &id,
                                   const UIRect &sourceRect) {
  auto component = getComponent(id);
  if (component) {
    component->m_imageSourceRect = sourceRect;
    component->m_useImageSourceRect = true;
  }
}

void UIManager::clearImageSourceRect(const std::string &id) {
  auto component = getComponent(id);
  if (component) {
    component->m_imageSourceRect = UIRect{};
    component->m_useImageSourceRect = false;
  }
}

void UIManager::setValue(const std::string &id, float value) {
  // Performance optimization: Check cache first to avoid mutex lock + hash lookup when value unchanged
  auto cacheIt = m_valueCache.find(id);
  if (cacheIt != m_valueCache.end() && cacheIt->second == value) {
    return; // Value unchanged, skip expensive getComponent() call
  }

  auto component = getComponent(id);
  if (component) {
    float clampedValue =
        std::clamp(value, component->m_minValue, component->m_maxValue);
    if (component->m_value != clampedValue) {
      component->m_value = clampedValue;
      m_valueCache[id] = clampedValue; // Update cache
      if (component->m_onValueChanged) {
        component->m_onValueChanged(clampedValue);
      }
    } else {
      // Value was already at clampedValue, ensure cache is updated
      m_valueCache[id] = clampedValue;
    }
  }
}

void UIManager::setChecked(const std::string &id, bool checked) {
  auto component = getComponent(id);
  if (component) {
    component->m_checked = checked;
  }
}

void UIManager::setStyle(const std::string &id, const UIStyle &style) {
  auto component = getComponent(id);
  if (component) {
    component->m_style = style;
  }
}

// Data binding methods
void UIManager::bindText(const std::string &id,
                         std::function<std::string()> binding) {
  auto component = getComponent(id);
  if (component) {
    // PERFORMANCE: Track binding count for early exit optimization in update()
    if (!component->m_textBinding && binding) {
      ++m_activeBindingCount;
    } else if (component->m_textBinding && !binding) {
      --m_activeBindingCount;
    }
    component->m_textBinding = std::move(binding);
  }
}

void UIManager::bindList(
    const std::string &id,
    std::function<void(std::vector<std::string>&, std::vector<std::pair<std::string, int>>&)> binding) {
  auto component = getComponent(id);
  if (component) {
    // PERFORMANCE: Track binding count for early exit optimization in update()
    if (!component->m_listBinding && binding) {
      ++m_activeBindingCount;
    } else if (component->m_listBinding && !binding) {
      --m_activeBindingCount;
    }
    component->m_listBinding = std::move(binding);
  }
}

void UIManager::markBindingDirty(const std::string &id) {
  auto component = getComponent(id);
  if (component) {
    component->m_bindingDirty = true;
  }
}

void UIManager::markAllBindingsDirty() {
  for (auto &[id, component] : m_components) {
    if (component) {
      component->m_bindingDirty = true;
    }
  }
}

// Text background methods for label and title readability
void UIManager::enableTextBackground(const std::string &id, bool enable) {
  auto component = getComponent(id);
  if (component && (component->m_type == UIComponentType::LABEL ||
                    component->m_type == UIComponentType::TITLE)) {
    component->m_style.useTextBackground = enable;
  }
}

void UIManager::setTextBackgroundColor(const std::string &id, SDL_Color color) {
  auto component = getComponent(id);
  if (component && (component->m_type == UIComponentType::LABEL ||
                    component->m_type == UIComponentType::TITLE)) {
    component->m_style.textBackgroundColor = color;
  }
}

void UIManager::setTextBackgroundPadding(const std::string &id, int padding) {
  auto component = getComponent(id);
  if (component && (component->m_type == UIComponentType::LABEL ||
                    component->m_type == UIComponentType::TITLE)) {
    component->m_style.textBackgroundPadding = padding;
  }
}

// Component property getters
std::string UIManager::getText(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_text : "";
}

std::string UIManager::getTexture(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_textureID : "";
}

UIRect UIManager::getImageSourceRect(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_imageSourceRect : UIRect{};
}

float UIManager::getValue(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_value : 0.0f;
}

bool UIManager::getChecked(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_checked : false;
}

UIRect UIManager::getBounds(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_bounds : UIRect{0, 0, 0, 0};
}

UIState UIManager::getComponentState(const std::string &id) const {
  auto component = getComponent(id);
  return component ? component->m_state : UIState::NORMAL;
}

// Event handling
bool UIManager::isButtonClicked(const std::string &id) const {
  return std::find(m_clickedButtons.begin(), m_clickedButtons.end(), id) !=
         m_clickedButtons.end();
}

bool UIManager::isButtonPressed(const std::string &id) const {
  auto component = getComponent(id);
  return component && component->m_state == UIState::PRESSED;
}

bool UIManager::isButtonHovered(const std::string &id) const {
  return std::find(m_hoveredComponents.begin(), m_hoveredComponents.end(),
                   id) != m_hoveredComponents.end();
}

bool UIManager::isComponentFocused(const std::string &id) const {
  return m_focusedComponent == id;
}

void UIManager::setKeyboardSelection(const std::string &id) {
  m_keyboardSelection = id;
}

void UIManager::clearKeyboardSelection() {
  m_keyboardSelection.clear();
}

void UIManager::simulateClick(const std::string &id) {
  auto component = getComponent(id);
  if (!component) {
    return;
  }
  // Mirror the type-specific effects of a mouse click (see update()):
  // checkboxes toggle their state before firing onClick so the callback reads
  // the new checked value. Buttons just fire the callback.
  if (component->m_type == UIComponentType::CHECKBOX) {
    component->m_checked = !component->m_checked;
  }
  if (component->m_onClick) {
    m_deferredCallbacks.push_back(component->m_onClick);
    m_clickedButtons.push_back(id);
  }
}

// Callback setters
void UIManager::setOnClick(const std::string &id,
                           std::function<void()> callback) {
  auto component = getComponent(id);
  if (component) {
    component->m_onClick = std::move(callback);
  }
}

void UIManager::setOnValueChanged(const std::string &id,
                                  std::function<void(float)> callback) {
  auto component = getComponent(id);
  if (component) {
    component->m_onValueChanged = std::move(callback);
  }
}

void UIManager::setOnTextChanged(
    const std::string &id, std::function<void(const std::string &)> callback) {
  auto component = getComponent(id);
  if (component) {
    component->m_onTextChanged = std::move(callback);
  }
}

void UIManager::setOnHover(const std::string &id,
                           std::function<void()> callback) {
  auto component = getComponent(id);
  if (component) {
    component->m_onHover = std::move(callback);
  }
}

void UIManager::setOnFocus(const std::string &id,
                           std::function<void()> callback) {
  auto component = getComponent(id);
  if (component) {
    component->m_onFocus = std::move(callback);
  }
}

// Layout management
void UIManager::createLayout(const std::string &id, UILayoutType type,
                             const UIRect &bounds) {
  auto layout = std::make_shared<UILayout>();
  layout->m_id = id;
  layout->m_type = type;
  // Apply global scale for resolution-aware sizing
  layout->m_bounds = scaleRect(bounds);

  m_layouts[id] = layout;
}

void UIManager::addComponentToLayout(const std::string &layoutID,
                                     const std::string &componentID) {
  auto layout = getLayout(layoutID);
  if (layout && hasComponent(componentID)) {
    layout->m_childComponents.push_back(componentID);
    updateLayout(layoutID);
  }
}

void UIManager::removeComponentFromLayout(const std::string &layoutID,
                                          const std::string &componentID) {
  auto layout = getLayout(layoutID);
  if (layout) {
    auto &children = layout->m_childComponents;
    children.erase(std::remove(children.begin(), children.end(), componentID),
                   children.end());
    updateLayout(layoutID);
  }
}

void UIManager::updateLayout(const std::string &layoutID) {
  auto layout = getLayout(layoutID);
  if (!layout) {
    return;
  }

  switch (layout->m_type) {
  case UILayoutType::ABSOLUTE_POS:
    applyAbsoluteLayout(layout);
    break;
  case UILayoutType::FLOW:
    applyFlowLayout(layout);
    break;
  case UILayoutType::GRID:
    applyGridLayout(layout);
    break;
  case UILayoutType::STACK:
    applyStackLayout(layout);
    break;
  case UILayoutType::ANCHOR:
    applyAnchorLayout(layout);
    break;
  }
}

void UIManager::setLayoutSpacing(const std::string &layoutID, int spacing) {
  auto layout = getLayout(layoutID);
  if (layout) {
    layout->m_spacing = spacing;
    updateLayout(layoutID);
  }
}

void UIManager::setLayoutColumns(const std::string &layoutID, int columns) {
  auto layout = getLayout(layoutID);
  if (layout) {
    layout->m_columns = columns;
    updateLayout(layoutID);
  }
}

void UIManager::setLayoutAlignment(const std::string &layoutID,
                                   UIAlignment alignment) {
  auto layout = getLayout(layoutID);
  if (layout) {
    layout->m_alignment = alignment;
    updateLayout(layoutID);
  }
}

// Progress bar specific methods
void UIManager::updateProgressBar(const std::string &id, float value) {
  setValue(id, value);
}

void UIManager::setProgressBarRange(const std::string &id, float minVal,
                                    float maxVal) {
  auto component = getComponent(id);
  if (component && component->m_type == UIComponentType::PROGRESS_BAR) {
    component->m_minValue = minVal;
    component->m_maxValue = maxVal;
    component->m_value = std::clamp(component->m_value, minVal, maxVal);
  }
}

// List specific methods
void UIManager::addListItem(const std::string &listID,
                            const std::string &item) {

  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST) {
    component->m_listItems.push_back(item);
    component->m_listItemsDirty = true;
    // Trigger auto-sizing to accommodate new content
    calculateOptimalSize(component);
  }
}

void UIManager::removeListItem(const std::string &listID, int index) {

  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST && index >= 0 &&
      index < static_cast<int>(component->m_listItems.size())) {
    component->m_listItems.erase(component->m_listItems.begin() + index);
    component->m_listItemsDirty = true;
    if (component->m_selectedIndex == index) {
      component->m_selectedIndex = -1;
    } else if (component->m_selectedIndex > index) {
      component->m_selectedIndex--;
    }
    // Trigger auto-sizing (grow-only behavior will prevent shrinking)
    calculateOptimalSize(component);
  }
}

void UIManager::clearList(const std::string &listID) {
  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST) {
    component->m_listItems.clear();
    component->m_listItemsDirty = true;
    component->m_selectedIndex = -1;
  }
}

int UIManager::getSelectedListItem(const std::string &listID) const {
  auto component = getComponent(listID);
  return (component && component->m_type == UIComponentType::LIST)
             ? component->m_selectedIndex
             : -1;
}

void UIManager::setSelectedListItem(const std::string &listID, int index) {
  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST) {
    if (index == -1) {
      component->m_selectedIndex = -1;
      return;
    }

    if (index >= 0 && index < static_cast<int>(component->m_listItems.size())) {
      component->m_selectedIndex = index;
    }
  }
}

void UIManager::setListMaxItems(const std::string &listID, int maxItems) {
  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST) {
    // Store max items in a custom property (we'll use the maxLength field for
    // this)
    component->m_maxLength = maxItems;

    // Trim existing items if they exceed the new limit
    if (static_cast<int>(component->m_listItems.size()) > maxItems) {
      // Keep only the last maxItems entries
      auto &items = component->m_listItems;
      auto startIt = items.end() - maxItems;
      items.erase(items.begin(), startIt);
      component->m_listItemsDirty = true;

      // Adjust selected index if needed
      if (component->m_selectedIndex >= maxItems) {
        component->m_selectedIndex =
            -1; // Clear selection if it's now out of bounds
      }
    }
  }
}

void UIManager::addListItemWithAutoScroll(const std::string &listID,
                                          const std::string &item) {

  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST) {
    // Add the new item
    component->m_listItems.push_back(item);
    component->m_listItemsDirty = true;

    // Check if we need to enforce max items limit
    int maxItems =
        component->m_maxLength; // Using maxLength field to store max items
    if (maxItems > 0 &&
        static_cast<int>(component->m_listItems.size()) > maxItems) {
      // Remove the oldest item
      component->m_listItems.erase(component->m_listItems.begin());

      // Adjust selected index if needed
      if (component->m_selectedIndex > 0) {
        component->m_selectedIndex--;
      } else if (component->m_selectedIndex == 0) {
        component->m_selectedIndex =
            -1; // Clear selection if first item was removed
      }
    }

    // Auto-scroll by selecting the last item (optional behavior)
    // Comment this out if you don't want auto-selection
    // component->m_selectedIndex = static_cast<int>(component->m_listItems.size())
    // - 1;

    // Trigger auto-sizing to accommodate new content
    calculateOptimalSize(component);
  }
}

void UIManager::clearListItems(const std::string &listID) {

  auto component = getComponent(listID);
  if (component && component->m_type == UIComponentType::LIST) {
    component->m_listItems.clear();
    component->m_listItemsDirty = true;
    component->m_selectedIndex = -1;
  }
}

void UIManager::addEventLogEntry(const std::string &logID,
                                 const std::string &entry) {

  auto component = getComponent(logID);
  if (component && component->m_type == UIComponentType::EVENT_LOG) {
    // Add the entry directly (let the caller handle timestamps if needed)
    component->m_listItems.push_back(entry);
    component->m_listItemsDirty = true;

    // Enforce max entries limit - scroll old events out
    int maxEntries = component->m_maxLength;
    if (maxEntries > 0 &&
        static_cast<int>(component->m_listItems.size()) > maxEntries) {
      // Remove oldest events (FIFO behavior)
      while (static_cast<int>(component->m_listItems.size()) > maxEntries) {
        component->m_listItems.erase(component->m_listItems.begin());
      }
    }

    // Event logs are display-only for game events, no selection
    component->m_selectedIndex = -1;

    // Auto-scroll to bottom to show newest events
  }
}

void UIManager::clearEventLog(const std::string &logID) {

  auto component = getComponent(logID);
  if (component && component->m_type == UIComponentType::EVENT_LOG) {
    component->m_listItems.clear();
    component->m_listItemsDirty = true;
    component->m_selectedIndex = -1;

    // Event logs use fixed size for game events display
  }
}

void UIManager::setEventLogMaxEntries(const std::string &logID,
                                      int maxEntries) {

  auto component = getComponent(logID);
  if (component && component->m_type == UIComponentType::EVENT_LOG) {
    component->m_maxLength = maxEntries;
    component->m_listItemsDirty = true;

    // Trim existing entries if needed
    if (static_cast<int>(component->m_listItems.size()) > maxEntries) {
      while (static_cast<int>(component->m_listItems.size()) > maxEntries) {
        component->m_listItems.erase(component->m_listItems.begin());
      }
    }

    // Event logs use fixed size for game events display
  }
}

void UIManager::setTitleAlignment(const std::string &titleID,
                                  UIAlignment alignment) {
  auto component = getComponent(titleID);
  if (!component || component->m_type != UIComponentType::TITLE) {
    return;
  }
  component->m_style.textAlign = alignment;

  // Titles with their text-align set to CENTER_CENTER should also recenter
  // themselves against the surrounding container. If the title has a parent
  // (panel/dialog), center within the parent's bounds; otherwise fall back
  // to window-centering. Only reposition when auto-sizing owns the width —
  // callers who set an explicit width are assumed to have positioned them.
  if (alignment != UIAlignment::CENTER_CENTER ||
      !component->m_autoSize || !component->m_autoWidth) {
    return;
  }

  if (auto parent = getComponent(component->m_parentId)) {
    component->m_bounds.x = parent->m_bounds.x +
        (parent->m_bounds.width - component->m_bounds.width) / 2;
  } else {
    const int windowWidth = GameEngine::Instance().getWidthInPixels();
    component->m_bounds.x = (windowWidth - component->m_bounds.width) / 2;
  }
}

void UIManager::centerTitleInContainer(const std::string &titleID,
                                       int containerX, int containerWidth) {
  auto component = getComponent(titleID);
  if (component && component->m_type == UIComponentType::TITLE) {
    // Center the auto-sized title within the container
    int titleWidth = component->m_bounds.width;
    component->m_bounds.x = containerX + (containerWidth - titleWidth) / 2;
  }
}

void UIManager::setLabelAlignment(const std::string &labelID,
                                  UIAlignment alignment) {
  auto component = getComponent(labelID);
  if (component && component->m_type == UIComponentType::LABEL) {
    component->m_style.textAlign = alignment;
  }
}


void UIManager::enableEventLogAutoUpdate(const std::string &logID,
                                         float interval) {
  auto component = getComponent(logID);
  if (component && component->m_type == UIComponentType::EVENT_LOG) {
    EventLogState state;
    state.m_timer = 0.0f;
    state.m_messageIndex = 0;
    state.m_updateInterval = interval;
    state.m_autoUpdate = true;
    m_eventLogStates[logID] = state;
  }
}

void UIManager::disableEventLogAutoUpdate(const std::string &logID) {
  auto it = m_eventLogStates.find(logID);
  if (it != m_eventLogStates.end()) {
    it->second.m_autoUpdate = false;
  }
}

void UIManager::updateEventLogs(float deltaTime) {
  // Update timers only - no sample message auto-generation
  // States can query timer state via getEventLogState() if needed
  for (auto &[logID, state] : m_eventLogStates) {
    if (!state.m_autoUpdate)
      continue;

    state.m_timer += deltaTime;

    // Reset timer when interval exceeded - let state handle actual content
    if (state.m_timer >= state.m_updateInterval) {
      state.m_timer = 0.0f;
      state.m_messageIndex++;
    }
  }
}

// Input field specific methods
void UIManager::setInputFieldPlaceholder(const std::string &id,
                                         const std::string &placeholder) {
  auto component = getComponent(id);
  if (component && component->m_type == UIComponentType::INPUT_FIELD) {
    component->m_placeholder = placeholder;
  }
}

void UIManager::setInputFieldMaxLength(const std::string &id, int maxLength) {
  auto component = getComponent(id);
  if (component && component->m_type == UIComponentType::INPUT_FIELD) {
    component->m_maxLength = maxLength;
  }
}

bool UIManager::isInputFieldFocused(const std::string &id) const {
  return isComponentFocused(id);
}

// Animation system
void UIManager::animateMove(const std::string &id, const UIRect &targetBounds,
                            float duration, std::function<void()> onComplete) {
  auto component = getComponent(id);
  if (!component) {
    return;
  }

  auto animation = std::make_shared<UIAnimation>();
  animation->m_componentID = id;
  animation->m_duration = duration;
  animation->m_elapsed = 0.0f;
  animation->m_active = true;
  animation->m_startBounds = component->m_bounds;
  animation->m_targetBounds = targetBounds;
  animation->m_onComplete = std::move(onComplete);

  // Remove any existing animation for this component
  stopAnimation(id);

  m_animations.push_back(animation);
}

void UIManager::animateColor(const std::string &id,
                             const SDL_Color &targetColor, float duration,
                             std::function<void()> onComplete) {
  auto component = getComponent(id);
  if (!component) {
    return;
  }

  auto animation = std::make_shared<UIAnimation>();
  animation->m_componentID = id;
  animation->m_duration = duration;
  animation->m_elapsed = 0.0f;
  animation->m_active = true;
  animation->m_startColor = component->m_style.backgroundColor;
  animation->m_targetColor = targetColor;
  animation->m_onComplete = std::move(onComplete);

  // Remove any existing animation for this component
  stopAnimation(id);

  m_animations.push_back(animation);
}

void UIManager::stopAnimation(const std::string &id) {
  m_animations.erase(
      std::remove_if(m_animations.begin(), m_animations.end(),
                     [&id](const std::shared_ptr<UIAnimation> &anim) {
                       return anim->m_componentID == id;
                     }),
      m_animations.end());
}

bool UIManager::isAnimating(const std::string &id) const {
  return std::any_of(m_animations.begin(), m_animations.end(),
                     [&id](const std::shared_ptr<UIAnimation> &anim) {
                       return anim->m_componentID == id && anim->m_active;
                     });
}

// Theme management
void UIManager::loadTheme(const UITheme &theme) {
  m_currentTheme = theme;
  applyCurrentThemeToComponents();
}

void UIManager::setDefaultTheme() {
  // Default theme now uses dark theme as the base
  setDarkTheme();
}

void UIManager::setLightTheme() {
  UITheme lightTheme;
  lightTheme.m_name = "light";
  m_currentThemeMode = "light";

  // Button style - improved contrast and professional appearance
  UIStyle buttonStyle;
  buttonStyle.backgroundColor = {.r=60, .g=120, .b=180, .a=255};
  buttonStyle.borderColor = {.r=255, .g=255, .b=255, .a=255};
  buttonStyle.textColor = {.r=255, .g=255, .b=255, .a=255};
  buttonStyle.hoverColor = {.r=80, .g=140, .b=200, .a=255};
  buttonStyle.pressedColor = {.r=40, .g=100, .b=160, .a=255};
  buttonStyle.highlightOnMouseHover = true;
  buttonStyle.showTooltipOnMouseHover = true;
  buttonStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  buttonStyle.textAlign = UIAlignment::CENTER_CENTER;
  buttonStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::BUTTON] = buttonStyle;

  // Button Danger style - red buttons for Back, Quit, Exit, Delete, etc.
  UIStyle dangerButtonStyle = buttonStyle;
  dangerButtonStyle.backgroundColor = {.r=180, .g=50, .b=50, .a=255};
  dangerButtonStyle.hoverColor = {.r=200, .g=70, .b=70, .a=255};
  dangerButtonStyle.pressedColor = {.r=160, .g=30, .b=30, .a=255};
  lightTheme.m_componentStyles[UIComponentType::BUTTON_DANGER] =
      dangerButtonStyle;

  // Button Success style - green buttons for Save, Confirm, Accept, etc.
  UIStyle successButtonStyle = buttonStyle;
  successButtonStyle.backgroundColor = {.r=50, .g=150, .b=50, .a=255};
  successButtonStyle.hoverColor = {.r=70, .g=170, .b=70, .a=255};
  successButtonStyle.pressedColor = {.r=30, .g=130, .b=30, .a=255};
  lightTheme.m_componentStyles[UIComponentType::BUTTON_SUCCESS] =
      successButtonStyle;

  // Button Warning style - orange buttons for Caution, Reset, etc.
  UIStyle warningButtonStyle = buttonStyle;
  warningButtonStyle.backgroundColor = {.r=200, .g=140, .b=50, .a=255};
  warningButtonStyle.hoverColor = {.r=220, .g=160, .b=70, .a=255};
  warningButtonStyle.pressedColor = {.r=180, .g=120, .b=30, .a=255};
  lightTheme.m_componentStyles[UIComponentType::BUTTON_WARNING] =
      warningButtonStyle;

  // Label style - enhanced contrast
  UIStyle labelStyle;
  labelStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0}; // Transparent
  labelStyle.textColor = {.r=20, .g=20, .b=20, .a=255};  // Dark text for light backgrounds
  labelStyle.textAlign = UIAlignment::CENTER_LEFT;
  labelStyle.fontID = UIConstants::FONT_UI;
  // Text background enabled by default for readability on any background
  labelStyle.useTextBackground = true;
  labelStyle.textBackgroundColor = {.r=255, .g=255, .b=255, .a=100}; // More transparent white
  labelStyle.textBackgroundPadding = UIConstants::LABEL_TEXT_BG_PADDING;
  lightTheme.m_componentStyles[UIComponentType::LABEL] = labelStyle;

  // Panel style - light overlay for subtle UI separation
  UIStyle panelStyle;
  panelStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=40}; // Very light overlay (15% opacity)
  panelStyle.hoverColor = panelStyle.backgroundColor;
  panelStyle.pressedColor = panelStyle.backgroundColor;
  panelStyle.borderWidth = UIConstants::BORDER_WIDTH_NONE;
  panelStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::PANEL] = panelStyle;

  // Progress bar style - enhanced visibility
  UIStyle progressStyle;
  progressStyle.backgroundColor = {.r=40, .g=40, .b=40, .a=255};
  progressStyle.borderColor = {.r=180, .g=180, .b=180, .a=255}; // Stronger borders
  progressStyle.hoverColor = {.r=0, .g=180, .b=0, .a=255};      // Green fill
  progressStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  progressStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::PROGRESS_BAR] = progressStyle;

  // Input field style - light background with dark text
  UIStyle inputStyle;
  inputStyle.backgroundColor = {.r=245, .g=245, .b=245, .a=255};
  inputStyle.textColor = {.r=20, .g=20, .b=20, .a=255}; // Dark text for good contrast
  inputStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  inputStyle.hoverColor = {.r=235, .g=245, .b=255, .a=255};
  inputStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  inputStyle.textAlign = UIAlignment::CENTER_LEFT;
  inputStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::INPUT_FIELD] = inputStyle;

  // List style - light background with enhanced item height
  UIStyle listStyle;
  listStyle.backgroundColor = {.r=240, .g=240, .b=240, .a=255};
  listStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  listStyle.textColor = {.r=20, .g=20, .b=20, .a=255};     // Dark text on light background
  listStyle.hoverColor = {.r=180, .g=200, .b=255, .a=255}; // Light blue selection
  listStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  // Calculate list item height based on font metrics
  listStyle.listItemHeight =
      UIConstants::DEFAULT_LIST_ITEM_HEIGHT; // Will be calculated dynamically during rendering
  listStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::LIST] = listStyle;

  // Slider style - enhanced borders
  UIStyle sliderStyle;
  sliderStyle.backgroundColor = {.r=100, .g=100, .b=100, .a=255};
  sliderStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  sliderStyle.hoverColor = {.r=60, .g=120, .b=180, .a=255}; // Blue handle
  sliderStyle.pressedColor = {.r=40, .g=100, .b=160, .a=255};
  sliderStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  sliderStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::SLIDER] = sliderStyle;

  // Checkbox style - enhanced visibility
  UIStyle checkboxStyle = buttonStyle;
  checkboxStyle.backgroundColor = {.r=180, .g=180, .b=180, .a=255};
  checkboxStyle.hoverColor = {.r=200, .g=200, .b=200, .a=255};
  checkboxStyle.textColor = {.r=20, .g=20, .b=20, .a=255}; // Dark text for light backgrounds
  checkboxStyle.highlightOnMouseHover = false;
  checkboxStyle.showTooltipOnMouseHover = false;
  checkboxStyle.textAlign = UIAlignment::CENTER_LEFT;
  checkboxStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::CHECKBOX] = checkboxStyle;

  // Tooltip style
  UIStyle tooltipStyle = panelStyle;
  tooltipStyle.backgroundColor = {.r=40, .g=40, .b=40, .a=230}; // More opaque for tooltips
  tooltipStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  tooltipStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  tooltipStyle.textColor = {.r=255, .g=255, .b=255, .a=255}; // White text for dark tooltip background
  tooltipStyle.fontID = UIConstants::FONT_TOOLTIP;
  lightTheme.m_componentStyles[UIComponentType::TOOLTIP] = tooltipStyle;

  // Image component uses transparent background
  UIStyle imageStyle;
  imageStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};
  imageStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::IMAGE] = imageStyle;

  // Event log style - similar to list but optimized for display-only
  UIStyle eventLogStyle = listStyle;
  // Calculate event log item height based on font metrics
  eventLogStyle.listItemHeight =
      24; // Will be calculated dynamically during rendering
  eventLogStyle.backgroundColor = {.r=245, .g=245, .b=250, .a=160};    // Semi-transparent light background
  eventLogStyle.textColor = {.r=0, .g=0, .b=0, .a=255}; // Black text for maximum contrast
  eventLogStyle.borderColor = {.r=120, .g=120, .b=140, .a=180}; // Less transparent border
  lightTheme.m_componentStyles[UIComponentType::EVENT_LOG] = eventLogStyle;

  // Title style - large, prominent text for headings
  UIStyle titleStyle;
  titleStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0}; // Transparent background
  titleStyle.textColor = {.r=0, .g=198, .b=230, .a=255}; // Dark Cyan color for titles
  titleStyle.fontSize = UIConstants::TITLE_FONT_SIZE;                  // Use native title font size
  titleStyle.textAlign = UIAlignment::CENTER_LEFT;
  titleStyle.fontID = UIConstants::FONT_TITLE;
  // Text background enabled by default for readability on any background
  titleStyle.useTextBackground = true;
  titleStyle.textBackgroundColor = {.r=20, .g=20, .b=20, .a=120}; // More transparent dark for gold text
  titleStyle.textBackgroundPadding = UIConstants::TITLE_TEXT_BG_PADDING;
  lightTheme.m_componentStyles[UIComponentType::TITLE] = titleStyle;

  // Dialog style - solid background for modal dialogs
  UIStyle dialogStyle;
  dialogStyle.backgroundColor = {.r=245, .g=245, .b=245, .a=255}; // Light solid background
  dialogStyle.borderColor = {.r=120, .g=120, .b=120, .a=255}; // Dark border for definition
  dialogStyle.borderWidth = UIConstants::BORDER_WIDTH_DIALOG;
  dialogStyle.fontID = UIConstants::FONT_UI;
  lightTheme.m_componentStyles[UIComponentType::DIALOG] = dialogStyle;

  m_currentTheme = lightTheme;
  applyCurrentThemeToComponents();
}

void UIManager::setDarkTheme() {
  UITheme darkTheme;
  darkTheme.m_name = "dark";
  m_currentThemeMode = "dark";

  // Button style - enhanced contrast for dark theme
  UIStyle buttonStyle;
  buttonStyle.backgroundColor = {.r=50, .g=50, .b=60, .a=255};
  buttonStyle.borderColor = {.r=180, .g=180, .b=180, .a=255}; // Brighter borders
  buttonStyle.textColor = {.r=255, .g=255, .b=255, .a=255};
  buttonStyle.hoverColor = {.r=70, .g=70, .b=80, .a=255};
  buttonStyle.pressedColor = {.r=30, .g=30, .b=40, .a=255};
  buttonStyle.highlightOnMouseHover = true;
  buttonStyle.showTooltipOnMouseHover = true;
  buttonStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  buttonStyle.textAlign = UIAlignment::CENTER_CENTER;
  buttonStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::BUTTON] = buttonStyle;

  // Button Danger style - red buttons for Back, Quit, Exit, Delete, etc.
  UIStyle dangerButtonStyle = buttonStyle;
  dangerButtonStyle.backgroundColor = {.r=200, .g=60, .b=60, .a=255};
  dangerButtonStyle.hoverColor = {.r=220, .g=80, .b=80, .a=255};
  dangerButtonStyle.pressedColor = {.r=180, .g=40, .b=40, .a=255};
  darkTheme.m_componentStyles[UIComponentType::BUTTON_DANGER] = dangerButtonStyle;

  // Button Success style - green buttons for Save, Confirm, Accept, etc.
  UIStyle successButtonStyle = buttonStyle;
  successButtonStyle.backgroundColor = {.r=60, .g=160, .b=60, .a=255};
  successButtonStyle.hoverColor = {.r=80, .g=180, .b=80, .a=255};
  successButtonStyle.pressedColor = {.r=40, .g=140, .b=40, .a=255};
  darkTheme.m_componentStyles[UIComponentType::BUTTON_SUCCESS] =
      successButtonStyle;

  // Button Warning style - orange buttons for Caution, Reset, etc.
  UIStyle warningButtonStyle = buttonStyle;
  warningButtonStyle.backgroundColor = {.r=220, .g=150, .b=60, .a=255};
  warningButtonStyle.hoverColor = {.r=240, .g=170, .b=80, .a=255};
  warningButtonStyle.pressedColor = {.r=200, .g=130, .b=40, .a=255};
  darkTheme.m_componentStyles[UIComponentType::BUTTON_WARNING] =
      warningButtonStyle;

  // Label style - pure white text for maximum contrast
  UIStyle labelStyle;
  labelStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};   // Transparent
  labelStyle.textColor = {.r=255, .g=255, .b=255, .a=255}; // Pure white
  labelStyle.textAlign = UIAlignment::CENTER_LEFT;
  labelStyle.fontID = UIConstants::FONT_UI;
  // Text background enabled by default for readability on any background
  labelStyle.useTextBackground = true;
  labelStyle.textBackgroundColor = {.r=0, .g=0, .b=0, .a=100}; // More transparent black
  labelStyle.textBackgroundPadding = UIConstants::LABEL_TEXT_BG_PADDING;
  darkTheme.m_componentStyles[UIComponentType::LABEL] = labelStyle;

  // Panel style - slightly more overlay for dark theme
  UIStyle panelStyle;
  panelStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=50}; // 19% opacity
  panelStyle.hoverColor = panelStyle.backgroundColor;
  panelStyle.pressedColor = panelStyle.backgroundColor;
  panelStyle.borderWidth = UIConstants::BORDER_WIDTH_NONE;
  panelStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::PANEL] = panelStyle;

  // Progress bar style
  UIStyle progressStyle;
  progressStyle.backgroundColor = {.r=20, .g=20, .b=20, .a=255};
  progressStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  progressStyle.hoverColor = {.r=0, .g=180, .b=0, .a=255}; // Green fill
  progressStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  progressStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::PROGRESS_BAR] = progressStyle;

  // Input field style - dark theme
  UIStyle inputStyle;
  inputStyle.backgroundColor = {.r=40, .g=40, .b=40, .a=255};
  inputStyle.textColor = {.r=255, .g=255, .b=255, .a=255}; // White text
  inputStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  inputStyle.hoverColor = {.r=50, .g=50, .b=50, .a=255};
  inputStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  inputStyle.textAlign = UIAlignment::CENTER_LEFT;
  inputStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::INPUT_FIELD] = inputStyle;

  // List style - dark theme
  UIStyle listStyle;
  listStyle.backgroundColor = {.r=35, .g=35, .b=35, .a=255};
  listStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  listStyle.textColor = {.r=255, .g=255, .b=255, .a=255}; // White text
  listStyle.hoverColor = {.r=60, .g=80, .b=150, .a=255};  // Blue selection
  listStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  // Calculate list item height based on font metrics
  listStyle.listItemHeight =
      UIConstants::DEFAULT_LIST_ITEM_HEIGHT; // Will be calculated dynamically during rendering
  listStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::LIST] = listStyle;

  // Slider style
  UIStyle sliderStyle;
  sliderStyle.backgroundColor = {.r=30, .g=30, .b=30, .a=255};
  sliderStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  sliderStyle.hoverColor = {.r=60, .g=120, .b=180, .a=255}; // Blue handle
  sliderStyle.pressedColor = {.r=40, .g=100, .b=160, .a=255};
  sliderStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  sliderStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::SLIDER] = sliderStyle;

  // Checkbox style
  UIStyle checkboxStyle = buttonStyle;
  checkboxStyle.backgroundColor = {.r=60, .g=60, .b=60, .a=255};
  checkboxStyle.hoverColor = {.r=80, .g=80, .b=80, .a=255};
  checkboxStyle.textColor = {.r=255, .g=255, .b=255, .a=255};
  checkboxStyle.highlightOnMouseHover = false;
  checkboxStyle.showTooltipOnMouseHover = false;
  checkboxStyle.textAlign = UIAlignment::CENTER_LEFT;
  checkboxStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::CHECKBOX] = checkboxStyle;

  // Tooltip style
  UIStyle tooltipStyle;
  tooltipStyle.backgroundColor = {.r=20, .g=20, .b=20, .a=240};
  tooltipStyle.borderColor = {.r=180, .g=180, .b=180, .a=255};
  tooltipStyle.borderWidth = UIConstants::BORDER_WIDTH_NORMAL;
  tooltipStyle.textColor = {.r=255, .g=255, .b=255, .a=255};
  tooltipStyle.fontID = UIConstants::FONT_TOOLTIP;
  darkTheme.m_componentStyles[UIComponentType::TOOLTIP] = tooltipStyle;

  // Image component uses transparent background
  UIStyle imageStyle;
  imageStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};
  imageStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::IMAGE] = imageStyle;

  // Event log style - similar to list but optimized for display-only
  UIStyle eventLogStyle = listStyle;
  // Calculate event log item height based on font metrics
  eventLogStyle.listItemHeight =
      24; // Will be calculated dynamically during rendering
  eventLogStyle.backgroundColor = {.r=25, .g=30, .b=35, .a=80}; // Highly transparent dark background
  eventLogStyle.textColor = {.r=255, .g=255, .b=255, .a=255}; // Pure white text for maximum contrast
  eventLogStyle.borderColor = {.r=100, .g=120, .b=140, .a=100}; // Highly transparent blue-gray border
  darkTheme.m_componentStyles[UIComponentType::EVENT_LOG] = eventLogStyle;

  // Title style - large, prominent text for headings
  UIStyle titleStyle;
  titleStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0}; // Transparent background
  titleStyle.textColor = {.r=0, .g=198, .b=230, .a=255}; // Dark Cyan color for titles
  titleStyle.fontSize = UIConstants::TITLE_FONT_SIZE;                  // Use native title font size
  titleStyle.textAlign = UIAlignment::CENTER_LEFT;
  titleStyle.fontID = UIConstants::FONT_TITLE;
  // Text background enabled by default for readability on any background
  titleStyle.useTextBackground = true;
  titleStyle.textBackgroundColor = {.r=0, .g=0, .b=0, .a=120}; // More transparent black for gold text
  titleStyle.textBackgroundPadding = UIConstants::TITLE_TEXT_BG_PADDING;
  darkTheme.m_componentStyles[UIComponentType::TITLE] = titleStyle;

  // Dialog style - solid background for modal dialogs
  UIStyle dialogStyle;
  dialogStyle.backgroundColor = {.r=45, .g=45, .b=45, .a=255}; // Dark solid background
  dialogStyle.borderColor = {.r=160, .g=160, .b=160, .a=255}; // Light border for definition
  dialogStyle.borderWidth = UIConstants::BORDER_WIDTH_DIALOG;
  dialogStyle.fontID = UIConstants::FONT_UI;
  darkTheme.m_componentStyles[UIComponentType::DIALOG] = dialogStyle;

  m_currentTheme = darkTheme;
  applyCurrentThemeToComponents();
}

void UIManager::setThemeMode(const std::string &mode) {
  if (mode == "light") {
    setLightTheme();
  } else if (mode == "dark") {
    setDarkTheme();
  } else if (mode == "default") {
    // For backward compatibility, default now uses dark theme
    setDarkTheme();
  }
}

const std::string &UIManager::getCurrentThemeMode() const {
  return m_currentThemeMode;
}

void UIManager::createOverlay(int windowWidth, int windowHeight) {
  // Remove existing overlay if it exists
  removeOverlay();

  // Create semi-transparent overlay panel using current theme's panel style
  createPanel("__overlay", {0, 0, windowWidth, windowHeight});

  // Set overlay z-order to render behind dialogs and other components
  auto overlay = getComponent("__overlay");
  if (overlay) {
    overlay->m_zOrder = UIConstants::ZORDER_OVERLAY;
    invalidateComponentCache();
  }

  // Set positioning to always fill window on resize (fixedWidth/Height = -1 means full window dimensions)
  // This ensures overlay properly resizes during fullscreen toggles and window resize events
  setComponentPositioning("__overlay", {UIPositionMode::TOP_ALIGNED, 0, 0, -1, -1});
}

void UIManager::removeOverlay() {
  // Remove the overlay panel if it exists
  if (hasComponent("__overlay")) {
    removeComponent("__overlay");
  }
}

void UIManager::removeComponentsWithPrefix(const std::string &prefix) {

  // Collect components to remove (can't modify map while iterating)
  std::vector<std::string> componentsToRemove;
  componentsToRemove.reserve(UIConstants::DEFAULT_COMPONENT_BATCH_SIZE); // Reserve capacity for performance

  for (const auto &[id, component] : m_components) {
    if (id.substr(0, prefix.length()) == prefix) {
      componentsToRemove.push_back(id);
    }
  }

  // Remove collected components
  // BUGFIX: Use removeComponent() instead of direct erase to properly handle:
  // - Binding count decrements (m_textBinding/m_listBinding)
  // - Cache invalidation
  // - Layout removal
  // - Focus clearing
  for (const auto &id : componentsToRemove) {
    removeComponent(id);
  }
}

void UIManager::clearAllComponents() {
  // Enhanced clearAllComponents - preserve theme background but clear
  // everything else
  std::vector<std::string> componentsToRemove;
  componentsToRemove.reserve(UIConstants::MAX_COMPONENT_BATCH_SIZE); // Reserve capacity for performance

  for (const auto &[id, component] : m_components) {
    if (id != "__overlay") {
      componentsToRemove.push_back(id);
    }
  }

  for (const auto &id : componentsToRemove) {
    removeComponent(id);
  }

  // Clear other collections
  m_layouts.clear();
  m_animations.clear();
  m_clickedButtons.clear();
  m_hoveredComponents.clear();
  m_focusedComponent.clear();
  m_hoveredTooltip.clear();
  m_hoveredTooltipCandidate.clear();
}

void UIManager::resetToDefaultTheme() {
  // Reset to default dark theme (only used by states that actually change
  // themes)
  setDarkTheme();
  m_currentThemeMode = "dark";
}

void UIManager::cleanupForStateTransition() {
  // Comprehensive cleanup for safe state transitions

  // Clear all UI components (reset binding count since we're clearing everything)
  m_components.clear();
  m_sortedComponentsCache.clear();
  m_activeBindingCount = 0;
  invalidateComponentCache();

  // Clear value/text caches
  m_valueCache.clear();
  m_textCache.clear();

  // Clear all layouts
  m_layouts.clear();

  // Stop and clear all animations
  m_animations.clear();

  // Clear any queued callbacks and frame-local GPU batch descriptors.
  m_deferredCallbacks.clear();
  clearFrameRenderBatches();

  // Clear all interaction state
  m_clickedButtons.clear();
  m_hoveredComponents.clear();
  m_focusedComponent.clear();
  m_keyboardSelection.clear();
  m_hoveredTooltip.clear();
  m_hoveredTooltipCandidate.clear();
  m_tooltipTimer = 0.0f;

  // Clear event log states
  m_eventLogStates.clear();

  // Remove overlay if present
  removeOverlay();

  // Reset to default theme
  resetToDefaultTheme();

  // Reset mouse state
  m_lastMousePosition = Vector2D(0, 0);
  m_mousePressed = false;
  m_mouseReleased = false;

  // Reset global settings to defaults
  m_globalStyle = UIStyle{};
  m_globalFontID = UIConstants::FONT_DEFAULT;
  // NOTE: m_globalScale is NOT reset here - it's resolution-dependent and should
  // persist across state transitions. Only init() and onWindowResize() modify it.

  UI_INFO("UIManager prepared for state transition");
}

void UIManager::prepareForStateTransition() {
  // Simplified public interface that delegates to the comprehensive cleanup
  cleanupForStateTransition();
}

void UIManager::applyThemeToComponent(const std::string &id,
                                      UIComponentType type) {
  auto component = getComponent(id);
  applyThemeStyle(component, type);
}

void UIManager::setGlobalStyle(const UIStyle &style) { m_globalStyle = style; }

// Utility methods
void UIManager::setGlobalFont(const std::string &fontID) {
  m_globalFontID = fontID;

  // Update all components to use the new font
  for (const auto &[id, component] : m_components) {
    if (component) {
      component->m_style.fontID = fontID;
    }
  }
}

void UIManager::setGlobalScale(float scale) { m_globalScale = scale; }

float UIManager::calculateOptimalScale(int width, int height) const {
  // Use baseline resolution from UIConstants for consistent UI scaling
  // Cap at max UI scale to prevent UI from scaling larger than original on high resolutions
  float scale = std::min(width / UIConstants::BASELINE_WIDTH_F, height / UIConstants::BASELINE_HEIGHT_F);
  return std::min(UIConstants::MAX_UI_SCALE, scale);  // Cap at maximum UI scale
}

// Private helper methods
std::shared_ptr<UIComponent> UIManager::getComponent(const std::string &id) {
  auto it = m_components.find(id);
  return (it != m_components.end()) ? it->second : nullptr;
}

std::shared_ptr<const UIComponent>
UIManager::getComponent(const std::string &id) const {
  auto it = m_components.find(id);
  return (it != m_components.end()) ? it->second : nullptr;
}

std::shared_ptr<UILayout> UIManager::getLayout(const std::string &id) {
  auto it = m_layouts.find(id);
  return (it != m_layouts.end()) ? it->second : nullptr;
}

void UIManager::registerComponent(const std::shared_ptr<UIComponent> &component,
                                  const std::string &parentId,
                                  bool autoSize) {
  if (!component) {
    return;
  }

  m_components[component->m_id] = component;
  linkToParent(component, parentId);

  if (autoSize) {
    calculateOptimalSize(component);
  }

  invalidateComponentCache();
}

void UIManager::applyThemeStyle(const std::shared_ptr<UIComponent> &component,
                                UIComponentType type) const {
  if (!component) {
    return;
  }

  const UIAlignment preservedAlignment = component->m_style.textAlign;
  component->m_style = m_currentTheme.getStyle(type);
  component->m_style.textAlign = preservedAlignment;

  if (component->m_hasBackdropAncestor &&
      (component->m_type == UIComponentType::LABEL ||
       component->m_type == UIComponentType::TITLE)) {
    component->m_style.useTextBackground = false;
  }
}

void UIManager::applyCurrentThemeToComponents() const {
  for (const auto &entry : m_components) {
    const auto &component = entry.second;
    if (component) {
      applyThemeStyle(component, component->m_type);
    }
  }
}

int UIManager::calculateListItemHeight(
    const std::shared_ptr<UIComponent> &component) const {
  int lineHeight = 0;
  const int scaledPadding =
      static_cast<int>(UIConstants::LIST_ITEM_PADDING * m_globalScale);
  int itemHeight =
      static_cast<int>(UIConstants::DEFAULT_LIST_ITEM_HEIGHT * m_globalScale);

  if (component &&
      FontManager::Instance().getFontMetrics(component->m_style.fontID,
                                             &lineHeight, nullptr, nullptr)) {
    itemHeight = lineHeight + scaledPadding;
  }

  return std::max(1, itemHeight);
}

void UIManager::handleInput() {
  const auto &inputManager = InputManager::Instance();

  // Get mouse position
  Vector2D const mousePos = inputManager.getMousePosition();
  m_lastMousePosition = mousePos;

  // Check mouse state
  bool const mouseDown = inputManager.getMouseButtonState(LEFT);
  bool mouseJustPressed = mouseDown && !m_mousePressed;
  bool mouseJustReleased = !mouseDown && m_mousePressed;

  m_mousePressed = mouseDown;
  m_mouseReleased = mouseJustReleased;

  // Clear previous hover state
  m_hoveredComponents.clear();
  m_hoveredTooltipCandidate.clear();

  // Process components in reverse z-order (top to bottom, const ref avoids copy)
  bool mouseHandled = false;
  const auto& sortedComponents = getSortedComponents();
  for (auto it = sortedComponents.rbegin(); it != sortedComponents.rend(); ++it) {
    auto& component = *it;
    if (!component || !component->m_visible || !component->m_enabled) {
        continue;
    }

    if (mouseHandled) {
      // Reset state for components below
      if (component->m_state == UIState::HOVERED ||
          component->m_state == UIState::PRESSED) {
        component->m_state = UIState::NORMAL;
      }
      continue;
    }

    // InputManager already converts coordinates to logical coordinates
    int mouseX = static_cast<int>(mousePos.getX());
    int mouseY = static_cast<int>(mousePos.getY());

    bool isHovered = component->m_bounds.contains(mouseX, mouseY);

    if (isHovered) {
      m_hoveredComponents.push_back(component->m_id);
      if (m_hoveredTooltipCandidate.empty() &&
          component->m_style.showTooltipOnMouseHover) {
        m_hoveredTooltipCandidate = component->m_id;
      }

      // Handle hover state
      if (component->m_style.highlightOnMouseHover &&
          component->m_state == UIState::NORMAL) {
        component->m_state = UIState::HOVERED;
        if (component->m_onHover) {
          m_deferredCallbacks.push_back(component->m_onHover);
        }
      } else if (!component->m_style.highlightOnMouseHover &&
                 component->m_state == UIState::HOVERED) {
        component->m_state = UIState::NORMAL;
      }

      // Modal-style components (e.g. the modal overlay) swallow input for any
      // lower-z component beneath the cursor. We mark mouseHandled so the rest
      // of the loop short-circuits, preventing click-through to the UI below.
      if (component->m_blocksInputBelow) {
        mouseHandled = true;
        continue;
      }

      // Handle click/press for interactive components
      if (component->m_type == UIComponentType::BUTTON ||
          component->m_type == UIComponentType::BUTTON_DANGER ||
          component->m_type == UIComponentType::BUTTON_SUCCESS ||
          component->m_type == UIComponentType::BUTTON_WARNING ||
          component->m_type == UIComponentType::CHECKBOX ||
          component->m_type == UIComponentType::SLIDER) {

        if (mouseJustPressed) {
          component->m_state = UIState::PRESSED;
          m_focusedComponent = component->m_id;
          if (component->m_onFocus) {
            m_deferredCallbacks.push_back(component->m_onFocus);
          }
          mouseHandled = true;  // Prevent components below from receiving press
        }

        if (mouseJustReleased && component->m_state == UIState::PRESSED) {
          // Handle click
          if (component->m_type == UIComponentType::BUTTON ||
              component->m_type == UIComponentType::BUTTON_DANGER ||
              component->m_type == UIComponentType::BUTTON_SUCCESS ||
              component->m_type == UIComponentType::BUTTON_WARNING) {
            m_clickedButtons.push_back(component->m_id);
            if (component->m_onClick) {
              m_deferredCallbacks.push_back(component->m_onClick);
            }
          } else if (component->m_type == UIComponentType::CHECKBOX) {
            component->m_checked = !component->m_checked;
            if (component->m_onClick) {
              m_deferredCallbacks.push_back(component->m_onClick);
            }
          }
          component->m_state = component->m_style.highlightOnMouseHover
                                   ? UIState::HOVERED
                                   : UIState::NORMAL;
          mouseHandled = true;
        }

        // Handle slider dragging
        if (component->m_type == UIComponentType::SLIDER &&
            component->m_state == UIState::PRESSED) {
          float relativeX = (mousePos.getX() - component->m_bounds.x) /
                            static_cast<float>(component->m_bounds.width);
          float newValue =
              component->m_minValue +
              relativeX * (component->m_maxValue - component->m_minValue);
          setValue(component->m_id, newValue);
         }

      }

      // Handle input field focus
      if (component->m_type == UIComponentType::INPUT_FIELD && mouseJustPressed) {
        m_focusedComponent = component->m_id;
        component->m_state = UIState::FOCUSED;
        if (component->m_onFocus) {
          m_deferredCallbacks.push_back(component->m_onFocus);
        }
        mouseHandled = true;
      }

      // Handle list selection
      if (component->m_type == UIComponentType::LIST && mouseJustPressed) {
        const int itemHeight = calculateListItemHeight(component);
        int itemIndex = static_cast<int>(
            (mousePos.getY() - component->m_bounds.y) / itemHeight);
        if (itemIndex >= 0 &&
            itemIndex < static_cast<int>(component->m_listItems.size())) {
          component->m_selectedIndex = itemIndex;
          if (component->m_onClick) {
            m_deferredCallbacks.push_back(component->m_onClick);
          }
        }
        mouseHandled = true;
      }
    } else {
      // Not hovered
      if (component->m_state == UIState::HOVERED) {
        component->m_state = UIState::NORMAL;
      }
      if (component->m_state == UIState::PRESSED && mouseJustReleased) {
        component->m_state = UIState::NORMAL;
      }
    }
  }

  // Handle focus loss
  if (mouseJustPressed && !mouseHandled) {
    if (!m_focusedComponent.empty()) {
      auto focusedComponent = getComponent(m_focusedComponent);
      if (focusedComponent && focusedComponent->m_state == UIState::FOCUSED) {
        focusedComponent->m_state = UIState::NORMAL;
      }
    }
    m_focusedComponent.clear();
  }

  // Gamepad/keyboard selection wins over mouse hover when active.
  // MenuNavigation::applySelection() only sets m_keyboardSelection when a
  // gamepad is connected, so a non-empty value implies gamepad-driven menu
  // navigation. In that mode the mouse cursor is typically stale (user isn't
  // moving it), and letting a stale mouse hover suppress the gamepad highlight
  // means the selected component has no visible highlight.
  if (!m_keyboardSelection.empty()) {
    // Clear any HOVERED state the mouse-hover loop just set on other
    // components so we render exactly one highlight (the gamepad target).
    for (const auto &id : m_hoveredComponents) {
      auto other = getComponent(id);
      if (other && other->m_state == UIState::HOVERED) {
        other->m_state = UIState::NORMAL;
      }
    }
    m_hoveredComponents.clear();

    auto selected = getComponent(m_keyboardSelection);
    if (selected && selected->m_visible && selected->m_enabled &&
        selected->m_state != UIState::PRESSED) {
      selected->m_state = UIState::HOVERED;
      m_hoveredComponents.push_back(m_keyboardSelection);
    }
  }
}

void UIManager::updateAnimations(float deltaTime) {
  for (auto it = m_animations.begin(); it != m_animations.end();) {
    auto &anim = *it;
    if (!anim->m_active) {
      it = m_animations.erase(it);
      continue;
    }

    anim->m_elapsed += deltaTime;
    float t = std::min(anim->m_elapsed / anim->m_duration, 1.0f);

    auto component = getComponent(anim->m_componentID);
    if (component) {
      // Apply animation
      if (anim->m_startBounds.width > 0) {
        // Position/size animation
        component->m_bounds =
            interpolateRect(anim->m_startBounds, anim->m_targetBounds, t);
      } else {
        // Color animation
        component->m_style.backgroundColor =
            interpolateColor(anim->m_startColor, anim->m_targetColor, t);
      }
    }

    if (t >= 1.0f) {
      // Animation complete
      anim->m_active = false;
      if (anim->m_onComplete) {
        anim->m_onComplete();
      }
      it = m_animations.erase(it);
    } else {
      ++it;
    }
  }
}

void UIManager::updateTooltips(float deltaTime) {
  if (!m_tooltipsEnabled) {
    return;
  }

  if (!m_hoveredTooltipCandidate.empty()) {
    if (m_hoveredTooltip != m_hoveredTooltipCandidate) {
      m_hoveredTooltip = m_hoveredTooltipCandidate;
      m_tooltipTimer = 0.0f;
    }
    m_tooltipTimer += deltaTime;
  } else {
    m_hoveredTooltip.clear();
    m_tooltipTimer = 0.0f;
  }
}

bool UIManager::isClickOnUI(const Vector2D& screenPos) const {
    int const mouseX = static_cast<int>(screenPos.getX());
    int const mouseY = static_cast<int>(screenPos.getY());

    for (const auto& [id, component] : m_components) {
        if (component && component->m_visible && component->m_enabled) {
            if (component->m_bounds.contains(mouseX, mouseY)) {
                return true; // Click is on a UI element
            }
        }
    }

    return false; // Click is not on any UI element
}
// Layout implementations
void UIManager::applyAbsoluteLayout(
    const std::shared_ptr<UILayout> & /* layout */) {
  // Absolute layout doesn't change component positions
}

void UIManager::applyFlowLayout(const std::shared_ptr<UILayout> &layout) {
  if (!layout)
    return;

  int currentX = layout->m_bounds.x;
  int currentY = layout->m_bounds.y;
  int maxHeight = 0;

  for (const auto &componentID : layout->m_childComponents) {
    auto component = getComponent(componentID);
    if (!component)
      continue;

    // Check if we need to wrap to next line
    if (currentX + component->m_bounds.width >
        layout->m_bounds.x + layout->m_bounds.width) {
      currentX = layout->m_bounds.x;
      currentY += maxHeight + layout->m_spacing;
      maxHeight = 0;
    }

    component->m_bounds.x = currentX;
    component->m_bounds.y = currentY;

    currentX += component->m_bounds.width + layout->m_spacing;
    maxHeight = std::max(maxHeight, component->m_bounds.height);
  }
}

void UIManager::applyGridLayout(const std::shared_ptr<UILayout> &layout) {
  if (!layout || layout->m_columns <= 0)
    return;

  int cellWidth = layout->m_bounds.width / layout->m_columns;
  int cellHeight = layout->m_bounds.height / layout->m_rows;

  for (size_t i = 0; i < layout->m_childComponents.size(); ++i) {
    auto component = getComponent(layout->m_childComponents[i]);
    if (!component)
      continue;

    int col = i % layout->m_columns;
    int row = i / layout->m_columns;

    component->m_bounds.x = layout->m_bounds.x + col * cellWidth;
    component->m_bounds.y = layout->m_bounds.y + row * cellHeight;
    component->m_bounds.width = cellWidth - layout->m_spacing;
    component->m_bounds.height = cellHeight - layout->m_spacing;
  }
}

void UIManager::applyStackLayout(const std::shared_ptr<UILayout> &layout) {
  if (!layout)
    return;

  int currentY = layout->m_bounds.y;

  for (const auto &componentID : layout->m_childComponents) {
    auto component = getComponent(componentID);
    if (!component)
      continue;

    component->m_bounds.x = layout->m_bounds.x;
    component->m_bounds.y = currentY;
    component->m_bounds.width = layout->m_bounds.width;

    currentY += component->m_bounds.height + layout->m_spacing;
  }
}

void UIManager::applyAnchorLayout(const std::shared_ptr<UILayout> &layout) {
  // TODO: Implement anchor-based layout
  applyAbsoluteLayout(layout);
}

SDL_Color UIManager::interpolateColor(const SDL_Color &start,
                                      const SDL_Color &end, float t) {
  return {.r = static_cast<Uint8>(start.r + (end.r - start.r) * t),
          .g = static_cast<Uint8>(start.g + (end.g - start.g) * t),
          .b = static_cast<Uint8>(start.b + (end.b - start.b) * t),
          .a = static_cast<Uint8>(start.a + (end.a - start.a) * t)};
}

UIRect UIManager::interpolateRect(const UIRect &start, const UIRect &end,
                                  float t) {
  return {static_cast<int>(start.x + (end.x - start.x) * t),
          static_cast<int>(start.y + (end.y - start.y) * t),
          static_cast<int>(start.width + (end.width - start.width) * t),
          static_cast<int>(start.height + (end.height - start.height) * t)};
}

// Auto-sizing implementation
void UIManager::calculateOptimalSize(const std::string &id) {
  auto component = getComponent(id);
  if (component) {
    calculateOptimalSize(component);
  }
}

void UIManager::calculateOptimalSize(const std::shared_ptr<UIComponent> &component) {
  if (!component || !component->m_autoSize) {
    return;
  }

  int contentWidth = 0;
  int contentHeight = 0;

  if (!measureComponentContent(component, &contentWidth, &contentHeight)) {
    return; // Failed to measure content
  }

  // Apply content padding (scaled for resolution-aware sizing)
  int scaledContentPadding = static_cast<int>(component->m_contentPadding * m_globalScale);

  // Implement grow-only behavior for lists to prevent shrinking
  if (component->m_type == UIComponentType::LIST) {
    // Update minimum bounds to current size to prevent shrinking
    component->m_minBounds.width =
        std::max(component->m_minBounds.width, component->m_bounds.width);
    component->m_minBounds.height =
        std::max(component->m_minBounds.height, component->m_bounds.height);
  }

  // Apply size constraints - ONLY modify width/height, preserve x/y position
  if (component->m_autoWidth) {
    int totalWidth = contentWidth + (scaledContentPadding * 2);
    int oldWidth = component->m_bounds.width;
    component->m_bounds.width =
        std::max(component->m_minBounds.width,
                 std::min(totalWidth, component->m_maxBounds.width));

    // Automatically center only titles and labels with CENTER alignment when
    // width changes
    if (component->m_style.textAlign == UIAlignment::CENTER_CENTER &&
        component->m_bounds.width != oldWidth &&
        (component->m_type == UIComponentType::TITLE ||
         component->m_type == UIComponentType::LABEL)) {
      // Get pixel width for centering calculation
      const auto &gameEngine = GameEngine::Instance();
      int windowWidth = gameEngine.getWidthInPixels();
      component->m_bounds.x = (windowWidth - component->m_bounds.width) / 2;
    }
  }

  if (component->m_autoHeight) {
    int totalHeight = contentHeight + (scaledContentPadding * 2);
    component->m_bounds.height =
        std::max(component->m_minBounds.height,
                 std::min(totalHeight, component->m_maxBounds.height));
  }

  // CRITICAL: Never modify component->m_bounds.x or component->m_bounds.y
  // Auto-sizing only affects dimensions, not position

  // Trigger content changed callback if present
  if (component->m_onContentChanged) {
    component->m_onContentChanged();
  }
}

bool UIManager::measureComponentContent(
    const std::shared_ptr<UIComponent> &component, int *width, int *height) {
  if (!component || !width || !height) {
    return false;
  }

  auto &fontManager = FontManager::Instance();

  switch (component->m_type) {
  case UIComponentType::BUTTON:
  case UIComponentType::BUTTON_DANGER:
  case UIComponentType::BUTTON_SUCCESS:
  case UIComponentType::BUTTON_WARNING:
  case UIComponentType::LABEL:
  case UIComponentType::TITLE:
    if (!component->m_text.empty()) {
      // Check if text contains newlines - use multiline measurement if so
      if (component->m_text.find('\n') != std::string::npos) {
        return fontManager.measureMultilineText(
            component->m_text, component->m_style.fontID, 0, width, height);
      } else {
        return fontManager.measureText(component->m_text, component->m_style.fontID,
                                       width, height);
      }
    }
    *width = component->m_minBounds.width;
    *height = component->m_minBounds.height;
    return true;

  case UIComponentType::INPUT_FIELD:
    // For input fields, measure placeholder or current text
    if (!component->m_text.empty()) {
      fontManager.measureText(component->m_text, component->m_style.fontID, width,
                              height);
    } else if (!component->m_placeholder.empty()) {
      fontManager.measureText(component->m_placeholder, component->m_style.fontID,
                              width, height);
    } else {
      // Default to reasonable input field size
      fontManager.measureText("Sample Text", component->m_style.fontID, width,
                              height);
    }
    // Input fields need extra space for cursor and interaction (scaled)
    *width += static_cast<int>(UIConstants::INPUT_CURSOR_SPACE * m_globalScale);
    return true;

  case UIComponentType::LIST: {
    const int itemHeight = calculateListItemHeight(component);

    // Calculate based on list items and item height
    if (!component->m_listItems.empty()) {
      int maxItemWidth = 0;
      for (const auto &item : component->m_listItems) {
        int itemWidth = 0;
        if (fontManager.measureText(item, component->m_style.fontID, &itemWidth,
                                    nullptr)) {
          maxItemWidth = std::max(maxItemWidth, itemWidth);
        } else {
          // If text measurement fails, estimate based on character count
          // Assume ~12px per character for UI fonts
          maxItemWidth =
              std::max(maxItemWidth, static_cast<int>(item.length() * UIConstants::CHAR_WIDTH_ESTIMATE));
        }
      }
      int const scaledScrollbarSpace = static_cast<int>(UIConstants::SCROLLBAR_WIDTH * m_globalScale);
      *width = std::max(maxItemWidth + scaledScrollbarSpace,
                        UIConstants::MIN_LIST_WIDTH); // Add scrollbar space, minimum list width
      *height = itemHeight * static_cast<int>(component->m_listItems.size());
    } else {
      // Provide reasonable defaults for empty lists
      *width = UIConstants::DEFAULT_LIST_WIDTH;             // Default width
      *height = itemHeight * UIConstants::DEFAULT_LIST_VISIBLE_ITEMS; // Height for default visible items
    }
    return true;
  }

  case UIComponentType::EVENT_LOG:
    // Fixed size for game event display
    *width = component->m_bounds.width;
    *height = component->m_bounds.height;
    return true;

  case UIComponentType::TOOLTIP:
    if (!component->m_text.empty()) {
      return fontManager.measureText(component->m_text, component->m_style.fontID,
                                     width, height);
    }
    break;

  default:
    // For other component types, use current bounds or minimums
    *width = std::max(component->m_bounds.width, component->m_minBounds.width);
    *height = std::max(component->m_bounds.height, component->m_minBounds.height);
    return true;
  }

  // Fallback to minimum bounds
  *width = component->m_minBounds.width;
  *height = component->m_minBounds.height;
  return true;
}

void UIManager::invalidateLayout(const std::string &layoutID) {
  // Mark layout for recalculation on next update
  // For now, immediately recalculate
  recalculateLayout(layoutID);
}

void UIManager::recalculateLayout(const std::string &layoutID) {
  auto layout = getLayout(layoutID);
  if (!layout) {
    return;
  }

  // First, auto-size all child components
  for (const auto &componentID : layout->m_childComponents) {
    calculateOptimalSize(componentID);
  }

  // Then apply the layout with new sizes
  updateLayout(layoutID);
}

void UIManager::enableAutoSizing(const std::string &id, bool enable) {
  auto component = getComponent(id);
  if (component) {
    component->m_autoSize = enable;
    if (enable) {
      calculateOptimalSize(component);
    }
  }
}

void UIManager::setAutoSizingConstraints(const std::string &id,
                                         const UIRect &minBounds,
                                         const UIRect &maxBounds) {
  auto component = getComponent(id);
  if (component) {
    component->m_minBounds = minBounds;
    component->m_maxBounds = maxBounds;
    calculateOptimalSize(component);
  }
}

// Auto-detection methods
int UIManager::getWidthInPixels() const {
  const auto &gameEngine = GameEngine::Instance();
  return gameEngine.getWidthInPixels();
}

int UIManager::getHeightInPixels() const {
  const auto &gameEngine = GameEngine::Instance();
  return gameEngine.getHeightInPixels();
}

// Auto-detecting overlay creation
void UIManager::createOverlay() {
  // Use baseline dimensions from UIConstants - createOverlay(width, height) will scale to logical space
  createOverlay(UIConstants::BASELINE_WIDTH, UIConstants::BASELINE_HEIGHT);
}

// Convenience positioning methods
void UIManager::createTitleAtTop(const std::string &id, const std::string &text,
                                 int height) {
  // Use baseline width from UIConstants - createTitle() will scale to logical space
  createTitle(id, {0, UIConstants::TITLE_TOP_OFFSET, UIConstants::BASELINE_WIDTH, height}, text);
  setTitleAlignment(id, UIAlignment::CENTER_CENTER);

  // Apply positioning using unified API
  setComponentPositioning(id, {UIPositionMode::TOP_ALIGNED,
                               0,
                               UIConstants::TITLE_TOP_OFFSET,
                               -1,  // -1 = use full window width
                               height});
}

void UIManager::createButtonAtBottom(const std::string &id,
                                     const std::string &text, int width,
                                     int height) {
  // Use baseline height from UIConstants - createButtonDanger() will scale to logical space
  createButtonDanger(id, {UIConstants::BUTTON_BOTTOM_OFFSET, UIConstants::BASELINE_HEIGHT - height - UIConstants::BUTTON_BOTTOM_OFFSET, width, height},
                     text);

  // Apply positioning using unified API
  setComponentPositioning(id, {UIPositionMode::BOTTOM_ALIGNED,
                               UIConstants::BUTTON_BOTTOM_OFFSET,
                               UIConstants::BUTTON_BOTTOM_OFFSET,
                               width,
                               height});
}

void UIManager::createCenteredDialog(const std::string &id, int width,
                                     int height, const std::string &theme) {
  // Use baseline dimensions from UIConstants - createModal() will scale to logical space
  // Calculate centered position in baseline space
  int const x = (UIConstants::BASELINE_WIDTH - width) / 2;
  int const y = (UIConstants::BASELINE_HEIGHT - height) / 2;

  // Overlay also uses baseline dimensions
  createModal(id, {x, y, width, height}, theme, UIConstants::BASELINE_WIDTH, UIConstants::BASELINE_HEIGHT);

  // Apply positioning using unified API (dialog itself)
  setComponentPositioning(id, {UIPositionMode::CENTERED_BOTH,
                               0,
                               0,
                               width,
                               height});
}

void UIManager::createCenteredButton(const std::string &id, int offsetY,
                                     int width, int height,
                                     const std::string &text) {
  // Calculate baseline center position
  int const centerX = (UIConstants::BASELINE_WIDTH - width) / 2;
  int const centerY = UIConstants::BASELINE_HEIGHT / 2 + offsetY;

  createButton(id, {centerX, centerY, width, height}, text);

  // Apply positioning using unified API
  setComponentPositioning(id, {UIPositionMode::CENTERED_BOTH,
                               0,
                               offsetY,
                               width,
                               height});
}

void UIManager::createPanelAtBottomRight(const std::string &id, int width, int height,
                                         int offsetX, int offsetY) {
  // Calculate initial bounds in baseline coordinates
  int const x = UIConstants::BASELINE_WIDTH - width - offsetX;
  int const y = UIConstants::BASELINE_HEIGHT - height - offsetY;
  createPanel(id, UIRect{x, y, width, height});
  setComponentPositioning(id, {UIPositionMode::BOTTOM_RIGHT, offsetX, offsetY, width, height});
}

void UIManager::createLabelAtBottomRight(const std::string &id, const std::string &text,
                                         int width, int height, int offsetX, int offsetY) {
  // Calculate initial bounds in baseline coordinates
  int const x = UIConstants::BASELINE_WIDTH - width - offsetX;
  int const y = UIConstants::BASELINE_HEIGHT - height - offsetY;
  createLabel(id, UIRect{x, y, width, height}, text);
  setComponentPositioning(id, {UIPositionMode::BOTTOM_RIGHT, offsetX, offsetY, width, height});
}

// Auto-repositioning system implementation
void UIManager::onWindowResize(int newWidthInPixels, int newHeightInPixels) {

  UI_DEBUG(std::format("Window resized: {}x{} - auto-repositioning UI components",
                       newWidthInPixels, newHeightInPixels));

  // Recalculate UI scale for new resolution (1920x1080 baseline, capped at 1.0)
  m_globalScale = calculateOptimalScale(newWidthInPixels, newHeightInPixels);
  UI_INFO(std::format("UI scale updated to {} for new resolution {}x{}",
                      m_globalScale, newWidthInPixels, newHeightInPixels));

  repositionAllComponents(newWidthInPixels, newHeightInPixels);
  m_currentWidthInPixels = newWidthInPixels;
  m_currentHeightInPixels = newHeightInPixels;
}

void UIManager::repositionAllComponents(int width, int height) {
  // No lock needed - UI is single-threaded (main thread only)
  for (auto &[id, component] : m_components) {
    if (component) {
      applyPositioning(component, width, height);
    }
  }
}

void UIManager::applyPositioning(std::shared_ptr<UIComponent> component,
                                 int width, int height) {
  if (!component) {
    return;
  }

  auto &pos = component->m_positioning;
  auto &bounds = component->m_bounds;

  // Update dimensions if fixed sizes specified, applying global scale for resolution adaptation
  // Special cases:
  //   widthPercent/heightPercent > 0 = percentage of window dimension (takes precedence)
  //   -1 = use full window dimension
  //   < -1 = use full window dimension minus the absolute value (for margins)
  if (pos.widthPercent > 0.0f && pos.widthPercent <= 1.0f) {
    bounds.width = static_cast<int>(width * pos.widthPercent);  // Percentage-based width
  } else if (pos.fixedWidth == -1) {
    bounds.width = width;
  } else if (pos.fixedWidth < -1) {
    bounds.width = width + static_cast<int>(pos.fixedWidth * m_globalScale);  // Scale negative margin
  } else if (pos.fixedWidth > 0) {
    bounds.width = static_cast<int>(pos.fixedWidth * m_globalScale);  // Scale fixed width
  }

  if (pos.heightPercent > 0.0f && pos.heightPercent <= 1.0f) {
    bounds.height = static_cast<int>(height * pos.heightPercent);  // Percentage-based height
  } else if (pos.fixedHeight == -1) {
    bounds.height = height;
  } else if (pos.fixedHeight < -1) {
    bounds.height = height + static_cast<int>(pos.fixedHeight * m_globalScale);  // Scale negative margin
  } else if (pos.fixedHeight > 0) {
    bounds.height = static_cast<int>(pos.fixedHeight * m_globalScale);  // Scale fixed height
  }

  // Apply positioning based on mode, with scaled offsets for resolution adaptation
  int scaledOffsetX = static_cast<int>(pos.offsetX * m_globalScale);
  int scaledOffsetY = static_cast<int>(pos.offsetY * m_globalScale);

  switch (pos.mode) {
  case UIPositionMode::ABSOLUTE:
    // No change - keep current position
    break;

  case UIPositionMode::CENTERED_H:
    // Horizontally centered + offsetX, fixed offsetY
    bounds.x = (width - bounds.width) / 2 + scaledOffsetX;
    bounds.y = scaledOffsetY;
    break;

  case UIPositionMode::CENTERED_V:
    // Vertically centered + offsetY, fixed offsetX
    bounds.x = scaledOffsetX;
    bounds.y = (height - bounds.height) / 2 + scaledOffsetY;
    break;

  case UIPositionMode::CENTERED_BOTH:
    // Center both axes + offsets
    bounds.x = (width - bounds.width) / 2 + scaledOffsetX;
    bounds.y = (height - bounds.height) / 2 + scaledOffsetY;
    break;

  case UIPositionMode::TOP_ALIGNED:
    // Top-left: x = offsetX, y = offsetY
    bounds.x = scaledOffsetX;
    bounds.y = scaledOffsetY;
    break;

  case UIPositionMode::TOP_RIGHT:
    // Top-right: x = right - width - offsetX, y = offsetY
    bounds.x = width - bounds.width - scaledOffsetX;
    bounds.y = scaledOffsetY;
    break;

  case UIPositionMode::BOTTOM_ALIGNED:
    // Bottom edge - height - offsetY, fixed offsetX
    bounds.x = scaledOffsetX;
    bounds.y = height - bounds.height - scaledOffsetY;
    break;

  case UIPositionMode::BOTTOM_CENTERED:
    // Bottom edge - height - offsetY, horizontally centered + offsetX
    bounds.x = (width - bounds.width) / 2 + scaledOffsetX;
    bounds.y = height - bounds.height - scaledOffsetY;
    break;

  case UIPositionMode::BOTTOM_RIGHT:
    // Bottom-right corner: right edge - width - offsetX, bottom edge - height - offsetY
    bounds.x = width - bounds.width - scaledOffsetX;
    bounds.y = height - bounds.height - scaledOffsetY;
    break;

  case UIPositionMode::LEFT_ALIGNED:
    // Left edge + offsetX, vertically centered + offsetY
    bounds.x = scaledOffsetX;
    bounds.y = (height - bounds.height) / 2 + scaledOffsetY;
    break;

  case UIPositionMode::RIGHT_ALIGNED:
    // Right edge - width - offsetX, vertically centered + offsetY
    bounds.x = width - bounds.width - scaledOffsetX;
    bounds.y = (height - bounds.height) / 2 + scaledOffsetY;
    break;
  }
}

void UIManager::setComponentPositioning(const std::string &id,
                                        const UIPositioning &positioning) {
  auto component = getComponent(id);
  if (component) {
    component->m_positioning = positioning;
    // Immediately apply the new positioning
    applyPositioning(component, m_currentWidthInPixels, m_currentHeightInPixels);
  }
}

void UIManager::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer) {
  clearFrameRenderBatches();

  if (m_components.empty()) {
    return;
  }

  auto& primPool = gpuRenderer.getPrimitiveVertexPool();
  auto& uiPool = gpuRenderer.getUIVertexPool();

  auto* primBase = static_cast<VoidLight::ColorVertex*>(primPool.getMappedPtr());
  auto* uiBase = static_cast<VoidLight::SpriteVertex*>(uiPool.getMappedPtr());

  if (!primBase || !uiBase) {
    return;
  }

  uint32_t primOffset = 0;
  uint32_t uiOffset = 0;
  const uint32_t primitiveVertexLimit = static_cast<uint32_t>(
      std::min(primPool.getMaxVertices(),
               static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
  const uint32_t uiVertexLimit = static_cast<uint32_t>(
      std::min(uiPool.getMaxVertices(),
               static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
  const float viewportHeight = static_cast<float>(gpuRenderer.getViewportHeight());

  // Helper to add a filled rectangle
  auto addFilledRect = [&](const UIRect& rect, const SDL_Color& color) {
    if (primitiveVertexLimit < 6 || primOffset > primitiveVertexLimit - 6) return;

    float x = static_cast<float>(rect.x);
    float y = static_cast<float>(rect.y);
    float w = static_cast<float>(rect.width);
    float h = static_cast<float>(rect.height);
    float top = viewportHeight - y;
    float bottom = top - h;

    VoidLight::ColorVertex* v = primBase + primOffset;
    // Triangle 1
    v[0] = {.x=x,     .y=top,    .r=color.r, .g=color.g, .b=color.b, .a=color.a};
    v[1] = {.x=x + w, .y=top,    .r=color.r, .g=color.g, .b=color.b, .a=color.a};
    v[2] = {.x=x + w, .y=bottom, .r=color.r, .g=color.g, .b=color.b, .a=color.a};
    // Triangle 2
    v[3] = {.x=x,     .y=top,    .r=color.r, .g=color.g, .b=color.b, .a=color.a};
    v[4] = {.x=x + w, .y=bottom, .r=color.r, .g=color.g, .b=color.b, .a=color.a};
    v[5] = {.x=x,     .y=bottom, .r=color.r, .g=color.g, .b=color.b, .a=color.a};

    primOffset += 6;
    m_uiPrimitiveVertexCount = primOffset;
  };

  // Helper to add border (4 thin rectangles)
  auto addBorder = [&](const UIRect& rect, const SDL_Color& color, int width) {
    if (width <= 0) return;
    // Top
    addFilledRect({rect.x, rect.y, rect.width, width}, color);
    // Bottom
    addFilledRect({rect.x, rect.y + rect.height - width, rect.width, width}, color);
    // Left
    addFilledRect({rect.x, rect.y + width, width, rect.height - 2 * width}, color);
    // Right
    addFilledRect({rect.x + rect.width - width, rect.y + width, width, rect.height - 2 * width}, color);
  };

  auto appendImageBatch = [&](SDL_GPUTexture* texture, uint32_t vertexOffset,
                              uint32_t vertexCount) {
    if (!texture || vertexCount == 0) {
      return;
    }

    if (!m_imageRenderBatches.empty()) {
      auto& last = m_imageRenderBatches.back();
      if (last.texture == texture &&
          last.vertexOffset + last.vertexCount == vertexOffset) {
        last.vertexCount += vertexCount;
        return;
      }
    }

    VoidLight::UITextureDrawBatch batch;
    batch.texture = texture;
    batch.vertexOffset = vertexOffset;
    batch.vertexCount = vertexCount;
    m_imageRenderBatches.push_back(batch);
  };

  auto appendTextBatch = [&](SDL_GPUTexture* texture, VoidLight::UITextPipelineKind pipeline,
                             uint32_t vertexOffset, uint32_t vertexCount) {
    if (!texture || vertexCount == 0) {
      return;
    }

    if (!m_textRenderBatches.empty()) {
      auto& last = m_textRenderBatches.back();
      if (last.texture == texture &&
          last.pipeline == pipeline &&
          last.vertexOffset + last.vertexCount == vertexOffset) {
        last.vertexCount += vertexCount;
        return;
      }
    }

    VoidLight::UITextDrawBatch batch;
    batch.texture = texture;
    batch.pipeline = pipeline;
    batch.vertexOffset = vertexOffset;
    batch.vertexCount = vertexCount;
    m_textRenderBatches.push_back(batch);
  };

  auto addImage = [&](const std::shared_ptr<UIComponent>& component) {
    if (!component || component->m_textureID.empty()) {
      return;
    }

    auto textureData =
        TextureManager::Instance().getGPUTextureData(component->m_textureID);
    if (!textureData || !textureData->texture || !textureData->texture->get()) {
      return;
    }

    UIRect srcRect = component->m_useImageSourceRect
                         ? component->m_imageSourceRect
                         : UIRect{0, 0,
                                  static_cast<int>(textureData->width),
                                  static_cast<int>(textureData->height)};
    if (srcRect.width <= 0 || srcRect.height <= 0 ||
        component->m_bounds.width <= 0 || component->m_bounds.height <= 0 ||
        uiVertexLimit < 6 || uiOffset > uiVertexLimit - 6) {
      return;
    }

    const float textureWidth = textureData->width > 0.0f ? textureData->width : 1.0f;
    const float textureHeight = textureData->height > 0.0f ? textureData->height : 1.0f;
    const float u0 = static_cast<float>(srcRect.x) / textureWidth;
    const float v0 = static_cast<float>(srcRect.y) / textureHeight;
    const float u1 = static_cast<float>(srcRect.x + srcRect.width) / textureWidth;
    const float v1 = static_cast<float>(srcRect.y + srcRect.height) / textureHeight;

    const float x = static_cast<float>(component->m_bounds.x);
    const float y = static_cast<float>(component->m_bounds.y);
    const float w = static_cast<float>(component->m_bounds.width);
    const float h = static_cast<float>(component->m_bounds.height);
    const float top = viewportHeight - y;
    const float bottom = top - h;

    VoidLight::SpriteVertex* v = uiBase + uiOffset;
    v[0] = {.x=x,     .y=top,    .u=u0, .v=v0, .r=255, .g=255, .b=255, .a=255};
    v[1] = {.x=x + w, .y=top,    .u=u1, .v=v0, .r=255, .g=255, .b=255, .a=255};
    v[2] = {.x=x + w, .y=bottom, .u=u1, .v=v1, .r=255, .g=255, .b=255, .a=255};
    v[3] = {.x=x,     .y=top,    .u=u0, .v=v0, .r=255, .g=255, .b=255, .a=255};
    v[4] = {.x=x + w, .y=bottom, .u=u1, .v=v1, .r=255, .g=255, .b=255, .a=255};
    v[5] = {.x=x,     .y=bottom, .u=u0, .v=v1, .r=255, .g=255, .b=255, .a=255};

    appendImageBatch(textureData->texture->get(), uiOffset, 6);
    uiOffset += 6;
  };

  // Helper to add text with optional background
  auto& fontMgr = FontManager::Instance();
  auto addText = [&](const std::string& textKey, const std::string& text,
                     const std::string& fontID, int x, int y,
                     const SDL_Color& color, int alignment,
                     bool useBackground = false, const SDL_Color& bgColor = {.r=0, .g=0, .b=0, .a=0},
                     int bgPadding = 0, const UIRect* clampBounds = nullptr) {
    if (text.empty()) return;

    int textWidth = 0;
    int textHeight = 0;
    if (!fontMgr.prepareGPUText(textKey, text, fontID, &textWidth, &textHeight)) {
      return;
    }

    // Calculate position based on alignment
    float dstX = static_cast<float>(x);
    float dstY = static_cast<float>(y);
    float dstW = static_cast<float>(textWidth);
    float dstH = static_cast<float>(textHeight);

    switch (alignment) {
      case 1: // Left (vertically centered)
        dstY -= dstH / 2;
        break;
      case 2: // Right (vertically centered)
        dstX -= dstW;
        dstY -= dstH / 2;
        break;
      case 3: // Top-left
        // x and y stay as-is (top-left corner)
        break;
      case 4: // Top-center
        dstX -= dstW / 2;
        break;
      case 5: // Top-right
        dstX -= dstW;
        break;
      default: // Center (0)
        dstX -= dstW / 2;
        dstY -= dstH / 2;
        break;
    }

    // Draw background rectangle if enabled (added to the primitive family, renders before text)
    if (useBackground && bgColor.a > 0) {
      UIRect bgRect;
      bgRect.x = static_cast<int>(dstX) - bgPadding;
      bgRect.y = static_cast<int>(dstY) - bgPadding;
      bgRect.width = static_cast<int>(dstW) + (bgPadding * 2);
      bgRect.height = static_cast<int>(dstH) + (bgPadding * 2);
      // Safety net: clamp the text-bg to the source component's declared
      // bounds so a glyph wider than the container can never paint past the
      // component's edge (e.g. small-scale resolutions where the font is
      // floored at MIN_UI_FONT_SIZE but bounds are scaled).
      if (clampBounds && bgRect.width > 0 && bgRect.height > 0) {
        const int right = std::min(bgRect.x + bgRect.width,
                                   clampBounds->x + clampBounds->width);
        const int bottom = std::min(bgRect.y + bgRect.height,
                                    clampBounds->y + clampBounds->height);
        bgRect.x = std::max(bgRect.x, clampBounds->x);
        bgRect.y = std::max(bgRect.y, clampBounds->y);
        bgRect.width = std::max(0, right - bgRect.x);
        bgRect.height = std::max(0, bottom - bgRect.y);
      }
      if (bgRect.width > 0 && bgRect.height > 0) {
        addFilledRect(bgRect, bgColor);
      }
    }

    // Raster UI text should land on whole pixels to avoid linear-filter
    // coverage loss at glyph edges when centered inside integer UI bounds.
    dstX = std::round(dstX);
    dstY = std::round(dstY);

    TTF_GPUAtlasDrawSequence* drawSequence = fontMgr.getGPUTextDrawData(textKey);
    if (!drawSequence) {
      return;
    }

    VoidLight::SpriteVertex* v = uiBase + uiOffset;
    for (TTF_GPUAtlasDrawSequence* seq = drawSequence; seq != nullptr; seq = seq->next) {
      if (!seq->atlas_texture || !seq->xy || !seq->uv || !seq->indices ||
          seq->num_indices <= 0 || seq->num_vertices <= 0) {
        continue;
      }

      const uint32_t indexCount = static_cast<uint32_t>(seq->num_indices);
      if (indexCount > uiVertexLimit || uiOffset > uiVertexLimit - indexCount) {
        return;
      }

      v = uiBase + uiOffset;
      SDL_Color drawColor = color;
      auto pipeline = VoidLight::UITextPipelineKind::Alpha;
      if (seq->image_type == TTF_IMAGE_COLOR) {
        drawColor = {.r=255, .g=255, .b=255, .a=color.a};
        pipeline = VoidLight::UITextPipelineKind::Color;
      } else if (seq->image_type == TTF_IMAGE_SDF) {
        pipeline = VoidLight::UITextPipelineKind::SDF;
      }
      for (int i = 0; i < seq->num_indices; ++i) {
        int sourceIndex = seq->indices[i];
        if (sourceIndex < 0 || sourceIndex >= seq->num_vertices) {
          return;
        }

        const SDL_FPoint& pos = seq->xy[sourceIndex];
        const SDL_FPoint& uv = seq->uv[sourceIndex];
        // SDL3_ttf GPU text already provides UVs in SDL_GPU convention.
        v[i] = {.x=dstX + pos.x, .y=(viewportHeight - dstY) + pos.y,
                .u=uv.x, .v=uv.y,
                .r=drawColor.r, .g=drawColor.g, .b=drawColor.b, .a=drawColor.a};
      }

      appendTextBatch(seq->atlas_texture, pipeline, uiOffset, indexCount);
      uiOffset += indexCount;
    }
  };

  // Record components by z-priority inside the fixed UI render families.
  const auto& sortedComponents = getSortedComponents();

  // Modal render occlusion: fixed families intentionally do not provide
  // arbitrary cross-family z-order. Modal overlays instead become an explicit
  // render cutoff; lower normal UI is not recorded into any family.
  int modalCullZ = std::numeric_limits<int>::min();
  for (const auto& component : sortedComponents) {
    if (component && component->m_visible && component->m_occludesRenderingBelow) {
      modalCullZ = std::max(modalCullZ, component->m_zOrder);
    }
  }

  for (const auto& component : sortedComponents) {
    if (!component || !component->m_visible) continue;
    if (component->m_zOrder < modalCullZ) continue;

    SDL_Color bgColor = component->m_style.backgroundColor;

    // Determine background color based on state
    switch (component->m_state) {
      case UIState::HOVERED:
        bgColor = component->m_style.hoverColor;
        break;
      case UIState::PRESSED:
        bgColor = component->m_style.pressedColor;
        break;
      case UIState::DISABLED:
        bgColor = component->m_style.disabledColor;
        break;
      default:
        break;
    }

    // Render based on component type
    switch (component->m_type) {
      case UIComponentType::BUTTON:
      case UIComponentType::BUTTON_DANGER:
      case UIComponentType::BUTTON_SUCCESS:
      case UIComponentType::BUTTON_WARNING:
      case UIComponentType::PANEL:
      case UIComponentType::DIALOG:
        // Draw background
        addFilledRect(component->m_bounds, bgColor);
        // Draw border
        if (component->m_style.borderWidth > 0) {
          addBorder(component->m_bounds, component->m_style.borderColor,
                    component->m_style.borderWidth);
        }
        // Draw text for buttons
        if (!component->m_text.empty() &&
            (component->m_type == UIComponentType::BUTTON ||
             component->m_type == UIComponentType::BUTTON_DANGER ||
             component->m_type == UIComponentType::BUTTON_SUCCESS ||
             component->m_type == UIComponentType::BUTTON_WARNING)) {
          int textX = component->m_bounds.x + component->m_bounds.width / 2;
          int textY = component->m_bounds.y + component->m_bounds.height / 2;
          addText(component->m_id, component->m_text, component->m_style.fontID,
                  textX, textY, component->m_style.textColor, 0);
        }
        break;

      case UIComponentType::LABEL:
      case UIComponentType::TITLE:
        if (component->m_style.backgroundColor.a > 0) {
          addFilledRect(component->m_bounds, component->m_style.backgroundColor);
        }
        if (!component->m_text.empty()) {
          int textX, textY, alignment;
          int scaledPadding = static_cast<int>(component->m_style.padding * m_globalScale);

          switch (component->m_style.textAlign) {
            case UIAlignment::CENTER_CENTER:
              textX = component->m_bounds.x + component->m_bounds.width / 2;
              textY = component->m_bounds.y + component->m_bounds.height / 2;
              alignment = 0; // center
              break;
            case UIAlignment::CENTER_RIGHT:
              textX = component->m_bounds.x + component->m_bounds.width - scaledPadding;
              textY = component->m_bounds.y + component->m_bounds.height / 2;
              alignment = 2; // right
              break;
            case UIAlignment::CENTER_LEFT:
              textX = component->m_bounds.x + scaledPadding;
              textY = component->m_bounds.y + component->m_bounds.height / 2;
              alignment = 1; // left
              break;
            case UIAlignment::TOP_CENTER:
              textX = component->m_bounds.x + component->m_bounds.width / 2;
              textY = component->m_bounds.y + scaledPadding;
              alignment = 4; // top-center
              break;
            case UIAlignment::TOP_LEFT:
              textX = component->m_bounds.x + scaledPadding;
              textY = component->m_bounds.y + scaledPadding;
              alignment = 3; // top-left
              break;
            case UIAlignment::TOP_RIGHT:
              textX = component->m_bounds.x + component->m_bounds.width - scaledPadding;
              textY = component->m_bounds.y + scaledPadding;
              alignment = 5; // top-right
              break;
            default:
              // CENTER_LEFT is default
              textX = component->m_bounds.x + scaledPadding;
              textY = component->m_bounds.y + component->m_bounds.height / 2;
              alignment = 1; // left
              break;
          }

          // Only use text backgrounds for components with transparent backgrounds
          bool needsBackground = component->m_style.useTextBackground &&
                                 component->m_style.backgroundColor.a == 0;
          int scaledTextBgPadding = static_cast<int>(component->m_style.textBackgroundPadding * m_globalScale);

          addText(component->m_id, component->m_text, component->m_style.fontID,
                  textX, textY, component->m_style.textColor, alignment,
                  needsBackground, component->m_style.textBackgroundColor,
                  scaledTextBgPadding, &component->m_bounds);
        }
        break;

      case UIComponentType::PROGRESS_BAR:
        // Draw background
        addFilledRect(component->m_bounds, component->m_style.backgroundColor);
        // Draw border
        if (component->m_style.borderWidth > 0) {
          addBorder(component->m_bounds, component->m_style.borderColor,
                    component->m_style.borderWidth);
        }
        // Draw fill
        {
          float range = component->m_maxValue - component->m_minValue;
          float progress = (range != 0.0f)
                               ? (component->m_value - component->m_minValue) / range
                               : 0.0f;
          progress = std::clamp(progress, 0.0f, 1.0f);
          int fillWidth = static_cast<int>(component->m_bounds.width * progress);
          if (fillWidth > 0) {
            UIRect fillRect = {component->m_bounds.x, component->m_bounds.y,
                               fillWidth, component->m_bounds.height};
            addFilledRect(fillRect, component->m_style.hoverColor);
          }
        }
        break;

      case UIComponentType::CHECKBOX:
        {
          // Scale checkbox size and padding for resolution-aware sizing
          int scaledCheckboxSize = static_cast<int>(UIConstants::CHECKBOX_SIZE * m_globalScale);
          int scaledPadding = static_cast<int>(component->m_style.padding * m_globalScale);

          // Draw checkbox box
          UIRect boxBounds;
          boxBounds.x = component->m_bounds.x;
          boxBounds.y = component->m_bounds.y +
                        (component->m_bounds.height - scaledCheckboxSize) / 2;
          boxBounds.width = scaledCheckboxSize;
          boxBounds.height = scaledCheckboxSize;

          SDL_Color boxColor = component->m_style.backgroundColor;
          if (component->m_state == UIState::HOVERED) {
            boxColor = component->m_style.hoverColor;
          }
          addFilledRect(boxBounds, boxColor);

          // Draw border around checkbox box
          addBorder(boxBounds, component->m_style.borderColor, 1);

          // Draw checkmark if checked
          if (component->m_checked) {
            int checkX = boxBounds.x + boxBounds.width / 2;
            int checkY = boxBounds.y + boxBounds.height / 2;
            m_scratchTextKey.clear();
            std::format_to(std::back_inserter(m_scratchTextKey), "{}#check",
                           component->m_id);
            addText(m_scratchTextKey, "X",
                    component->m_style.fontID, checkX, checkY,
                    component->m_style.textColor, 0);  // Center
          }

          // Draw label text
          if (!component->m_text.empty()) {
            int textX = boxBounds.x + boxBounds.width + scaledPadding;
            int textY = component->m_bounds.y + component->m_bounds.height / 2;
            addText(component->m_id, component->m_text, component->m_style.fontID, textX, textY,
                    component->m_style.textColor, 1);  // Left aligned
          }
        }
        break;

      case UIComponentType::SLIDER:
        {
          // When the slider is the keyboard-selected component (HOVERED state
          // in our selection model), draw a highlight border around the full
          // slider bounds so the player gets visual feedback. Sliders always
          // render their handle in hoverColor, so the state alone isn't visible.
          if (component->m_state == UIState::HOVERED) {
            addBorder(component->m_bounds, component->m_style.hoverColor, 2);
          }

          // Draw track
          UIRect trackRect = {
              component->m_bounds.x,
              component->m_bounds.y + component->m_bounds.height / 2 - UIConstants::SLIDER_TRACK_OFFSET,
              component->m_bounds.width,
              UIConstants::SLIDER_TRACK_HEIGHT
          };
          addFilledRect(trackRect, component->m_style.backgroundColor);
          addBorder(trackRect, component->m_style.borderColor, UIConstants::BORDER_WIDTH_NORMAL);

          // Calculate handle position
          float range = component->m_maxValue - component->m_minValue;
          float progress = (range != 0.0f)
                               ? (component->m_value - component->m_minValue) / range
                               : 0.0f;
          progress = std::clamp(progress, 0.0f, 1.0f);

          int handleX = component->m_bounds.x +
                        static_cast<int>((component->m_bounds.width - UIConstants::SLIDER_HANDLE_WIDTH) * progress);
          UIRect handleRect = {
              handleX,
              component->m_bounds.y,
              UIConstants::SLIDER_HANDLE_WIDTH,
              component->m_bounds.height
          };

          SDL_Color handleColor = component->m_style.hoverColor;
          if (component->m_state == UIState::PRESSED) {
            handleColor = component->m_style.pressedColor;
          }

          addFilledRect(handleRect, handleColor);
          addBorder(handleRect, component->m_style.borderColor, 1);
        }
        break;

      case UIComponentType::INPUT_FIELD:
        {
          SDL_Color inputBgColor = component->m_style.backgroundColor;
          if (component->m_state == UIState::FOCUSED) {
            inputBgColor = component->m_style.hoverColor;
          }

          // Draw background
          addFilledRect(component->m_bounds, inputBgColor);

          // Draw border (blue if focused)
          SDL_Color borderColor = component->m_style.borderColor;
          if (component->m_state == UIState::FOCUSED) {
            borderColor = {.r=100, .g=150, .b=255, .a=255};  // Blue focus border
          }
          addBorder(component->m_bounds, borderColor, component->m_style.borderWidth);

          // Scale padding
          int scaledPadding = static_cast<int>(component->m_style.padding * m_globalScale);

          // Draw text or placeholder
          std::string displayText =
              component->m_text.empty() ? component->m_placeholder : component->m_text;
          if (!displayText.empty()) {
            SDL_Color textColor = component->m_text.empty()
                                      ? SDL_Color{128, 128, 128, 255}  // Placeholder gray
                                      : component->m_style.textColor;

            int textX = component->m_bounds.x + scaledPadding;
            int textY = component->m_bounds.y + component->m_bounds.height / 2;
            addText(component->m_id, displayText, component->m_style.fontID,
                    textX, textY, textColor, 1);  // Left aligned
          }
        }
        break;

      case UIComponentType::LIST:
        {
          // Draw background
          addFilledRect(component->m_bounds, component->m_style.backgroundColor);

          // Draw border
          if (component->m_style.borderWidth > 0) {
            addBorder(component->m_bounds, component->m_style.borderColor,
                      component->m_style.borderWidth);
          }

          // Scale padding and item height
          int scaledPadding = static_cast<int>(component->m_style.padding * m_globalScale);
          int scaledItemHeight = calculateListItemHeight(component);

          // Draw list items
          int itemY = component->m_bounds.y + scaledPadding;
          int itemHeight = scaledItemHeight;

          for (size_t i = 0; i < component->m_listItems.size(); ++i) {
            UIRect itemBounds = {
                component->m_bounds.x,
                itemY,
                component->m_bounds.width,
                itemHeight
            };

            // Highlight selected item
            if (static_cast<int>(i) == component->m_selectedIndex) {
              addFilledRect(itemBounds, component->m_style.hoverColor);
            }

            // Draw item text
            int textX = component->m_bounds.x + scaledPadding * 2;
            int textY = itemY + itemHeight / 2;
            m_scratchTextKey.clear();
            std::format_to(std::back_inserter(m_scratchTextKey), "{}#item{}",
                           component->m_id, i);
            addText(m_scratchTextKey,
                    component->m_listItems[i], component->m_style.fontID,
                    textX, textY, component->m_style.textColor, 1);  // Left aligned

            itemY += itemHeight;

            // Don't overflow bounds
            if (itemY + itemHeight > component->m_bounds.y + component->m_bounds.height) {
              break;
            }
          }
        }
        break;

      case UIComponentType::EVENT_LOG:
        {
          // Draw background
          addFilledRect(component->m_bounds, component->m_style.backgroundColor);

          // Draw border
          if (component->m_style.borderWidth > 0) {
            addBorder(component->m_bounds, component->m_style.borderColor,
                      component->m_style.borderWidth);
          }

          // Scale padding and item height
          int scaledPadding = static_cast<int>(component->m_style.padding * m_globalScale);
          int scaledItemHeight = static_cast<int>(component->m_style.listItemHeight * m_globalScale);

          // Event logs scroll from bottom to top (newest entries at bottom)
          int itemHeight = scaledItemHeight;
          int availableHeight = component->m_bounds.height - (2 * scaledPadding);
          int maxVisibleItems = availableHeight / itemHeight;

          // Calculate start index for bottom-aligned rendering
          int startIndex = 0;
          if (static_cast<int>(component->m_listItems.size()) > maxVisibleItems) {
            startIndex = static_cast<int>(component->m_listItems.size()) - maxVisibleItems;
          }

          // Draw visible items from startIndex
          int itemY = component->m_bounds.y + scaledPadding;
          for (size_t i = static_cast<size_t>(startIndex); i < component->m_listItems.size(); ++i) {
            int textX = component->m_bounds.x + scaledPadding * 2;
            int textY = itemY + itemHeight / 2;
            m_scratchTextKey.clear();
            std::format_to(std::back_inserter(m_scratchTextKey), "{}#log{}",
                           component->m_id, i);
            addText(m_scratchTextKey,
                    component->m_listItems[i], component->m_style.fontID,
                    textX, textY, component->m_style.textColor, 1);  // Left aligned

            itemY += itemHeight;

            // Don't overflow bounds
            if (itemY + itemHeight > component->m_bounds.y + component->m_bounds.height) {
              break;
            }
          }
        }
        break;

      case UIComponentType::IMAGE:
        addImage(component);
        break;

      case UIComponentType::TOOLTIP:
        // Tooltips are rendered separately (after all components)
        break;

      default:
        break;
    }
  }

  // Render tooltip if visible (m_hoveredTooltip stores the component ID)
  if (m_tooltipsEnabled && !m_hoveredTooltip.empty() && m_tooltipTimer >= m_tooltipDelay) {
    auto tooltipComponent = getComponent(m_hoveredTooltip);
    if (tooltipComponent && !tooltipComponent->m_text.empty() &&
        tooltipComponent->m_type != UIComponentType::TITLE &&
        tooltipComponent->m_text.find('\n') == std::string::npos) {

      // Scale padding for resolution-aware sizing
      int scaledPaddingWidth = static_cast<int>(UIConstants::TOOLTIP_PADDING_WIDTH * m_globalScale);
      int scaledPaddingHeight = static_cast<int>(UIConstants::TOOLTIP_PADDING_HEIGHT * m_globalScale);
      int scaledMouseOffset = static_cast<int>(UIConstants::TOOLTIP_MOUSE_OFFSET * m_globalScale);

      // Get tooltip text dimensions for background sizing
      int tooltipTextWidth = 0;
      int tooltipTextHeight = 0;
      std::string tooltipKey = std::format("{}#tooltip", tooltipComponent->m_id);
      if (fontMgr.prepareGPUText(tooltipKey, tooltipComponent->m_text,
                                 tooltipComponent->m_style.fontID,
                                 &tooltipTextWidth, &tooltipTextHeight)) {
        int tooltipWidth = tooltipTextWidth + scaledPaddingWidth;
        int tooltipHeight = tooltipTextHeight + scaledPaddingHeight;

        // Position tooltip near mouse using m_lastMousePosition (updated in handleInput)
        int tooltipX = static_cast<int>(m_lastMousePosition.getX()) + scaledMouseOffset;
        int tooltipY = static_cast<int>(m_lastMousePosition.getY()) - tooltipHeight - scaledMouseOffset;

        // Clamp tooltip to screen bounds
        if (tooltipX + tooltipWidth > m_currentWidthInPixels) {
          tooltipX = m_currentWidthInPixels - tooltipWidth;
        }
        if (tooltipY < 0) {
          tooltipY = static_cast<int>(m_lastMousePosition.getY()) + scaledMouseOffset * 2;
        }

        UIRect tooltipBounds = {tooltipX, tooltipY, tooltipWidth, tooltipHeight};

        // Draw tooltip background
        SDL_Color tooltipBg = {.r=50, .g=50, .b=50, .a=230};
        addFilledRect(tooltipBounds, tooltipBg);

        // Draw tooltip border
        SDL_Color tooltipBorder = {.r=100, .g=100, .b=100, .a=255};  // Default border color
        addBorder(tooltipBounds, tooltipBorder, 1);

        // Draw tooltip text (centered in the tooltip box)
        int textX = tooltipX + tooltipWidth / 2;
        int textY = tooltipY + tooltipHeight / 2;
        addText(tooltipKey, tooltipComponent->m_text, tooltipComponent->m_style.fontID,
                textX, textY, tooltipComponent->m_style.textColor, 0);
      }
    }
  }

  primPool.setWrittenVertexCount(primOffset);
  uiPool.setWrittenVertexCount(uiOffset);
}

void UIManager::renderGPU(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* pass) {
  gpuRenderer.renderUIBatches(pass, m_uiPrimitiveVertexCount,
                              m_imageRenderBatches, m_textRenderBatches);
}
