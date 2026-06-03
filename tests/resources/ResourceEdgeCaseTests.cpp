/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ResourceEdgeCaseTests
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "entities/Resource.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "utils/ResourceHandle.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <random>
#include <set>
#include <vector>

using VoidLight::ResourceHandle;

struct ResourceEdgeCaseFixture {
  ResourceTemplateManager *templateManager;
  EntityDataManager *entityDataManager;
  WorldResourceManager *worldManager;
  VoidLight::ThreadSystem *threadSystem;

  ResourceEdgeCaseFixture() {
    // Initialize ThreadSystem first for threading tests
    threadSystem = &VoidLight::ThreadSystem::Instance();
    if (threadSystem->isShutdown() || threadSystem->getThreadCount() == 0) {
      bool initSuccess = threadSystem->init();
      if (!initSuccess && threadSystem->getThreadCount() == 0) {
        throw std::runtime_error(
            "Failed to initialize ThreadSystem for threading tests");
      }
    }

    templateManager = &ResourceTemplateManager::Instance();
    entityDataManager = &EntityDataManager::Instance();
    worldManager = &WorldResourceManager::Instance();

    // Clean and reinitialize managers
    templateManager->clean();
    entityDataManager->clean();
    worldManager->clean();

    BOOST_REQUIRE(templateManager->init());
    BOOST_REQUIRE(entityDataManager->init());
    BOOST_REQUIRE(worldManager->init());
  }

  ~ResourceEdgeCaseFixture() {
    worldManager->clean();
    entityDataManager->clean();
    templateManager->clean();
  }

  ResourcePtr
  createTestResource(const std::string &name,
                     ResourceCategory category = ResourceCategory::Material,
                     ResourceType type = ResourceType::RawResource) {
    auto handle = templateManager->generateHandle();
    std::string id = "test_" + name;
    return std::make_shared<Resource>(handle, id, name, category, type);
  }
};

BOOST_FIXTURE_TEST_SUITE(ResourceEdgeCaseTestSuite, ResourceEdgeCaseFixture)

//==============================================================================
// Handle Lifecycle Edge Cases
//==============================================================================

BOOST_AUTO_TEST_CASE(TestHandleOverflowProtection) {
  // Test behavior when handle ID approaches maximum values
  std::vector<ResourceHandle> handles;

  // Generate a large number of handles to test for overflow protection
  const size_t NUM_HANDLES = 10000;
  handles.reserve(NUM_HANDLES);

  for (size_t i = 0; i < NUM_HANDLES; ++i) {
    auto handle = templateManager->generateHandle();
    BOOST_CHECK(handle.isValid());
    handles.push_back(handle);
  }

  // Verify all handles are unique
  std::set<ResourceHandle> uniqueHandles(handles.begin(), handles.end());
  BOOST_CHECK_EQUAL(uniqueHandles.size(), handles.size());
}

BOOST_AUTO_TEST_CASE(TestStaleHandleDetection) {
  // Create and register a resource
  auto resource = createTestResource("TestStaleResource");
  auto handle = resource->getHandle();

  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));
  BOOST_CHECK(templateManager->getResourceTemplate(handle) != nullptr);

  // Remove the resource to make handle stale
  templateManager->removeResourceTemplate(handle);

  // Verify stale handle is detected
  BOOST_CHECK(templateManager->getResourceTemplate(handle) == nullptr);

  // Test operations with stale handle
  auto newResource = createTestResource("NewResource");
  auto newHandle = newResource->getHandle();

  // Verify new handle is different from stale one
  BOOST_CHECK(handle.getId() != newHandle.getId() ||
              handle.getGeneration() != newHandle.getGeneration());
}

