/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef NPC_SPAWN_HPP
#define NPC_SPAWN_HPP

#include "entities/EntityHandle.hpp"
#include "managers/EntityDataTypes.hpp"
#include "utils/Vector2D.hpp"
#include <string>

namespace VoidLight {

/**
 * @brief Create an NPC and optionally override the factory suggestedBehavior.
 *
 * createNPCWithRaceClass always auto-registers classes.json suggestedBehavior.
 * Empty behaviorOverride leaves that assignment (no second assign).
 * Non-empty behaviorOverride calls assignBehavior after create, which writes
 * both homeRole and the current behaviorType.
 */
[[nodiscard]] EntityHandle spawnNpc(const Vector2D& position,
                                    const std::string& race,
                                    const std::string& charClass,
                                    Sex sex = Sex::Unknown,
                                    uint8_t factionOverride = 0xFF,
                                    const std::string& behaviorOverride = {});

}

#endif
