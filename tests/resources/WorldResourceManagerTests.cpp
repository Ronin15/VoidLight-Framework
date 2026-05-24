/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE WorldResourceManagerTests
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "core/ThreadSystem.hpp"
#include "entities/Resource.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldResourceManager.hpp"

// Helper function to find resource handle by name
VoidLight::ResourceHandle
findResourceByName(ResourceTemplateManager *manager, const std::string &name) {
  for (int cat = 0; cat < static_cast<int>(ResourceCategory::COUNT); ++cat) {
    auto resources =
        manager->getResourcesByCategory(static_cast<ResourceCategory>(cat));
    for (const auto &resource : resources) {
      if (resource && resource->getName() == name) {
        return resource->getHandle();
      }
    }
  }
  return VoidLight::ResourceHandle(); // Invalid handle
}

// Helper function to get or load resource by name
VoidLight::ResourceHandle
getOrLoadResourceByName(ResourceTemplateManager *manager,
                        const std::string &name) {
  auto handle = findResourceByName(manager, name);
  if (handle.isValid()) {
    return handle;
  }

  manager->loadResourcesFromJson("res/data/materials.json");

  handle = findResourceByName(manager, name);
  return handle;
}

class WorldResourceManagerTestFixture {
public:
  WorldResourceManagerTestFixture() {
    // Initialize ThreadSystem first for threading tests
    threadSystem = &VoidLight::ThreadSystem::Instance();
    if (threadSystem->isShutdown() || threadSystem->getThreadCount() == 0) {
      bool initSuccess = threadSystem->init();
      if (!initSuccess && threadSystem->getThreadCount() == 0) {
        throw std::runtime_error("Failed to initialize ThreadSystem for threading tests");
      }
    }

    // Initialize ResourceTemplateManager singleton first (required for EDM)
    templateManager = &ResourceTemplateManager::Instance();
    BOOST_REQUIRE(templateManager != nullptr);
    bool templateInitialized = templateManager->init();
    BOOST_REQUIRE(templateInitialized);

    // Initialize EntityDataManager for inventory operations
    entityDataManager = &EntityDataManager::Instance();
    BOOST_REQUIRE(entityDataManager != nullptr);
    bool edmInitialized = entityDataManager->init();
    BOOST_REQUIRE(edmInitialized);

    // Initialize WorldResourceManager singleton
    worldManager = &WorldResourceManager::Instance();
    BOOST_REQUIRE(worldManager != nullptr);
    bool worldInitialized = worldManager->init();
    BOOST_REQUIRE(worldInitialized);

    // Get common resource handles for tests
    goldHandle = getOrLoadResourceByName(templateManager, "Platinum Coins");
    potionHandle = getOrLoadResourceByName(templateManager, "Super Health Potion");
    oreHandle = getOrLoadResourceByName(templateManager, "Mithril Ore");
    swordHandle = getOrLoadResourceByName(templateManager, "Magic Sword");
  }

  ~WorldResourceManagerTestFixture() {
    worldManager->clean();
    entityDataManager->clean();
    templateManager->clean();
  }

protected:
  ResourceTemplateManager *templateManager;
  EntityDataManager *entityDataManager;
  WorldResourceManager *worldManager;
  VoidLight::ThreadSystem *threadSystem;

  // Common resource handles
  VoidLight::ResourceHandle goldHandle;
  VoidLight::ResourceHandle potionHandle;
  VoidLight::ResourceHandle oreHandle;
  VoidLight::ResourceHandle swordHandle;
};

BOOST_FIXTURE_TEST_SUITE(WorldResourceManagerTestSuite,
                         WorldResourceManagerTestFixture)

//==============================================================================
// Singleton and Initialization Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestSingletonPattern) {
  WorldResourceManager *instance1 = &WorldResourceManager::Instance();
  WorldResourceManager *instance2 = &WorldResourceManager::Instance();

  BOOST_CHECK(instance1 == instance2);
  BOOST_CHECK(instance1 == worldManager);
}

