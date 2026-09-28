/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/render/ResourceRenderController.hpp"
#include "core/Logger.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "utils/Camera.hpp"
#include "utils/GPUSceneRecorder.hpp"
#include "utils/Vector2D.hpp"
#include "gpu/SpriteBatch.hpp"
#include <cmath>
#include <vector>

namespace {

[[nodiscard]] uint16_t getContainerFrameWidth(const ContainerRenderData& renderData, bool isOpen) {
    const bool useOpenVariant = isOpen && (renderData.openAtlasX != 0 || renderData.openAtlasY != 0);
    return useOpenVariant ? renderData.openFrameWidth : renderData.frameWidth;
}

[[nodiscard]] uint16_t getContainerFrameHeight(const ContainerRenderData& renderData, bool isOpen) {
    const bool useOpenVariant = isOpen && (renderData.openAtlasX != 0 || renderData.openAtlasY != 0);
    return useOpenVariant ? renderData.openFrameHeight : renderData.frameHeight;
}

} // namespace

void ResourceRenderController::update(float deltaTime, const VoidLight::Camera& camera) {
    updateDroppedItemAnimations(deltaTime, camera);
    updateContainerStates(deltaTime, camera);
}

void ResourceRenderController::updateDroppedItemAnimations(float deltaTime, const VoidLight::Camera& camera) {
    auto& edm = EntityDataManager::Instance();
    auto& wrm = WorldResourceManager::Instance();

    // Query items in camera view + buffer (not all items in world)
    Vector2D cameraCenter = camera.getPosition();
    const auto& viewport = camera.getViewport();
    float animationRadius = std::sqrt(viewport.width * viewport.width +
                                viewport.height * viewport.height) *
            0.5f +
        ANIMATION_BUFFER;

    m_visibleItemIndices.clear();
    wrm.queryDroppedItemsInRadius(cameraCenter, animationRadius, m_visibleItemIndices);

    for (size_t idx : m_visibleItemIndices) {
        const auto& hot = edm.getStaticHotDataByIndex(idx); // Static pool accessor
        if (!hot.isAlive()) continue;

        auto& r = edm.getItemRenderDataByTypeIndex(hot.typeLocalIndex);

        // Update bob phase (sine wave)
        r.bobPhase += BOB_SPEED * deltaTime;
        if (r.bobPhase >= TWO_PI) {
            r.bobPhase -= TWO_PI;
        }

        // Update frame animation (if multi-frame)
        if (r.numFrames > 1) {
            float speed = static_cast<float>(r.animSpeedMs) * 0.001f;
            r.animTimer += deltaTime;
            if (r.animTimer >= speed) {
                r.currentFrame = (r.currentFrame + 1) % r.numFrames;
                r.animTimer -= speed;
            }
        }
    }
}

void ResourceRenderController::updateContainerStates(float,
    const VoidLight::Camera&) {
    // Container open/close animations will be implemented when containers are added
    // For now, containers are static (just open or closed state)
}

void ResourceRenderController::recordGPUDroppedItems(const VoidLight::GPUSceneContext& ctx,
    const VoidLight::Camera& camera) {
    RESOURCE_RENDER_WARN_IF(!ctx.spriteBatch, "recordGPUDroppedItems: ctx.spriteBatch is null");
    if (!ctx.spriteBatch) { return; }

    auto& edm = EntityDataManager::Instance();
    auto& wrm = WorldResourceManager::Instance();

    // Query visible items using rendered camera center (not camera.getPosition()
    // which lags in Follow mode due to blend factor)
    Vector2D cameraCenter = ctx.cameraCenter;
    const auto& viewport = camera.getViewport();
    float visibleRadius = std::sqrt(viewport.width * viewport.width +
                              viewport.height * viewport.height) *
        0.5f;

    m_visibleItemIndices.clear();
    wrm.queryDroppedItemsInRadius(cameraCenter, visibleRadius, m_visibleItemIndices);

    const float alpha = ctx.interpolationAlpha;

    for (size_t idx : m_visibleItemIndices) {
        const auto& hot = edm.getStaticHotDataByIndex(idx);
        if (!hot.isAlive()) { continue; }

        const auto& r = edm.getItemRenderDataByTypeIndex(hot.typeLocalIndex);

        // Skip unmapped textures (atlasX and atlasY both 0) - will use default when wired up
        if (r.atlasX == 0 && r.atlasY == 0) { continue; }

        // Interpolate position
        float interpX = hot.transform.previousPosition.getX() +
            (hot.transform.position.getX() - hot.transform.previousPosition.getX()) * alpha;
        float interpY = hot.transform.previousPosition.getY() +
            (hot.transform.position.getY() - hot.transform.previousPosition.getY()) * alpha;

        // Add bobbing offset
        float bobOffset = std::sin(r.bobPhase) * r.bobAmplitude;

        // Source rect from atlas coords
        float srcX = static_cast<float>(r.atlasX + r.currentFrame * r.frameWidth);
        float srcY = static_cast<float>(r.atlasY);
        float srcW = static_cast<float>(r.frameWidth);
        float srcH = static_cast<float>(r.frameHeight);

        // Destination rect (screen space, centered with bob)
        float halfW = srcW * 0.5f;
        float halfH = srcH * 0.5f;
        float dstX = interpX - ctx.cameraX - halfW;
        float dstY = interpY - ctx.cameraY - halfH + bobOffset;

        ctx.spriteBatch->draw(srcX, srcY, srcW, srcH, dstX, dstY, srcW, srcH);
    }
}

