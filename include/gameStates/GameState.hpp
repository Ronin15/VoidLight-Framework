/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <SDL3/SDL_gpu.h>
#include <cstdint>

class GameStateManager;
namespace VoidLight {
class GPURenderer;
}

enum class GameStateId : uint8_t
{
    LOGO,
    LOADING,
    MAIN_MENU,
    SETTINGS_MENU,
    GAME_PLAY,
    GAME_OVER,
    PAUSE,
    AI_DEMO,
    EVENT_DEMO,
    COUNT
};

// pure virtual for inheritance
class GameState {
 public:
  virtual bool enter() = 0;
  virtual void update(float deltaTime) = 0;
  virtual void handleInput() = 0;
  virtual bool exit() = 0;
  virtual void pause() {}
  virtual void resume() {}
  virtual GameStateId getStateId() const = 0;
  virtual ~GameState() = default;

  /**
   * True when this state owns a scene pass (world, diorama, logo sprites).
   * GameStateManager records/renders scene from the highest stacked state
   * that returns true, and UI from the top state.
   */
  virtual bool hasGPUScene() const { return false; }

  /**
   * Record scene vertices (world/diorama/particles) before the scene pass.
   */
  virtual void recordGPUSceneVertices([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                                      [[maybe_unused]] float interpolationAlpha) {}

  /**
   * Record UI vertices before the swapchain UI pass.
   */
  virtual void recordGPUUIVertices([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer) {}

  /**
   * Issue GPU draw calls during scene pass.
   */
  virtual void renderGPUScene([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                               [[maybe_unused]] SDL_GPURenderPass* scenePass,
                               [[maybe_unused]] float interpolationAlpha) {}

  /**
   * Render UI/overlays during swapchain pass.
   * UI renders at exact screen positions - no interpolation needed.
   */
  virtual void renderGPUUI([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                            [[maybe_unused]] SDL_GPURenderPass* swapchainPass) {}

  // State manager access - set by GameStateManager when state is registered
  void setStateManager(GameStateManager* manager) { mp_stateManager = manager; }

 protected:
  GameStateManager* mp_stateManager = nullptr;
};
#endif  // GAME_STATE_HPP
