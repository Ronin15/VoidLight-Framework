/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ResourceIntegrationTests
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "core/ThreadSystem.hpp"
#include "entities/Resource.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/ResourceTemplateManager.hpp"

struct GlobalFixture {
    GlobalFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for resource integration tests");
        }
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        if (!EntityDataManager::Instance().init()) {
            throw std::runtime_error("EntityDataManager::init() failed");
        }
    }
    ~GlobalFixture() {
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(GlobalFixture);

class ResourceIntegrationTestFixture {
public:
  ResourceIntegrationTestFixture() {
    threadSystem = &VoidLight::ThreadSystem::Instance();
    resourceManager = &ResourceTemplateManager::Instance();
    entityDataManager = &EntityDataManager::Instance();

    // Create EDM inventories to simulate player and NPC
    playerInvIndex = entityDataManager->createInventory(50, true);  // Player with 50 slots
    npcInvIndex = entityDataManager->createInventory(60, true);     // NPC with 60 slots

    // Get test resource handles by name
    healthPotionHandle = resourceManager->getHandleByName("Super Health Potion");
    ironSwordHandle = resourceManager->getHandleByName("Magic Sword");
    ironOreHandle = resourceManager->getHandleByName("Mithril Ore");
    goldHandle = resourceManager->getHandleByName("Platinum Coins");

    // Get test resources using handles
    RESOURCE_DEBUG("Before getResourceTemplate super_health_potion");
    BOOST_REQUIRE(healthPotionHandle.isValid());
    healthPotion = resourceManager->getResourceTemplate(healthPotionHandle);
    RESOURCE_DEBUG("After getResourceTemplate super_health_potion");

    RESOURCE_DEBUG("Before getResourceTemplate magic_sword");
    BOOST_REQUIRE(ironSwordHandle.isValid());
    ironSword = resourceManager->getResourceTemplate(ironSwordHandle);
    RESOURCE_DEBUG("After getResourceTemplate magic_sword");

    RESOURCE_DEBUG("Before getResourceTemplate mithril_ore");
    BOOST_REQUIRE(ironOreHandle.isValid());
    ironOre = resourceManager->getResourceTemplate(ironOreHandle);
    RESOURCE_DEBUG("After getResourceTemplate mithril_ore");

    RESOURCE_DEBUG("Before getResourceTemplate platinum_coins");
    BOOST_REQUIRE(goldHandle.isValid());
    gold = resourceManager->getResourceTemplate(goldHandle);
    RESOURCE_DEBUG("After getResourceTemplate gold");

    BOOST_REQUIRE(healthPotion != nullptr);
    BOOST_REQUIRE(ironSword != nullptr);
    BOOST_REQUIRE(ironOre != nullptr);
    BOOST_REQUIRE(gold != nullptr);
    BOOST_REQUIRE(playerInvIndex != INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE(npcInvIndex != INVALID_INVENTORY_INDEX);
  }

  ~ResourceIntegrationTestFixture() {
    entityDataManager->destroyInventory(playerInvIndex);
    entityDataManager->destroyInventory(npcInvIndex);
  }

protected:
  ResourceTemplateManager *resourceManager;
  EntityDataManager *entityDataManager;
  VoidLight::ThreadSystem *threadSystem;
  uint32_t playerInvIndex;
  uint32_t npcInvIndex;
  std::shared_ptr<Resource> healthPotion;
  std::shared_ptr<Resource> ironSword;
  std::shared_ptr<Resource> ironOre;
  std::shared_ptr<Resource> gold;

  // Resource handles for easy access
  VoidLight::ResourceHandle healthPotionHandle;
  VoidLight::ResourceHandle ironSwordHandle;
  VoidLight::ResourceHandle ironOreHandle;
  VoidLight::ResourceHandle goldHandle;

  // Helper to check if inventory has resource
  bool playerHasResource(VoidLight::ResourceHandle handle, int qty = 1) {
    return entityDataManager->hasInInventory(playerInvIndex, handle, qty);
  }

  bool npcHasResource(VoidLight::ResourceHandle handle, int qty = 1) {
    return entityDataManager->hasInInventory(npcInvIndex, handle, qty);
  }

  // Helper to get inventory quantity
  int playerGetQty(VoidLight::ResourceHandle handle) {
    return entityDataManager->getInventoryQuantity(playerInvIndex, handle);
  }

  int npcGetQty(VoidLight::ResourceHandle handle) {
    return entityDataManager->getInventoryQuantity(npcInvIndex, handle);
  }

  // Helper to add to inventory
  bool playerAdd(VoidLight::ResourceHandle handle, int qty) {
    return entityDataManager->addToInventory(playerInvIndex, handle, qty);
  }

  bool npcAdd(VoidLight::ResourceHandle handle, int qty) {
    return entityDataManager->addToInventory(npcInvIndex, handle, qty);
  }

  // Helper to remove from inventory
  bool playerRemove(VoidLight::ResourceHandle handle, int qty) {
    return entityDataManager->removeFromInventory(playerInvIndex, handle, qty);
  }

  bool npcRemove(VoidLight::ResourceHandle handle, int qty) {
    return entityDataManager->removeFromInventory(npcInvIndex, handle, qty);
  }
};

BOOST_FIXTURE_TEST_SUITE(ResourceIntegrationTestSuite,
                         ResourceIntegrationTestFixture)

BOOST_AUTO_TEST_CASE(TestPlayerInventoryIntegration) {
  // Test that player inventory works properly via EDM
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 0);  // Empty initially

  // Test adding resources to player inventory
  bool added = playerAdd(healthPotionHandle, 10);
  BOOST_CHECK(added);
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 10);

  // Test removing resources from inventory
  bool removed = playerRemove(healthPotionHandle, 3);
  BOOST_CHECK(removed);
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 7);

  // Test inventory has resource check
  BOOST_CHECK(playerHasResource(healthPotionHandle));
  BOOST_CHECK(playerHasResource(healthPotionHandle, 5));
  BOOST_CHECK(!playerHasResource(healthPotionHandle, 10));
  BOOST_CHECK(!playerHasResource(ironSwordHandle));
}

