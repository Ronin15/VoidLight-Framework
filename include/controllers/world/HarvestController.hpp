/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HARVEST_CONTROLLER_HPP
#define HARVEST_CONTROLLER_HPP

/**
 * @file HarvestController.hpp
 * @brief Unified controller for progress-based resource harvesting
 *
 * HarvestController handles all harvestable resources (wood, stone, ore, gems,
 * herbs) with progress-based harvesting. Each HarvestType has different duration
 * and behavior.
 *
 * Features:
 * - Progress-based harvesting with type-specific durations
 * - Movement cancellation (moving during harvest cancels it)
 * - Tile-based deposit support (ore/gem deposits in world tiles)
 * - EDM harvestable entity support (trees, stones via WorldResourceManager)
 *
 * This controller implements IUpdatable for per-frame progress tracking.
 *
 * Ownership: ControllerRegistry owns the controller instance.
 */

#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include "world/HarvestType.hpp"
#include <memory>
#include <string_view>
#include <vector>

// Forward declarations
class Player;

class HarvestController : public ControllerBase, public IUpdatable {
public:
    /**
     * @brief Construct HarvestController with required player reference
     * @param player Shared pointer to the player (required)
     */
    explicit HarvestController(std::shared_ptr<Player> player);

    ~HarvestController() override = default;

    // Movable (inherited from base)
    HarvestController(HarvestController&&) noexcept = default;
    HarvestController& operator=(HarvestController&&) noexcept = default;

    // --- ControllerBase interface ---

    void subscribe() override;

    [[nodiscard]] std::string_view getName() const override { return "HarvestController"; }

    // --- IUpdatable interface ---

    /**
     * @brief Update harvest progress
     * @param deltaTime Frame delta time in seconds
     *
     * Checks for movement cancellation and advances harvest timer.
     * Completes harvest when timer reaches duration.
     */
    void update(float deltaTime) override;

    // --- Harvest API ---

    /**
     * @brief Start harvesting the nearest resource
     * @return true if harvest started, false if no valid target
     *
     * Finds nearest harvestable (EDM entity or tile deposit) within range,
     * validates it's not depleted, and starts progress timer.
     */
    bool startHarvest();

    /**
     * @brief Cancel current harvest operation
     *
     * Called automatically on player movement or manually via input.
     */
    void cancelHarvest();

    /**
     * @brief Check if currently harvesting
     * @return true if harvest is in progress
     */
    [[nodiscard]] bool isHarvesting() const { return m_isHarvesting; }

    /**
     * @brief Get current harvest progress (0.0 to 1.0)
     * @return Progress ratio, 0.0 if not harvesting
     */
    [[nodiscard]] float getProgress() const;

    /**
     * @brief Get the type of current harvest
     * @return HarvestType of current target, Gathering if not harvesting
     */
    [[nodiscard]] VoidLight::HarvestType getCurrentType() const { return m_currentType; }

    /**
     * @brief Get action verb for UI display ("Mining...", "Chopping...", etc.)
     * @return Action verb string view
     */
    [[nodiscard]] std::string_view getActionVerb() const;

    /**
     * @brief Get the world position of current harvest target
     * @return Target position, or (0,0) if not harvesting
     */
    [[nodiscard]] Vector2D getTargetPosition() const { return m_targetPosition; }

    // Configuration constants (reach is VoidLight::HarvestCommit::HARVEST_RANGE)
    static constexpr float MOVEMENT_CANCEL_THRESHOLD = 8.0f;

private:
    /**
     * @brief Find the nearest harvestable within range
     * @param outHandle Output: Entity handle if EDM harvestable found
     * @param outStaticIndex Output: Static entity index for EDM harvestables
     * @return true if harvestable found
     *
     * Searches both WRM spatial index and nearby tile deposits.
     */
    bool findNearestHarvestable(EntityHandle& outHandle, size_t& outStaticIndex);

    /**
     * @brief Complete the harvest and award resources
     *
     * Depletes the target through HarvestCommit::commit (shared with AI
     * foragers), then adds the yield to inventory or drops it on the ground.
     */
    void completeHarvest();

    std::weak_ptr<Player> mp_player;

    // Harvest state
    bool m_isHarvesting{false};
    float m_harvestTimer{0.0f};
    float m_harvestDuration{0.0f};
    EntityHandle m_currentTarget{};
    VoidLight::HarvestType m_currentType{VoidLight::HarvestType::Gathering};
    Vector2D m_harvestStartPos{}; // Player position when harvest started
    Vector2D m_targetPosition{}; // Target harvestable position

    // Reusable buffers to avoid per-frame allocations
    std::vector<size_t> m_harvestableIndicesBuffer;
};

#endif // HARVEST_CONTROLLER_HPP
