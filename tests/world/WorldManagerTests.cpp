/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE WorldManagerTests
#include <boost/test/unit_test.hpp>

#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "events/WorldEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "world/WorldData.hpp"
#include "core/Logger.hpp"
#include <memory>
#include <vector>
#include <atomic>

using namespace VoidLight;

class WorldManagerTestFixture {
public:
    WorldManagerTestFixture() {
        // Initialize ResourceTemplateManager first (required for resource creation)
        resourceTemplateManager = &ResourceTemplateManager::Instance();
        BOOST_REQUIRE(resourceTemplateManager != nullptr);
        bool templateInitialized = resourceTemplateManager->init();
        BOOST_REQUIRE(templateInitialized);
        
        // Initialize WorldResourceManager (required dependency)
        worldResourceManager = &WorldResourceManager::Instance();
        BOOST_REQUIRE(worldResourceManager != nullptr);
        bool resourceInitialized = worldResourceManager->init();
        BOOST_REQUIRE(resourceInitialized);
        
        // Initialize WorldManager
        worldManager = &WorldManager::Instance();
        BOOST_REQUIRE(worldManager != nullptr);
        bool managerInitialized = worldManager->init();
        BOOST_REQUIRE(managerInitialized);
    }
    
    ~WorldManagerTestFixture() {
        // Clean up in reverse order
        worldManager->clean();
        worldResourceManager->clean();
        resourceTemplateManager->clean();
    }
    
protected:
    WorldResourceManager* worldResourceManager;
    WorldManager* worldManager;
    ResourceTemplateManager* resourceTemplateManager;
};

BOOST_FIXTURE_TEST_SUITE(WorldManagerTestSuite, WorldManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestSingletonPattern) {
    WorldManager* instance1 = &WorldManager::Instance();
    WorldManager* instance2 = &WorldManager::Instance();
    
    BOOST_CHECK(instance1 == instance2);
    BOOST_CHECK(instance1 == worldManager);
}

BOOST_AUTO_TEST_CASE(TestInitialization) {
    BOOST_CHECK(worldManager->isInitialized());
    BOOST_CHECK(!worldManager->isShutdown());
    BOOST_CHECK(!worldManager->hasActiveWorld());
    BOOST_CHECK(worldManager->getCurrentWorldId().empty());
}

BOOST_AUTO_TEST_CASE(TestLoadNewWorld) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 12345;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    bool loadResult = worldManager->loadNewWorld(config);
    
    BOOST_CHECK(loadResult);
    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK(!worldManager->getCurrentWorldId().empty());
    
    worldManager->withWorldDataRead([&](const VoidLight::WorldData* worldData) {
        BOOST_REQUIRE(worldData != nullptr);
        BOOST_CHECK_EQUAL(worldData->grid.size(), 20);
        BOOST_CHECK_EQUAL(worldData->grid[0].size(), 20);
    });
}

BOOST_AUTO_TEST_CASE(TestUnloadWorldRemovesWRMState) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 67890;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    const std::string worldId = worldManager->getCurrentWorldId();
    BOOST_REQUIRE(!worldId.empty());
    BOOST_REQUIRE(worldResourceManager->hasWorld(worldId));

    worldManager->unloadWorld();

    BOOST_CHECK(!worldManager->hasActiveWorld());
    BOOST_CHECK(!worldResourceManager->hasWorld(worldId));
    BOOST_CHECK(worldResourceManager->getActiveWorld().empty());
}