BOOST_AUTO_TEST_CASE(TestInvalidHandleOperations) {
  ResourceHandle invalidHandle; // Default invalid handle

  BOOST_CHECK(!invalidHandle.isValid());
  BOOST_CHECK_EQUAL(invalidHandle.getId(), ResourceHandle::INVALID_ID);
  BOOST_CHECK_EQUAL(invalidHandle.getGeneration(),
                    ResourceHandle::INVALID_GENERATION);

  // Test operations with invalid handle
  BOOST_CHECK(templateManager->getResourceTemplate(invalidHandle) == nullptr);

  // Test EDM inventory operations with invalid handle (should fail gracefully)
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  // Adding invalid resource should fail
  bool added = entityDataManager->addToInventory(invIndex, invalidHandle, 100);
  BOOST_CHECK(!added);

  // Query with invalid handle should return 0
  int qty = entityDataManager->getInventoryQuantity(invIndex, invalidHandle);
  BOOST_CHECK_EQUAL(qty, 0);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
}

//==============================================================================
// Concurrent Access and Race Conditions
//==============================================================================

BOOST_AUTO_TEST_CASE(TestConcurrentHandleGeneration) {
  const int NUM_THREADS = 8;
  const int HANDLES_PER_THREAD = 1000;

  std::vector<std::future<std::vector<ResourceHandle>>> futures;
  std::vector<ResourceHandle> allHandles;

  // Launch multiple tasks generating handles using ThreadSystem
  for (int t = 0; t < NUM_THREADS; ++t) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this]() -> std::vector<ResourceHandle> {
          std::vector<ResourceHandle> threadHandles;
          threadHandles.reserve(HANDLES_PER_THREAD);

          for (int i = 0; i < HANDLES_PER_THREAD; ++i) {
            auto handle = templateManager->generateHandle();
            threadHandles.push_back(handle);
          }
          return threadHandles;
        },
        VoidLight::TaskPriority::Normal, "HandleGenerationTask");

    futures.push_back(std::move(future));
  }

  // Collect all handles
  for (auto &future : futures) {
    auto threadHandles = future.get();
    allHandles.insert(allHandles.end(), threadHandles.begin(),
                      threadHandles.end());
  }

  // Process any events generated during handle creation
  EventManager::Instance().update();

  // Validate all handles are valid
  for (const auto &handle : allHandles) {
    BOOST_CHECK(handle.isValid());
  }

  // Verify all handles are unique (no race conditions in generation)
  std::set<ResourceHandle> uniqueHandles(allHandles.begin(), allHandles.end());
  BOOST_CHECK_EQUAL(uniqueHandles.size(), allHandles.size());
}

BOOST_AUTO_TEST_CASE(TestConcurrentInventoryOperations) {
  // Create test resources
  auto goldResource = createTestResource("ConcurrentGold");
  auto silverResource = createTestResource("ConcurrentSilver");

  BOOST_REQUIRE(templateManager->registerResourceTemplate(goldResource));
  BOOST_REQUIRE(templateManager->registerResourceTemplate(silverResource));

  auto goldHandle = goldResource->getHandle();
  auto silverHandle = silverResource->getHandle();

  // Create EDM inventory with initial resources
  uint32_t invIndex = entityDataManager->createInventory(100, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  // Set initial quantities
  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, goldHandle, 1000));
  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, silverHandle, 2000));

  const int NUM_THREADS = 4;
  const int OPERATIONS_PER_THREAD = 100;

  std::atomic<int> totalAdded{0};
  std::atomic<int> totalRemoved{0};
  std::vector<std::future<void>> futures;

  // Launch tasks performing concurrent operations using ThreadSystem
  for (int t = 0; t < NUM_THREADS; ++t) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &totalAdded, &totalRemoved]() -> void {
          std::random_device rd;
          std::mt19937 gen(rd());
          std::uniform_int_distribution<> amountDist(1, 10);
          std::uniform_int_distribution<> operationDist(0, 1);
          std::uniform_int_distribution<> resourceDist(0, 1);

          for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
            auto handle = (resourceDist(gen) == 0) ? goldHandle : silverHandle;
            int amount = amountDist(gen);

            if (operationDist(gen) == 0) {
              // Add operation
              if (entityDataManager->addToInventory(invIndex, handle, amount)) {
                totalAdded.fetch_add(amount, std::memory_order_relaxed);
              }
            } else {
              // Remove operation
              if (entityDataManager->removeFromInventory(invIndex, handle, amount)) {
                totalRemoved.fetch_add(amount, std::memory_order_relaxed);
              }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
          }
        },
        VoidLight::TaskPriority::Normal, "ConcurrentInventoryOpsTask");

    futures.push_back(std::move(future));
  }

  // Wait for all operations to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Verify final state is consistent
  auto finalGold = entityDataManager->getInventoryQuantity(invIndex, goldHandle);
  auto finalSilver = entityDataManager->getInventoryQuantity(invIndex, silverHandle);

  BOOST_CHECK_GE(finalGold, 0);
  BOOST_CHECK_GE(finalSilver, 0);

  // Total final quantity should equal initial + added - removed
  auto expectedTotal = 3000 + totalAdded.load() - totalRemoved.load();
  auto actualTotal = finalGold + finalSilver;

  BOOST_CHECK_EQUAL(actualTotal, expectedTotal);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
}

