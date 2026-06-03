/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/OverlayDemoState.hpp"
#include "managers/UIManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/InputManager.hpp"
#include "managers/GameStateManager.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"

#include "gpu/GPURenderer.hpp"

#include <string>

// OverlayDemoState Implementation
OverlayDemoState::OverlayDemoState() {
    // Pure UIManager approach - no UIScreen
}

bool OverlayDemoState::enter() {
    GAMESTATE_INFO("Entering Overlay Demo State");

    // Reset theme to prevent contamination from other states
    auto& ui = UIManager::Instance();
    ui.resetToDefaultTheme();

    // Create persistent control components using auto-detecting methods

    // Add title using auto-positioning
    ui.createTitleAtTop("overlay_control_title", "Overlay Demo State", 30);

    // Control buttons that persist across all modes using auto-detected dimensions
    ui.createButtonAtBottom("overlay_control_back_btn", "Back", 100, 40);

    ui.createButton("overlay_control_next_mode_btn", {140, ui.getHeightInPixels() - 60, 150, 40}, "Next Mode");
    ui.setComponentPositioning("overlay_control_next_mode_btn", {UIPositionMode::BOTTOM_ALIGNED, 140, 20, 150, 40});

    ui.createLabel("overlay_control_instructions", {310, ui.getHeightInPixels() - 55, 400, 30}, "Space = Next Mode, B = Back");
    ui.setComponentPositioning("overlay_control_instructions", {UIPositionMode::BOTTOM_ALIGNED, 310, 15, 400, 30});

    // Set up button callbacks
    ui.setOnClick("overlay_control_back_btn", [this]() {
        handleBackButton();
    });

    ui.setOnClick("overlay_control_next_mode_btn", [this]() {
        handleModeSwitch();
    });



    // Start with no overlay mode
    m_currentMode = DemoMode::NO_OVERLAY;
    setupModeUI();

    return true;
}

void OverlayDemoState::update(float deltaTime) {
    // Update transition timer
    m_transitionTimer += deltaTime;

    // Process UI input (click detection, hover states, callbacks)
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.update(0.0f);  // UI updates are not time-dependent in this state
    }
}

bool OverlayDemoState::exit() {
    GAMESTATE_INFO("Exiting Overlay Demo State");

    // Clean up UI components using simplified method
    auto& ui = UIManager::Instance();
    ui.prepareForStateTransition();

    return true;
}

void OverlayDemoState::switchToNextMode() {
    // Cycle through modes
    switch (m_currentMode) {
        case DemoMode::NO_OVERLAY:
            m_currentMode = DemoMode::LIGHT_OVERLAY;
            break;
        case DemoMode::LIGHT_OVERLAY:
            m_currentMode = DemoMode::DARK_OVERLAY;
            break;
        case DemoMode::DARK_OVERLAY:
            m_currentMode = DemoMode::LIGHT_MODAL_OVERLAY;
            break;
        case DemoMode::LIGHT_MODAL_OVERLAY:
            m_currentMode = DemoMode::MODAL_OVERLAY;
            break;
        case DemoMode::MODAL_OVERLAY:
            m_currentMode = DemoMode::NO_OVERLAY;
            break;
    }

    setupModeUI();
    m_transitionTimer = 0.0f;
}

void OverlayDemoState::setupModeUI() {
    clearCurrentUI();

    switch (m_currentMode) {
        case DemoMode::NO_OVERLAY:
            setupNoOverlayMode();
            break;
        case DemoMode::LIGHT_OVERLAY:
            setupLightOverlayMode();
            break;
        case DemoMode::DARK_OVERLAY:
            setupDarkOverlayMode();
            break;
        case DemoMode::LIGHT_MODAL_OVERLAY:
            setupLightModalOverlayMode();
            break;
        case DemoMode::MODAL_OVERLAY:
            setupModalOverlayMode();
            break;
    }
}

void OverlayDemoState::clearCurrentUI() {
    auto& ui = UIManager::Instance();

    // Remove overlay
    ui.removeOverlay();

    // Only remove demo components, not control components
    ui.removeComponentsWithPrefix("overlay_demo_");
    
    // Reset to default theme to prevent modal themes from contaminating other modes
    ui.resetToDefaultTheme();
}

