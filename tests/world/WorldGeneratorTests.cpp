/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE WorldGeneratorTests
#include <boost/test/unit_test.hpp>

#include "world/WorldGenerator.hpp"
#include "world/WorldData.hpp"
#include <memory>

using namespace VoidLight;

BOOST_AUTO_TEST_SUITE(WorldGeneratorTestSuite)

BOOST_AUTO_TEST_CASE(TestBasicWorldGeneration) {
    WorldGenerationConfig config;
    config.width = 50;
    config.height = 50;
    config.seed = 12345;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.15f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    auto world = WorldGenerator::generateWorld(config);
    
    BOOST_REQUIRE(world != nullptr);
    BOOST_CHECK_EQUAL(world->grid.size(), 50);
    BOOST_CHECK_EQUAL(world->grid[0].size(), 50);
    BOOST_CHECK(!world->worldId.empty());
}

BOOST_AUTO_TEST_CASE(TestDeterministicGeneration) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 54321;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    auto world1 = WorldGenerator::generateWorld(config);
    auto world2 = WorldGenerator::generateWorld(config);
    
    BOOST_REQUIRE(world1 != nullptr);
    BOOST_REQUIRE(world2 != nullptr);
    
    // Same seed should produce identical worlds
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            BOOST_CHECK_EQUAL(static_cast<int>(world1->grid[y][x].biome), 
                             static_cast<int>(world2->grid[y][x].biome));
            BOOST_CHECK_EQUAL(static_cast<int>(world1->grid[y][x].obstacleType), 
                             static_cast<int>(world2->grid[y][x].obstacleType));
            BOOST_CHECK_EQUAL(world1->grid[y][x].isWater, world2->grid[y][x].isWater);
            BOOST_CHECK_CLOSE(world1->grid[y][x].elevation, world2->grid[y][x].elevation, 0.001f);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestBiomeDistribution) {
    WorldGenerationConfig config;
    config.width = 100;
    config.height = 100;
    config.seed = 98765;
    config.elevationFrequency = 0.05f;
    config.humidityFrequency = 0.05f;
    config.waterLevel = 0.2f;
    config.mountainLevel = 0.8f;
    
    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);
    
    int biomeCount[static_cast<int>(Biome::OCEAN) + 1] = {0};
    int waterTileCount = 0;
    
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const Tile& tile = world->grid[y][x];
            biomeCount[static_cast<int>(tile.biome)]++;
            if (tile.isWater) {
                waterTileCount++;
            }
        }
    }
    
    // Verify that we have various biomes
    int biomesPresent = 0;
    for (int i = 0; i <= static_cast<int>(Biome::OCEAN); ++i) {
        if (biomeCount[i] > 0) {
            biomesPresent++;
        }
    }
    
    BOOST_CHECK_GE(biomesPresent, 3); // At least 3 different biomes
    BOOST_CHECK_GT(waterTileCount, 0); // Some water tiles should exist
}

BOOST_AUTO_TEST_CASE(TestObstaclePlacement) {
    WorldGenerationConfig config;
    config.width = 50;
    config.height = 50;
    config.seed = 11111;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.1f; // Low water level to have more land
    config.mountainLevel = 0.9f; // High mountain level
    
    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);
    
    int obstacleCount = 0;
    int waterObstacleCount = 0;
    
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const Tile& tile = world->grid[y][x];
            
            if (tile.obstacleType != ObstacleType::NONE) {
                obstacleCount++;
                
                // No obstacles should be in water
                if (tile.isWater) {
                    waterObstacleCount++;
                }
            }
        }
    }
    
    BOOST_CHECK_GT(obstacleCount, 0); // Should have some obstacles
    BOOST_CHECK_EQUAL(waterObstacleCount, 0); // No obstacles in water
}

