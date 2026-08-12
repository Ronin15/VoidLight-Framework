/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef GAME_STATE_MANAGER_HPP
#define GAME_STATE_MANAGER_HPP

#include <memory>
#include <unordered_map>
#include <vector>
#include <SDL3/SDL_gpu.h>
#include "gameStates/GameState.hpp"

namespace VoidLight {
class GPURenderer;
}

/**
 * Owns the active GameState stack and transition policy.
 *
 * Transition contract (standard exit-then-enter):
 * - changeState / changeStateClearingStack: exit leaving state(s) first, then
 *   enter the destination. If enter() fails, the previous state is restored
 *   when possible so the stack is never left empty without a recovery attempt.
 * - Full-screen replace (stack empty after exits): GameStateManager clears
 *   global UI via UIManager::prepareForStateTransition() before enter().
 *   Destination enter() only builds UI; it may defensively wipe-first.
 * - pushState / popState: overlay transitions — no full UI clear. Overlay
 *   states add/remove their own widgets; the underlying state keeps its UI.
 */
class GameStateManager {

 public:
  GameStateManager();
  void addState(std::unique_ptr<GameState> state);
  void pushState(GameStateId stateId);
  void popState();
  void changeState(GameStateId stateId);
  void changeStateClearingStack(GameStateId stateId);

  void update(float deltaTime);
  void handleInput();

  /**
   * Record vertices for GPU rendering (called before scene pass).
   * Delegates to active state's recordGPUVertices().
   */
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer, float interpolationAlpha);

  /**
   * Issue GPU draw calls during scene pass.
   * Delegates to active state's renderGPUScene().
   */
  void renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                       SDL_GPURenderPass* scenePass,
                       float interpolationAlpha);

  /**
   * Render UI/overlays during swapchain pass.
   * Delegates to active state's renderGPUUI().
   */
  void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                    SDL_GPURenderPass* swapchainPass);

  bool hasState(GameStateId stateId) const;
  std::shared_ptr<GameState> getState(GameStateId stateId) const;
  void removeState(GameStateId stateId);
  void clearAllStates();

  // Frame data pushed from GameEngine - avoids states calling GameEngine::Instance()
  void setCurrentFPS(float fps) { m_currentFPS = fps; }
  float getCurrentFPS() const { return m_currentFPS; }

 private:
  // Full-screen replace only (no underlying state left on the stack).
  void clearUIForFullScreenReplace();

  // All registered states, available for activation
  std::unordered_map<GameStateId, std::shared_ptr<GameState>> m_registeredStates;
  // The stack of active states
  std::vector<std::shared_ptr<GameState>> m_activeStates;

  float m_lastDeltaTime{0.0f}; // Store deltaTime from update to pass to render
  float m_currentFPS{0.0f};    // Current FPS pushed from GameEngine
};

#endif  // GAME_STATE_MANAGER_HPP
