/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "world/WorldPopulation.hpp"
#include "core/Logger.hpp"
#include "world/NpcSpawn.hpp"
#include "world/WorldData.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace VoidLight {
namespace {

[[nodiscard]] bool inBounds(int x, int y, int width, int height) noexcept
{
    return x >= 0 && y >= 0 && x < width && y < height;
}

[[nodiscard]] size_t tileIndex(int x, int y, int width) noexcept
{
    return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
}

[[nodiscard]] bool isUsed(const std::vector<uint8_t>& used, int x, int y, int width) noexcept
{
    return used[tileIndex(x, y, width)] != 0;
}

void markUsed(std::vector<uint8_t>& used, int x, int y, int width)
{
    used[tileIndex(x, y, width)] = 1;
}

[[nodiscard]] bool isWalkable(const Tile& tile) noexcept
{
    return !tile.isWater && tile.obstacleType == ObstacleType::NONE && tile.buildingId == 0;
}

[[nodiscard]] bool isInsideAnySettlement(const WorldData& world, int tileX, int tileY) noexcept
{
    for (const auto& settlement : world.settlements)
    {
        if (settlement.containsTile(tileX, tileY))
        {
            return true;
        }
    }
    return false;
}

template <typename Pred>
[[nodiscard]] bool findInRings(const WorldData& world,
                               int cx,
                               int cy,
                               int maxRing,
                               int width,
                               int height,
                               const std::vector<uint8_t>& used,
                               Pred pred,
                               int& outX,
                               int& outY)
{
    for (int ring = 0; ring <= maxRing; ++ring)
    {
        const int minY = cy - ring;
        const int maxY = cy + ring;
        const int minX = cx - ring;
        const int maxX = cx + ring;
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (ring > 0 && x > minX && x < maxX && y > minY && y < maxY)
                {
                    continue;
                }
                if (!inBounds(x, y, width, height) || isUsed(used, x, y, width))
                {
                    continue;
                }
                if (pred(world.grid[static_cast<size_t>(y)][static_cast<size_t>(x)], x, y))
                {
                    outX = x;
                    outY = y;
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] Vector2D tileCenterPixels(int tileX, int tileY) noexcept
{
    return Vector2D(static_cast<float>(tileX) * TILE_SIZE + TILE_SIZE * 0.5f,
                    static_cast<float>(tileY) * TILE_SIZE + TILE_SIZE * 0.5f);
}

bool trySpawnNpc(std::vector<EntityHandle>& outHandles,
                 std::vector<uint8_t>& used,
                 int width,
                 int tileX,
                 int tileY,
                 const std::string& race,
                 const std::string& charClass,
                 uint8_t factionOverride,
                 const std::string& behaviorOverride = {})
{
    if (outHandles.size() >= WorldPopulation::MAX_POPULATED_NPCS_PER_WORLD)
    {
        return false;
    }

    EntityHandle handle = spawnNpc(tileCenterPixels(tileX, tileY), race, charClass,
                                   Sex::Unknown, factionOverride, behaviorOverride);
    if (!handle.isValid())
    {
        return false;
    }

    markUsed(used, tileX, tileY, width);
    outHandles.push_back(handle);
    return true;
}

} // namespace

void WorldPopulation::populate(const WorldData& world, std::vector<EntityHandle>& outHandles)
{
    const int height = static_cast<int>(world.grid.size());
    const int width = height > 0 ? static_cast<int>(world.grid[0].size()) : 0;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    std::vector<uint8_t> used(static_cast<size_t>(width) * static_cast<size_t>(height), 0);

    for (const auto& settlement : world.settlements)
    {
        if (outHandles.size() >= MAX_POPULATED_NPCS_PER_WORLD)
        {
            return;
        }

        const int cx = settlement.centerTileX;
        const int cy = settlement.centerTileY;
        const int radius = settlement.radiusTiles;

        int tileX = 0;
        int tileY = 0;
        const auto walkablePred = [](const Tile& tile, int, int) {
            return isWalkable(tile);
        };
        if (findInRings(world, cx, cy, radius, width, height, used, walkablePred, tileX, tileY))
        {
            trySpawnNpc(outHandles, used, width, tileX, tileY, "Human", "GeneralMerchant",
                        0xFF);
        }

        for (int guardIndex = 0; guardIndex < 2; ++guardIndex)
        {
            if (outHandles.size() >= MAX_POPULATED_NPCS_PER_WORLD)
            {
                return;
            }

            const auto buildingPred = [&settlement](const Tile& tile, int x, int y) {
                return tile.isTopLeftOfBuilding && settlement.containsTile(x, y);
            };
            const auto walkableInRadiusPred = [&settlement](const Tile& tile, int x, int y) {
                return isWalkable(tile) && settlement.containsTile(x, y);
            };

            if (findInRings(world, cx, cy, radius, width, height, used, buildingPred, tileX, tileY) ||
                findInRings(world, cx, cy, radius, width, height, used, walkableInRadiusPred,
                            tileX, tileY))
            {
                trySpawnNpc(outHandles, used, width, tileX, tileY, "Human", "Guard", 0xFF);
            }
        }

        for (int villagerIndex = 0; villagerIndex < 4; ++villagerIndex)
        {
            if (outHandles.size() >= MAX_POPULATED_NPCS_PER_WORLD)
            {
                return;
            }

            const auto villagerPred = [&settlement](const Tile& tile, int x, int y) {
                return isWalkable(tile) && settlement.containsTile(x, y);
            };
            if (findInRings(world, cx, cy, radius, width, height, used, villagerPred, tileX, tileY))
            {
                trySpawnNpc(outHandles, used, width, tileX, tileY, "Human", "Villager", 0xFF);
            }
        }
    }

    uint32_t wildernessSpawned = 0;
    for (int blockY = 0; blockY < height; blockY += WILDERNESS_BLOCK_TILES)
    {
        for (int blockX = 0; blockX < width; blockX += WILDERNESS_BLOCK_TILES)
        {
            if (wildernessSpawned >= MAX_WILDERNESS_NPCS_PER_WORLD ||
                outHandles.size() >= MAX_POPULATED_NPCS_PER_WORLD)
            {
                return;
            }

            const int blockMaxY = std::min(blockY + WILDERNESS_BLOCK_TILES, height);
            const int blockMaxX = std::min(blockX + WILDERNESS_BLOCK_TILES, width);
            bool spawned = false;
            for (int y = blockY; y < blockMaxY && !spawned; ++y)
            {
                for (int x = blockX; x < blockMaxX; ++x)
                {
                    if (isUsed(used, x, y, width) || isInsideAnySettlement(world, x, y))
                    {
                        continue;
                    }
                    const Tile& tile = world.grid[static_cast<size_t>(y)][static_cast<size_t>(x)];
                    if (!isWalkable(tile) ||
                        (tile.biome != Biome::FOREST && tile.biome != Biome::HAUNTED))
                    {
                        continue;
                    }
                    if (trySpawnNpc(outHandles, used, width, x, y, "Human", "Warrior", 1))
                    {
                        ++wildernessSpawned;
                        spawned = true;
                        break;
                    }
                }
            }
        }
    }

    WORLD_MANAGER_DEBUG(std::format(
        "WorldPopulation: spawned {} NPCs ({} wilderness) for world {}",
        outHandles.size(), wildernessSpawned, world.worldId));
}

}