//==============================================================================
// Memory Pressure and Resource Exhaustion
//==============================================================================

BOOST_AUTO_TEST_CASE(TestLargeNumberOfResources) {
  const size_t LARGE_COUNT = 50000;
  std::vector<ResourcePtr> resources;
  resources.reserve(LARGE_COUNT);

  // Create large number of resources
  for (size_t i = 0; i < LARGE_COUNT; ++i) {
    auto resource = createTestResource("LargeTest_" + std::to_string(i));
    resources.push_back(resource);

    // Add every 100th resource to template manager to test memory usage
    if (i % 100 == 0) {
      BOOST_CHECK(templateManager->registerResourceTemplate(resource));
    }
  }

  // Verify system stability under memory pressure
  BOOST_CHECK(templateManager->isInitialized());
  BOOST_CHECK_GT(templateManager->getResourceTemplateCount(), 0);

  // Test cleanup under memory pressure
  resources.clear();

  // Force garbage collection and verify system stability
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  BOOST_CHECK(templateManager->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestExtremeQuantityValues) {
  auto resource = createTestResource("ExtremeQuantityTest");
  // Set a known max stack size for predictable capacity testing
  resource->setMaxStackSize(100);
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  auto handle = resource->getHandle();

  // Create inventory with 100 slots
  const uint16_t SLOT_COUNT = 100;
  uint32_t invIndex = entityDataManager->createInventory(SLOT_COUNT, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  // Calculate expected capacity: slots * maxStackSize
  const int MAX_STACK = 100;
  const int MAX_CAPACITY = SLOT_COUNT * MAX_STACK;  // 10,000 items

  // Test adding up to capacity
  bool added = entityDataManager->addToInventory(invIndex, handle, MAX_CAPACITY);
  BOOST_CHECK(added);
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle),
                    MAX_CAPACITY);

  // Test adding beyond capacity fails
  bool addedBeyond = entityDataManager->addToInventory(invIndex, handle, 1);
  BOOST_CHECK(!addedBeyond);  // Should fail - inventory full

  // Test underflow protection - removing more than available
  bool removed = entityDataManager->removeFromInventory(invIndex, handle, MAX_CAPACITY + 100);
  BOOST_CHECK(!removed);  // Should fail
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle),
                    MAX_CAPACITY);  // Unchanged

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_CASE(TestInventoryAddFailureDoesNotPartiallyMutate) {
  auto resource = createTestResource("AtomicInventoryAddTest");
  resource->setMaxStackSize(10);
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  auto handle = resource->getHandle();
  uint32_t invIndex = entityDataManager->createInventory(1, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  const int maxStack = templateManager->getMaxStackSize(handle);
  const int actualInlineCapacity =
      static_cast<int>(InventoryData::INLINE_SLOT_COUNT) * maxStack;
  const int initialQuantity = actualInlineCapacity - 2;

  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, handle, initialQuantity));
  BOOST_CHECK(!entityDataManager->addToInventory(invIndex, handle, 3));
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle),
                    initialQuantity);

  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_CASE(TestInventorySlotSwapPreservesOccupiedStacks) {
  auto firstResource = createTestResource("SlotSwapFirst");
  auto secondResource = createTestResource("SlotSwapSecond");
  BOOST_REQUIRE(templateManager->registerResourceTemplate(firstResource));
  BOOST_REQUIRE(templateManager->registerResourceTemplate(secondResource));

  const auto firstHandle = firstResource->getHandle();
  const auto secondHandle = secondResource->getHandle();
  uint32_t invIndex = entityDataManager->createInventory(5, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, firstHandle, 4));
  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, secondHandle, 7));

  const uint16_t usedSlotsBefore = entityDataManager->getInventoryData(invIndex).usedSlots;
  BOOST_REQUIRE(entityDataManager->swapInventorySlots(invIndex, 0, 1));

  const InventorySlotData slot0 = entityDataManager->getInventorySlot(invIndex, 0);
  const InventorySlotData slot1 = entityDataManager->getInventorySlot(invIndex, 1);
  BOOST_CHECK(slot0.resourceHandle == secondHandle);
  BOOST_CHECK_EQUAL(slot0.quantity, 7);
  BOOST_CHECK(slot1.resourceHandle == firstHandle);
  BOOST_CHECK_EQUAL(slot1.quantity, 4);
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryData(invIndex).usedSlots, usedSlotsBefore);

  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_CASE(TestInventorySlotSwapMovesOccupiedStackToEmptySlot) {
  auto resource = createTestResource("SlotMoveToEmpty");
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  const auto handle = resource->getHandle();
  uint32_t invIndex = entityDataManager->createInventory(5, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, handle, 6));

  const uint16_t usedSlotsBefore = entityDataManager->getInventoryData(invIndex).usedSlots;
  BOOST_REQUIRE(entityDataManager->swapInventorySlots(invIndex, 0, 4));

  BOOST_CHECK(entityDataManager->getInventorySlot(invIndex, 0).isEmpty());
  const InventorySlotData slot4 = entityDataManager->getInventorySlot(invIndex, 4);
  BOOST_CHECK(slot4.resourceHandle == handle);
  BOOST_CHECK_EQUAL(slot4.quantity, 6);
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryData(invIndex).usedSlots, usedSlotsBefore);

  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_CASE(TestInventorySlotSwapAllowsSameSlotNoOpAndRejectsInvalidInput) {
  auto resource = createTestResource("SlotSwapInvalidInput");
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  const auto handle = resource->getHandle();
  uint32_t invIndex = entityDataManager->createInventory(2, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, handle, 3));

  BOOST_CHECK(entityDataManager->swapInventorySlots(invIndex, 0, 0));
  BOOST_CHECK(entityDataManager->getInventorySlot(invIndex, 0).resourceHandle == handle);
  BOOST_CHECK(!entityDataManager->swapInventorySlots(INVALID_INVENTORY_INDEX, 0, 1));
  BOOST_CHECK(!entityDataManager->swapInventorySlots(invIndex, 0, 2));
  BOOST_CHECK(!entityDataManager->swapInventorySlots(invIndex, 2, 0));

  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_CASE(TestInventorySlotSwapCrossesInlineAndOverflowStorage) {
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  std::vector<ResourceHandle> handles;
  handles.reserve(9);
  for (int i = 0; i < 9; ++i) {
    auto resource = createTestResource("InlineOverflowSlot" + std::to_string(i));
    BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));
    handles.push_back(resource->getHandle());
    BOOST_REQUIRE(entityDataManager->addToInventory(invIndex, handles.back(), i + 1));
  }

  const uint16_t usedSlotsBefore = entityDataManager->getInventoryData(invIndex).usedSlots;
  BOOST_REQUIRE(entityDataManager->swapInventorySlots(
      invIndex, 0, InventoryData::INLINE_SLOT_COUNT));

  const InventorySlotData inlineSlot = entityDataManager->getInventorySlot(invIndex, 0);
  const InventorySlotData overflowSlot =
      entityDataManager->getInventorySlot(invIndex, InventoryData::INLINE_SLOT_COUNT);
  BOOST_CHECK(inlineSlot.resourceHandle == handles[InventoryData::INLINE_SLOT_COUNT]);
  BOOST_CHECK_EQUAL(inlineSlot.quantity, InventoryData::INLINE_SLOT_COUNT + 1);
  BOOST_CHECK(overflowSlot.resourceHandle == handles[0]);
  BOOST_CHECK_EQUAL(overflowSlot.quantity, 1);
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryData(invIndex).usedSlots, usedSlotsBefore);

  entityDataManager->destroyInventory(invIndex);
}

