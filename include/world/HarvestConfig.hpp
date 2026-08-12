/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HARVEST_CONFIG_HPP
#define HARVEST_CONFIG_HPP

/**
 * @file HarvestConfig.hpp
 * @brief Configuration for harvesting system
 *
 * Provides type-specific harvest behavior (duration, stamina, action verbs)
 * and maps tile ObstacleType deposits to harvestable resource configurations.
 *
 * All configuration is compile-time constant for zero runtime overhead.
 */

#include "world/HarvestType.hpp"
#include "world/WorldData.hpp"
#include <cstdint>
#include <string_view>

namespace VoidLight {

/**
 * @brief Configuration for a specific harvest type
 *
 * Defines how long harvesting takes, stamina cost, and display text.
 */
struct HarvestTypeConfig {
    float baseDuration{1.0f};              // Seconds to complete harvest
    float staminaCost{0.0f};               // Stamina consumed (future use)
    std::string_view actionVerb{"Harvesting..."}; // Display text during harvest
};

/**
 * @brief Configuration for harvestable tile deposits
 *
 * Maps ObstacleType deposits (ore, gems) to their resource yields.
 */
struct DepositConfig {
    std::string_view resourceId{};                 // Resource template ID ("iron_ore", ...)
    int yieldMin{0};                               // Minimum yield per harvest
    int yieldMax{0};                               // Maximum yield per harvest
    float respawnTime{0.0f};                       // Seconds until deposit respawns
    HarvestType harvestType{HarvestType::Gathering}; // Type of harvesting required
};

// ============================================================================
// LOOKUP FUNCTIONS
// ============================================================================

/**
 * @brief Get configuration for a harvest type
 * @param type The harvest type to look up
 * @return Configuration for the specified type
 */
[[nodiscard]] const HarvestTypeConfig& getHarvestTypeConfig(HarvestType type);

/**
 * @brief Get configuration for a tile obstacle deposit
 * @param obstacle The obstacle type to look up
 * @return Configuration for the deposit (returns default if not a deposit)
 */
[[nodiscard]] const DepositConfig& getDepositConfig(ObstacleType obstacle);

/**
 * @brief Check if an obstacle type is a harvestable deposit
 * @param obstacle The obstacle type to check
 * @return true if the obstacle is a harvestable deposit (ore, gem, tree, rock)
 */
[[nodiscard]] bool isHarvestableObstacle(ObstacleType obstacle);

/**
 * @brief Get harvest type from obstacle type
 * @param obstacle The obstacle type
 * @return The appropriate HarvestType for this obstacle
 */
[[nodiscard]] HarvestType getHarvestTypeForObstacle(ObstacleType obstacle);

/**
 * @brief Get harvest type from resource ID
 * @param resourceId The resource template ID (e.g., "wood", "iron_ore")
 * @return The appropriate HarvestType for this resource
 *
 * Used when creating harvestables to set the correct harvest type
 * based on what resource they yield.
 */
[[nodiscard]] HarvestType getHarvestTypeForResource(std::string_view resourceId);

} // namespace VoidLight

#endif // HARVEST_CONFIG_HPP