void OverlayDemoState::setupNoOverlayMode() {
    auto& ui = UIManager::Instance();

    // NO OVERLAY - Perfect for HUD elements
    // This shows how HUD elements look without any background interference

    // Mode indicator with standardized spacing using UIConstants
    const int modeY = UIConstants::INFO_FIRST_LINE_Y;
    const int descY = modeY + UIConstants::INFO_LABEL_HEIGHT + UIConstants::INFO_LINE_SPACING;

    ui.createLabel(MODE_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, modeY, 400, UIConstants::INFO_LABEL_HEIGHT},
                   "Mode: HUD Elements (No Overlay)");
    ui.setComponentPositioning(MODE_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, modeY,
                                            400, UIConstants::INFO_LABEL_HEIGHT});

    ui.createLabel(DESCRIPTION_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, descY,
                                        std::min(600, ui.getWidthInPixels() - 2*UIConstants::INFO_LABEL_MARGIN_X),
                                        UIConstants::INFO_LABEL_HEIGHT},
                   "Perfect for: Health bars, Score, Minimap, Chat\nGame content remains fully visible");
    ui.setComponentPositioning(DESCRIPTION_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, descY,
                                                   -2*UIConstants::INFO_LABEL_MARGIN_X, UIConstants::INFO_LABEL_HEIGHT});

    // Enable text backgrounds for readability over variable backgrounds
    ui.enableTextBackground(MODE_LABEL, true);
    ui.enableTextBackground(DESCRIPTION_LABEL, true);

    // Simulate HUD elements with proper positioning for fullscreen compatibility
    ui.createProgressBar(HEALTH_BAR, {20, 150, 200, 25}, 0.0f, 100.0f);
    ui.setValue(HEALTH_BAR, 75.0f);
    ui.setComponentPositioning(HEALTH_BAR, {UIPositionMode::TOP_ALIGNED, 20, 150, 200, 25});

    ui.createLabel(SCORE_LABEL, {20, 185, 150, 20}, "Score: 12,450");
    ui.setComponentPositioning(SCORE_LABEL, {UIPositionMode::TOP_ALIGNED, 20, 185, 150, 20});

    // Minimap simulation with right-alignment for fullscreen compatibility
    ui.createPanel(MINIMAP_PANEL, {ui.getWidthInPixels() - 160, 20, 140, 140});
    ui.setComponentPositioning(MINIMAP_PANEL, {UIPositionMode::RIGHT_ALIGNED, 20, 20, 140, 140});

    // HUD elements and minimap use theme styling - no custom colors needed
}

void OverlayDemoState::setupLightOverlayMode() {
    auto& ui = UIManager::Instance();

    // LIGHT OVERLAY - Set theme BEFORE creating components
    ui.setThemeMode("light");
    ui.createOverlay(); // Auto-detecting overlay creation

    // Mode indicator with standardized spacing using UIConstants
    const int modeY = UIConstants::INFO_FIRST_LINE_Y;
    const int descY = modeY + UIConstants::INFO_LABEL_HEIGHT + UIConstants::INFO_LINE_SPACING;

    ui.createLabel(MODE_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, modeY, 400, UIConstants::INFO_LABEL_HEIGHT},
                   "Mode: Main Menu (Light Theme)");
    ui.setComponentPositioning(MODE_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, modeY,
                                            400, UIConstants::INFO_LABEL_HEIGHT});

    ui.createLabel(DESCRIPTION_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, descY,
                                        std::min(600, ui.getWidthInPixels() - 2*UIConstants::INFO_LABEL_MARGIN_X),
                                        UIConstants::INFO_LABEL_HEIGHT},
                   "Perfect for: Main menus, Settings screens\nSubtle separation from background");
    ui.setComponentPositioning(DESCRIPTION_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, descY,
                                                   -2*UIConstants::INFO_LABEL_MARGIN_X, UIConstants::INFO_LABEL_HEIGHT});

    // Enable text backgrounds for readability over variable backgrounds
    ui.enableTextBackground(MODE_LABEL, true);
    ui.enableTextBackground(DESCRIPTION_LABEL, true);

    // Simulate menu buttons using baseline coordinates for centering
    const int buttonX = UIConstants::BASELINE_WIDTH/2 - 100; // Center in baseline space
    ui.createButton(MENU_BUTTON_1, {buttonX, 150, 200, 50}, "New Game");
    ui.createButton(MENU_BUTTON_2, {buttonX, 220, 200, 50}, "Load Game");
    ui.createButton(MENU_BUTTON_3, {buttonX, 290, 200, 50}, "Options");

    // Set positioning to CENTERED_H for proper resizing during fullscreen toggle
    ui.setComponentPositioning(MENU_BUTTON_1, {UIPositionMode::CENTERED_H, 0, 150, 200, 50});
    ui.setComponentPositioning(MENU_BUTTON_2, {UIPositionMode::CENTERED_H, 0, 220, 200, 50});
    ui.setComponentPositioning(MENU_BUTTON_3, {UIPositionMode::CENTERED_H, 0, 290, 200, 50});
}

