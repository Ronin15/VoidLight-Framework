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
   * Record vertices for GPU rendering (called before scene pass).
   * Override in states that support GPU rendering.
   * @param gpuRenderer Reference to GPURenderer for accessing vertex pools/batches
   * @param interpolationAlpha Interpolation factor for smooth rendering
   */
  virtual void recordGPUVertices([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                                  [[maybe_unused]] float interpolationAlpha) {}

  /**
   * Issue GPU draw calls during scene pass.
   * Override in states that support GPU rendering.
   * @param gpuRenderer Reference to GPURenderer
   * @param scenePass Active scene render pass
   * @param interpolationAlpha Interpolation factor
   */
  virtual void renderGPUScene([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                               [[maybe_unused]] SDL_GPURenderPass* scenePass,
                               [[maybe_unused]] float interpolationAlpha) {}

  /**
   * Render UI/overlays during swapchain pass.
   * Override in states that need to render UI with GPU.
   * UI renders at exact screen positions - no interpolation needed.
   * @param gpuRenderer Reference to GPURenderer
   * @param swapchainPass Active swapchain render pass
   */
  virtual void renderGPUUI([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                            [[maybe_unused]] SDL_GPURenderPass* swapchainPass) {}

  /**
   * Check if this state supports GPU rendering.
   * @return true if GPU render methods are implemented
   */
  virtual bool supportsGPURendering() const { return false; }

  // State manager access - set by GameStateManager when state is registered
  void setStateManager(GameStateManager* manager) { mp_stateManager = manager; }

 protected:
  GameStateManager* mp_stateManager = nullptr;
};
#endif  // GAME_STATE_HPP