BOOST_AUTO_TEST_CASE(TestInitialization) {
  BOOST_CHECK(worldManager->isInitialized());

  // Test that we start with a default world
  auto worlds = worldManager->getWorldIds();
  BOOST_CHECK_EQUAL(worlds.size(), 1);
  BOOST_CHECK(worldManager->hasWorld("default"));
}

//==============================================================================
// World Creation and Removal Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestWorldCreationAndRemoval) {
  const std::string worldId = "test_world";

  // Test world creation
  bool created = worldManager->createWorld(worldId);
  BOOST_CHECK(created);
  BOOST_CHECK(worldManager->hasWorld(worldId));

  // Test that creating the same world again fails
  bool createdAgain = worldManager->createWorld(worldId);
  BOOST_CHECK(!createdAgain);

  // Test getting all worlds (should include default + test_world)
  auto worlds = worldManager->getWorldIds();
  BOOST_CHECK_EQUAL(worlds.size(), 2);
  BOOST_CHECK(std::find(worlds.begin(), worlds.end(), worldId) != worlds.end());
  BOOST_CHECK(std::find(worlds.begin(), worlds.end(), "default") != worlds.end());

  // Test world removal
  bool removed = worldManager->removeWorld(worldId);
  BOOST_CHECK(removed);
  BOOST_CHECK(!worldManager->hasWorld(worldId));

  // Test that removing a non-existent world fails
  bool removedAgain = worldManager->removeWorld(worldId);
  BOOST_CHECK(!removedAgain);

  // Test that we're back to just default world
  worlds = worldManager->getWorldIds();
  BOOST_CHECK_EQUAL(worlds.size(), 1);
  BOOST_CHECK(std::find(worlds.begin(), worlds.end(), "default") != worlds.end());
}

BOOST_AUTO_TEST_CASE(TestRemoveWorldClearsSpatialIndices) {
  const std::string worldId = "spatial_cleanup_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  worldManager->setActiveWorld(worldId);
  BOOST_REQUIRE(goldHandle.isValid());
  BOOST_REQUIRE(oreHandle.isValid());

  EntityHandle droppedItem = entityDataManager->createDroppedItem(
      Vector2D(64.0f, 64.0f), goldHandle, 3, worldId);
  EntityHandle harvestable = entityDataManager->createHarvestable(
      Vector2D(96.0f, 64.0f), oreHandle, 1, 2, 30.0f, worldId);
  EntityHandle container = entityDataManager->createContainer(
      Vector2D(128.0f, 64.0f), ContainerType::Chest, 8, 0, worldId);
  BOOST_REQUIRE(droppedItem.isValid());
  BOOST_REQUIRE(harvestable.isValid());
  BOOST_REQUIRE(container.isValid());

  std::vector<size_t> indices;
  BOOST_CHECK_GT(worldManager->queryDroppedItemsInRadius(Vector2D(64.0f, 64.0f), 32.0f, indices), 0);
  BOOST_CHECK_GT(worldManager->queryHarvestablesInRadius(Vector2D(96.0f, 64.0f), 32.0f, indices), 0);
  BOOST_CHECK_GT(worldManager->queryContainersInRadius(Vector2D(128.0f, 64.0f), 32.0f, indices), 0);

  BOOST_REQUIRE(worldManager->removeWorld(worldId));

  BOOST_CHECK(worldManager->getActiveWorld().empty());
  BOOST_CHECK_EQUAL(worldManager->queryDroppedItemsInRadius(Vector2D(64.0f, 64.0f), 32.0f, indices), 0);
  BOOST_CHECK(indices.empty());
  indices.push_back(999);
  BOOST_CHECK_EQUAL(worldManager->queryHarvestablesInRadius(Vector2D(96.0f, 64.0f), 32.0f, indices), 0);
  BOOST_CHECK(indices.empty());
  indices.push_back(999);
  BOOST_CHECK_EQUAL(worldManager->queryContainersInRadius(Vector2D(128.0f, 64.0f), 32.0f, indices), 0);
  BOOST_CHECK(indices.empty());
}