BOOST_AUTO_TEST_CASE(TestWorldUnloadedHandlerCanQueryWorldManager) {
    BOOST_REQUIRE(EventManager::Instance().init());

    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 24680;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    bool sawUnload = false;
    bool queriedWorldManager = false;
    const auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::World,
        [&](const EventData& data) {
            if (!std::dynamic_pointer_cast<WorldUnloadedEvent>(data.event)) {
                return;
            }

            sawUnload = true;

            int width = -1;
            int height = -1;
            queriedWorldManager = !WorldManager::Instance().getWorldDimensions(width, height);
            BOOST_CHECK(!WorldManager::Instance().hasActiveWorld());
        });

    worldManager->unloadWorld();

    BOOST_CHECK(sawUnload);
    BOOST_CHECK(queriedWorldManager);

    EventManager::Instance().removeHandler(token);
    EventManager::Instance().clean();
}

BOOST_AUTO_TEST_CASE(TestWorldReplacementUnloadsBeforeActivatingNewWorld) {
    BOOST_REQUIRE(EventManager::Instance().init());

    WorldGenerationConfig config1;
    config1.width = 20;
    config1.height = 20;
    config1.seed = 13579;
    config1.elevationFrequency = 0.1f;
    config1.humidityFrequency = 0.1f;
    config1.waterLevel = 0.3f;
    config1.mountainLevel = 0.7f;

    WorldGenerationConfig config2 = config1;
    config2.seed = 97531;

    BOOST_REQUIRE(worldManager->loadNewWorld(config1));
    const std::string firstWorldId = worldManager->getCurrentWorldId();
    BOOST_REQUIRE(!firstWorldId.empty());

    bool sawUnload = false;
    bool hadNoReplacementWorldDuringUnload = false;
    bool hadNoActiveResourceWorldDuringUnload = false;
    const auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::World,
        [&](const EventData& data) {
            const auto event =
                std::dynamic_pointer_cast<WorldUnloadedEvent>(data.event);
            if (!event || event->getWorldId() != firstWorldId) {
                return;
            }

            sawUnload = true;
            hadNoReplacementWorldDuringUnload =
                !WorldManager::Instance().hasActiveWorld();
            hadNoActiveResourceWorldDuringUnload =
                WorldResourceManager::Instance().getActiveWorld().empty();
        });

    BOOST_REQUIRE(worldManager->loadNewWorld(config2));
    BOOST_CHECK(sawUnload);
    BOOST_CHECK(hadNoReplacementWorldDuringUnload);
    BOOST_CHECK(hadNoActiveResourceWorldDuringUnload);
    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK_NE(firstWorldId, worldManager->getCurrentWorldId());

    EventManager::Instance().removeHandler(token);
    EventManager::Instance().clean();
}

