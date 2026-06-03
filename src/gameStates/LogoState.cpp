/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "gameStates/LogoState.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "managers/SoundManager.hpp"
#include "managers/FontManager.hpp"
#include "managers/TextureManager.hpp"
#include "managers/GameStateManager.hpp"
#include <algorithm>
#include <cmath>

#include "gpu/GPURenderer.hpp"
#include "gpu/GPUTypes.hpp"
#include "gpu/GPUVertexPool.hpp"
#include "gpu/SpriteBatch.hpp"
#include <SDL3/SDL_gpu.h>


bool LogoState::enter() {
  GAMESTATE_INFO("Entering LOGO State");

  // Pause all gameplay managers during logo display
  GameEngine::Instance().setGlobalPause(true);

  // Reset timer when entering state
  m_stateTimer = 0.0f;

  // Calculate initial layout
  recalculateLayout();

  // Cache SoundManager reference for better performance
  SoundManager& soundMgr = SoundManager::Instance();
  soundMgr.playSFX("sfx_logo", 0, 0);//change right value from 0 -> 1. For dev.
  return true;
}

void LogoState::recalculateLayout() {
  // Cache layout calculations
  const GameEngine& gameEngine = GameEngine::Instance();
  m_windowWidth = gameEngine.getWidthInPixels();
  m_windowHeight = gameEngine.getHeightInPixels();

  // Calculate scale factor for resolution-aware sizing (1920x1080 baseline)
  // Cap at 1.0 to prevent logos from scaling larger than original at high resolutions
  float scale = std::min(1.0f, std::min(m_windowWidth / 1920.0f, m_windowHeight / 1080.0f));

  // Scale all image dimensions proportionally
  m_bannerSize = static_cast<int>(256 * scale);
  m_engineSize = static_cast<int>(128 * scale);
  m_sdlSize = static_cast<int>(128 * scale);
  m_cppSize = static_cast<int>(50 * scale);

  // Calculate positions
  m_bannerX = m_windowWidth / 2 - m_bannerSize / 2;
  m_bannerY = (m_windowHeight / 2) - static_cast<int>(300 * scale);

  m_engineX = m_windowWidth / 2 - m_engineSize / 2;
  m_engineY = (m_windowHeight / 2) + static_cast<int>(10 * scale);

  m_cppX = m_windowWidth / 2 + static_cast<int>(155 * scale);
  m_cppY = (m_windowHeight / 2) + static_cast<int>(205 * scale);

  m_sdlX = (m_windowWidth / 2) - (m_sdlSize / 2) + static_cast<int>(20 * scale);
  m_sdlY = (m_windowHeight / 2) + static_cast<int>(260 * scale);

  m_titleY = (m_windowHeight / 2) + static_cast<int>(180 * scale);
  m_subtitleY = (m_windowHeight / 2) + static_cast<int>(220 * scale);
  m_versionY = (m_windowHeight / 2) + static_cast<int>(260 * scale);
}

void LogoState::update(float deltaTime) {
  m_stateTimer += deltaTime;

  if (m_stateTimer > 3.0f) {
    // Use immediate state change - proper enter/exit sequencing handles timing
    if (mp_stateManager->hasState(GameStateId::MAIN_MENU)) {
      mp_stateManager->changeState(GameStateId::MAIN_MENU);
    }
  }
}
bool LogoState::exit() {
  GAMESTATE_INFO("Exiting LOGO State");

  // LogoState doesn't create UI components, so no UI cleanup needed

  return true;
}

void LogoState::handleInput() {
  // LogoState doesn't need input handling
}


