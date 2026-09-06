/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef FACTION_STANCE_HPP
#define FACTION_STANCE_HPP

#include <cstdint>

/**
 * @brief Directed faction relationship used by AIManager's 16×16 stance table.
 *
 * Allied is 0 so default-constructed FactionStance{} is Allied — callers that
 * need a Neutral row (BehaviorContext) must fill Neutral explicitly.
 */
enum class FactionStance : uint8_t {
    Allied = 0,
    Neutral = 1,
    Hostile = 2
};

#endif // FACTION_STANCE_HPP