void OverlayDemoState::setupDarkOverlayMode() {
    auto& ui = UIManager::Instance();

    // DARK OVERLAY - Set theme BEFORE creating components
    ui.setThemeMode("dark");
    ui.createOverlay(); // Auto-detecting overlay creation

    // Mode indicator with standardized spacing using UIConstants
    const int modeY = UIConstants::INFO_FIRST_LINE_Y;
    const int descY = modeY + UIConstants::INFO_LABEL_HEIGHT + UIConstants::INFO_LINE_SPACING;

    ui.createLabel(MODE_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, modeY, 400, UIConstants::INFO_LABEL_HEIGHT},
                   "Mode: Pause Menu (Dark Theme)");
    ui.setComponentPositioning(MODE_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, modeY,
                                            400, UIConstants::INFO_LABEL_HEIGHT});

    ui.createLabel(DESCRIPTION_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, descY,
                                        std::min(600, ui.getWidthInPixels() - 2*UIConstants::INFO_LABEL_MARGIN_X),
                                        UIConstants::INFO_LABEL_HEIGHT},
                   "Perfect for: Pause menus, In-game menus\nDarker theme for focus during gameplay");
    ui.setComponentPositioning(DESCRIPTION_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, descY,
                                                   -2*UIConstants::INFO_LABEL_MARGIN_X, UIConstants::INFO_LABEL_HEIGHT});

    // Simulate pause menu using baseline coordinates for centering
    const int buttonX = UIConstants::BASELINE_WIDTH/2 - 100; // Center in baseline space
    ui.createButton(MENU_BUTTON_1, {buttonX, 150, 200, 50}, "Resume");
    ui.createButton(MENU_BUTTON_2, {buttonX, 220, 200, 50}, "Settings");
    ui.createButtonDanger(MENU_BUTTON_3, {buttonX, 290, 200, 50}, "Quit to Menu");

    // Set positioning to CENTERED_H for proper resizing during fullscreen toggle
    ui.setComponentPositioning(MENU_BUTTON_1, {UIPositionMode::CENTERED_H, 0, 150, 200, 50});
    ui.setComponentPositioning(MENU_BUTTON_2, {UIPositionMode::CENTERED_H, 0, 220, 200, 50});
    ui.setComponentPositioning(MENU_BUTTON_3, {UIPositionMode::CENTERED_H, 0, 290, 200, 50});

}

