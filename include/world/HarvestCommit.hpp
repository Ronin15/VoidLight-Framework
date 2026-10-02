/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HARVEST_COMMIT_HPP
#define HARVEST_COMMIT_HPP

/**
 * @file HarvestCommit.hpp
 * @brief Shared main-thread harvest depletion path for player and AI
 *
 * HarvestCommit is the single place that depletes an EDM harvestable. It
 * validates the generation-safe handle, enforces an optional area reserve,
 * rolls the yield, writes the EDM depletion, bumps the WRM harvestable
 * version, and dispatches HarvestResourceEvent plus ScarcityEvent when the
 * area is running out. Inventory handling stays with the caller.
 *
 * WorldResourceManager stays a registry: it only versions and counts.
 * Main-thread only.
 */

#include "entities/EntityHandle.hpp"
#include "utils/ResourceHandle.hpp"
#include "utils/Vector2D.hpp"
#include <cstddef>
#include <optional>

namespace VoidLight::HarvestCommit {

// Interaction reach shared by the player controller and AI foragers (px).
inline constexpr float HARVEST_RANGE = 48.0f;
// Area used for reserve and scarcity counts (px).
inline constexpr float SCARCITY_RADIUS = 512.0f;
// A depletion leaving fewer than this many available nodes emits ScarcityEvent.
inline constexpr size_t SCARCITY_THRESHOLD = 2;
// NPC harvests never leave fewer than this many available nodes in the area.
inline constexpr size_t NPC_HARVEST_RESERVE = 1;

struct HarvestYield {
    ResourceHandle resource{};
    int quantity{0};
    Vector2D position{0.0f, 0.0f};
};

/**
 * @brief Deplete a harvestable and return its yield.
 * @param harvestable Generation-safe handle of the target harvestable
 * @param harvester Entity performing the harvest (carried on ScarcityEvent)
 * @param minRemaining Available nodes (excluding this one) that must remain in
 *        SCARCITY_RADIUS; NPCs pass NPC_HARVEST_RESERVE, the player passes 0
 * @return Yield on success; nullopt when stale, already depleted, or the
 *         reserve would be violated
 */
[[nodiscard]] std::optional<HarvestYield> commit(EntityHandle harvestable,
    EntityHandle harvester,
    size_t minRemaining);

} // namespace VoidLight::HarvestCommit

#endif // HARVEST_COMMIT_HPP
