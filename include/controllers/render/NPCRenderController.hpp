/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef NPC_RENDER_CONTROLLER_HPP
#define NPC_RENDER_CONTROLLER_HPP

/**
 * @file NPCRenderController.hpp
 * @brief Velocity-based NPC rendering controller (data-driven approach)
 *
 * Renders NPCs using NPCRenderData from EntityDataManager.
 * Animation is velocity-based: Idle when stationary, Moving when velocity > threshold.
 * No NPC class needed - all data comes from EDM.
 *
 * Usage:
 *   - Add as member in GameState
 *   - Call update(deltaTime) in GameState::update()
 *   - Call recordGPU(ctx) during vertex recording phase
 */

#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"

namespace VoidLight {
struct GPUSceneContext;
}

class NPCRenderController : public ControllerBase, public IUpdatable {
public:
    NPCRenderController() = default;
    ~NPCRenderController() override = default;

    // Non-copyable, non-movable
    NPCRenderController(const NPCRenderController&) = delete;
    NPCRenderController& operator=(const NPCRenderController&) = delete;
    NPCRenderController(NPCRenderController&&) = delete;
    NPCRenderController& operator=(NPCRenderController&&) = delete;

    // ControllerBase interface
    void subscribe() override {} // No events needed
    [[nodiscard]] std::string_view getName() const override { return "NPCRenderController"; }

    // IUpdatable - advances animation frames based on velocity
    void update(float deltaTime) override;

    /**
     * @brief Record NPC sprites to the GPU sprite batch
     * @param ctx Scene context with sprite batch and camera params
     *
     * Called during vertex recording phase. Uses ctx.spriteBatch->draw()
     * for atlas-based rendering. Batch lifecycle managed by GPUSceneRecorder.
     */
    void recordGPU(const VoidLight::GPUSceneContext& ctx);

private:
    static constexpr float MOVEMENT_THRESHOLD = 15.0f; // Velocity threshold for Moving/Idle
    static constexpr float MOVEMENT_THRESHOLD_SQ = MOVEMENT_THRESHOLD * MOVEMENT_THRESHOLD; // Squared for lengthSquared() comparison
};

#endif // NPC_RENDER_CONTROLLER_HPP