BOOST_AUTO_TEST_CASE(TestDestroyContainerUnregistersInventory) {
  const std::string worldId = "container_inventory_cleanup_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  worldManager->setActiveWorld(worldId);

  EntityHandle container = entityDataManager->createContainer(
      Vector2D(128.0f, 64.0f), ContainerType::Chest, 8, 0, worldId);
  BOOST_REQUIRE(container.isValid());

  std::vector<size_t> containerIndices;
  BOOST_REQUIRE_EQUAL(
      worldManager->queryContainersInRadius(Vector2D(128.0f, 64.0f), 32.0f, containerIndices),
      1);
  const size_t staticIndex = containerIndices.front();
  BOOST_REQUIRE_NE(staticIndex, SIZE_MAX);
  const auto& hot = entityDataManager->getStaticHotDataByIndex(staticIndex);
  const uint32_t inventoryIndex =
      entityDataManager->getContainerData(hot.typeLocalIndex).inventoryIndex;
  BOOST_REQUIRE_NE(inventoryIndex, INVALID_INVENTORY_INDEX);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 1);

  entityDataManager->destroyEntity(container);

  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 0);
  BOOST_CHECK(!entityDataManager->isValidInventoryIndex(inventoryIndex));
}

//==============================================================================
// Inventory Registration Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestInventoryRegistration) {
  const std::string worldId = "inventory_test_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  BOOST_REQUIRE(goldHandle.isValid());

  // Create an EDM inventory
  uint32_t inventoryIndex = entityDataManager->createInventory(20, true);
  BOOST_REQUIRE(inventoryIndex != INVALID_INVENTORY_INDEX);

  // Register it with the world
  worldManager->registerInventory(inventoryIndex, worldId);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 1);

  // Add resources to the inventory via EDM
  bool added = entityDataManager->addToInventory(inventoryIndex, goldHandle, 100);
  BOOST_CHECK(added);

  // Query via WorldResourceManager - should see the resources
  auto total = worldManager->queryInventoryTotal(worldId, goldHandle);
  BOOST_CHECK_EQUAL(total, 100);

  // Unregister inventory
  worldManager->unregisterInventory(inventoryIndex);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 0);

  // Query should return 0 now (inventory unregistered)
  total = worldManager->queryInventoryTotal(worldId, goldHandle);
  BOOST_CHECK_EQUAL(total, 0);

  // Cleanup
  entityDataManager->destroyInventory(inventoryIndex);
  worldManager->removeWorld(worldId);
}

BOOST_AUTO_TEST_CASE(TestMultipleInventoriesInWorld) {
  const std::string worldId = "multi_inventory_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  BOOST_REQUIRE(goldHandle.isValid());
  BOOST_REQUIRE(oreHandle.isValid());

  // Create multiple inventories
  uint32_t inv1 = entityDataManager->createInventory(10, true);
  uint32_t inv2 = entityDataManager->createInventory(10, true);
  uint32_t inv3 = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(inv1 != INVALID_INVENTORY_INDEX && inv2 != INVALID_INVENTORY_INDEX && inv3 != INVALID_INVENTORY_INDEX);

  // Register all with the world
  worldManager->registerInventory(inv1, worldId);
  worldManager->registerInventory(inv2, worldId);
  worldManager->registerInventory(inv3, worldId);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 3);

  // Add different amounts to each inventory
  entityDataManager->addToInventory(inv1, goldHandle, 100);
  entityDataManager->addToInventory(inv2, goldHandle, 200);
  entityDataManager->addToInventory(inv3, goldHandle, 300);
  entityDataManager->addToInventory(inv1, oreHandle, 50);

  // Query totals
  auto goldTotal = worldManager->queryInventoryTotal(worldId, goldHandle);
  BOOST_CHECK_EQUAL(goldTotal, 600);  // 100 + 200 + 300

  auto oreTotal = worldManager->queryInventoryTotal(worldId, oreHandle);
  BOOST_CHECK_EQUAL(oreTotal, 50);

  // Get all world resources
  auto allResources = worldManager->getWorldResources(worldId);
  BOOST_CHECK(allResources.find(goldHandle) != allResources.end());
  BOOST_CHECK_EQUAL(allResources[goldHandle], 600);

  // Cleanup
  entityDataManager->destroyInventory(inv1);
  entityDataManager->destroyInventory(inv2);
  entityDataManager->destroyInventory(inv3);
  worldManager->removeWorld(worldId);
}

