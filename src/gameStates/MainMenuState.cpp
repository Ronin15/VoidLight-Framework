/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/MainMenuState.hpp"
#include "managers/UIManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/FontManager.hpp"
#include "managers/GameStateManager.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "utils/MenuNavigation.hpp"

#include "gpu/GPURenderer.hpp"

#include <thread>
#include <chrono>

bool MainMenuState::enter() {
  GAMESTATE_INFO("Entering MAIN MENU State");

  VoidLight::MenuNavigation::reset();

  // Pause all game managers to reduce power draw while at menu
  GameEngine::Instance().setGlobalPause(true);

  auto& ui = UIManager::Instance();
  auto& fontMgr = FontManager::Instance();

  // Wait briefly for fonts to be loaded before creating UI components.
  // Avoid unbounded waits on macOS where early resize/display events can trigger
  // a reload loop. Proceed after a short timeout and let UI relayout when fonts finish.
  constexpr int kMaxWaitMs = 1500; // 1.5s max wait
  int waitedMs = 0;
  while (!fontMgr.areFontsLoaded() && waitedMs < kMaxWaitMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    waitedMs += 1;
  }

  // Create title using auto-positioning
  ui.createTitleAtTop("mainmenu_title", "VoidLight Engine - Main Menu", 60);

  // Create menu buttons with centered positioning
  int buttonWidth = 300;
  int buttonHeight = 50;
  int buttonSpacing = 20;

  // Calculate relative offsets from center for 8 buttons
  // Total height: 8 buttons * 50px + 7 gaps * 20px = 540px
  // Center the group by starting at -270 from center
  int buttonStep = buttonHeight + buttonSpacing; // 70px between each button
  int firstButtonOffset = -270; // Top of centered button group

  // Use createCenteredButton helper for streamlined button creation
  ui.createCenteredButton("mainmenu_start_game_btn", firstButtonOffset, buttonWidth, buttonHeight, "Start Game");
  ui.createCenteredButton("mainmenu_ai_demo_btn", firstButtonOffset + buttonStep, buttonWidth, buttonHeight, "AI Demo");
  ui.createCenteredButton("mainmenu_advanced_ai_demo_btn", firstButtonOffset + 2 * buttonStep, buttonWidth, buttonHeight, "Advanced AI Demo");
  ui.createCenteredButton("mainmenu_event_demo_btn", firstButtonOffset + 3 * buttonStep, buttonWidth, buttonHeight, "Event Demo");
  ui.createCenteredButton("mainmenu_ui_example_btn", firstButtonOffset + 4 * buttonStep, buttonWidth, buttonHeight, "UI Demo");
  ui.createCenteredButton("mainmenu_overlay_demo_btn", firstButtonOffset + 5 * buttonStep, buttonWidth, buttonHeight, "Overlay Demo");
  ui.createCenteredButton("mainmenu_settings_btn", firstButtonOffset + 6 * buttonStep, buttonWidth, buttonHeight, "Settings");

  // Exit button uses danger style (manual creation + positioning)
  ui.createButtonDanger("mainmenu_exit_btn", {ui.getWidthInPixels()/2 - buttonWidth/2, ui.getHeightInPixels()/2 + firstButtonOffset + 7 * buttonStep, buttonWidth, buttonHeight}, "Exit");
  ui.setComponentPositioning("mainmenu_exit_btn", {UIPositionMode::CENTERED_BOTH, 0, firstButtonOffset + 7 * buttonStep, buttonWidth, buttonHeight});

  // Set up button callbacks - capture mp_stateManager for proper architecture
  ui.setOnClick("mainmenu_start_game_btn", [this]() {
    mp_stateManager->changeState(GameStateId::GAME_PLAY);
  });

  ui.setOnClick("mainmenu_ai_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::AI_DEMO);
  });

  ui.setOnClick("mainmenu_advanced_ai_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::ADVANCED_AI_DEMO);
  });

  ui.setOnClick("mainmenu_event_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::EVENT_DEMO);
  });

  ui.setOnClick("mainmenu_ui_example_btn", [this]() {
    mp_stateManager->changeState(GameStateId::UI_DEMO);
  });

  ui.setOnClick("mainmenu_overlay_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::OVERLAY_DEMO);
  });

  ui.setOnClick("mainmenu_settings_btn", [this]() {
    mp_stateManager->changeState(GameStateId::SETTINGS_MENU);
  });

  ui.setOnClick("mainmenu_exit_btn", [this]() {
    openQuitDialog();
  });

  // --- Quit-confirm dialog (hidden by default) ---
  const int dialogWidth  = UIConstants::DEFAULT_DIALOG_WIDTH;
  const int dialogHeight = UIConstants::DEFAULT_DIALOG_HEIGHT;
  const int dialogX = (UIConstants::BASELINE_WIDTH  - dialogWidth)  / 2;
  const int dialogY = (UIConstants::BASELINE_HEIGHT - dialogHeight) / 2;

  ui.createCenteredDialog("mainmenu_quit_dialog_panel", dialogWidth, dialogHeight, "dark");

  ui.createLabel("mainmenu_quit_dialog_title",
                 {dialogX + 20, dialogY + 20, 360, 30},
                 "Quit Game?",
                 "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_title",
                             {UIPositionMode::CENTERED_BOTH, 0, -65, 360, 30});

  ui.createLabel("mainmenu_quit_dialog_text",
                 {dialogX + 20, dialogY + 60, 360, 40},
                 "Exit to desktop?",
                 "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_text",
                             {UIPositionMode::CENTERED_BOTH, 0, -20, 360, 40});

  ui.createButtonSuccess("mainmenu_quit_dialog_yes_btn",
                         {dialogX + 50, dialogY + 120, 100, 40},
                         "Quit",
                         "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_yes_btn",
                             {UIPositionMode::CENTERED_BOTH, 100, 40, 100, 40});
  ui.setOnClick("mainmenu_quit_dialog_yes_btn", []() {
    GameEngine::Instance().setRunning(false);
  });

  ui.createButtonWarning("mainmenu_quit_dialog_cancel_btn",
                         {dialogX + 250, dialogY + 120, 100, 40},
                         "Cancel",
                         "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_cancel_btn",
                             {UIPositionMode::CENTERED_BOTH, -100, 40, 100, 40});
  ui.setOnClick("mainmenu_quit_dialog_cancel_btn", [this]() {
    closeQuitDialog();
  });

  // Hide the modal layer (overlay + dialog panel + its children cascade via linkToParent) until triggered.
  ui.setComponentVisible("__overlay",                  false);
  ui.setComponentVisible("mainmenu_quit_dialog_panel", false);

  m_selectedIndex = 0;
  VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);

  return true;
}

