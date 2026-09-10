/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef WORLD_POPULATION_HPP
#define WORLD_POPULATION_HPP

#include "entities/EntityHandle.hpp"
#include <cstdint>
#include <vector>

namespace VoidLight {

struct WorldData;

struct WorldPopulation {
    static constexpr uint32_t MAX_POPULATED_NPCS_PER_WORLD = 256;
    static constexpr uint32_t MAX_HOSTILES_PER_WORLD = 32;  // Wilderness faction-1 cap (not agro)
    static constexpr int HOSTILE_BLOCK_TILES = 64;
    static void populate(const WorldData& world, std::vector<EntityHandle>& outHandles);
};

}

#endif