//==============================================================================
// Harvestable Registration Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestHarvestableRegistration) {
  const std::string worldId = "harvestable_test_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  BOOST_REQUIRE(oreHandle.isValid());

  // Create a harvestable via EDM
  Vector2D pos(100.0f, 100.0f);
  EntityHandle harvestableHandle = entityDataManager->createHarvestable(
      pos, oreHandle, 1, 5, 60.0f);  // yields 1-5, respawns in 60s
  BOOST_REQUIRE(harvestableHandle.isValid());

  size_t edmIndex = entityDataManager->getIndex(harvestableHandle);
  BOOST_REQUIRE(edmIndex != SIZE_MAX);

  // Register with world
  worldManager->registerHarvestable(edmIndex, worldId);
  BOOST_CHECK_EQUAL(worldManager->getHarvestableCount(worldId), 1);

  // Query harvestable total (should return yieldMax for non-depleted)
  auto total = worldManager->queryHarvestableTotal(worldId, oreHandle);
  BOOST_CHECK_GE(total, 1);  // At least yieldMax

  // Unregister
  worldManager->unregisterHarvestable(edmIndex);
  BOOST_CHECK_EQUAL(worldManager->getHarvestableCount(worldId), 0);

  // Cleanup
  entityDataManager->destroyEntity(harvestableHandle);
  worldManager->removeWorld(worldId);
}

BOOST_AUTO_TEST_CASE(TestCombinedInventoryAndHarvestableQuery) {
  const std::string worldId = "combined_query_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  BOOST_REQUIRE(oreHandle.isValid());

  // Create inventory with ore
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  worldManager->registerInventory(invIndex, worldId);
  entityDataManager->addToInventory(invIndex, oreHandle, 100);

  // Create harvestable with ore yield
  Vector2D pos(100.0f, 100.0f);
  EntityHandle harvHandle = entityDataManager->createHarvestable(
      pos, oreHandle, 5, 10, 60.0f);
  BOOST_REQUIRE(harvHandle.isValid());
  size_t harvIndex = entityDataManager->getIndex(harvHandle);
  worldManager->registerHarvestable(harvIndex, worldId);

  // Query inventory total only
  auto invTotal = worldManager->queryInventoryTotal(worldId, oreHandle);
  BOOST_CHECK_EQUAL(invTotal, 100);

  // Query harvestable total only
  auto harvTotal = worldManager->queryHarvestableTotal(worldId, oreHandle);
  BOOST_CHECK_GE(harvTotal, 5);  // At least yieldMax

  // Query combined world total
  auto worldTotal = worldManager->queryWorldTotal(worldId, oreHandle);
  BOOST_CHECK_GE(worldTotal, invTotal + harvTotal);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
  entityDataManager->destroyEntity(harvHandle);
  worldManager->removeWorld(worldId);
}