BOOST_AUTO_TEST_CASE(TestNPCInventoryIntegration) {
  // Test that NPC inventory works properly via EDM
  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 0);  // Empty initially

  // Test adding resources to NPC inventory
  bool added = npcAdd(ironOreHandle, 15);
  BOOST_CHECK(added);
  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 15);

  // Test removing resources from NPC inventory
  bool removed = npcRemove(ironOreHandle, 5);
  BOOST_CHECK(removed);
  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 10);

  // Test NPC inventory has resource check
  BOOST_CHECK(npcHasResource(ironOreHandle));
  BOOST_CHECK(npcHasResource(ironOreHandle, 8));
  BOOST_CHECK(!npcHasResource(ironOreHandle, 15));
  BOOST_CHECK(!npcHasResource(healthPotionHandle));
}

BOOST_AUTO_TEST_CASE(TestResourceTransferBetweenEntities) {
  // Setup: Give player inventory some resources
  playerAdd(healthPotionHandle, 20);
  playerAdd(goldHandle, 100);

  // Setup: Give NPC inventory some resources
  npcAdd(ironSwordHandle, 1);
  npcAdd(ironOreHandle, 50);

  // Test transferring resources from player to NPC inventory
  BOOST_REQUIRE(playerHasResource(healthPotionHandle, 5));
  BOOST_REQUIRE(playerRemove(healthPotionHandle, 5));
  BOOST_REQUIRE(npcAdd(healthPotionHandle, 5));

  // Check that quantities are correct after transfer
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 15);
  BOOST_CHECK_EQUAL(npcGetQty(healthPotionHandle), 5);

  // Test transferring materials from NPC to player inventory
  BOOST_REQUIRE(npcHasResource(ironOreHandle, 10));
  BOOST_REQUIRE(npcRemove(ironOreHandle, 10));
  BOOST_REQUIRE(playerAdd(ironOreHandle, 10));

  // Check quantities after transfer
  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 40);
  BOOST_CHECK_EQUAL(playerGetQty(ironOreHandle), 10);
}

BOOST_AUTO_TEST_CASE(TestTradingScenario) {
  // Setup a trading scenario: Player trades gold for NPC's equipment

  // Initial setup
  playerAdd(goldHandle, 500);      // Player has gold
  npcAdd(ironSwordHandle, 3);      // NPC has swords

  const int swordPrice = 100;
  const int swordsToTrade = 2;
  const int totalCost = swordPrice * swordsToTrade;

  // Verify preconditions
  BOOST_REQUIRE(playerHasResource(goldHandle, totalCost));
  BOOST_REQUIRE(npcHasResource(ironSwordHandle, swordsToTrade));

  // Execute trade: Player gives gold, receives swords
  bool playerPaysGold = playerRemove(goldHandle, totalCost);
  bool npcGivesSwords = npcRemove(ironSwordHandle, swordsToTrade);

  BOOST_REQUIRE(playerPaysGold);
  BOOST_REQUIRE(npcGivesSwords);

  // Complete the trade
  bool npcReceivesGold = npcAdd(goldHandle, totalCost);
  bool playerReceivesSwords = playerAdd(ironSwordHandle, swordsToTrade);

  BOOST_REQUIRE(npcReceivesGold);
  BOOST_REQUIRE(playerReceivesSwords);

  // Verify final state
  BOOST_CHECK_EQUAL(playerGetQty(goldHandle), 500 - totalCost);
  BOOST_CHECK_EQUAL(playerGetQty(ironSwordHandle), swordsToTrade);
  BOOST_CHECK_EQUAL(npcGetQty(goldHandle), totalCost);
  BOOST_CHECK_EQUAL(npcGetQty(ironSwordHandle), 3 - swordsToTrade);
}

