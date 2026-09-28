/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef RESOURCE_RENDER_CONTROLLER_HPP
#define RESOURCE_RENDER_CONTROLLER_HPP

/**
 * @file ResourceRenderController.hpp
 * @brief Unified rendering controller for dropped items and containers
 *
 * Renders resources using data from EntityDataManager:
 * - DroppedItems: Bobbing animation, frame cycling
 * - Containers: Open/closed state rendering
 *
 * Usage:
 *   - Add as member in GameState
 *   - Call update(deltaTime) in GameState::update()
 *   - Call GPU record methods during vertex recording
 */

#include "controllers/ControllerBase.hpp"
#include <vector>

namespace VoidLight {
class Camera;
struct GPUSceneContext;
}

class ResourceRenderController : public ControllerBase {
public:
    ResourceRenderController() = default;
    ~ResourceRenderController() override = default;

    // Non-copyable, non-movable
    ResourceRenderController(const ResourceRenderController&) = delete;
    ResourceRenderController& operator=(const ResourceRenderController&) = delete;
    ResourceRenderController(ResourceRenderController&&) = delete;
    ResourceRenderController& operator=(ResourceRenderController&&) = delete;

    // ControllerBase interface
    void subscribe() override {} // No events needed
    [[nodiscard]] std::string_view getName() const override { return "ResourceRenderController"; }

    /**
     * @brief Update animations for visible resources only
     * @param deltaTime Frame delta time
     * @param camera Camera for viewport-based culling (only animate visible + buffer)
     */
    void update(float deltaTime, const VoidLight::Camera& camera);

    /**
     * @brief Record dropped items to GPU sprite batch
     * @param ctx Scene context with sprite batch and camera params
     * @param camera Camera for spatial queries
     */
    void recordGPUDroppedItems(const VoidLight::GPUSceneContext& ctx,
        const VoidLight::Camera& camera);

    /**
     * @brief Record containers to GPU sprite batch
     * @param ctx Scene context with sprite batch and camera params
     * @param camera Camera for spatial queries
     */
    void recordGPUContainers(const VoidLight::GPUSceneContext& ctx,
        const VoidLight::Camera& camera);

private:
    // Update helpers - use camera-based queries for efficiency
    void updateDroppedItemAnimations(float deltaTime, const VoidLight::Camera& camera);
    void updateContainerStates(float deltaTime, const VoidLight::Camera& camera);
    // Reusable buffers for spatial queries (avoid per-frame allocations)
    std::vector<size_t> m_visibleItemIndices;
    std::vector<size_t> m_visibleContainerIndices;

    // Animation constants
    static constexpr float BOB_SPEED = 3.0f; // Radians per second for bobbing
    static constexpr float TWO_PI = 6.28318530718f; // 2 * PI for wrapping
    static constexpr float ANIMATION_BUFFER = 128.0f; // Extra radius beyond viewport for animation
};

#endif // RESOURCE_RENDER_CONTROLLER_HPP