//==============================================================================
// Malformed Input and Error Recovery
//==============================================================================

BOOST_AUTO_TEST_CASE(TestNullPointerHandling) {
  // Test adding null resource template
  BOOST_CHECK(!templateManager->registerResourceTemplate(nullptr));

  // Test operations with null inventory component
  auto resource = createTestResource("NullTest");
  auto handle = resource->getHandle();

  // These operations should handle null gracefully
  BOOST_CHECK_NO_THROW(templateManager->getResourceTemplate(handle));
}

BOOST_AUTO_TEST_CASE(TestEmptyStringHandling) {
  // Test resource with empty name
  auto emptyNameResource = createTestResource("");
  BOOST_CHECK(templateManager->registerResourceTemplate(emptyNameResource));

  // Test getHandleByName with empty string
  auto handle = templateManager->getHandleByName("");
  BOOST_CHECK(handle.isValid()); // Should find the empty-named resource
}

BOOST_AUTO_TEST_CASE(TestDuplicateResourceHandling) {
  auto resource1 = createTestResource("DuplicateTest");
  auto resource2 = createTestResource("DuplicateTest"); // Same name, different handle

  BOOST_CHECK(templateManager->registerResourceTemplate(resource1));
  // Second registration should fail due to duplicate name enforcement
  BOOST_CHECK(!templateManager->registerResourceTemplate(resource2));

  // Only the first resource should be registered
  BOOST_CHECK_GT(templateManager->getResourceTemplateCount(), 0);

  // Name lookup should return the first resource
  auto foundHandle = templateManager->getHandleByName("DuplicateTest");
  BOOST_CHECK(foundHandle.isValid());
  BOOST_CHECK_EQUAL(foundHandle.getId(), resource1->getHandle().getId());
}

