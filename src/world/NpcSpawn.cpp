/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "world/NpcSpawn.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"

namespace VoidLight {

EntityHandle spawnNpc(const Vector2D& position,
                      const std::string& race,
                      const std::string& charClass,
                      Sex sex,
                      uint8_t factionOverride,
                      const std::string& behaviorOverride)
{
    EntityHandle handle = EntityDataManager::Instance().createNPCWithRaceClass(
        position, race, charClass, sex, factionOverride);
    if (!handle.isValid() || behaviorOverride.empty()) {
        return handle;
    }

    AIManager::Instance().assignBehavior(handle, behaviorOverride);
    return handle;
}

}