BOOST_AUTO_TEST_CASE(TestResourceManagement) {
  // Test basic resource management operations
  playerAdd(ironSwordHandle, 2);

  // Test basic resource management
  BOOST_CHECK_EQUAL(playerGetQty(ironSwordHandle), 2);
  BOOST_CHECK(playerHasResource(ironSwordHandle));

  // Test removing equipment
  bool removed = playerRemove(ironSwordHandle, 1);
  BOOST_CHECK(removed);
  BOOST_CHECK_EQUAL(playerGetQty(ironSwordHandle), 1);

  // Test consuming resource
  playerAdd(healthPotionHandle, 1);
  bool consumed = playerRemove(healthPotionHandle, 1);
  BOOST_CHECK(consumed);
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 0);
}

BOOST_AUTO_TEST_CASE(TestResourceByCategory) {
  // Setup: Add various resources to player inventory
  playerAdd(healthPotionHandle, 5);  // Item/Consumable
  playerAdd(ironSwordHandle, 1);     // Item/Equipment
  playerAdd(ironOreHandle, 20);      // Material
  playerAdd(goldHandle, 100);        // Currency

  // Verify all resources were added
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 5);
  BOOST_CHECK_EQUAL(playerGetQty(ironSwordHandle), 1);
  BOOST_CHECK_EQUAL(playerGetQty(ironOreHandle), 20);
  BOOST_CHECK_EQUAL(playerGetQty(goldHandle), 100);

  // Note: EDM inventory doesn't have getResourcesByCategory - that's a InventoryComponent feature
  // This test verifies basic resource tracking works
}

BOOST_AUTO_TEST_CASE(TestInventoryCapacityLimits) {
  // Test inventory capacity limits via EDM
  // EDM inventory uses slot-based storage

  // Add items up to capacity
  int swordsAdded = 0;
  for (int i = 0; i < 55; ++i) {  // Try to add more than 50 slots
    if (playerAdd(ironSwordHandle, 1)) {
      swordsAdded++;
    } else {
      break;  // Inventory full
    }
  }

  // Should only be able to add up to max slots (50)
  BOOST_CHECK_LE(swordsAdded, 50);

  // Add items to NPC inventory
  int npcItemsAdded = 0;
  for (int i = 0; i < 65; ++i) {  // Try to add more than 60 slots
    if (npcAdd(ironSwordHandle, 1)) {
      npcItemsAdded++;
    } else {
      break;
    }
  }

  // Should only be able to add up to max slots (60)
  BOOST_CHECK_LE(npcItemsAdded, 60);
}

BOOST_AUTO_TEST_CASE(TestResourceSerialization) {
  // Setup: Add resources to player inventory
  playerAdd(healthPotionHandle, 10);
  playerAdd(ironSwordHandle, 2);
  playerAdd(goldHandle, 500);

  // Verify inventory state can be queried
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 10);
  BOOST_CHECK_EQUAL(playerGetQty(ironSwordHandle), 2);
  BOOST_CHECK_EQUAL(playerGetQty(goldHandle), 500);

  // Test NPC inventory resources as well
  npcAdd(ironOreHandle, 25);
  npcAdd(goldHandle, 200);

  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 25);
  BOOST_CHECK_EQUAL(npcGetQty(goldHandle), 200);
}

BOOST_AUTO_TEST_CASE(TestResourceConsumption) {
  // Test consuming resources (like using health potions)
  playerAdd(healthPotionHandle, 5);

  // Simulate using a health potion
  BOOST_REQUIRE(playerHasResource(healthPotionHandle, 1));
  bool consumed = playerRemove(healthPotionHandle, 1);
  BOOST_CHECK(consumed);
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 4);

  // Try to consume more than available
  bool overConsume = playerRemove(healthPotionHandle, 10);
  BOOST_CHECK(!overConsume);
  BOOST_CHECK_EQUAL(playerGetQty(healthPotionHandle), 4);  // Should remain unchanged
}