BOOST_AUTO_TEST_CASE(TestWaterConsistency) {
    WorldGenerationConfig config;
    config.width = 30;
    config.height = 30;
    config.seed = 99999;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.4f;
    config.mountainLevel = 0.8f;

    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);

    // Check water tile consistency
    // Note: Rivers have isWater=true but preserve original biome for decorations
    // Only ocean tiles have both isWater=true AND biome=OCEAN
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const Tile& tile = world->grid[y][x];

            // Water tiles should have no blocking obstacles
            if (tile.isWater) {
                BOOST_CHECK_EQUAL(tile.obstacleType, ObstacleType::NONE);
            }

            // OCEAN biome tiles must be water (but water isn't always OCEAN - rivers keep original biome)
            if (tile.biome == Biome::OCEAN) {
                BOOST_CHECK(tile.isWater);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(TestElevationRange) {
    WorldGenerationConfig config;
    config.width = 25;
    config.height = 25;
    config.seed = 42424;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);
    
    float minElevation = 1.0f;
    float maxElevation = 0.0f;
    
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            float elevation = world->grid[y][x].elevation;
            minElevation = std::min(minElevation, elevation);
            maxElevation = std::max(maxElevation, elevation);
            
            // Elevation should be normalized to [0, 1]
            BOOST_CHECK_GE(elevation, 0.0f);
            BOOST_CHECK_LE(elevation, 1.0f);
        }
    }
    
    // Should have some variation in elevation
    BOOST_CHECK_LT(minElevation, maxElevation);
    BOOST_CHECK_GT(maxElevation - minElevation, 0.1f);
}

BOOST_AUTO_TEST_CASE(TestSmallWorld) {
    WorldGenerationConfig config;
    config.width = 5;
    config.height = 5;
    config.seed = 1;
    config.elevationFrequency = 0.2f;
    config.humidityFrequency = 0.2f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    auto world = WorldGenerator::generateWorld(config);
    
    BOOST_REQUIRE(world != nullptr);
    BOOST_CHECK_EQUAL(world->grid.size(), 5);
    BOOST_CHECK_EQUAL(world->grid[0].size(), 5);
    
    // Even small worlds should be valid
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const Tile& tile = world->grid[y][x];
            BOOST_CHECK_GE(tile.elevation, 0.0f);
            BOOST_CHECK_LE(tile.elevation, 1.0f);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestLargeWorld) {
    WorldGenerationConfig config;
    config.width = 200;
    config.height = 200;
    config.seed = 77777;
    config.elevationFrequency = 0.02f;
    config.humidityFrequency = 0.03f;
    config.waterLevel = 0.25f;
    config.mountainLevel = 0.75f;

    auto world = WorldGenerator::generateWorld(config);

    BOOST_REQUIRE(world != nullptr);
    BOOST_CHECK_EQUAL(world->grid.size(), 200);
    BOOST_CHECK_EQUAL(world->grid[0].size(), 200);

    // Large worlds should still have proper biome distribution
    int biomeCount[static_cast<int>(Biome::OCEAN) + 1] = {0};

    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 200; ++x) {
            biomeCount[static_cast<int>(world->grid[y][x].biome)]++;
        }
    }

    // Large worlds should have all major biomes represented
    BOOST_CHECK_GT(biomeCount[static_cast<int>(Biome::FOREST)], 0);
    BOOST_CHECK_GT(biomeCount[static_cast<int>(Biome::PLAINS)], 0);
    BOOST_CHECK_GT(biomeCount[static_cast<int>(Biome::DESERT)], 0);
    BOOST_CHECK_GT(biomeCount[static_cast<int>(Biome::MOUNTAIN)], 0);
}