void OverlayDemoState::setupModalOverlayMode() {
    auto& ui = UIManager::Instance();

    // Mode indicator with standardized spacing using UIConstants
    const int modeY = UIConstants::INFO_FIRST_LINE_Y;
    const int descY = modeY + UIConstants::INFO_LABEL_HEIGHT + UIConstants::INFO_LINE_SPACING;

    ui.createLabel(MODE_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, modeY, 400, UIConstants::INFO_LABEL_HEIGHT},
                   "Mode: Modal Dialog (Strong Overlay)");
    ui.setComponentPositioning(MODE_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, modeY,
                                            400, UIConstants::INFO_LABEL_HEIGHT});

    ui.createLabel(DESCRIPTION_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, descY,
                                        std::min(600, ui.getWidthInPixels() - 2*UIConstants::INFO_LABEL_MARGIN_X),
                                        UIConstants::INFO_LABEL_HEIGHT},
                   "Perfect for: Confirmation dialogs, Settings panels\nStrong overlay demands attention");
    ui.setComponentPositioning(DESCRIPTION_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, descY,
                                                   -2*UIConstants::INFO_LABEL_MARGIN_X, UIConstants::INFO_LABEL_HEIGHT});

    // Calculate dialog position in baseline space (1920x1080) using UIConstants
    const int dialogWidth = UIConstants::DEFAULT_DIALOG_WIDTH;
    const int dialogHeight = UIConstants::DEFAULT_DIALOG_HEIGHT;
    const int dialogX = (UIConstants::BASELINE_WIDTH - dialogWidth) / 2;   // 760
    const int dialogY = (UIConstants::BASELINE_HEIGHT - dialogHeight) / 2; // 440

    // Create centered dialog with standardized dimensions
    ui.createCenteredDialog("overlay_demo_dialog_panel", dialogWidth, dialogHeight, "dark");

    // Position child components linked to the dialog panel — linkToParent auto-elevates
    // z-order and suppresses redundant text backgrounds.
    ui.createLabel("overlay_demo_dialog_title", {dialogX + 20, dialogY + 20, 360, 30}, "Confirm Action",
                   "overlay_demo_dialog_panel");
    ui.createLabel("overlay_demo_dialog_text", {dialogX + 20, dialogY + 60, 360, 40}, "Are you sure you want to quit?",
                   "overlay_demo_dialog_panel");

    ui.createButtonSuccess("overlay_demo_modal_yes_btn", {dialogX + 50, dialogY + 120, 100, 40}, "Yes",
                           "overlay_demo_dialog_panel");
    ui.createButtonWarning("overlay_demo_modal_cancel_btn", {dialogX + 250, dialogY + 120, 100, 40}, "Cancel",
                           "overlay_demo_dialog_panel");

    // Set CENTERED_BOTH positioning for all dialog children to move with dialog during fullscreen toggle
    // Offsets calculated from baseline center (960, 540)
    ui.setComponentPositioning("overlay_demo_dialog_title", {UIPositionMode::CENTERED_BOTH, 0, -65, 360, 30});
    ui.setComponentPositioning("overlay_demo_dialog_text", {UIPositionMode::CENTERED_BOTH, 0, -20, 360, 40});
    ui.setComponentPositioning("overlay_demo_modal_yes_btn", {UIPositionMode::CENTERED_BOTH, -100, 40, 100, 40});
    ui.setComponentPositioning("overlay_demo_modal_cancel_btn", {UIPositionMode::CENTERED_BOTH, 100, 40, 100, 40});

    // All styling handled by UIManager theme - no custom colors in state
}