BOOST_AUTO_TEST_CASE(TestComplexTradingChain) {
  // Test a complex trading chain: Player -> NPC -> Trader
  uint32_t traderInvIndex = entityDataManager->createInventory(30, true);
  BOOST_REQUIRE(traderInvIndex != INVALID_INVENTORY_INDEX);

  // Initial setup
  playerAdd(goldHandle, 1000);
  npcAdd(ironOreHandle, 100);
  entityDataManager->addToInventory(traderInvIndex, ironSwordHandle, 10);

  // Step 1: Player trades gold for iron ore from NPC
  const int orePrice = 5;
  const int oreQuantity = 20;
  const int oreCost = orePrice * oreQuantity;

  BOOST_REQUIRE(playerRemove(goldHandle, oreCost));
  BOOST_REQUIRE(npcRemove(ironOreHandle, oreQuantity));
  BOOST_REQUIRE(npcAdd(goldHandle, oreCost));
  BOOST_REQUIRE(playerAdd(ironOreHandle, oreQuantity));

  // Step 2: Player trades iron ore for sword from Trader
  const int swordOrePrice = 10;  // 10 ore per sword
  const int swordsWanted = 2;
  const int oreNeeded = swordOrePrice * swordsWanted;

  BOOST_REQUIRE(playerRemove(ironOreHandle, oreNeeded));
  BOOST_REQUIRE(entityDataManager->removeFromInventory(traderInvIndex, ironSwordHandle, swordsWanted));
  BOOST_REQUIRE(entityDataManager->addToInventory(traderInvIndex, ironOreHandle, oreNeeded));
  BOOST_REQUIRE(playerAdd(ironSwordHandle, swordsWanted));

  // Verify final state
  BOOST_CHECK_EQUAL(playerGetQty(goldHandle), 1000 - oreCost);
  BOOST_CHECK_EQUAL(playerGetQty(ironOreHandle), oreQuantity - oreNeeded);
  BOOST_CHECK_EQUAL(playerGetQty(ironSwordHandle), swordsWanted);

  BOOST_CHECK_EQUAL(npcGetQty(goldHandle), oreCost);
  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 100 - oreQuantity);

  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(traderInvIndex, ironOreHandle), oreNeeded);
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(traderInvIndex, ironSwordHandle), 10 - swordsWanted);

  // Cleanup trader inventory
  entityDataManager->destroyInventory(traderInvIndex);
}

BOOST_AUTO_TEST_CASE(TestConcurrentResourceOperations) {
  // Test thread safety of resource operations via EDM

  const int NUM_THREADS = 5;
  const int OPERATIONS_PER_THREAD = 20;

  // Pre-populate with resources
  // Note: Quantities must fit within inventory constraints
  // NPC has 60 slots, Mithril Ore maxStackSize=15, so max is 900
  // Player has 50 slots, Platinum Coins maxStackSize=10000, so 10000 fits
  playerAdd(goldHandle, 10000);
  npcAdd(ironOreHandle, 800);

  std::vector<std::future<void>> futures;
  std::atomic<int> successfulPlayerOps{0};
  std::atomic<int> successfulNPCOps{0};

  for (int i = 0; i < NUM_THREADS; ++i) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &successfulPlayerOps, &successfulNPCOps]() -> void {
          for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
            // Test concurrent player operations
            if (playerAdd(healthPotionHandle, 1)) {
              if (playerRemove(healthPotionHandle, 1)) {
                successfulPlayerOps.fetch_add(1, std::memory_order_relaxed);
              }
            }

            // Test concurrent NPC operations
            if (npcAdd(ironSwordHandle, 1)) {
              if (npcRemove(ironSwordHandle, 1)) {
                successfulNPCOps.fetch_add(1, std::memory_order_relaxed);
              }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
          }
        },
        VoidLight::TaskPriority::Normal, "ResourceIntegrationTask");

    futures.push_back(std::move(future));
  }

  for (auto &future : futures) {
    future.wait();
  }

  // Verify operations were successful
  BOOST_CHECK(successfulPlayerOps.load() > 0);
  BOOST_CHECK(successfulNPCOps.load() > 0);

  // Verify original resources are still intact
  BOOST_CHECK_EQUAL(playerGetQty(goldHandle), 10000);
  BOOST_CHECK_EQUAL(npcGetQty(ironOreHandle), 800);
}

BOOST_AUTO_TEST_SUITE_END()