//==============================================================================
// Query Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestQueryNonexistentWorld) {
  // Query a world that doesn't exist
  auto total = worldManager->queryInventoryTotal("nonexistent_world", goldHandle);
  BOOST_CHECK_EQUAL(total, 0);

  auto harvTotal = worldManager->queryHarvestableTotal("nonexistent_world", goldHandle);
  BOOST_CHECK_EQUAL(harvTotal, 0);

  auto worldTotal = worldManager->queryWorldTotal("nonexistent_world", goldHandle);
  BOOST_CHECK_EQUAL(worldTotal, 0);
}

BOOST_AUTO_TEST_CASE(TestQueryInvalidResourceHandle) {
  const std::string worldId = "default";
  VoidLight::ResourceHandle invalidHandle;

  auto total = worldManager->queryInventoryTotal(worldId, invalidHandle);
  BOOST_CHECK_EQUAL(total, 0);
}

BOOST_AUTO_TEST_CASE(TestHasResource) {
  const std::string worldId = "has_resource_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));
  BOOST_REQUIRE(goldHandle.isValid());

  // Create inventory
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  worldManager->registerInventory(invIndex, worldId);

  // Add resources
  entityDataManager->addToInventory(invIndex, goldHandle, 50);

  // Test hasResource
  BOOST_CHECK(worldManager->hasResource(worldId, goldHandle, 1));
  BOOST_CHECK(worldManager->hasResource(worldId, goldHandle, 50));
  BOOST_CHECK(!worldManager->hasResource(worldId, goldHandle, 100));
  BOOST_CHECK(!worldManager->hasResource(worldId, potionHandle, 1));

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
  worldManager->removeWorld(worldId);
}

//==============================================================================
// Multiple World Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestMultipleWorlds) {
  std::vector<std::string> worldIds = {"world1", "world2", "world3"};
  BOOST_REQUIRE(goldHandle.isValid());

  // Create multiple worlds
  for (const auto &worldId : worldIds) {
    BOOST_REQUIRE(worldManager->createWorld(worldId));
  }

  // Create inventories for each world
  std::vector<uint32_t> inventories;
  for (size_t i = 0; i < worldIds.size(); ++i) {
    uint32_t invIndex = entityDataManager->createInventory(10, true);
    BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
    worldManager->registerInventory(invIndex, worldIds[i]);
    inventories.push_back(invIndex);

    // Add different amounts to each world
    int64_t amount = (i + 1) * 100;  // 100, 200, 300
    entityDataManager->addToInventory(invIndex, goldHandle, static_cast<int>(amount));
  }

  // Verify each world has the correct amount
  for (size_t i = 0; i < worldIds.size(); ++i) {
    int64_t expected = (i + 1) * 100;
    int64_t actual = worldManager->queryInventoryTotal(worldIds[i], goldHandle);
    BOOST_CHECK_EQUAL(actual, expected);
  }

  // Cleanup
  for (auto invIndex : inventories) {
    entityDataManager->destroyInventory(invIndex);
  }
  for (const auto &worldId : worldIds) {
    worldManager->removeWorld(worldId);
  }
}

BOOST_AUTO_TEST_CASE(TestWorldIsolation) {
  const std::string world1 = "world1";
  const std::string world2 = "world2";
  BOOST_REQUIRE(goldHandle.isValid());

  BOOST_REQUIRE(worldManager->createWorld(world1));
  BOOST_REQUIRE(worldManager->createWorld(world2));

  // Create inventories for each world
  uint32_t inv1 = entityDataManager->createInventory(10, true);
  uint32_t inv2 = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(inv1 != INVALID_INVENTORY_INDEX && inv2 != INVALID_INVENTORY_INDEX);

  worldManager->registerInventory(inv1, world1);
  worldManager->registerInventory(inv2, world2);

  // Add different amounts
  // Note: EDM uses hardcoded maxStack=99, so 10 slots * 99 = 990 max capacity
  entityDataManager->addToInventory(inv1, goldHandle, 100);
  entityDataManager->addToInventory(inv2, goldHandle, 500);

  // Verify isolation
  BOOST_CHECK_EQUAL(worldManager->queryInventoryTotal(world1, goldHandle), 100);
  BOOST_CHECK_EQUAL(worldManager->queryInventoryTotal(world2, goldHandle), 500);

  // Modify one world (add 500 more, staying within capacity limits)
  entityDataManager->addToInventory(inv1, goldHandle, 500);

  BOOST_CHECK_EQUAL(worldManager->queryInventoryTotal(world1, goldHandle), 600);
  BOOST_CHECK_EQUAL(worldManager->queryInventoryTotal(world2, goldHandle), 500);  // Unchanged

  // Cleanup
  entityDataManager->destroyInventory(inv1);
  entityDataManager->destroyInventory(inv2);
  worldManager->removeWorld(world1);
  worldManager->removeWorld(world2);
}

