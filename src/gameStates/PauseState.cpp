/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/PauseState.hpp"
#include "gameStates/SettingsMenuState.hpp"
#include "managers/InputManager.hpp"
#include "managers/UIManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/GameStateManager.hpp"
#include "core/GameEngine.hpp"
#include "utils/MenuNavigation.hpp"

#include "gpu/GPURenderer.hpp"

bool PauseState::enter() {
  // Cache manager references at function start
  auto& gameEngine = GameEngine::Instance();
  auto& ui = UIManager::Instance();

  VoidLight::MenuNavigation::reset();

  // Pause all game managers via GameEngine (collision, pathfinding, AI, particles, GameTime)
  gameEngine.setGlobalPause(true);

  // Create pause state UI
  int windowWidth = gameEngine.getWidthInPixels();
  int windowHeight = gameEngine.getHeightInPixels();

  // Create overlay background to dim the game behind the pause menu
  ui.createOverlay(windowWidth, windowHeight);
  // Overlay auto-repositions via createOverlay's positioning rules

  ui.createTitle("pause_title", {0, UIConstants::TITLE_TOP_OFFSET * 10, windowWidth, UIConstants::DEFAULT_TITLE_HEIGHT},
                 "Game Paused");
  ui.setTitleAlignment("pause_title", UIAlignment::CENTER_CENTER);
  // Set auto-repositioning: centered horizontally, fixed Y position
  ui.setComponentPositioning("pause_title", {UIPositionMode::CENTERED_H, 0, UIConstants::TITLE_TOP_OFFSET * 10,
                                             -1, UIConstants::DEFAULT_TITLE_HEIGHT});

  // Create centered buttons for pause menu
  int buttonWidth = 200;
  int buttonHeight = 40;
  int buttonSpacing = 60;
  int firstButtonY = 50;  // Offset from center

  ui.createCenteredButton("pause_resume_btn", firstButtonY, buttonWidth, buttonHeight, "Resume Game");
  ui.createCenteredButton("pause_settings_btn", firstButtonY + buttonSpacing, buttonWidth, buttonHeight, "Settings");
  ui.createCenteredButton("pause_mainmenu_btn", firstButtonY + 2 * buttonSpacing, buttonWidth, buttonHeight, "Main Menu");

  // Set button callbacks - capture mp_stateManager for proper architecture
  ui.setOnClick("pause_resume_btn", [this]() {
      mp_stateManager->popState();
  });

  ui.setOnClick("pause_settings_btn", [this]() {
      if (auto* settingsState = dynamic_cast<SettingsMenuState*>(
              mp_stateManager->getState(GameStateId::SETTINGS_MENU).get())) {
        settingsState->setReturnState(GameStateId::PAUSE);
      }
      mp_stateManager->changeState(GameStateId::SETTINGS_MENU);
  });

  ui.setOnClick("pause_mainmenu_btn", [this]() {
      openMainMenuConfirm();
  });

  // --- Return-to-main-menu confirm dialog (hidden by default) ---
  // Going to the main menu abandons the current run (matches
  // changeStateClearingStack's full teardown), so this warns before
  // discarding progress -- mirrors MainMenuState's quit-confirm dialog.
  const int dialogWidth  = UIConstants::DEFAULT_DIALOG_WIDTH;
  const int dialogHeight = UIConstants::DEFAULT_DIALOG_HEIGHT;
  const int dialogX = (UIConstants::BASELINE_WIDTH  - dialogWidth)  / 2;
  const int dialogY = (UIConstants::BASELINE_HEIGHT - dialogHeight) / 2;

  ui.createPanel("pause_confirm_panel", {dialogX, dialogY, dialogWidth, dialogHeight});
  ui.setComponentPositioning("pause_confirm_panel",
                             {UIPositionMode::CENTERED_BOTH, 0, 0, dialogWidth, dialogHeight});

  ui.createLabel("pause_confirm_title",
                 {dialogX + 20, dialogY + 20, 360, 30},
                 "Return to Main Menu?",
                 "pause_confirm_panel");
  ui.setComponentPositioning("pause_confirm_title",
                             {UIPositionMode::CENTERED_BOTH, 0, -65, 360, 30});

  ui.createLabel("pause_confirm_text",
                 {dialogX + 20, dialogY + 60, 360, 40},
                 "Unsaved progress will be lost.",
                 "pause_confirm_panel");
  ui.setComponentPositioning("pause_confirm_text",
                             {UIPositionMode::CENTERED_BOTH, 0, -20, 360, 40});

  ui.createButtonDanger("pause_confirm_yes_btn",
                        {dialogX + 25, dialogY + 120, 150, 40},
                        "Main Menu",
                        "pause_confirm_panel");
  ui.setComponentPositioning("pause_confirm_yes_btn",
                             {UIPositionMode::CENTERED_BOTH, 100, 40, 150, 40});
  ui.setOnClick("pause_confirm_yes_btn", [this]() {
      mp_stateManager->changeStateClearingStack(GameStateId::MAIN_MENU);
  });

  ui.createButtonWarning("pause_confirm_cancel_btn",
                         {dialogX + 225, dialogY + 120, 150, 40},
                         "Cancel",
                         "pause_confirm_panel");
  ui.setComponentPositioning("pause_confirm_cancel_btn",
                             {UIPositionMode::CENTERED_BOTH, -100, 40, 150, 40});
  ui.setOnClick("pause_confirm_cancel_btn", [this]() {
      closeMainMenuConfirm();
  });

  // Hidden until "Main Menu" is clicked — visibility cascades to all
  // linked children (title/text/buttons) via their parentId.
  ui.setComponentVisible("pause_confirm_panel", false);

  m_confirmDialogOpen = false;
  m_selectedIndex = 0;
  VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);

  return true;
}

