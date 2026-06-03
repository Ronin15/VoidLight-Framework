/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/GameOverState.hpp"
#include "core/GameEngine.hpp"
#include "managers/GameStateManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/UIManager.hpp"

#include "gpu/GPURenderer.hpp"

bool GameOverState::enter() {
  auto& gameEngine = GameEngine::Instance();
  auto& ui = UIManager::Instance();

  gameEngine.setGlobalPause(true);

  const int windowWidth = gameEngine.getWidthInPixels();
  const int windowHeight = gameEngine.getHeightInPixels();

  ui.createOverlay(windowWidth, windowHeight);

  ui.createTitle("gameover_title",
                 {0, UIConstants::TITLE_TOP_OFFSET * 8, windowWidth,
                  UIConstants::DEFAULT_TITLE_HEIGHT},
                 "Game Over");
  ui.setTitleAlignment("gameover_title", UIAlignment::CENTER_CENTER);
  ui.setComponentPositioning(
      "gameover_title",
      {UIPositionMode::CENTERED_H, 0, UIConstants::TITLE_TOP_OFFSET * 8, -1,
       UIConstants::DEFAULT_TITLE_HEIGHT});

  ui.createLabel("gameover_message",
                 {windowWidth / 2 - 250, windowHeight / 2 - 70, 500, 40},
                 "The player has fallen.");
  ui.setLabelAlignment("gameover_message", UIAlignment::CENTER_CENTER);
  ui.setComponentPositioning("gameover_message",
                             {UIPositionMode::CENTERED_BOTH, 0, -70, 500, 40});

  constexpr int buttonWidth = 220;
  constexpr int buttonHeight = 45;
  constexpr int buttonSpacing = 60;

  ui.createCenteredButton("gameover_retry_btn", 10, buttonWidth, buttonHeight,
                          "Retry");
  ui.createCenteredButton("gameover_mainmenu_btn", 10 + buttonSpacing,
                          buttonWidth, buttonHeight, "Main Menu");

  ui.setOnClick("gameover_retry_btn", [this]() {
    mp_stateManager->changeState(GameStateId::GAME_PLAY);
  });

  ui.setOnClick("gameover_mainmenu_btn", [this]() {
    mp_stateManager->changeState(GameStateId::MAIN_MENU);
  });

  return true;
}

void GameOverState::update(float) {
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.update(0.0f);
  }
}

void GameOverState::handleInput() {
  const auto& inputMgr = InputManager::Instance();

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_R)) {
    mp_stateManager->changeState(GameStateId::GAME_PLAY);
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_M)) {
    mp_stateManager->changeState(GameStateId::MAIN_MENU);
  }
}

bool GameOverState::exit() {
  auto& ui = UIManager::Instance();
  ui.prepareForStateTransition();
  return true;
}


void GameOverState::recordGPUVertices(
    VoidLight::GPURenderer& gpuRenderer,
    float) {
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.recordGPUVertices(gpuRenderer);
  }
}

void GameOverState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                SDL_GPURenderPass* swapchainPass) {
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.renderGPU(gpuRenderer, swapchainPass);
  }
}