void MainMenuState::update(float) {
  // Process UI input (click detection, hover states, callbacks)
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.update(0.0f);
  }
  if (m_quitDialogOpen) {
    VoidLight::MenuNavigation::applySelection(kQuitDialogNavOrder, m_selectedIndex);
  } else {
    VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);
  }
}

bool MainMenuState::exit() {
  GAMESTATE_INFO("Exiting MAIN MENU State");

  // NOTE: Do NOT unpause here - gameplay states will unpause on enter
  // This keeps systems paused during menu-to-menu transitions

  // Clean up UI components using simplified method
  auto& ui = UIManager::Instance();
  ui.prepareForStateTransition();

  return true;
}

void MainMenuState::handleInput() {
  const auto& inputManager = InputManager::Instance();

  if (m_quitDialogOpen) {
    VoidLight::MenuNavigation::readInputs(kQuitDialogNavOrder, m_selectedIndex);
    if (VoidLight::MenuNavigation::cancelPressed()) {
      closeQuitDialog();
    }
    return;
  }

  VoidLight::MenuNavigation::readInputs(kNavOrder, m_selectedIndex);
  if (VoidLight::MenuNavigation::cancelPressed()) {
    openQuitDialog();
  }

  // Developer debug shortcuts for demo states — Debug builds only.
  VOIDLIGHT_DEBUG_ONLY(
    if (inputManager.wasKeyPressed(SDL_SCANCODE_A)) {
        mp_stateManager->changeState(GameStateId::AI_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_E)) {
        mp_stateManager->changeState(GameStateId::EVENT_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_U)) {
        mp_stateManager->changeState(GameStateId::UI_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_O)) {
        mp_stateManager->changeState(GameStateId::OVERLAY_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_S)) {
        mp_stateManager->changeState(GameStateId::SETTINGS_MENU);
    }
  )
}

void MainMenuState::openQuitDialog()
{
  m_quitDialogOpen = true;
  auto& ui = UIManager::Instance();

  // The modal overlay (z=500) dims menu buttons and title beneath it; the dialog
  // panel (z=600) and its linked children render above everything. No hide hacks needed.
  ui.setComponentVisible("__overlay",                  true);
  ui.setComponentVisible("mainmenu_quit_dialog_panel", true);

  // Default focus: Cancel (index 0 in kQuitDialogNavOrder)
  m_selectedIndex = 0;
  VoidLight::MenuNavigation::reset();
}

void MainMenuState::closeQuitDialog()
{
  m_quitDialogOpen = false;
  auto& ui = UIManager::Instance();

  ui.setComponentVisible("__overlay",                  false);
  ui.setComponentVisible("mainmenu_quit_dialog_panel", false);

  // Return focus to the Exit button (last item in main menu nav order)
  m_selectedIndex = kNavOrder.size() - 1;
  VoidLight::MenuNavigation::reset();
}

void MainMenuState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                       float) {
  // MainMenuState uses UIManager for all rendering
  // UIManager records its vertices (buttons, panels, text)
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.recordGPUVertices(gpuRenderer);
  }
}

void MainMenuState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                 SDL_GPURenderPass* swapchainPass) {
  // Render UIManager components to swapchain
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.renderGPU(gpuRenderer, swapchainPass);
  }
}
