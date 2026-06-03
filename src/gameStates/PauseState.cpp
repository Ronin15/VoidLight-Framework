/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/PauseState.hpp"
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
  ui.createCenteredButton("pause_mainmenu_btn", firstButtonY + buttonSpacing, buttonWidth, buttonHeight, "Main Menu");

  // Set button callbacks - capture mp_stateManager for proper architecture
  ui.setOnClick("pause_resume_btn", [this]() {
      mp_stateManager->popState();
  });

  ui.setOnClick("pause_mainmenu_btn", [this]() {
      mp_stateManager->changeStateClearingStack(GameStateId::MAIN_MENU);
  });

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
    VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);
}

bool PauseState::exit() {
  // Resume all game managers via GameEngine (collision, pathfinding, AI, particles, GameTime)
  GameEngine::Instance().setGlobalPause(false);

  // Only clean up PauseState-specific UI components
  // Do NOT use prepareForStateTransition() as it would clear GamePlayState's preserved UI
  auto& ui = UIManager::Instance();
  ui.clearKeyboardSelection();
  ui.removeComponent("pause_title");
  ui.removeComponent("pause_resume_btn");
  ui.removeComponent("pause_mainmenu_btn");
  ui.removeOverlay();  // Remove the pause overlay to restore GamePlayState visibility

  return true;
}


void PauseState::handleInput() {
  const auto& inputMgr = InputManager::Instance();

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