BOOST_AUTO_TEST_CASE(TestBuildingGeneration) {
    WorldGenerationConfig config;
    config.width = 100;
    config.height = 100;
    config.seed = 55555;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.2f; // Low water for more land
    config.mountainLevel = 0.9f; // High mountain threshold to have more forest

    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);

    // Find all buildings and validate their structure
    std::map<uint32_t, std::vector<std::pair<int, int>>> buildingTiles;
    int buildingTileCount = 0;

    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const Tile& tile = world->grid[y][x];

            if (tile.obstacleType == ObstacleType::BUILDING && tile.buildingId > 0) {
                buildingTiles[tile.buildingId].push_back({x, y});
                buildingTileCount++;
            }
        }
    }

    // Validate each building
    for (const auto& [buildingId, tiles] : buildingTiles) {
        // Buildings should be 2x2 (4 tiles) or multiples thereof for connected buildings
        BOOST_CHECK_GE(tiles.size(), 4);
        BOOST_CHECK_EQUAL(tiles.size() % 4, 0); // Should be multiple of 4

        // Verify all tiles have same buildingId
        for (const auto& [x, y] : tiles) {
            BOOST_CHECK_EQUAL(world->grid[y][x].buildingId, buildingId);
            BOOST_CHECK_EQUAL(world->grid[y][x].obstacleType, ObstacleType::BUILDING);
        }

        // Verify tiles form contiguous blocks (2x2)
        // Find bounding box
        int minX = tiles[0].first, maxX = tiles[0].first;
        int minY = tiles[0].second, maxY = tiles[0].second;

        for (const auto& [x, y] : tiles) {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }

        // Building dimensions should be at least 2x2
        BOOST_CHECK_GE(maxX - minX + 1, 2);
        BOOST_CHECK_GE(maxY - minY + 1, 2);

        // Verify no partial buildings (incomplete 2x2 blocks)
        // Each building should have a complete top-left tile that starts a 2x2 block
        bool hasValidTopLeft = false;
        for (const auto& [x, y] : tiles) {
            // Check if this is a top-left of a 2x2 block
            if (x + 1 <= maxX && y + 1 <= maxY) {
                // Check if the 2x2 block exists
                bool is2x2 = true;
                for (int dy = 0; dy < 2 && is2x2; ++dy) {
                    for (int dx = 0; dx < 2 && is2x2; ++dx) {
                        if (x + dx >= config.width || y + dy >= config.height) {
                            is2x2 = false;
                            break;
                        }
                        const Tile& checkTile = world->grid[y + dy][x + dx];
                        if (checkTile.buildingId != buildingId ||
                            checkTile.obstacleType != ObstacleType::BUILDING) {
                            is2x2 = false;
                        }
                    }
                }
                if (is2x2) {
                    hasValidTopLeft = true;
                    break;
                }
            }
        }

        BOOST_CHECK_MESSAGE(hasValidTopLeft,
            "Building " + std::to_string(buildingId) + " does not have a valid 2x2 structure");
    }

    // Should have at least some buildings in a 100x100 world
    BOOST_CHECK_GT(buildingTiles.size(), 0);
    BOOST_CHECK_GT(buildingTileCount, 0);
}

BOOST_AUTO_TEST_CASE(TestSettlementRecordsPersisted) {
    WorldGenerationConfig config;
    config.width = 100;
    config.height = 100;
    config.seed = 55555;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.2f;
    config.mountainLevel = 0.9f;

    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);
    BOOST_REQUIRE(!world->settlements.empty());

    for (size_t i = 0; i < world->settlements.size(); ++i) {
        const SettlementRecord& record = world->settlements[i];
        BOOST_CHECK_EQUAL(record.id, static_cast<uint32_t>(i + 1));
        BOOST_CHECK_EQUAL(record.radiusTiles, 12);
        BOOST_CHECK_EQUAL(record.faction, 0);
        BOOST_CHECK_GT(record.buildingCount, 0);
        BOOST_REQUIRE(record.centerTileY >= 0);
        BOOST_REQUIRE(record.centerTileX >= 0);
        BOOST_REQUIRE(static_cast<size_t>(record.centerTileY) < world->grid.size());
        BOOST_REQUIRE(static_cast<size_t>(record.centerTileX) <
                      world->grid[static_cast<size_t>(record.centerTileY)].size());
        BOOST_CHECK(record.biome ==
                    world->grid[static_cast<size_t>(record.centerTileY)]
                               [static_cast<size_t>(record.centerTileX)].biome);
    }
}

BOOST_AUTO_TEST_CASE(TestBuildingNoOverlap) {
    WorldGenerationConfig config;
    config.width = 50;
    config.height = 50;
    config.seed = 88888;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.1f;
    config.mountainLevel = 0.9f;

    auto world = WorldGenerator::generateWorld(config);
    BOOST_REQUIRE(world != nullptr);

    // Verify that no buildings overlap with water or other obstacles
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const Tile& tile = world->grid[y][x];

            if (tile.obstacleType == ObstacleType::BUILDING) {
                // Buildings should not be on water
                BOOST_CHECK_MESSAGE(!tile.isWater,
                    "Building at (" + std::to_string(x) + ", " + std::to_string(y) + ") is on water");

                // Buildings should not be on mountain or ocean biomes
                BOOST_CHECK_NE(tile.biome, Biome::MOUNTAIN);
                BOOST_CHECK_NE(tile.biome, Biome::OCEAN);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