BOOST_AUTO_TEST_CASE(TestHarvestablesUseConfiguredHarvestTypes) {
    WorldGenerationConfig config;
    config.width = 40;
    config.height = 40;
    config.seed = 12345;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.2f;
    config.mountainLevel = 0.6f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    const auto& edm = EntityDataManager::Instance();
    std::vector<size_t> nearbyHarvestables;

    bool foundTree = false;
    worldManager->withWorldDataRead([&](const VoidLight::WorldData* worldData) {
        BOOST_REQUIRE(worldData != nullptr);
        for (size_t y = 0; y < worldData->grid.size() && !foundTree; ++y) {
            for (size_t x = 0; x < worldData->grid[y].size() && !foundTree; ++x) {
                if (worldData->grid[y][x].obstacleType != ObstacleType::TREE) {
                    continue;
                }

                Vector2D pos(static_cast<float>(x) * TILE_SIZE + TILE_SIZE * 0.5f,
                             static_cast<float>(y) * TILE_SIZE + TILE_SIZE * 0.5f);
                nearbyHarvestables.clear();
                worldResourceManager->queryHarvestablesInRadius(pos, 8.0f, nearbyHarvestables);
                BOOST_REQUIRE_MESSAGE(!nearbyHarvestables.empty(), "Expected harvestable at tree obstacle");

                const auto handle = edm.getStaticHandle(nearbyHarvestables.front());
                const auto& harvestable = edm.getHarvestableData(handle);
                BOOST_CHECK(harvestable.harvestType == HarvestType::Chopping);
                foundTree = true;
            }
        }
    });
    BOOST_REQUIRE_MESSAGE(foundTree, "Expected at least one tree obstacle in generated world");

    bool foundIronDeposit = false;
    worldManager->withWorldDataRead([&](const VoidLight::WorldData* worldData) {
        BOOST_REQUIRE(worldData != nullptr);
        for (size_t y = 0; y < worldData->grid.size() && !foundIronDeposit; ++y) {
            for (size_t x = 0; x < worldData->grid[y].size() && !foundIronDeposit; ++x) {
                if (worldData->grid[y][x].obstacleType != ObstacleType::IRON_DEPOSIT) {
                    continue;
                }

                Vector2D pos(static_cast<float>(x) * TILE_SIZE + TILE_SIZE * 0.5f,
                             static_cast<float>(y) * TILE_SIZE + TILE_SIZE * 0.5f);
                nearbyHarvestables.clear();
                worldResourceManager->queryHarvestablesInRadius(pos, 8.0f, nearbyHarvestables);
                BOOST_REQUIRE_MESSAGE(!nearbyHarvestables.empty(), "Expected harvestable at iron deposit obstacle");

                const auto handle = edm.getStaticHandle(nearbyHarvestables.front());
                const auto& harvestable = edm.getHarvestableData(handle);
                BOOST_CHECK(harvestable.harvestType == HarvestType::Mining);
                foundIronDeposit = true;
            }
        }
    });
    BOOST_WARN_MESSAGE(foundIronDeposit, "Generated world had no iron deposit obstacle to validate mining harvest type");
}

BOOST_AUTO_TEST_CASE(TestGetTileAt) {
    WorldGenerationConfig config;
    config.width = 10;
    config.height = 10;
    config.seed = 54321;
    config.elevationFrequency = 0.2f;
    config.humidityFrequency = 0.2f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    
    // Test valid positions
    auto tile = worldManager->getTileCopyAt(5, 5);
    BOOST_CHECK(tile.has_value());
    
    tile = worldManager->getTileCopyAt(0, 0);
    BOOST_CHECK(tile.has_value());
    
    tile = worldManager->getTileCopyAt(9, 9);
    BOOST_CHECK(tile.has_value());
    
    // Test invalid positions
    tile = worldManager->getTileCopyAt(-1, 5);
    BOOST_CHECK(!tile.has_value());
    
    tile = worldManager->getTileCopyAt(5, -1);
    BOOST_CHECK(!tile.has_value());
    
    tile = worldManager->getTileCopyAt(10, 5);
    BOOST_CHECK(!tile.has_value());
    
    tile = worldManager->getTileCopyAt(5, 10);
    BOOST_CHECK(!tile.has_value());
}

BOOST_AUTO_TEST_CASE(TestIsValidPosition) {
    WorldGenerationConfig config;
    config.width = 15;
    config.height = 10;
    config.seed = 11111;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    
    // Test valid positions
    BOOST_CHECK(worldManager->isValidPosition(0, 0));
    BOOST_CHECK(worldManager->isValidPosition(14, 9));
    BOOST_CHECK(worldManager->isValidPosition(7, 5));
    
    // Test invalid positions
    BOOST_CHECK(!worldManager->isValidPosition(-1, 0));
    BOOST_CHECK(!worldManager->isValidPosition(0, -1));
    BOOST_CHECK(!worldManager->isValidPosition(15, 0));
    BOOST_CHECK(!worldManager->isValidPosition(0, 10));
    BOOST_CHECK(!worldManager->isValidPosition(20, 20));
}