//==============================================================================
// Statistics Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestStatistics) {
  const std::string worldId = "stats_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));

  // Reset stats for this test
  worldManager->resetStats();

  // Create and register inventory
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  worldManager->registerInventory(invIndex, worldId);

  // Perform some queries and verify they return valid results
  auto goldQty = worldManager->queryInventoryTotal(worldId, goldHandle);
  auto oreQty = worldManager->queryInventoryTotal(worldId, oreHandle);
  auto potionQty = worldManager->queryWorldTotal(worldId, potionHandle);

  BOOST_CHECK_GE(goldQty, 0);
  BOOST_CHECK_GE(oreQty, 0);
  BOOST_CHECK_GE(potionQty, 0);

  // Check stats
  auto stats = worldManager->getStats();
  BOOST_CHECK_GE(stats.queryCount.load(), 3);
  BOOST_CHECK_GE(stats.inventoriesRegistered.load(), 1);

  // Reset stats
  worldManager->resetStats();
  auto resetStats = worldManager->getStats();
  BOOST_CHECK_EQUAL(resetStats.queryCount.load(), 0);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
  worldManager->removeWorld(worldId);
}

//==============================================================================
// Thread Safety Tests
//==============================================================================

BOOST_AUTO_TEST_CASE(TestConcurrentInventoryOperations) {
  const int NUM_THREADS = 5;
  const int OPERATIONS_PER_THREAD = 50;
  const std::string worldId = "concurrent_test_world";

  BOOST_REQUIRE(worldManager->createWorld(worldId));
  BOOST_REQUIRE(goldHandle.isValid());

  // Create initial inventory
  uint32_t invIndex = entityDataManager->createInventory(100, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  worldManager->registerInventory(invIndex, worldId);

  std::atomic<int> successfulAdds{0};
  std::atomic<int> successfulQueries{0};
  std::vector<std::future<void>> futures;

  // Create tasks that perform concurrent operations
  for (int i = 0; i < NUM_THREADS; ++i) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &successfulAdds, &successfulQueries]() -> void {
          for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
            // Concurrent add to inventory
            if (entityDataManager->addToInventory(invIndex, goldHandle, 1)) {
              successfulAdds.fetch_add(1, std::memory_order_relaxed);
            }

            // Concurrent query
            auto qty = worldManager->queryInventoryTotal(worldId, goldHandle);
            if (qty >= 0) {
              successfulQueries.fetch_add(1, std::memory_order_relaxed);
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
          }
        },
        VoidLight::TaskPriority::Normal, "ConcurrentInventoryTask");

    futures.push_back(std::move(future));
  }

  // Wait for all tasks
  for (auto &future : futures) {
    future.wait();
  }

  // Verify operations completed
  BOOST_CHECK(successfulAdds.load() > 0);
  BOOST_CHECK_EQUAL(successfulQueries.load(), NUM_THREADS * OPERATIONS_PER_THREAD);

  // Verify supported concurrent inventory adds preserve every successful mutation.
  auto finalQty = worldManager->queryInventoryTotal(worldId, goldHandle);
  BOOST_CHECK_EQUAL(finalQty, successfulAdds.load());

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
  worldManager->removeWorld(worldId);
}