void LogoState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                  float) {
  // Check if window dimensions changed
  GameEngine& gameEngine = GameEngine::Instance();
  int currentWidth = gameEngine.getWidthInPixels();
  int currentHeight = gameEngine.getHeightInPixels();
  if (currentWidth != m_windowWidth || currentHeight != m_windowHeight) {
    recalculateLayout();
  }

  m_drawCommands.clear();
  TextureManager& texMgr = TextureManager::Instance();

  auto& vertexPool = gpuRenderer.getSpriteVertexPool();
  auto* basePtr = static_cast<VoidLight::SpriteVertex*>(vertexPool.getMappedPtr());
  if (!basePtr) {
    return;
  }

  const auto* sceneTexture = gpuRenderer.getSceneTexture();
  if (!sceneTexture) {
    return;
  }
  const float sceneHeight = static_cast<float>(sceneTexture->getHeight());

  constexpr float sceneScale = 1.0f;  // Render at screen coordinates
  uint32_t vertexOffset = 0;

  // Helper to add a logo sprite — preserves aspect ratio, fits within size x size
  auto addLogo = [&](const char* textureName, int x, int y, int size) {
    auto texData = texMgr.getGPUTextureData(textureName);
    if (!texData || !texData->texture) {
      return;
    }

    // Scale to fit within size while preserving aspect ratio
    float aspect = (texData->height > 0)
        ? static_cast<float>(texData->width) / static_cast<float>(texData->height)
        : 1.0f;
    float sw = static_cast<float>(size) * sceneScale;
    float sh = sw / aspect;

    // Write 4 vertices for this quad
    VoidLight::SpriteVertex* v = basePtr + vertexOffset;
     float sx = static_cast<float>(x) * sceneScale;
     float sy = static_cast<float>(y) * sceneScale;
     float top = sceneHeight - sy;
     float bottom = top - sh;

     // Top-left
     v[0] = {.x=sx,      .y=top,    .u=0.0f, .v=0.0f, .r=255, .g=255, .b=255, .a=255};
     // Top-right
     v[1] = {.x=sx + sw, .y=top,    .u=1.0f, .v=0.0f, .r=255, .g=255, .b=255, .a=255};
     // Bottom-right
     v[2] = {.x=sx + sw, .y=bottom, .u=1.0f, .v=1.0f, .r=255, .g=255, .b=255, .a=255};
     // Bottom-left
     v[3] = {.x=sx,      .y=bottom, .u=0.0f, .v=1.0f, .r=255, .g=255, .b=255, .a=255};

    // Record draw command
    GPUDrawCommand cmd;
    cmd.texture = texData->texture->get();
    cmd.vertexOffset = vertexOffset;
    cmd.vertexCount = 4;
    m_drawCommands.push_back(cmd);
    vertexOffset += 4;
  };

  // Add all logos
  addLogo("HammerForgedBanner", m_bannerX, m_bannerY, m_bannerSize);
  addLogo("VoidLightEngine", m_engineX, m_engineY, m_engineSize);
  addLogo("cpp", m_cppX, m_cppY, m_cppSize);
  addLogo("sdl_logo", m_sdlX, m_sdlY, m_sdlSize);

  // Set vertex count for pool upload
  vertexPool.setWrittenVertexCount(vertexOffset);

  // Record text vertices to UI vertex pool (rendered to swapchain)
  m_textDrawBatches.clear();
  FontManager& fontMgr = FontManager::Instance();

  auto& uiPool = gpuRenderer.getUIVertexPool();
  auto* uiBasePtr = static_cast<VoidLight::SpriteVertex*>(uiPool.getMappedPtr());
  if (!uiBasePtr) {
    return;
  }

  uint32_t uiVertexOffset = 0;
  int centerX = m_windowWidth / 2;
  const float viewportHeight = static_cast<float>(gpuRenderer.getViewportHeight());

  // Helper to add text
  auto addText = [&](const std::string& key, const std::string& text, int x, int y) {
    int textWidth = 0;
    int textHeight = 0;
    if (!fontMgr.prepareGPUText(key, text, "fonts_Arial", &textWidth, &textHeight)) {
      return;
    }

    float dstX = std::round(static_cast<float>(x) - textWidth / 2.0f);
    float dstY = std::round(static_cast<float>(y) - textHeight / 2.0f);

    TTF_GPUAtlasDrawSequence* drawSequence = fontMgr.getGPUTextDrawData(key);
    if (!drawSequence) {
      return;
    }

    for (TTF_GPUAtlasDrawSequence* seq = drawSequence; seq != nullptr; seq = seq->next) {
      if (!seq->atlas_texture || !seq->xy || !seq->uv || !seq->indices ||
          seq->num_indices <= 0 || seq->num_vertices <= 0) {
        continue;
      }

      VoidLight::SpriteVertex* v = uiBasePtr + uiVertexOffset;
      SDL_Color drawColor = {.r=200, .g=200, .b=200, .a=255};
      auto pipeline = VoidLight::UITextPipelineKind::Alpha;
      if (seq->image_type == TTF_IMAGE_COLOR) {
        drawColor = {.r=255, .g=255, .b=255, .a=255};
        pipeline = VoidLight::UITextPipelineKind::Color;
      } else if (seq->image_type == TTF_IMAGE_SDF) {
        pipeline = VoidLight::UITextPipelineKind::SDF;
      }
      for (int i = 0; i < seq->num_indices; ++i) {
        int sourceIndex = seq->indices[i];
        if (sourceIndex < 0 || sourceIndex >= seq->num_vertices) {
          return;
        }

        const SDL_FPoint& pos = seq->xy[sourceIndex];
        const SDL_FPoint& uv = seq->uv[sourceIndex];
        // SDL3_ttf GPU text already provides UVs in SDL_GPU convention.
        v[i] = {.x=dstX + pos.x, .y=(viewportHeight - dstY) + pos.y,
                .u=uv.x, .v=uv.y,
                .r=drawColor.r, .g=drawColor.g, .b=drawColor.b, .a=drawColor.a};
      }

      const uint32_t indexCount = static_cast<uint32_t>(seq->num_indices);
      VoidLight::UITextDrawBatch batch;
      batch.texture = seq->atlas_texture;
      batch.pipeline = pipeline;
      batch.vertexOffset = uiVertexOffset;
      batch.vertexCount = indexCount;
      m_textDrawBatches.push_back(batch);
      uiVertexOffset += indexCount;
    }
  };

  addText("logo:title", "*.:[ VoidLight Engine ]:.*", centerX, m_titleY);
  addText("logo:subtitle", "Powered by SDL3", centerX, m_subtitleY);
  addText("logo:version", "v0.9.0", centerX, m_versionY);

  uiPool.setWrittenVertexCount(uiVertexOffset);
}