BOOST_AUTO_TEST_CASE(TestUpdateTile) {
    WorldGenerationConfig config;
    config.width = 5;
    config.height = 5;
    config.seed = 22222;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    
    // Get original tile
    const auto originalTile = worldManager->getTileCopyAt(2, 2);
    BOOST_REQUIRE(originalTile.has_value());
    
    // Create modified tile
    Tile newTile = *originalTile;
    newTile.biome = Biome::DESERT;
    newTile.obstacleType = ObstacleType::ROCK;
    newTile.elevation = 0.8f;
    
    // Update tile
    bool updateResult = worldManager->updateTile(2, 2, newTile);
    BOOST_CHECK(updateResult);
    
    // Verify the update
    const auto updatedTile = worldManager->getTileCopyAt(2, 2);
    BOOST_REQUIRE(updatedTile.has_value());
    BOOST_CHECK_EQUAL(updatedTile->biome, Biome::DESERT);
    BOOST_CHECK_EQUAL(updatedTile->obstacleType, ObstacleType::ROCK);
    BOOST_CHECK_CLOSE(updatedTile->elevation, 0.8f, 0.001f);
    
    // Test invalid position update
    bool invalidUpdate = worldManager->updateTile(-1, -1, newTile);
    BOOST_CHECK(!invalidUpdate);
}

BOOST_AUTO_TEST_CASE(TestHarvestResource) {
    WorldGenerationConfig config;
    config.width = 50;
    config.height = 50;
    config.seed = 33333;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.1f; // Low water level for more land
    config.mountainLevel = 0.9f; // High mountain level
    
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    
    // Find a tile with an obstacle
    int obstacleX = -1, obstacleY = -1;
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            const auto tile = worldManager->getTileCopyAt(x, y);
            if (tile.has_value() && tile->obstacleType != ObstacleType::NONE) {
                obstacleX = x;
                obstacleY = y;
                break;
            }
        }
        if (obstacleX != -1) break;
    }
    
    // We should find at least one obstacle in a 50x50 world
    BOOST_REQUIRE(obstacleX != -1);
    BOOST_REQUIRE(obstacleY != -1);
    
    // Verify tile has obstacle before harvesting
    const auto beforeHarvest = worldManager->getTileCopyAt(obstacleX, obstacleY);
    BOOST_REQUIRE(beforeHarvest.has_value());
    BOOST_CHECK(beforeHarvest->obstacleType != ObstacleType::NONE);
    
    // Harvest the resource
    bool harvestResult = worldManager->handleHarvestResource(1, obstacleX, obstacleY);
    BOOST_CHECK(harvestResult);
    
    // Verify tile no longer has obstacle
    const auto afterHarvest = worldManager->getTileCopyAt(obstacleX, obstacleY);
    BOOST_REQUIRE(afterHarvest.has_value());
    BOOST_CHECK_EQUAL(afterHarvest->obstacleType, ObstacleType::NONE);
}

BOOST_AUTO_TEST_CASE(TestHarvestEmptyTile) {
    WorldGenerationConfig config;
    config.width = 10;
    config.height = 10;
    config.seed = 44444;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.8f; // High water level, mostly water tiles
    config.mountainLevel = 0.9f;
    
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    
    // Find a tile without obstacles (likely water)
    int emptyX = -1, emptyY = -1;
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            const auto tile = worldManager->getTileCopyAt(x, y);
            if (tile.has_value() && tile->obstacleType == ObstacleType::NONE) {
                emptyX = x;
                emptyY = y;
                break;
            }
        }
        if (emptyX != -1) break;
    }
    
    BOOST_REQUIRE(emptyX != -1);
    
    // Try to harvest from empty tile
    bool harvestResult = worldManager->handleHarvestResource(1, emptyX, emptyY);
    BOOST_CHECK(!harvestResult); // Should fail
}

BOOST_AUTO_TEST_CASE(TestRenderingState) {
    BOOST_CHECK(worldManager->isRenderingEnabled()); // Default enabled
    
    worldManager->enableRendering(false);
    BOOST_CHECK(!worldManager->isRenderingEnabled());
    
    worldManager->enableRendering(true);
    BOOST_CHECK(worldManager->isRenderingEnabled());
}

