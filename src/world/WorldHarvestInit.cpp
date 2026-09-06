/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "world/WorldHarvestInit.hpp"
#include "core/Logger.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "utils/Vector2D.hpp"
#include "world/HarvestConfig.hpp"
#include "world/WorldData.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <string_view>

namespace VoidLight {

void WorldHarvestInit::initialize(const WorldData& world)
{
    if (world.grid.empty()) {
        WORLD_MANAGER_WARN("Cannot initialize resources - no world loaded");
        return;
    }

    WORLD_MANAGER_INFO(std::format("Initializing world resources for world: {}",
                                   world.worldId));

    const auto& resourceMgr = ResourceTemplateManager::Instance();

    int totalTiles = 0;
    int forestTiles = 0;
    int mountainTiles = 0;
    int swampTiles = 0;
    int celestialTiles = 0;
    int highElevationTiles = 0;

    for (const auto& row : world.grid) {
        for (const auto& tile : row) {
            if (!tile.isWater) {
                ++totalTiles;

                switch (tile.biome) {
                case Biome::FOREST:
                    ++forestTiles;
                    break;
                case Biome::MOUNTAIN:
                    ++mountainTiles;
                    break;
                case Biome::SWAMP:
                    ++swampTiles;
                    break;
                case Biome::CELESTIAL:
                    ++celestialTiles;
                    break;
                default:
                    break;
                }

                if (tile.elevation > 0.7f) {
                    ++highElevationTiles;
                }
            }
        }
    }

    if (totalTiles == 0) {
        WORLD_MANAGER_WARN("No land tiles found for resource initialization");
        return;
    }

    try {
        auto& edm = EntityDataManager::Instance();
        const std::string& worldId = world.worldId;

        auto spawnHarvestablesAtObstacles = [&](std::string_view resourceId,
                                                ResourceHandle handle,
                                                ObstacleType targetObstacle,
                                                int yieldMin, int yieldMax,
                                                float respawnTime) {
            if (!handle.isValid()) {
                WORLD_MANAGER_ERROR(std::format(
                    "Invalid resource handle for obstacle type {}",
                    obstacleTypeToString(targetObstacle)));
                return;
            }

            int spawned = 0;
            const size_t gridHeight = world.grid.size();
            if (gridHeight == 0) {
                return;
            }
            const size_t gridWidth = world.grid[0].size();
            const auto harvestType = getHarvestTypeForResource(resourceId);

            for (size_t y = 0; y < gridHeight; ++y) {
                for (size_t x = 0; x < gridWidth; ++x) {
                    const auto& tile = world.grid[y][x];
                    if (tile.obstacleType != targetObstacle) {
                        continue;
                    }

                    Vector2D pos(static_cast<float>(x) * TILE_SIZE + TILE_SIZE * 0.5f,
                                 static_cast<float>(y) * TILE_SIZE + TILE_SIZE * 0.5f);

                    EntityHandle h = edm.createHarvestable(pos, handle, yieldMin, yieldMax,
                                                           respawnTime, worldId, harvestType);
                    if (h.isValid()) {
                        ++spawned;
                    }
                }
            }
            WORLD_MANAGER_INFO(std::format("Spawned {} harvestables of {} at {} obstacles",
                                           spawned, handle.toString(),
                                           obstacleTypeToString(targetObstacle)));
        };

        auto spawnHarvestablesInBiome = [&](std::string_view resourceId,
                                            ResourceHandle handle,
                                            Biome targetBiome,
                                            int count, int yieldMin, int yieldMax,
                                            float respawnTime) {
            if (!handle.isValid()) {
                WORLD_MANAGER_ERROR(std::format("Invalid resource handle for biome {}",
                                                biomeToString(targetBiome)));
                return;
            }
            if (count <= 0) {
                return;
            }

            int spawned = 0;
            const size_t gridHeight = world.grid.size();
            if (gridHeight == 0) {
                return;
            }
            const size_t gridWidth = world.grid[0].size();
            const auto harvestType = getHarvestTypeForResource(resourceId);

            for (size_t y = 0; y < gridHeight && spawned < count; ++y) {
                for (size_t x = 0; x < gridWidth && spawned < count; ++x) {
                    const auto& tile = world.grid[y][x];
                    if (tile.isWater) {
                        continue;
                    }
                    if (tile.biome != targetBiome) {
                        continue;
                    }
                    if (tile.obstacleType != ObstacleType::NONE) {
                        continue;
                    }
                    if ((x + y * 7) % 10 != 0) {
                        continue;
                    }

                    Vector2D pos(static_cast<float>(x) * TILE_SIZE + TILE_SIZE * 0.5f,
                                 static_cast<float>(y) * TILE_SIZE + TILE_SIZE * 0.5f);

                    EntityHandle h = edm.createHarvestable(pos, handle, yieldMin, yieldMax,
                                                           respawnTime, worldId, harvestType);
                    if (h.isValid()) {
                        ++spawned;
                    }
                }
            }
            WORLD_MANAGER_INFO(std::format(
                "Spawned {} harvestables of type {} ({}) in {} biome",
                spawned, handle.toString(), harvestTypeToString(harvestType),
                biomeToString(targetBiome)));
        };

        auto spawnHarvestablesAtElevation = [&](std::string_view resourceId,
                                                ResourceHandle handle,
                                                float minElevation, int count,
                                                int yieldMin, int yieldMax,
                                                float respawnTime) {
            if (!handle.isValid()) {
                WORLD_MANAGER_ERROR(std::format(
                    "Invalid resource handle for elevation >= {}", minElevation));
                return;
            }
            if (count <= 0) {
                return;
            }

            int spawned = 0;
            const size_t gridHeight = world.grid.size();
            if (gridHeight == 0) {
                return;
            }
            const size_t gridWidth = world.grid[0].size();
            const auto harvestType = getHarvestTypeForResource(resourceId);

            for (size_t y = 0; y < gridHeight && spawned < count; ++y) {
                for (size_t x = 0; x < gridWidth && spawned < count; ++x) {
                    const auto& tile = world.grid[y][x];
                    if (tile.isWater || tile.elevation < minElevation) {
                        continue;
                    }
                    if (tile.obstacleType != ObstacleType::NONE) {
                        continue;
                    }
                    if ((x + y * 11) % 12 != 0) {
                        continue;
                    }

                    Vector2D pos(static_cast<float>(x) * TILE_SIZE + TILE_SIZE * 0.5f,
                                 static_cast<float>(y) * TILE_SIZE + TILE_SIZE * 0.5f);

                    EntityHandle h = edm.createHarvestable(pos, handle, yieldMin, yieldMax,
                                                           respawnTime, worldId, harvestType);
                    if (h.isValid()) {
                        ++spawned;
                    }
                }
            }
            WORLD_MANAGER_INFO(std::format(
                "Spawned {} high-elevation harvestables of type {} ({})",
                spawned, handle.toString(), harvestTypeToString(harvestType)));
        };

        auto woodHandle = resourceMgr.getHandleById("wood");
        spawnHarvestablesAtObstacles("wood", woodHandle, ObstacleType::TREE, 1, 3, 60.0f);

        auto stoneHandle = resourceMgr.getHandleById("stone");
        spawnHarvestablesAtObstacles("stone", stoneHandle, ObstacleType::ROCK, 1, 3, 90.0f);

        auto ironHandle = resourceMgr.getHandleById("iron_ore");
        spawnHarvestablesAtObstacles("iron_ore", ironHandle, ObstacleType::IRON_DEPOSIT, 2, 5, 90.0f);

        auto goldHandle = resourceMgr.getHandleById("gold_ore");
        spawnHarvestablesAtObstacles("gold_ore", goldHandle, ObstacleType::GOLD_DEPOSIT, 1, 3, 150.0f);

        auto coalHandle = resourceMgr.getHandleById("coal");
        spawnHarvestablesAtObstacles("coal", coalHandle, ObstacleType::COAL_DEPOSIT, 3, 6, 75.0f);

        auto copperHandle = resourceMgr.getHandleById("copper_ore");
        spawnHarvestablesAtObstacles("copper_ore", copperHandle, ObstacleType::COPPER_DEPOSIT, 2, 4, 60.0f);

        auto mithrilHandle = resourceMgr.getHandleById("mithril_ore");
        spawnHarvestablesAtObstacles("mithril_ore", mithrilHandle, ObstacleType::MITHRIL_DEPOSIT, 1, 2, 300.0f);

        auto limestoneHandle = resourceMgr.getHandleById("limestone");
        spawnHarvestablesAtObstacles("limestone", limestoneHandle, ObstacleType::LIMESTONE_DEPOSIT, 2, 4, 120.0f);

        auto emeraldHandle = resourceMgr.getHandleById("rough_emerald");
        spawnHarvestablesAtObstacles("rough_emerald", emeraldHandle, ObstacleType::EMERALD_DEPOSIT, 1, 2, 180.0f);

        auto rubyHandle = resourceMgr.getHandleById("rough_ruby");
        spawnHarvestablesAtObstacles("rough_ruby", rubyHandle, ObstacleType::RUBY_DEPOSIT, 1, 2, 180.0f);

        auto sapphireHandle = resourceMgr.getHandleById("rough_sapphire");
        spawnHarvestablesAtObstacles("rough_sapphire", sapphireHandle, ObstacleType::SAPPHIRE_DEPOSIT, 1, 2, 180.0f);

        auto diamondHandle = resourceMgr.getHandleById("rough_diamond");
        spawnHarvestablesAtObstacles("rough_diamond", diamondHandle, ObstacleType::DIAMOND_DEPOSIT, 1, 1, 360.0f);

        if (forestTiles > 0) {
            auto enchantedWoodHandle = resourceMgr.getHandleById("enchanted_wood");
            spawnHarvestablesInBiome("enchanted_wood", enchantedWoodHandle, Biome::FOREST,
                                     std::max(1, forestTiles / 40), 1, 2, 120.0f);
        }

        if (celestialTiles > 0) {
            auto crystalHandle = resourceMgr.getHandleById("crystal_essence");
            spawnHarvestablesInBiome("crystal_essence", crystalHandle, Biome::CELESTIAL,
                                     std::max(1, celestialTiles / 30), 1, 2, 150.0f);
        }

        if (swampTiles > 0) {
            auto voidSilkHandle = resourceMgr.getHandleById("void_silk");
            spawnHarvestablesInBiome("void_silk", voidSilkHandle, Biome::SWAMP,
                                     std::max(1, swampTiles / 60), 1, 1, 200.0f);
        }

        if (mountainTiles > 0) {
            auto mountainStoneHandle = resourceMgr.getHandleById("stone");
            spawnHarvestablesInBiome("stone", mountainStoneHandle, Biome::MOUNTAIN,
                                     std::max(1, mountainTiles / 25), 2, 5, 90.0f);
        }

        if (highElevationTiles > 0) {
            auto enchantedStoneHandle = resourceMgr.getHandleById("enchanted_stone");
            spawnHarvestablesAtElevation("enchanted_stone", enchantedStoneHandle, 0.7f,
                                         std::max(1, highElevationTiles / 30),
                                         1, 3, 90.0f);
        }

        WORLD_MANAGER_INFO(std::format(
            "World harvestable initialization completed for {} ({} tiles processed)",
            world.worldId, totalTiles));

    } catch (const std::exception& ex) {
        WORLD_MANAGER_ERROR(std::format(
            "Error during world resource initialization: {}", ex.what()));
    }
}

}