//==============================================================================
// Performance Under Extreme Load
//==============================================================================

BOOST_AUTO_TEST_CASE(TestRapidOperationSequences) {
  auto resource = createTestResource("RapidTest");
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  auto handle = resource->getHandle();

  // Create EDM inventory
  uint32_t invIndex = entityDataManager->createInventory(100, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  const int RAPID_OPERATIONS = 10000;

  // Enable benchmark mode to disable debug logging during performance test
  VOIDLIGHT_ENABLE_BENCHMARK_MODE();

  auto startTime = std::chrono::high_resolution_clock::now();

  // Perform rapid add/remove sequences
  for (int i = 0; i < RAPID_OPERATIONS; ++i) {
    entityDataManager->addToInventory(invIndex, handle, 1);
    entityDataManager->removeFromInventory(invIndex, handle, 1);
  }

  // Process all deferred events before measuring end time
  EventManager::Instance().update();

  auto endTime = std::chrono::high_resolution_clock::now();

  // Re-enable logging for test output
  VOIDLIGHT_DISABLE_BENCHMARK_MODE();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - startTime);

  // Should complete in reasonable time
  BOOST_CHECK_LT(duration.count(), 1000);

  // Final quantity should be 0
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle), 0);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_CASE(TestHighFrequencyCallbacks) {
  auto resource = createTestResource("CallbackTest");
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  auto handle = resource->getHandle();
  const int EXPECTED_OPERATIONS = 1000;

  // Create EDM inventory
  uint32_t invIndex = entityDataManager->createInventory(100, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  // Perform operations that would trigger callbacks
  for (int i = 0; i < EXPECTED_OPERATIONS; ++i) {
    entityDataManager->addToInventory(invIndex, handle, 1);
    entityDataManager->removeFromInventory(invIndex, handle, 1);
  }

  // Allow time for any asynchronous callback processing
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Verify final state is consistent
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle), 0);

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
}

