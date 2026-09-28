/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HARVEST_TYPE_HPP
#define HARVEST_TYPE_HPP

#include <cstdint>
#include <string_view>
#include <iostream>

namespace VoidLight {

/**
 * @brief Type of harvesting action required for a resource
 *
 * Each HarvestType has different:
 * - Base duration (gathering is quick, mining is slow)
 * - Tool requirements (future: axe for chopping, pickaxe for mining)
 * - Animation and sound effects
 * - Skill progression (future)
 */
enum class HarvestType : uint8_t {
    Gathering = 0, // Herbs, flowers, bushes (fastest, no tool needed)
    Chopping = 1, // Trees → wood (requires axe)
    Mining = 2, // Ore/gem deposits (requires pickaxe)
    Quarrying = 3, // Stone, limestone (requires pickaxe)
    Fishing = 4, // Water resources (requires fishing rod, future)
    COUNT
};

/**
 * @brief Get string representation of HarvestType
 */
[[nodiscard]] constexpr std::string_view harvestTypeToString(HarvestType type) noexcept {
    switch (type) {
        case HarvestType::Gathering: return "Gathering";
        case HarvestType::Chopping: return "Chopping";
        case HarvestType::Mining: return "Mining";
        case HarvestType::Quarrying: return "Quarrying";
        case HarvestType::Fishing: return "Fishing";
        default: return "Unknown";
    }
}

/**
 * @brief Get action verb for UI display during harvesting
 */
[[nodiscard]] constexpr std::string_view harvestTypeToActionVerb(HarvestType type) noexcept {
    switch (type) {
        case HarvestType::Gathering: return "Gathering...";
        case HarvestType::Chopping: return "Chopping...";
        case HarvestType::Mining: return "Mining...";
        case HarvestType::Quarrying: return "Quarrying...";
        case HarvestType::Fishing: return "Fishing...";
        default: return "Harvesting...";
    }
}

// Stream operator for test output
inline std::ostream& operator<<(std::ostream& os, HarvestType type) {
    return os << harvestTypeToString(type);
}

} // namespace VoidLight

#endif // HARVEST_TYPE_HPP
