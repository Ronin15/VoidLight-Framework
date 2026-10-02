/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PROJECTILE_RENDER_CONTROLLER_HPP
#define PROJECTILE_RENDER_CONTROLLER_HPP

/**
 * @file ProjectileRenderController.hpp
 * @brief Renders projectile entities as bright green placeholder rectangles
 *
 * Follows NPCRenderController pattern: iterates active projectile indices,
 * interpolates position, draws via SpriteBatch. No animation state needed.
 *
 * Usage:
 *   - Add as member in GameState
 *   - Call recordGPU(ctx) during vertex recording phase
 */

#include "controllers/ControllerBase.hpp"

namespace VoidLight {
struct GPUSceneContext;
}

class ProjectileRenderController : public ControllerBase {
public:
    ProjectileRenderController() = default;
    ~ProjectileRenderController() override = default;

    ProjectileRenderController(const ProjectileRenderController&) = delete;
    ProjectileRenderController& operator=(const ProjectileRenderController&) = delete;
    ProjectileRenderController(ProjectileRenderController&&) = delete;
    ProjectileRenderController& operator=(ProjectileRenderController&&) = delete;

    // ControllerBase interface
    void subscribe() override {}
    [[nodiscard]] std::string_view getName() const override { return "ProjectileRenderController"; }

    /**
     * @brief Record projectile sprites to the GPU sprite batch
     * @param ctx Scene context with sprite batch and camera params
     */
    void recordGPU(const VoidLight::GPUSceneContext& ctx);

private:
    static constexpr float PROJECTILE_WIDTH = 16.0f;
    static constexpr float PROJECTILE_HEIGHT = 6.0f;
};

#endif // PROJECTILE_RENDER_CONTROLLER_HPP
