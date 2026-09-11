/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef FACTION_STANCE_HPP
#define FACTION_STANCE_HPP

#include <array>
#include <cstdint>

/**
 * @brief Directed faction relationship used by AIManager's 16×16 stance table.
 *
 * Allied is 0 so default-constructed FactionStance{} is Allied — a default
 * std::array is therefore all Allied. Use kNeutralFactionStanceRow when a
 * Neutral row is required (BehaviorContext out-of-range faction bind).
 */
enum class FactionStance : uint8_t {
    Allied = 0,
    Neutral = 1,
    Hostile = 2
};

inline constexpr uint8_t kFactionStanceRowSize = 16;

inline constexpr std::array<FactionStance, kFactionStanceRowSize> kNeutralFactionStanceRow = [] {
    std::array<FactionStance, kFactionStanceRowSize> row{};
    row.fill(FactionStance::Neutral);
    return row;
}();

#endif // FACTION_STANCE_HPP