void LogoState::renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                                SDL_GPURenderPass* scenePass,
                                float) {
  if (m_drawCommands.empty()) {
    return;
  }

  const auto* sceneTexture = gpuRenderer.getSceneTexture();
  if (!sceneTexture) {
    return;
  }

  // Create orthographic projection for scene texture (3x viewport size)
  float orthoMatrix[16];
  VoidLight::GPURenderer::createOrthoMatrix(
      0.0f, static_cast<float>(sceneTexture->getWidth()),
      0.0f, static_cast<float>(sceneTexture->getHeight()),
      orthoMatrix);

  // Push view-projection matrix
  gpuRenderer.pushViewProjection(scenePass, orthoMatrix);

  // Bind pipeline once
  SDL_BindGPUGraphicsPipeline(scenePass, gpuRenderer.getSpriteAlphaPipeline());

  // Bind vertex buffer once
  SDL_GPUBufferBinding vertexBinding{};
  vertexBinding.buffer = gpuRenderer.getSpriteVertexPool().getGPUBuffer();
  vertexBinding.offset = 0;
  SDL_BindGPUVertexBuffers(scenePass, 0, &vertexBinding, 1);

  // Bind index buffer once
  const auto& batch = gpuRenderer.getSpriteBatch();
  SDL_GPUBufferBinding indexBinding{};
  indexBinding.buffer = batch.getIndexBuffer();
  indexBinding.offset = 0;
  SDL_BindGPUIndexBuffer(scenePass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // Draw each texture with its vertices
  for (const auto& cmd : m_drawCommands) {
    // Bind texture
    SDL_GPUTextureSamplerBinding texSampler{};
    texSampler.texture = cmd.texture;
    texSampler.sampler = gpuRenderer.getNearestSampler();
    SDL_BindGPUFragmentSamplers(scenePass, 0, &texSampler, 1);

    // Draw this sprite (6 indices per quad)
    // firstIndex = (vertexOffset / 4) * 6 because index buffer has 6 indices per 4 vertices
    uint32_t firstIndex = (cmd.vertexOffset / 4) * 6;
    SDL_DrawGPUIndexedPrimitives(scenePass, 6, 1, firstIndex, 0, 0);
  }
}

void LogoState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                            SDL_GPURenderPass* swapchainPass) {
  gpuRenderer.renderUIBatches(swapchainPass, 0, {}, m_textDrawBatches);
}