BOOST_AUTO_TEST_CASE(TestCameraSettings) {
    worldManager->setCamera(10, 20);
    worldManager->setCameraViewport(80, 25);

    // Setters are void — verify they don't crash with valid and boundary values
    worldManager->setCamera(0, 0);
    worldManager->setCameraViewport(1, 1);
    worldManager->setCamera(-100, -200);
    worldManager->setCameraViewport(3840, 2160);
}

BOOST_AUTO_TEST_CASE(TestUnloadWorld) {
    WorldGenerationConfig config;
    config.width = 10;
    config.height = 10;
    config.seed = 55555;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;
    
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    BOOST_CHECK(worldManager->hasActiveWorld());
    
    worldManager->unloadWorld();
    BOOST_CHECK(!worldManager->hasActiveWorld());
    BOOST_CHECK(worldManager->getCurrentWorldId().empty());
    
    // Verify tile access returns null after unload
    const auto tile = worldManager->getTileCopyAt(5, 5);
    BOOST_CHECK(!tile.has_value());
}

BOOST_AUTO_TEST_CASE(TestMultipleWorldLoads) {
    WorldGenerationConfig config1;
    config1.width = 10;
    config1.height = 10;
    config1.seed = 1111;
    config1.elevationFrequency = 0.1f;
    config1.humidityFrequency = 0.1f;
    config1.waterLevel = 0.3f;
    config1.mountainLevel = 0.7f;
    
    WorldGenerationConfig config2;
    config2.width = 15;
    config2.height = 15;
    config2.seed = 2222;
    config2.elevationFrequency = 0.1f;
    config2.humidityFrequency = 0.1f;
    config2.waterLevel = 0.3f;
    config2.mountainLevel = 0.7f;
    
    // Load first world
    BOOST_REQUIRE(worldManager->loadNewWorld(config1));
    std::string firstWorldId = worldManager->getCurrentWorldId();
    
    worldManager->withWorldDataRead([&](const VoidLight::WorldData* firstWorldData) {
        BOOST_REQUIRE(firstWorldData != nullptr);
        BOOST_CHECK_EQUAL(firstWorldData->grid.size(), 10);
    });
    
    // Load second world (should replace first)
    BOOST_REQUIRE(worldManager->loadNewWorld(config2));
    std::string secondWorldId = worldManager->getCurrentWorldId();
    
    worldManager->withWorldDataRead([&](const VoidLight::WorldData* secondWorldData) {
        BOOST_REQUIRE(secondWorldData != nullptr);
        BOOST_CHECK_EQUAL(secondWorldData->grid.size(), 15);
    });
    
    // World IDs should be different
    BOOST_CHECK_NE(firstWorldId, secondWorldId);
    BOOST_CHECK(!worldResourceManager->hasWorld(firstWorldId));
    BOOST_CHECK(worldResourceManager->hasWorld(secondWorldId));
}