//==============================================================================
// System Integration Edge Cases
//==============================================================================

BOOST_AUTO_TEST_CASE(TestManagerShutdownAndReinit) {
  // Create resources
  auto resource = createTestResource("ShutdownTest");
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));

  auto handle = resource->getHandle();

  // Create EDM inventory
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);
  entityDataManager->addToInventory(invIndex, handle, 500);

  // Verify initial state
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle), 500);
  BOOST_CHECK_GT(templateManager->getResourceTemplateCount(), 0);

  // Shutdown managers
  worldManager->clean();
  entityDataManager->clean();
  templateManager->clean();

  // Verify shutdown state
  BOOST_CHECK(!templateManager->isInitialized());
  BOOST_CHECK_EQUAL(templateManager->getResourceTemplateCount(), 0);

  // Reinitialize
  BOOST_REQUIRE(templateManager->init());
  BOOST_REQUIRE(entityDataManager->init());
  BOOST_REQUIRE(worldManager->init());

  // Verify clean reinitialization
  BOOST_CHECK(templateManager->isInitialized());
  BOOST_CHECK_GT(templateManager->getResourceTemplateCount(), 0); // Default resources loaded

  // Original resource should be gone
  BOOST_CHECK(templateManager->getResourceTemplate(handle) == nullptr);
}

BOOST_AUTO_TEST_CASE(TestCrossManagerConsistency) {
  // Test consistency between template and EDM
  auto resource = createTestResource("ConsistencyTest");
  auto handle = resource->getHandle();

  // Add to template manager only
  BOOST_REQUIRE(templateManager->registerResourceTemplate(resource));
  BOOST_CHECK(templateManager->getResourceTemplate(handle) != nullptr);

  // Create EDM inventory
  uint32_t invIndex = entityDataManager->createInventory(10, true);
  BOOST_REQUIRE(invIndex != INVALID_INVENTORY_INDEX);

  // Initially no quantity
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle), 0);

  // Add quantity to inventory
  bool added = entityDataManager->addToInventory(invIndex, handle, 100);
  BOOST_CHECK(added);
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle), 100);

  // Allow EventManager to process any deferred events
  EventManager::Instance().update();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Remove from template manager
  templateManager->removeResourceTemplate(handle);
  BOOST_CHECK(templateManager->getResourceTemplate(handle) == nullptr);

  // Inventory quantity should still exist (EDM doesn't depend on template for existing data)
  BOOST_CHECK_EQUAL(entityDataManager->getInventoryQuantity(invIndex, handle), 100);

  // Allow EventManager to process any deferred events
  EventManager::Instance().update();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Operations on orphaned handle should fail (template doesn't exist)
  bool added2 = entityDataManager->addToInventory(invIndex, handle, 50);
  BOOST_CHECK(!added2);  // Should fail - resource template doesn't exist

  // Cleanup
  entityDataManager->destroyInventory(invIndex);
}

BOOST_AUTO_TEST_SUITE_END()