BOOST_AUTO_TEST_CASE(TestConcurrentWorldOperations) {
  const int NUM_THREADS = 5;
  const int WORLDS_PER_THREAD = 10;

  std::atomic<int> worldsCreated{0};
  std::atomic<int> worldsRemoved{0};
  std::vector<std::future<void>> futures;

  for (int i = 0; i < NUM_THREADS; ++i) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &worldsCreated, &worldsRemoved]() -> void {
          for (int j = 0; j < WORLDS_PER_THREAD; ++j) {
            std::string worldId = "concurrent_world_" + std::to_string(i) + "_" + std::to_string(j);

            if (worldManager->createWorld(worldId)) {
              worldsCreated.fetch_add(1, std::memory_order_relaxed);

              if (worldManager->removeWorld(worldId)) {
                worldsRemoved.fetch_add(1, std::memory_order_relaxed);
              }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
          }
        },
        VoidLight::TaskPriority::Normal, "ConcurrentWorldTask");

    futures.push_back(std::move(future));
  }

  // Wait for all tasks
  for (auto &future : futures) {
    future.wait();
  }

  // Verify all created worlds were removed
  BOOST_CHECK_EQUAL(worldsCreated.load(), worldsRemoved.load());
  BOOST_CHECK_EQUAL(worldsCreated.load(), NUM_THREADS * WORLDS_PER_THREAD);

  // Only default world should remain
  auto remainingWorlds = worldManager->getWorldIds();
  BOOST_CHECK_EQUAL(remainingWorlds.size(), 1);
  BOOST_CHECK(worldManager->hasWorld("default"));
}

//==============================================================================
// Edge Cases
//==============================================================================

BOOST_AUTO_TEST_CASE(TestEmptyWorldIdHandling) {
  // Empty world ID should not be created
  bool created = worldManager->createWorld("");
  BOOST_CHECK(!created);

  // Queries on empty world ID should return 0
  auto total = worldManager->queryInventoryTotal("", goldHandle);
  BOOST_CHECK_EQUAL(total, 0);
}

BOOST_AUTO_TEST_CASE(TestDoubleRegistration) {
  const std::string worldId = "double_reg_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));

  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  // Register once
  worldManager->registerInventory(invIndex, worldId);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 1);

  // Register again (should not duplicate)
  worldManager->registerInventory(invIndex, worldId);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 1);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
  worldManager->removeWorld(worldId);
}

BOOST_AUTO_TEST_CASE(TestUnregisterNonexistentInventory) {
  // Unregistering an inventory that was never registered should not crash
  BOOST_CHECK_NO_THROW(worldManager->unregisterInventory(99999));
}

BOOST_AUTO_TEST_CASE(TestWorldRemovalClearsRegistrations) {
  const std::string worldId = "removal_test_world";
  BOOST_REQUIRE(worldManager->createWorld(worldId));

  // Create and register inventory
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  worldManager->registerInventory(invIndex, worldId);
  entityDataManager->addToInventory(invIndex, goldHandle, 100);

  // Verify registration
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 1);
  BOOST_CHECK_EQUAL(worldManager->queryInventoryTotal(worldId, goldHandle), 100);

  // Remove the world
  worldManager->removeWorld(worldId);

  // World should be gone
  BOOST_CHECK(!worldManager->hasWorld(worldId));

  // Recreate world - should be empty
  worldManager->createWorld(worldId);
  BOOST_CHECK_EQUAL(worldManager->getInventoryCount(worldId), 0);
  BOOST_CHECK_EQUAL(worldManager->queryInventoryTotal(worldId, goldHandle), 0);

  // Note: The inventory still exists in EDM, just not registered
  // The calling code is responsible for cleaning up EDM entities

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
  worldManager->removeWorld(worldId);
}

BOOST_AUTO_TEST_SUITE_END()