BOOST_AUTO_TEST_CASE(TestWorldResourceInitialization) {
    // Configure a world with diverse biomes to ensure resource initialization
    WorldGenerationConfig config;
    config.width = 50;
    config.height = 50;
    config.seed = 999999;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.2f; // Low water level for more land biomes
    config.mountainLevel = 0.6f; // Moderate mountain level
    
    // Load the world - this should trigger the resource initialization
    bool loadResult = worldManager->loadNewWorld(config);
    BOOST_REQUIRE(loadResult);
    
    // Verify basic world loading
    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK(!worldManager->getCurrentWorldId().empty());
    
    std::string worldId = worldManager->getCurrentWorldId();
    
    // Check if harvestables were spawned and registered with WorldResourceManager
    // World resources are now tracked via harvestables (query-only API)
    auto woodHandle = resourceTemplateManager->getHandleById("wood");
    if (woodHandle.isValid()) {
        int64_t woodQuantity = worldResourceManager->queryWorldTotal(worldId, woodHandle);
        BOOST_CHECK_GT(woodQuantity, 0); // Should have some wood from harvestables
    } else {
        BOOST_WARN_MESSAGE(false, "Wood resource handle not found - this might indicate a resource template issue");
    }

    auto ironHandle = resourceTemplateManager->getHandleById("iron_ore");
    if (ironHandle.isValid()) {
        int64_t ironQuantity = worldResourceManager->queryWorldTotal(worldId, ironHandle);
        // Iron deposits may not spawn in smaller worlds depending on terrain generation
        BOOST_CHECK_GE(ironQuantity, 0); // Non-negative (may be 0 in small worlds)
    } else {
        BOOST_WARN_MESSAGE(false, "Iron ore resource handle not found");
    }

    // Check for gold_ore - a harvestable material found in mountain biomes
    auto goldHandle = resourceTemplateManager->getHandleById("gold_ore");
    if (goldHandle.isValid()) {
        int64_t goldQuantity = worldResourceManager->queryWorldTotal(worldId, goldHandle);
        // Gold ore is rarer than iron, may be 0 in small worlds with few mountains
        BOOST_CHECK_GE(goldQuantity, 0); // Non-negative
    } else {
        BOOST_WARN_MESSAGE(false, "Gold ore resource handle not found - check materials.json");
    }
    
    // Get all resources for this world to see what was actually initialized
    auto allResources = worldResourceManager->getWorldResources(worldId);
    BOOST_CHECK_GT(allResources.size(), 0); // Should have some resources
    // Log total resource types
    
    // Verify multiple world loading works with resource initialization
    WorldGenerationConfig config2 = config;
    config2.seed = 888888; // Different seed
    
    bool loadResult2 = worldManager->loadNewWorld(config2);
    BOOST_REQUIRE(loadResult2);
    
    std::string newWorldId = worldManager->getCurrentWorldId();
    BOOST_CHECK_NE(newWorldId, worldId); // Should be different world
    
    // Verify new world also has resources
    auto newWorldResources = worldResourceManager->getWorldResources(newWorldId);
    BOOST_CHECK_GT(newWorldResources.size(), 0); // Should have resources too
    // Log second world resource types
}

// ============================================================================
// CHUNK CACHE TESTS
// Tests for the chunk-based world rendering cache
// ============================================================================

BOOST_AUTO_TEST_CASE(TestClearChunkCache) {
    WorldGenerationConfig config;
    config.width = 30;
    config.height = 30;
    config.seed = 77777;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    BOOST_CHECK(worldManager->hasActiveWorld());

    worldManager->clearChunkCache();

    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK(worldManager->isInitialized());
    BOOST_CHECK(worldManager->getTileCopyAt(5, 5).has_value());
}

BOOST_AUTO_TEST_CASE(TestInvalidateChunk) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 88888;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    // Invalidate various chunk positions
    worldManager->invalidateChunk(0, 0);
    worldManager->invalidateChunk(1, 0);
    worldManager->invalidateChunk(0, 1);
    worldManager->invalidateChunk(1, 1);

    // Invalid chunk positions should be handled gracefully
    worldManager->invalidateChunk(-1, -1);
    worldManager->invalidateChunk(100, 100);

    // Manager should still be functional
    BOOST_CHECK(worldManager->hasActiveWorld());
}

BOOST_AUTO_TEST_CASE(TestChunkCacheOnTileUpdate) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 99999;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    // Get original tile
    const auto originalTile = worldManager->getTileCopyAt(5, 5);
    BOOST_REQUIRE(originalTile.has_value());

    // Create modified tile
    Tile newTile = *originalTile;
    newTile.biome = Biome::CELESTIAL;

    // Update tile - this should invalidate the chunk
    bool updateResult = worldManager->updateTile(5, 5, newTile);
    BOOST_CHECK(updateResult);

    // Verify the tile was updated
    const auto updatedTile = worldManager->getTileCopyAt(5, 5);
    BOOST_REQUIRE(updatedTile.has_value());
    BOOST_CHECK_EQUAL(updatedTile->biome, Biome::CELESTIAL);
}