void OverlayDemoState::setupLightModalOverlayMode() {
    auto& ui = UIManager::Instance();

    // Mode indicator with standardized spacing using UIConstants
    const int modeY = UIConstants::INFO_FIRST_LINE_Y;
    const int descY = modeY + UIConstants::INFO_LABEL_HEIGHT + UIConstants::INFO_LINE_SPACING;

    ui.createLabel(MODE_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, modeY, 400, UIConstants::INFO_LABEL_HEIGHT},
                   "Mode: Light Modal Dialog (Strong Overlay)");
    ui.setComponentPositioning(MODE_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, modeY,
                                            400, UIConstants::INFO_LABEL_HEIGHT});

    ui.createLabel(DESCRIPTION_LABEL, {UIConstants::INFO_LABEL_MARGIN_X, descY,
                                        std::min(600, ui.getWidthInPixels() - 2*UIConstants::INFO_LABEL_MARGIN_X),
                                        UIConstants::INFO_LABEL_HEIGHT},
                   "Perfect for: Light-themed dialogs, Settings panels\nLight strong overlay with good contrast");
    ui.setComponentPositioning(DESCRIPTION_LABEL, {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X, descY,
                                                   -2*UIConstants::INFO_LABEL_MARGIN_X, UIConstants::INFO_LABEL_HEIGHT});

    // Calculate dialog position in baseline space (1920x1080) using UIConstants
    const int dialogWidth = UIConstants::DEFAULT_DIALOG_WIDTH;
    const int dialogHeight = UIConstants::DEFAULT_DIALOG_HEIGHT;
    const int dialogX = (UIConstants::BASELINE_WIDTH - dialogWidth) / 2;   // 760
    const int dialogY = (UIConstants::BASELINE_HEIGHT - dialogHeight) / 2; // 440

    // Create centered dialog with standardized dimensions
    ui.createCenteredDialog("overlay_demo_dialog_panel", dialogWidth, dialogHeight, "light");

    // Position child components linked to the dialog panel — linkToParent auto-elevates
    // z-order and suppresses redundant text backgrounds.
    ui.createLabel("overlay_demo_dialog_title", {dialogX + 20, dialogY + 20, 360, 30}, "Confirm Action",
                   "overlay_demo_dialog_panel");
    ui.createLabel("overlay_demo_dialog_text", {dialogX + 20, dialogY + 60, 360, 40}, "Save changes before closing?",
                   "overlay_demo_dialog_panel");

    ui.createButtonSuccess("overlay_demo_modal_save_btn", {dialogX + 50, dialogY + 120, 100, 40}, "Save",
                           "overlay_demo_dialog_panel");
    ui.createButtonWarning("overlay_demo_modal_cancel_btn", {dialogX + 250, dialogY + 120, 100, 40}, "Cancel",
                           "overlay_demo_dialog_panel");

    // Set CENTERED_BOTH positioning for all dialog children to move with dialog during fullscreen toggle
    // Offsets calculated from baseline center (960, 540)
    ui.setComponentPositioning("overlay_demo_dialog_title", {UIPositionMode::CENTERED_BOTH, 0, -65, 360, 30});
    ui.setComponentPositioning("overlay_demo_dialog_text", {UIPositionMode::CENTERED_BOTH, 0, -20, 360, 40});
    ui.setComponentPositioning("overlay_demo_modal_save_btn", {UIPositionMode::CENTERED_BOTH, -100, 40, 100, 40});
    ui.setComponentPositioning("overlay_demo_modal_cancel_btn", {UIPositionMode::CENTERED_BOTH, 100, 40, 100, 40});

    // All styling handled by UIManager theme - no custom colors in state
}

std::string OverlayDemoState::getModeDescription() const {
    switch (m_currentMode) {
        case DemoMode::NO_OVERLAY:
            return "No Overlay - HUD Elements";
        case DemoMode::LIGHT_OVERLAY:
            return "Light Overlay - Main Menu";
        case DemoMode::DARK_OVERLAY:
            return "Dark Overlay - Pause Menu";
        case DemoMode::LIGHT_MODAL_OVERLAY:
            return "Light Modal Overlay - Light Dialog";
        case DemoMode::MODAL_OVERLAY:
            return "Modal Overlay - Dark Dialog";
        default:
            return "Unknown";
    }
}

void OverlayDemoState::handleModeSwitch() {
    switchToNextMode();
    GAMESTATE_DEBUG(std::format("Switched to: {}", getModeDescription()));
}

void OverlayDemoState::handleInput() {
    const auto& inputManager = InputManager::Instance();

    // Use InputManager's new event-driven key press detection
    if (inputManager.wasKeyPressed(SDL_SCANCODE_SPACE)) {
        handleModeSwitch();
    }

    if (inputManager.wasKeyPressed(SDL_SCANCODE_B)) {
        handleBackButton();
    }
}

void OverlayDemoState::handleBackButton() {
    mp_stateManager->changeState(GameStateId::MAIN_MENU);
}

void OverlayDemoState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                          float) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.recordGPUVertices(gpuRenderer);
    }
}

void OverlayDemoState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                    SDL_GPURenderPass* swapchainPass) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.renderGPU(gpuRenderer, swapchainPass);
    }
}

// Pure UIManager implementation - no UIScreen needed
