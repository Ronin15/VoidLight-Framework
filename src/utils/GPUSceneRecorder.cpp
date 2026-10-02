/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "utils/GPUSceneRecorder.hpp"
#include "utils/Camera.hpp"
#include "utils/FrameProfiler.hpp"
#include "core/Logger.hpp"
#include "gpu/GPURenderer.hpp"
#include "gpu/SpriteBatch.hpp"
#include "managers/TextureManager.hpp"
#include <SDL3/SDL_gpu.h>
#include <cmath>

namespace VoidLight {

GPUSceneRecorder::GPUSceneRecorder() = default;
GPUSceneRecorder::~GPUSceneRecorder() = default;
GPUSceneRecorder::GPUSceneRecorder(GPUSceneRecorder&&) noexcept = default;
GPUSceneRecorder& GPUSceneRecorder::operator=(GPUSceneRecorder&&) noexcept = default;

GPUSceneContext GPUSceneRecorder::beginRecording(GPURenderer& gpuRenderer,
    Camera& camera,
    float interpolationAlpha) {
    PROFILE_RENDER_GPU(RenderPhase::BeginScene);

    GPUSceneContext ctx;

    if (m_recordingActive) {
        GPU_SCENE_RECORDER_WARN("GPUSceneRecorder::beginRecording called while recording is already active");
        return ctx;
    }

    // Get atlas GPU texture for sprite batch
    auto atlasTexture = TextureManager::Instance().getGPUTextureData("atlas");
    if (!atlasTexture || !atlasTexture->texture) {
        GPU_SCENE_RECORDER_ERROR("GPUSceneRecorder: Atlas GPU texture not available");
        return ctx;
    }

    // Get viewport from GPURenderer (authoritative source for GPU path)
    float viewWidth = static_cast<float>(gpuRenderer.getViewportWidth());
    float viewHeight = static_cast<float>(gpuRenderer.getViewportHeight());

    // Get camera parameters
    float zoom = camera.getZoom();

    // Get interpolated camera offset
    float rawCameraX = 0.0f;
    float rawCameraY = 0.0f;
    Vector2D cameraCenter = camera.getRenderOffset(rawCameraX, rawCameraY, interpolationAlpha);

    // Snap to pixel for tile-aligned rendering
    float flooredCameraX = std::floor(rawCameraX);
    float flooredCameraY = std::floor(rawCameraY);

    // Calculate sub-pixel offset for smooth scrolling (applied in composite shader)
    float subPixelX = (rawCameraX - flooredCameraX) / viewWidth;
    float subPixelY = (rawCameraY - flooredCameraY) / viewHeight;

    // Set composite params for this frame (zoom and sub-pixel offset)
    gpuRenderer.setCompositeParams(zoom, subPixelX, subPixelY);

    // Get sprite batch and vertex pool
    auto& spriteBatch = gpuRenderer.getSpriteBatch();
    const auto& vertexPool = gpuRenderer.getSpriteVertexPool();

    // Get mapped vertex buffer (GPURenderer::beginFrame already mapped it)
    auto* writePtr = static_cast<SpriteVertex*>(vertexPool.getMappedPtr());
    if (!writePtr) {
        GPU_SCENE_RECORDER_ERROR("GPUSceneRecorder: Sprite vertex pool not mapped");
        return ctx;
    }

    // Begin sprite batch with atlas texture
    spriteBatch.begin(writePtr, vertexPool.getMaxVertices(),
        atlasTexture->texture->get(), gpuRenderer.getNearestSampler(),
        static_cast<float>(atlasTexture->width),
        static_cast<float>(atlasTexture->height),
        static_cast<float>(gpuRenderer.getSceneTexture()->getHeight()));

    // Store state for later phases
    m_recordingActive = true;
    m_spriteBatchActive = true;
    m_gpuRenderer = &gpuRenderer;
    m_spriteBatch = &spriteBatch;
    m_zoom = zoom;

    // Populate context
    ctx.cameraX = flooredCameraX;
    ctx.cameraY = flooredCameraY;
    ctx.viewWidth = viewWidth;
    ctx.viewHeight = viewHeight;
    ctx.zoom = zoom;
    ctx.interpolationAlpha = interpolationAlpha;
    ctx.cameraCenter = cameraCenter;
    ctx.spriteBatch = &spriteBatch;
    ctx.valid = true;

    return ctx;
}

void GPUSceneRecorder::endSpriteBatch() {
    if (!m_spriteBatchActive || !m_spriteBatch) {
        GPU_SCENE_RECORDER_WARN("GPUSceneRecorder::endSpriteBatch called without an active batch");
        return;
    }

    m_spriteBatch->end();
    m_spriteBatchActive = false;
}

void GPUSceneRecorder::endRecording() {
    PROFILE_RENDER_GPU(RenderPhase::EndScene);

    if (!m_recordingActive) {
        GPU_SCENE_RECORDER_WARN("GPUSceneRecorder::endRecording called without matching beginRecording");
        return;
    }

    // Ensure sprite batch is ended if caller forgot
    if (m_spriteBatchActive && m_spriteBatch) {
        GPU_SCENE_RECORDER_WARN("GPUSceneRecorder::endRecording: sprite batch not ended, ending now");
        m_spriteBatch->end();
        m_spriteBatchActive = false;
    }

    m_recordingActive = false;
    m_gpuRenderer = nullptr;
    m_spriteBatch = nullptr;
}

void GPUSceneRecorder::renderRecordedScene(GPURenderer& gpuRenderer,
    SDL_GPURenderPass* scenePass) {
    PROFILE_RENDER_GPU(RenderPhase::WorldTiles);

    auto& spriteBatch = gpuRenderer.getSpriteBatch();
    const auto& vertexPool = gpuRenderer.getSpriteVertexPool();

    // Skip if no sprites recorded
    if (!spriteBatch.hasSprites()) {
        return;
    }

    // Get scene texture for ortho matrix dimensions
    auto* sceneTexture = gpuRenderer.getSceneTexture();
    if (!sceneTexture) {
        return;
    }

    // Create orthographic projection for scene texture
    float orthoMatrix[16];
    GPURenderer::createOrthoMatrix(
        0.0f, static_cast<float>(sceneTexture->getWidth()),
        0.0f, static_cast<float>(sceneTexture->getHeight()),
        orthoMatrix);

    // Push view-projection matrix
    gpuRenderer.pushViewProjection(scenePass, orthoMatrix);

    // Issue draw call using the pre-recorded sprites
    spriteBatch.render(scenePass, gpuRenderer.getSpriteAlphaPipeline(),
        vertexPool.getGPUBuffer());
}

} // namespace VoidLight