void PauseState::update(float) {
    // Process UI input (click detection, hover states, callbacks)
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.update(0.0f);
    }
    if (m_confirmDialogOpen) {
        VoidLight::MenuNavigation::applySelection(kConfirmNavOrder, m_selectedIndex);
    } else {
        VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);
    }
}

bool PauseState::exit() {
  // Resume all game managers via GameEngine (collision, pathfinding, AI, particles, GameTime)
  GameEngine::Instance().setGlobalPause(false);

  // Overlay / stacked state: remove only our widgets. Never full-wipe UI —
  // GamePlay HUD must remain when popping back or when replacing this top
  // with Settings. Full-screen leave (changeStateClearingStack to MainMenu)
  // clears UI in GameStateManager after the whole stack has exited.
  auto& ui = UIManager::Instance();
  ui.clearKeyboardSelection();
  ui.removeComponent("pause_title");
  ui.removeComponent("pause_resume_btn");
  ui.removeComponent("pause_settings_btn");
  ui.removeComponent("pause_mainmenu_btn");
  ui.removeComponent("pause_confirm_panel");  // cascades to its linked children
  // Overlay is shared; resume() removes it when returning to GamePlay. Full
  // stack teardown clears it via GameStateManager's full-screen UI clear.

  return true;
}


void PauseState::handleInput() {
  const auto& inputMgr = InputManager::Instance();

  // Confirm dialog is modal — it consumes all input so Pause/R cannot
  // resume or fall through to the pause menu behind it.
  if (m_confirmDialogOpen) {
    VoidLight::MenuNavigation::readInputs(kConfirmNavOrder, m_selectedIndex);
    if (VoidLight::MenuNavigation::cancelPressed()) {
      closeMainMenuConfirm();
    }
    return;
  }

  VoidLight::MenuNavigation::readInputs(kNavOrder, m_selectedIndex);
  // MenuCancel or Pause both resume gameplay — symmetric with GamePlayState
  // which uses Command::Pause to enter PauseState. Both use isCommandPressed
  // (rising-edge) to avoid re-pausing on the same frame.
  if (VoidLight::MenuNavigation::cancelPressed() ||
      inputMgr.isCommandPressed(InputManager::Command::Pause)) {
      mp_stateManager->popState();
  }

  // Developer debug shortcut — R also resumes. Intentionally not rebindable.
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_R)) {
      mp_stateManager->popState();
  }
}

void PauseState::openMainMenuConfirm() {
  m_confirmDialogOpen = true;
  auto& ui = UIManager::Instance();

  ui.setComponentVisible("pause_resume_btn", false);
  ui.setComponentVisible("pause_settings_btn", false);
  ui.setComponentVisible("pause_mainmenu_btn", false);
  ui.setComponentVisible("pause_confirm_panel", true);

  // Default focus: Cancel (index 0 in kConfirmNavOrder)
  m_selectedIndex = 0;
  VoidLight::MenuNavigation::reset();
}

void PauseState::closeMainMenuConfirm() {
  m_confirmDialogOpen = false;
  auto& ui = UIManager::Instance();

  ui.setComponentVisible("pause_confirm_panel", false);
  ui.setComponentVisible("pause_resume_btn", true);
  ui.setComponentVisible("pause_settings_btn", true);
  ui.setComponentVisible("pause_mainmenu_btn", true);

  // Return focus to the Main Menu button (last item in kNavOrder)
  m_selectedIndex = kNavOrder.size() - 1;
  VoidLight::MenuNavigation::reset();
}

void PauseState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                    float) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.recordGPUVertices(gpuRenderer);
    }
}

void PauseState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                              SDL_GPURenderPass* swapchainPass) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.renderGPU(gpuRenderer, swapchainPass);
    }
}
