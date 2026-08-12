/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef GAME_OVER_STATE_HPP
#define GAME_OVER_STATE_HPP

#include "gameStates/GameState.hpp"

class GameOverState : public GameState {
 public:
  bool enter() override;
  void update(float deltaTime) override;
  void handleInput() override;
  bool exit() override;
  GameStateId getStateId() const override { return GameStateId::GAME_OVER; }

  // Sets which state Retry returns to (e.g. GAME_PLAY or ADVANCED_AI_DEMO).
  // Callers must set this before transitioning into GameOverState — it is not
  // reset automatically, since enter() runs before any of this state's own
  // code could otherwise re-derive the caller's identity. Main Menu always
  // goes to MAIN_MENU regardless of this value.
  void setReturnState(GameStateId state) { m_returnState = state; }

  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                         float interpolationAlpha) override;
  void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                   SDL_GPURenderPass* swapchainPass) override;
  bool supportsGPURendering() const override { return true; }

 private:
  // Retry destination — set via setReturnState() by the state that routed
  // here (GamePlayState, AdvancedAIDemoState, ...). Defaults to GAME_PLAY.
  GameStateId m_returnState = GameStateId::GAME_PLAY;
};

#endif  // GAME_OVER_STATE_HPP