BOOST_AUTO_TEST_CASE(TestChunkCacheClearedOnUnload) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 11111;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    BOOST_CHECK(worldManager->hasActiveWorld());

    // Unload world - should clear chunk cache
    worldManager->unloadWorld();

    // No world should exist
    BOOST_CHECK(!worldManager->hasActiveWorld());

    // Load new world - should start with fresh cache
    BOOST_REQUIRE(worldManager->loadNewWorld(config));
    BOOST_CHECK(worldManager->hasActiveWorld());
}

// ============================================================================
// SEASONAL TEXTURE TESTS
// Tests for seasonal texture handling in WorldManager
// ============================================================================

BOOST_AUTO_TEST_CASE(TestSetCurrentSeason) {
    WorldGenerationConfig config;
    config.width = 10;
    config.height = 10;
    config.seed = 22222;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    worldManager->setCurrentSeason(Season::Spring);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Spring);

    worldManager->setCurrentSeason(Season::Summer);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Summer);

    worldManager->setCurrentSeason(Season::Fall);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Fall);

    worldManager->setCurrentSeason(Season::Winter);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Winter);
}

BOOST_AUTO_TEST_CASE(TestSeasonChangeUpdatesCache) {
    WorldGenerationConfig config;
    config.width = 15;
    config.height = 15;
    config.seed = 33333;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    worldManager->setCurrentSeason(Season::Spring);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Spring);

    worldManager->setCurrentSeason(Season::Winter);

    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK(worldManager->isInitialized());
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Winter);

    const auto tile = worldManager->getTileCopyAt(5, 5);
    BOOST_CHECK(tile.has_value());
}

BOOST_AUTO_TEST_CASE(TestSeasonEventSubscription) {
    // Subscribe to season events - should not crash
    worldManager->subscribeToSeasonEvents();

    // World manager should still be functional
    BOOST_CHECK(worldManager->isInitialized());
    BOOST_CHECK(!worldManager->isShutdown());
}

BOOST_AUTO_TEST_CASE(TestSeasonalTextureIdConsistency) {
    WorldGenerationConfig config;
    config.width = 10;
    config.height = 10;
    config.seed = 44444;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    // Cycle through seasons multiple times
    for (int cycle = 0; cycle < 3; ++cycle) {
        worldManager->setCurrentSeason(Season::Spring);
        worldManager->setCurrentSeason(Season::Summer);
        worldManager->setCurrentSeason(Season::Fall);
        worldManager->setCurrentSeason(Season::Winter);
    }

    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK(worldManager->isInitialized());
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Winter);
}

BOOST_AUTO_TEST_CASE(TestSeasonChangeWithoutWorld) {
    // Make sure no world is loaded
    worldManager->unloadWorld();
    BOOST_CHECK(!worldManager->hasActiveWorld());

    worldManager->setCurrentSeason(Season::Summer);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Summer);
    worldManager->setCurrentSeason(Season::Winter);

    BOOST_CHECK(worldManager->isInitialized());
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Winter);
}

BOOST_AUTO_TEST_CASE(TestChunkCacheClearedOnSeasonChange) {
    WorldGenerationConfig config;
    config.width = 20;
    config.height = 20;
    config.seed = 55555;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(worldManager->loadNewWorld(config));

    worldManager->setCurrentSeason(Season::Spring);
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Spring);

    worldManager->setCurrentSeason(Season::Fall);

    BOOST_CHECK(worldManager->hasActiveWorld());
    BOOST_CHECK(worldManager->getCurrentSeason() == Season::Fall);

    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const auto tile = worldManager->getTileCopyAt(x, y);
            BOOST_CHECK(tile.has_value());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
