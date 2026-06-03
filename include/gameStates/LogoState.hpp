/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef LOGO_STATE_HPP
#define LOGO_STATE_HPP

#include "gameStates/GameState.hpp"
#include "gpu/UIRenderBatches.hpp"
#include <cstdint>
#include <vector>

struct SDL_GPUTexture;

class LogoState : public GameState {
 public:
  bool enter() override;
  void update(float deltaTime) override;
  void handleInput() override;
  bool exit() override;
  GameStateId getStateId() const override { return GameStateId::LOGO; }

  // GPU rendering support
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                          float interpolationAlpha) override;
  void renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                       SDL_GPURenderPass* scenePass,
                       float interpolationAlpha) override;
  void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                    SDL_GPURenderPass* swapchainPass) override;
  bool supportsGPURendering() const override { return true; }

 private:
  void recalculateLayout();

  float m_stateTimer{0.0f};

  // Cached layout calculations (computed once in enter())
  int m_windowWidth{0};
  int m_windowHeight{0};
  int m_bannerSize{0};
  int m_engineSize{0};
  int m_sdlSize{0};
  int m_cppSize{0};

  // Cached positions
  int m_bannerX{0}, m_bannerY{0};
  int m_engineX{0}, m_engineY{0};
  int m_cppX{0}, m_cppY{0};
  int m_sdlX{0}, m_sdlY{0};
  int m_titleY{0};
  int m_subtitleY{0};
  int m_versionY{0};

  // Scene logo sprite batches.
  struct GPUDrawCommand {
    SDL_GPUTexture* texture{nullptr};
    uint32_t vertexOffset{0};
    uint32_t vertexCount{0};
  };
  std::vector<GPUDrawCommand> m_drawCommands;

  // UI text batches are submitted by GPURenderer during the swapchain pass.
  std::vector<VoidLight::UITextDrawBatch> m_textDrawBatches;
};

#endif  // LOGO_STATE_HPP
