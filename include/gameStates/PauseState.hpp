/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef PAUSE_STATE_HPP
#define PAUSE_STATE_HPP

#include "gameStates/GameState.hpp"

#include <array>
#include <cstddef>
#include <string_view>

class PauseState : public GameState {
 public:
  bool enter() override;
  void update(float deltaTime) override;
  void handleInput() override;
  bool exit() override;
  GameStateId getStateId() const override { return GameStateId::PAUSE; }

  // GPU rendering support
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                         float interpolationAlpha) override;
  void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                   SDL_GPURenderPass* swapchainPass) override;
  bool supportsGPURendering() const override { return true; }

 private:
  static constexpr std::array<std::string_view, 2> kNavOrder{
      "pause_resume_btn",
      "pause_mainmenu_btn",
  };
  size_t m_selectedIndex{0};
};

#endif  // PAUSE_STATE_HPP
