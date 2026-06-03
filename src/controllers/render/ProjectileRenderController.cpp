/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/render/ProjectileRenderController.hpp"
#include "entities/EntityHandle.hpp"
#include "managers/EntityDataManager.hpp"
#include "gpu/SpriteBatch.hpp"
#include "utils/GPUSceneRecorder.hpp"
#include <cmath>

void ProjectileRenderController::recordGPU(const VoidLight::GPUSceneContext& ctx)
{
    if (!ctx.spriteBatch)
    {
        return;
    }

    auto& edm = EntityDataManager::Instance();
    const float alpha = ctx.interpolationAlpha;

    for (size_t idx : edm.getIndicesByKind(EntityKind::Projectile))
    {
        const auto& hot = edm.getHotDataByIndex(idx);
        if (!hot.isAlive()) { continue; }
        const auto& projectile = edm.getProjectileData(hot.typeLocalIndex);

        // Interpolate position for smooth rendering
        float interpX = hot.transform.previousPosition.getX() +
            (hot.transform.position.getX() - hot.transform.previousPosition.getX()) * alpha;
        float interpY = hot.transform.previousPosition.getY() +
            (hot.transform.position.getY() - hot.transform.previousPosition.getY()) * alpha;

        if (projectile.isEmbedded())
        {
            if (edm.isValidHandle(projectile.embeddedTarget))
            {
                // Target still alive — track its interpolated position and apply stored offset.
                const auto& targetTransform = edm.getTransform(projectile.embeddedTarget);
                interpX = targetTransform.previousPosition.getX() +
                    (targetTransform.position.getX() - targetTransform.previousPosition.getX()) * alpha;
                interpY = targetTransform.previousPosition.getY() +
                    (targetTransform.position.getY() - targetTransform.previousPosition.getY()) * alpha;
                interpX += projectile.embeddedOffsetX;
                interpY += projectile.embeddedOffsetY;
            }
            else if (!projectile.embeddedTarget.isValid())
            {
                // Embedded in world geometry (no target) — use fixed world offset.
                interpX += projectile.embeddedOffsetX;
                interpY += projectile.embeddedOffsetY;
            }
            // else: target handle was valid but entity is gone — keep last interpolated position.
        }

        // Destination rect (screen space, centered on position)
        float halfW = PROJECTILE_WIDTH * 0.5f;
        float halfH = PROJECTILE_HEIGHT * 0.5f;
        float dstX = interpX - ctx.cameraX - halfW;
        float dstY = interpY - ctx.cameraY - halfH;

        uint8_t alphaByte = 255;
        if (projectile.isEmbedded() &&
            projectile.lifetime < ProjectileData::EMBEDDED_FADE_SECONDS)
        {
            const float fadeRatio = std::clamp(
                projectile.lifetime / ProjectileData::EMBEDDED_FADE_SECONDS, 0.0f, 1.0f);
            alphaByte = static_cast<uint8_t>(255.0f * fadeRatio);
        }

        // Compute rotation angle from velocity (or stored angle for embedded)
        float angle = 0.0f;
        if (projectile.isEmbedded())
        {
            angle = projectile.embeddedAngle;
        }
        else
        {
            float vx = hot.transform.velocity.getX();
            float vy = hot.transform.velocity.getY();
            if (vx != 0.0f || vy != 0.0f)
            {
                angle = std::atan2(vy, vx);
            }
        }

        // Solid green placeholder: sample atlas center (guaranteed opaque in packed atlas)
        // using degenerate UV (single texel) so tint dominates the output color
        ctx.spriteBatch->drawUVRotated(0.5f, 0.5f, 0.5f, 0.5f,
                                       dstX, dstY, PROJECTILE_WIDTH, PROJECTILE_HEIGHT,
                                       angle,
                                       0, 255, 0, alphaByte);
    }
}