void ResourceRenderController::recordGPUContainers(const VoidLight::GPUSceneContext& ctx,
    const VoidLight::Camera& camera) {
    RESOURCE_RENDER_WARN_IF(!ctx.spriteBatch, "recordGPUContainers: ctx.spriteBatch is null");
    if (!ctx.spriteBatch) { return; }

    auto& edm = EntityDataManager::Instance();
    auto& wrm = WorldResourceManager::Instance();

    // Query visible containers using rendered camera center (not camera.getPosition()
    // which lags in Follow mode due to blend factor)
    Vector2D cameraCenter = ctx.cameraCenter;
    const auto& viewport = camera.getViewport();
    float visibleRadius = std::sqrt(viewport.width * viewport.width +
                              viewport.height * viewport.height) *
        0.5f;

    m_visibleContainerIndices.clear();
    wrm.queryContainersInRadius(cameraCenter, visibleRadius, m_visibleContainerIndices);

    const float alpha = ctx.interpolationAlpha;

    for (size_t idx : m_visibleContainerIndices) {
        const auto& hot = edm.getStaticHotDataByIndex(idx);
        if (!hot.isAlive()) { continue; }

        const auto& containerData = edm.getContainerData(hot.typeLocalIndex);
        const auto& r = edm.getContainerRenderDataByTypeIndex(hot.typeLocalIndex);
        const uint16_t frameWidth = getContainerFrameWidth(r, containerData.isOpen());
        const uint16_t frameHeight = getContainerFrameHeight(r, containerData.isOpen());

        // Choose atlas coords based on open/closed state
        const bool useOpenVariant = containerData.isOpen() && (r.openAtlasX != 0 || r.openAtlasY != 0);
        uint16_t atlasX = useOpenVariant ? r.openAtlasX : r.atlasX;
        uint16_t atlasY = useOpenVariant ? r.openAtlasY : r.atlasY;

        // Skip unmapped textures (both coords 0) - will use default when wired up
        if (atlasX == 0 && atlasY == 0) { continue; }

        // Interpolate position
        float interpX = hot.transform.previousPosition.getX() +
            (hot.transform.position.getX() - hot.transform.previousPosition.getX()) * alpha;
        float interpY = hot.transform.previousPosition.getY() +
            (hot.transform.position.getY() - hot.transform.previousPosition.getY()) * alpha;

        // Source rect from atlas coords
        float srcX = static_cast<float>(atlasX + r.currentFrame * frameWidth);
        float srcY = static_cast<float>(atlasY);
        float srcW = static_cast<float>(frameWidth);
        float srcH = static_cast<float>(frameHeight);

        // Destination rect (screen space, centered)
        float halfW = srcW * 0.5f;
        float halfH = srcH * 0.5f;
        float dstX = interpX - ctx.cameraX - halfW;
        float dstY = interpY - ctx.cameraY - halfH;

        ctx.spriteBatch->draw(srcX, srcY, srcW, srcH, dstX, dstY, srcW, srcH);
    }
}
