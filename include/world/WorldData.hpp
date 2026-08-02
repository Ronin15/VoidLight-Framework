/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef WORLD_DATA_HPP
#define WORLD_DATA_HPP

#include <vector>
#include <string>
#include <iostream>
#include "utils/ResourceHandle.hpp"

namespace VoidLight {

struct WorldGenerationConfig {
    int width;
    int height;
    int seed;
    float elevationFrequency;
    float humidityFrequency;
    float waterLevel;
    float mountainLevel;
};

// World rendering and spatial constants
constexpr float TILE_SIZE = 32.0f;  // Tile size in pixels

enum class Biome {
    DESERT,
    FOREST,
    PLAINS,     // Open grassland with sparse vegetation
    MOUNTAIN,
    SWAMP,
    HAUNTED,
    CELESTIAL,
    OCEAN
};

enum class ObstacleType {
    NONE,
    ROCK,
    TREE,
    WATER,
    BUILDING,
    // Ore deposits
    IRON_DEPOSIT,
    GOLD_DEPOSIT,
    COPPER_DEPOSIT,
    MITHRIL_DEPOSIT,
    LIMESTONE_DEPOSIT,
    COAL_DEPOSIT,
    // Gem deposits
    EMERALD_DEPOSIT,
    RUBY_DEPOSIT,
    SAPPHIRE_DEPOSIT,
    DIAMOND_DEPOSIT
};

enum class DecorationType : uint8_t {
    NONE = 0,
    FLOWER_BLUE,
    FLOWER_PINK,
    FLOWER_WHITE,
    FLOWER_YELLOW,
    MUSHROOM_PURPLE,
    MUSHROOM_TAN,
    GRASS_SMALL,
    GRASS_LARGE,
    BUSH,
    STUMP_SMALL,
    STUMP_MEDIUM,
    ROCK_SMALL,
    DEAD_LOG_HZ,
    DEAD_LOG_VERTICAL,
    LILY_PAD,
    WATER_FLOWER
};

// Stream operators for test output
inline std::ostream& operator<<(std::ostream& os, const Biome& biome) {
    switch (biome) {
        case Biome::DESERT: return os << "DESERT";
        case Biome::FOREST: return os << "FOREST";
        case Biome::PLAINS: return os << "PLAINS";
        case Biome::MOUNTAIN: return os << "MOUNTAIN";
        case Biome::SWAMP: return os << "SWAMP";
        case Biome::HAUNTED: return os << "HAUNTED";
        case Biome::CELESTIAL: return os << "CELESTIAL";
        case Biome::OCEAN: return os << "OCEAN";
        default: return os << "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, const ObstacleType& obstacle) {
    switch (obstacle) {
        case ObstacleType::NONE: return os << "NONE";
        case ObstacleType::ROCK: return os << "ROCK";
        case ObstacleType::TREE: return os << "TREE";
        case ObstacleType::WATER: return os << "WATER";
        case ObstacleType::BUILDING: return os << "BUILDING";
        // Ore deposits
        case ObstacleType::IRON_DEPOSIT: return os << "IRON_DEPOSIT";
        case ObstacleType::GOLD_DEPOSIT: return os << "GOLD_DEPOSIT";
        case ObstacleType::COPPER_DEPOSIT: return os << "COPPER_DEPOSIT";
        case ObstacleType::MITHRIL_DEPOSIT: return os << "MITHRIL_DEPOSIT";
        case ObstacleType::LIMESTONE_DEPOSIT: return os << "LIMESTONE_DEPOSIT";
        case ObstacleType::COAL_DEPOSIT: return os << "COAL_DEPOSIT";
        // Gem deposits
        case ObstacleType::EMERALD_DEPOSIT: return os << "EMERALD_DEPOSIT";
        case ObstacleType::RUBY_DEPOSIT: return os << "RUBY_DEPOSIT";
        case ObstacleType::SAPPHIRE_DEPOSIT: return os << "SAPPHIRE_DEPOSIT";
        case ObstacleType::DIAMOND_DEPOSIT: return os << "DIAMOND_DEPOSIT";
        default: return os << "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, const DecorationType& decoration) {
    switch (decoration) {
        case DecorationType::NONE: return os << "NONE";
        case DecorationType::FLOWER_BLUE: return os << "FLOWER_BLUE";
        case DecorationType::FLOWER_PINK: return os << "FLOWER_PINK";
        case DecorationType::FLOWER_WHITE: return os << "FLOWER_WHITE";
        case DecorationType::FLOWER_YELLOW: return os << "FLOWER_YELLOW";
        case DecorationType::MUSHROOM_PURPLE: return os << "MUSHROOM_PURPLE";
        case DecorationType::MUSHROOM_TAN: return os << "MUSHROOM_TAN";
        case DecorationType::GRASS_SMALL: return os << "GRASS_SMALL";
        case DecorationType::GRASS_LARGE: return os << "GRASS_LARGE";
        case DecorationType::BUSH: return os << "BUSH";
        case DecorationType::STUMP_SMALL: return os << "STUMP_SMALL";
        case DecorationType::STUMP_MEDIUM: return os << "STUMP_MEDIUM";
        case DecorationType::ROCK_SMALL: return os << "ROCK_SMALL";
        case DecorationType::DEAD_LOG_HZ: return os << "DEAD_LOG_HZ";
        case DecorationType::DEAD_LOG_VERTICAL: return os << "DEAD_LOG_VERTICAL";
        case DecorationType::LILY_PAD: return os << "LILY_PAD";
        case DecorationType::WATER_FLOWER: return os << "WATER_FLOWER";
        default: return os << "UNKNOWN";
    }
}

// String conversion functions for std::format compatibility
[[nodiscard]] constexpr std::string_view biomeToString(Biome biome) noexcept {
    switch (biome) {
        case Biome::DESERT: return "DESERT";
        case Biome::FOREST: return "FOREST";
        case Biome::PLAINS: return "PLAINS";
        case Biome::MOUNTAIN: return "MOUNTAIN";
        case Biome::SWAMP: return "SWAMP";
        case Biome::HAUNTED: return "HAUNTED";
        case Biome::CELESTIAL: return "CELESTIAL";
        case Biome::OCEAN: return "OCEAN";
        default: return "UNKNOWN";
    }
}

[[nodiscard]] constexpr std::string_view obstacleTypeToString(ObstacleType obstacle) noexcept {
    switch (obstacle) {
        case ObstacleType::NONE: return "NONE";
        case ObstacleType::ROCK: return "ROCK";
        case ObstacleType::TREE: return "TREE";
        case ObstacleType::WATER: return "WATER";
        case ObstacleType::BUILDING: return "BUILDING";
        case ObstacleType::IRON_DEPOSIT: return "IRON_DEPOSIT";
        case ObstacleType::GOLD_DEPOSIT: return "GOLD_DEPOSIT";
        case ObstacleType::COPPER_DEPOSIT: return "COPPER_DEPOSIT";
        case ObstacleType::MITHRIL_DEPOSIT: return "MITHRIL_DEPOSIT";
        case ObstacleType::LIMESTONE_DEPOSIT: return "LIMESTONE_DEPOSIT";
        case ObstacleType::COAL_DEPOSIT: return "COAL_DEPOSIT";
        case ObstacleType::EMERALD_DEPOSIT: return "EMERALD_DEPOSIT";
        case ObstacleType::RUBY_DEPOSIT: return "RUBY_DEPOSIT";
        case ObstacleType::SAPPHIRE_DEPOSIT: return "SAPPHIRE_DEPOSIT";
        case ObstacleType::DIAMOND_DEPOSIT: return "DIAMOND_DEPOSIT";
        default: return "UNKNOWN";
    }
}

struct Tile {
    Biome biome = Biome::PLAINS;
    ObstacleType obstacleType = ObstacleType::NONE;
    DecorationType decorationType = DecorationType::NONE;
    float elevation = 0.0f;
    bool isWater = false;
    VoidLight::ResourceHandle resourceHandle;

    // Building support for multi-tile structures
    uint32_t buildingId = 0;        // 0 = no building, >0 = unique building ID
    uint8_t buildingSize = 0;       // 0 = no building, 1-4 = connected building count
    bool isTopLeftOfBuilding = false;  // Pre-computed flag for render optimization

    // Harvestable deposit support (ore, gem deposits)
    // Lazy-created EDM entity index for tile-based deposits
    // UINT32_MAX = no entity created yet
    // TODO: Connect to tile-based deposit system when implemented
    // Currently harvestables are spawned at obstacle positions via WorldManager::spawnHarvestablesAtObstacles
    uint32_t harvestableIndex = UINT32_MAX;
};

struct WorldData {
    std::string worldId;
    std::vector<std::vector<Tile>> grid;
};

}

#endif
