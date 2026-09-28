/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/render/NPCRenderController.hpp"
#include "entities/EntityHandle.hpp"
#include "managers/EntityDataManager.hpp"
#include "gpu/SpriteBatch.hpp"
#include "utils/GPUSceneRecorder.hpp"
#include <SDL3/SDL.h>
#include <cmath>

void NPCRenderController::update(float deltaTime) {
    auto& edm = EntityDataManager::Instance();

    // Only update Active tier NPCs (same as AIManager)
    for (size_t idx : edm.getActiveIndices()) {
        const auto& hot = edm.getHotDataByIndex(idx);
        if (hot.kind != EntityKind::NPC) continue;

        auto& r = edm.getNPCRenderDataByTypeIndex(hot.typeLocalIndex);

        // Velocity check once - sets row, frames, speed, flip
        float vx = hot.transform.velocity.getX();
        float vy = hot.transform.velocity.getY();
        bool moving = (vx * vx + vy * vy) > MOVEMENT_THRESHOLD_SQ;

        r.currentRow = moving ? r.moveRow : r.idleRow;
        uint8_t frames = moving ? r.numMoveFrames : r.numIdleFrames;
        float speed = static_cast<float>(moving ? r.moveSpeedMs : r.idleSpeedMs) * 0.001f;

        r.animationAccumulator += deltaTime;
        if (r.animationAccumulator >= speed) {
            r.currentFrame = (r.currentFrame + 1) % frames;
            r.animationAccumulator -= speed;
        }

        if (vx < 0.0f) r.flipMode = static_cast<uint8_t>(SDL_FLIP_HORIZONTAL);
        else if (vx > 0.0f) r.flipMode = static_cast<uint8_t>(SDL_FLIP_NONE);
    }
}

void NPCRenderController::recordGPU(const VoidLight::GPUSceneContext& ctx) {
    if (!ctx.spriteBatch) { return; }

    auto& edm = EntityDataManager::Instance();
    const float alpha = ctx.interpolationAlpha;

    // Only render Active tier NPCs (same as AIManager)
    for (size_t idx : edm.getActiveIndices()) {
        const auto& hot = edm.getHotDataByIndex(idx);
        if (hot.kind != EntityKind::NPC) { continue; }

        const auto& r = edm.getNPCRenderDataByTypeIndex(hot.typeLocalIndex);

        // Interpolate position
        float interpX = hot.transform.previousPosition.getX() +
            (hot.transform.position.getX() - hot.transform.previousPosition.getX()) * alpha;
        float interpY = hot.transform.previousPosition.getY() +
            (hot.transform.position.getY() - hot.transform.previousPosition.getY()) * alpha;

        // Source rect from atlas
        float srcX = static_cast<float>(r.atlasX + r.currentFrame * r.frameWidth);
        float srcY = static_cast<float>(r.atlasY + r.currentRow * r.frameHeight);
        float srcW = static_cast<float>(r.frameWidth);
        float srcH = static_cast<float>(r.frameHeight);

        // Destination rect (screen space)
        float halfW = srcW * 0.5f;
        float halfH = srcH * 0.5f;
        float dstX = interpX - ctx.cameraX - halfW;
        float dstY = interpY - ctx.cameraY - halfH;

        // Handle horizontal flip via UV swap
        if (r.flipMode == static_cast<uint8_t>(SDL_FLIP_HORIZONTAL)) {
            ctx.spriteBatch->draw(srcX + srcW, srcY, -srcW, srcH,
                dstX, dstY, srcW, srcH);
        } else {
            ctx.spriteBatch->draw(srcX, srcY, srcW, srcH,
                dstX, dstY, srcW, srcH);
        }
    }
}
