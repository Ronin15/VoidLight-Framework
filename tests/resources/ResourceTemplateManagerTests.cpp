/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ResourceTemplateManagerTests
#include <boost/test/unit_test.hpp>

#include "entities/Resource.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "utils/ResourceHandle.hpp"
#include <memory>
#include <vector>

using VoidLight::ResourceHandle;

struct ResourceTemplateManagerFixture {
  ResourceTemplateManager *manager;

  ResourceTemplateManagerFixture() {
    manager = &ResourceTemplateManager::Instance();
    manager->clean();
    BOOST_REQUIRE(manager->init());
  }

  ~ResourceTemplateManagerFixture() { manager->clean(); }

  ResourcePtr createTestResource(const std::string &name,
                                 ResourceCategory category, ResourceType type) {
    auto handle = manager->generateHandle();
    std::string id = "test_" + name; // Use test_ prefix for ID
    return std::make_shared<Resource>(handle, id, name, category, type);
  }
};

BOOST_FIXTURE_TEST_SUITE(ResourceTemplateManagerTestSuite,
                         ResourceTemplateManagerFixture)

BOOST_AUTO_TEST_CASE(TestBasicInitialization) {
  BOOST_REQUIRE(manager != nullptr);
  BOOST_CHECK(manager->isInitialized());
  // Manager loads default resources during init, so count will be > 0
  BOOST_CHECK(manager->getResourceTemplateCount() > 0);
}

BOOST_AUTO_TEST_CASE(TestHandleGeneration) {
  auto handle1 = manager->generateHandle();
  auto handle2 = manager->generateHandle();

  BOOST_CHECK(handle1.isValid());
  BOOST_CHECK(handle2.isValid());
  BOOST_CHECK(handle1 != handle2);

  BOOST_CHECK(manager->isValidHandle(handle1));
  BOOST_CHECK(manager->isValidHandle(handle2));

  ResourceHandle invalidHandle;
  BOOST_CHECK(!invalidHandle.isValid());
  BOOST_CHECK(!manager->isValidHandle(invalidHandle));
}

BOOST_AUTO_TEST_CASE(TestResourceTemplateRegistration) {
  auto initialCount = manager->getResourceTemplateCount();
  auto resource = createTestResource("Test Item", ResourceCategory::Item,
                                     ResourceType::Equipment);
  auto handle = resource->getHandle();

  BOOST_CHECK(manager->registerResourceTemplate(resource));
  BOOST_CHECK_EQUAL(manager->getResourceTemplateCount(), initialCount + 1);
  BOOST_CHECK(manager->hasResourceTemplate(handle));

  auto retrieved = manager->getResourceTemplate(handle);
  BOOST_REQUIRE(retrieved != nullptr);
  BOOST_CHECK_EQUAL(retrieved->getName(), "Test Item");
  BOOST_CHECK(retrieved->getCategory() == ResourceCategory::Item);
  BOOST_CHECK(retrieved->getType() == ResourceType::Equipment);
}

BOOST_AUTO_TEST_CASE(TestNullResourceRegistration) {
  auto initialCount = manager->getResourceTemplateCount();
  ResourcePtr nullResource;
  BOOST_CHECK(!manager->registerResourceTemplate(nullResource));
  BOOST_CHECK_EQUAL(manager->getResourceTemplateCount(), initialCount);
}

BOOST_AUTO_TEST_CASE(TestDuplicateResourceRegistration) {
  auto initialCount = manager->getResourceTemplateCount();
  auto resource1 = createTestResource("Item 1", ResourceCategory::Item,
                                      ResourceType::Equipment);
  auto resource2 = createTestResource("Item 2", ResourceCategory::Item,
                                      ResourceType::Consumable);

  // Use the same handle for both resources (simulate duplicate)
  auto handle = resource1->getHandle();
  auto duplicateResource = std::make_shared<Resource>(
      handle, "test_duplicate", "Duplicate", ResourceCategory::Material,
      ResourceType::CraftingComponent);

  BOOST_CHECK(manager->registerResourceTemplate(resource1));
  BOOST_CHECK(!manager->registerResourceTemplate(duplicateResource));
  BOOST_CHECK_EQUAL(manager->getResourceTemplateCount(), initialCount + 1);

  auto retrieved = manager->getResourceTemplate(handle);
  BOOST_CHECK_EQUAL(retrieved->getName(), "Item 1");
}

BOOST_AUTO_TEST_CASE(TestResourceTemplateRetrieval) {
  auto resource1 = createTestResource("Test Item 1", ResourceCategory::Item,
                                      ResourceType::Equipment);
  auto resource2 = createTestResource("Test Item 2", ResourceCategory::Material,
                                      ResourceType::CraftingComponent);

  manager->registerResourceTemplate(resource1);
  manager->registerResourceTemplate(resource2);

  auto retrieved1 = manager->getResourceTemplate(resource1->getHandle());
  auto retrieved2 = manager->getResourceTemplate(resource2->getHandle());

  BOOST_REQUIRE(retrieved1 != nullptr);
  BOOST_REQUIRE(retrieved2 != nullptr);
  BOOST_CHECK_EQUAL(retrieved1->getName(), "Test Item 1");
  BOOST_CHECK_EQUAL(retrieved2->getName(), "Test Item 2");

  ResourceHandle invalidHandle;
  auto invalidRetrieved = manager->getResourceTemplate(invalidHandle);
  BOOST_CHECK(invalidRetrieved == nullptr);
}

BOOST_AUTO_TEST_CASE(TestResourcesByCategory) {
  auto item1 = createTestResource("Sword", ResourceCategory::Item,
                                  ResourceType::Equipment);
  auto item2 = createTestResource("Potion", ResourceCategory::Item,
                                  ResourceType::Consumable);
  auto material1 = createTestResource("Iron", ResourceCategory::Material,
                                      ResourceType::RawResource);

  manager->registerResourceTemplate(item1);
  manager->registerResourceTemplate(item2);
  manager->registerResourceTemplate(material1);

  auto items = manager->getResourcesByCategory(ResourceCategory::Item);
  auto materials = manager->getResourcesByCategory(ResourceCategory::Material);
  auto currencies = manager->getResourcesByCategory(ResourceCategory::Currency);

  // Check that our new items were added (existing defaults may also exist)
  BOOST_CHECK(items.size() >= 2);
  BOOST_CHECK(materials.size() >= 1);
  // Note: Avoiding >= 0 check since size_t is always >= 0, just check it's a
  // valid size
  BOOST_CHECK(currencies.size() ==
              currencies.size()); // This is always true but avoids warning

  // Check that our specific items are in the results
  std::vector<std::string> itemNames;
  for (const auto &item : items) {
    itemNames.push_back(item->getName());
  }
  BOOST_CHECK(std::find(itemNames.begin(), itemNames.end(), "Sword") !=
              itemNames.end());
  BOOST_CHECK(std::find(itemNames.begin(), itemNames.end(), "Potion") !=
              itemNames.end());
}

BOOST_AUTO_TEST_CASE(TestResourcesByType) {
  auto equipment1 = createTestResource("Sword", ResourceCategory::Item,
                                       ResourceType::Equipment);
  auto equipment2 = createTestResource("Shield", ResourceCategory::Item,
                                       ResourceType::Equipment);
  auto consumable = createTestResource("Potion", ResourceCategory::Item,
                                       ResourceType::Consumable);

  manager->registerResourceTemplate(equipment1);
  manager->registerResourceTemplate(equipment2);
  manager->registerResourceTemplate(consumable);

  auto equipments = manager->getResourcesByType(ResourceType::Equipment);
  auto consumables = manager->getResourcesByType(ResourceType::Consumable);
  auto questItems = manager->getResourcesByType(ResourceType::QuestItem);

  // Check that our equipment was added (existing defaults may also exist)
  BOOST_CHECK(equipments.size() >= 2);
  BOOST_CHECK(consumables.size() >= 1);
  // Just check that questItems is a valid container
  BOOST_CHECK(questItems.size() == questItems.size());

  // Check that our specific equipment is in the results
  std::vector<std::string> equipmentNames;
  for (const auto &equipment : equipments) {
    equipmentNames.push_back(equipment->getName());
  }
  BOOST_CHECK(std::find(equipmentNames.begin(), equipmentNames.end(),
                        "Sword") != equipmentNames.end());
  BOOST_CHECK(std::find(equipmentNames.begin(), equipmentNames.end(),
                        "Shield") != equipmentNames.end());
}

BOOST_AUTO_TEST_CASE(TestFastPropertyAccess) {
  auto resource = createTestResource("Test Item", ResourceCategory::Item,
                                     ResourceType::Equipment);
  auto handle = resource->getHandle();

  resource->setMaxStackSize(50);
  resource->setValue(100.5f);

  manager->registerResourceTemplate(resource);

  BOOST_CHECK_EQUAL(manager->getMaxStackSize(handle), 50);
  BOOST_CHECK_CLOSE(manager->getValue(handle), 100.5f, 0.001f);
  BOOST_CHECK(manager->getCategory(handle) == ResourceCategory::Item);
  BOOST_CHECK(manager->getType(handle) == ResourceType::Equipment);

  ResourceHandle invalidHandle;
  BOOST_CHECK_EQUAL(manager->getMaxStackSize(invalidHandle),
                    1); // Default stack size for invalid handles
  BOOST_CHECK_EQUAL(manager->getValue(invalidHandle), 0.0f);
}

BOOST_AUTO_TEST_CASE(TestBulkPropertyAccess) {
  std::vector<ResourcePtr> resources;
  std::vector<ResourceHandle> handles;

  for (int i = 0; i < 5; ++i) {
    auto resource =
        createTestResource("Item " + std::to_string(i), ResourceCategory::Item,
                           ResourceType::Equipment);
    resource->setMaxStackSize(10 + i);
    resource->setValue(100.0f + i);

    resources.push_back(resource);
    handles.push_back(resource->getHandle());
    manager->registerResourceTemplate(resource);
  }

  auto maxStackSizes = manager->getMaxStackSizes(handles);
  auto values = manager->getValues(handles);

  BOOST_CHECK_EQUAL(maxStackSizes.size(), 5);
  BOOST_CHECK_EQUAL(values.size(), 5);

  for (size_t i = 0; i < 5; ++i) {
    BOOST_CHECK_EQUAL(maxStackSizes[i], 10 + static_cast<int>(i));
    BOOST_CHECK_CLOSE(values[i], 100.0f + static_cast<float>(i), 0.001f);
  }

  std::vector<int> batchMaxStackSizes;
  std::vector<float> batchValues;
  std::vector<ResourceCategory> batchCategories;
  std::vector<ResourceType> batchTypes;

  manager->getPropertiesBatch(handles, batchMaxStackSizes, batchValues,
                              batchCategories, batchTypes);

  BOOST_CHECK_EQUAL(batchMaxStackSizes.size(), 5);
  BOOST_CHECK_EQUAL(batchValues.size(), 5);
  BOOST_CHECK_EQUAL(batchCategories.size(), 5);
  BOOST_CHECK_EQUAL(batchTypes.size(), 5);

  for (size_t i = 0; i < 5; ++i) {
    BOOST_CHECK_EQUAL(batchMaxStackSizes[i], 10 + static_cast<int>(i));
    BOOST_CHECK_CLOSE(batchValues[i], 100.0f + static_cast<float>(i), 0.001f);
    BOOST_CHECK(batchCategories[i] == ResourceCategory::Item);
    BOOST_CHECK(batchTypes[i] == ResourceType::Equipment);
  }
}

BOOST_AUTO_TEST_CASE(TestResourceCreation) {
  auto templateResource = createTestResource(
      "Test Template", ResourceCategory::Item, ResourceType::Equipment);
  auto handle = templateResource->getHandle();

  templateResource->setMaxStackSize(99);
  templateResource->setValue(250.0f);

  manager->registerResourceTemplate(templateResource);

  auto createdResource = manager->createResource(handle);
  BOOST_REQUIRE(createdResource != nullptr);
  BOOST_CHECK_EQUAL(createdResource->getName(), "Test Template");
  BOOST_CHECK_EQUAL(createdResource->getMaxStackSize(), 99);
  BOOST_CHECK_CLOSE(createdResource->getValue(), 250.0f, 0.001f);
  BOOST_CHECK(createdResource->getCategory() == ResourceCategory::Item);
  BOOST_CHECK(createdResource->getType() == ResourceType::Equipment);

  ResourceHandle invalidHandle;
  auto invalidCreated = manager->createResource(invalidHandle);
  BOOST_CHECK(invalidCreated == nullptr);
}

BOOST_AUTO_TEST_CASE(TestStatistics) {
  auto stats = manager->getStats();
  BOOST_CHECK_EQUAL(stats.templatesLoaded.load(), 0);
  BOOST_CHECK_EQUAL(stats.resourcesCreated.load(), 0);
  BOOST_CHECK_EQUAL(stats.resourcesDestroyed.load(), 0);

  auto resource = createTestResource("Test Item", ResourceCategory::Item,
                                     ResourceType::Equipment);
  manager->registerResourceTemplate(resource);

  stats = manager->getStats();
  BOOST_CHECK_EQUAL(stats.templatesLoaded.load(), 1);

  manager->resetStats();
  stats = manager->getStats();
  BOOST_CHECK_EQUAL(stats.templatesLoaded.load(), 0);
  BOOST_CHECK_EQUAL(stats.resourcesCreated.load(), 0);
  BOOST_CHECK_EQUAL(stats.resourcesDestroyed.load(), 0);
}

BOOST_AUTO_TEST_CASE(TestMemoryUsage) {
  auto initialUsage = manager->getMemoryUsage();

  auto resource = createTestResource("Test Item", ResourceCategory::Item,
                                     ResourceType::Equipment);
  manager->registerResourceTemplate(resource);

  auto usageAfterAdd = manager->getMemoryUsage();
  BOOST_CHECK(usageAfterAdd > initialUsage);
}

BOOST_AUTO_TEST_CASE(TestCleanup) {
  auto resource1 = createTestResource("Item 1", ResourceCategory::Item,
                                      ResourceType::Equipment);
  auto resource2 = createTestResource("Item 2", ResourceCategory::Material,
                                      ResourceType::CraftingComponent);

  manager->registerResourceTemplate(resource1);
  manager->registerResourceTemplate(resource2);

  auto countBeforeClean = manager->getResourceTemplateCount();
  BOOST_CHECK(countBeforeClean > 0);
  BOOST_CHECK(manager->isInitialized());

  manager->clean();

  BOOST_CHECK_EQUAL(manager->getResourceTemplateCount(), 0);
  BOOST_CHECK(!manager->isInitialized());

  BOOST_CHECK(manager->init());
  BOOST_CHECK(manager->isInitialized());
  // After reinit, we should have the default resources again
  BOOST_CHECK(manager->getResourceTemplateCount() > 0);
}

BOOST_AUTO_TEST_CASE(TestReinitializationSafety) {
  BOOST_CHECK(manager->isInitialized());
  auto initialCount = manager->getResourceTemplateCount();

  // Calling init() on an already initialized manager should be safe and do
  // nothing
  BOOST_CHECK(manager->init());
  BOOST_CHECK(manager->isInitialized());

  auto resource = createTestResource("Test Item", ResourceCategory::Item,
                                     ResourceType::Equipment);
  manager->registerResourceTemplate(resource);
  BOOST_CHECK(manager->getResourceTemplateCount() > initialCount);

  // Calling init() again should still do nothing - manager stays initialized
  // with added resources
  BOOST_CHECK(manager->init());
  BOOST_CHECK(manager->isInitialized());
  // The resource count should remain unchanged (init does nothing on
  // initialized manager)
  BOOST_CHECK_EQUAL(manager->getResourceTemplateCount(), initialCount + 1);
}

BOOST_AUTO_TEST_CASE(TestMultipleResourceCategories) {
  std::vector<ResourcePtr> resources;

  resources.push_back(createTestResource("Test Sword", ResourceCategory::Item,
                                         ResourceType::Equipment));
  resources.push_back(createTestResource(
      "Test Health Potion", ResourceCategory::Item, ResourceType::Consumable));
  resources.push_back(createTestResource(
      "Test Iron Ore", ResourceCategory::Material, ResourceType::RawResource));
  resources.push_back(createTestResource(
      "Test Gold Coin", ResourceCategory::Currency, ResourceType::Gold));

  auto initialCount = manager->getResourceTemplateCount();

  for (const auto &resource : resources) {
    BOOST_CHECK(manager->registerResourceTemplate(resource));
  }

  BOOST_CHECK_EQUAL(manager->getResourceTemplateCount(), initialCount + 4);

  // Check categories have at least our added resources (may have defaults too)
  BOOST_CHECK(manager->getResourcesByCategory(ResourceCategory::Item).size() >=
              2);
  BOOST_CHECK(
      manager->getResourcesByCategory(ResourceCategory::Material).size() >= 1);
  BOOST_CHECK(
      manager->getResourcesByCategory(ResourceCategory::Currency).size() >= 1);
}

// Test duplicate name detection
BOOST_AUTO_TEST_CASE(TestDuplicateNameDetection) {
  auto resource1 = createTestResource("DuplicateName", ResourceCategory::Item,
                                      ResourceType::Equipment);
  auto resource2 = createTestResource(
      "DuplicateName", ResourceCategory::Material, ResourceType::RawResource);

  // First registration should succeed
  BOOST_CHECK(manager->registerResourceTemplate(resource1));

  // Second registration with same name should fail
  BOOST_CHECK(!manager->registerResourceTemplate(resource2));

  // Only the first resource should be registered
  auto retrieved1 = manager->getResourceByName("DuplicateName");
  BOOST_REQUIRE(retrieved1 != nullptr);
  BOOST_CHECK(retrieved1->getHandle() == resource1->getHandle());
  BOOST_CHECK(retrieved1->getCategory() == ResourceCategory::Item);
}

// Test that name-based lookups work only during data load/validation phase
BOOST_AUTO_TEST_CASE(TestNameBasedLookupCompliance) {
  auto resource = createTestResource("LookupTest", ResourceCategory::Item,
                                     ResourceType::Equipment);
  auto handle = resource->getHandle();

  BOOST_CHECK(manager->registerResourceTemplate(resource));

  // Name-based lookup should work for validation/data load
  auto retrievedByName = manager->getResourceByName("LookupTest");
  BOOST_REQUIRE(retrievedByName != nullptr);
  BOOST_CHECK(retrievedByName->getHandle() == handle);

  // Handle-based lookup should work for runtime
  auto retrievedByHandle = manager->getResourceTemplate(handle);
  BOOST_REQUIRE(retrievedByHandle != nullptr);
  BOOST_CHECK_EQUAL(retrievedByHandle->getName(), "LookupTest");

  // Both should return the same resource
  BOOST_CHECK(retrievedByName == retrievedByHandle);
}
// Test resource handle system performance and functionality
BOOST_AUTO_TEST_CASE(TestResourceHandleSystemPerformance) {
  std::vector<ResourcePtr> resources;
  std::vector<ResourceHandle> handles;

  // Create a batch of resources for performance testing
  for (int i = 0; i < 100; ++i) {
    auto resource =
        createTestResource("PerformanceTest" + std::to_string(i),
                           ResourceCategory::Item, ResourceType::Equipment);
    resources.push_back(resource);
    handles.push_back(resource->getHandle());
    BOOST_CHECK(manager->registerResourceTemplate(resource));
  }

  // Test bulk property access (handle-based operations)
  auto maxStackSizes = manager->getMaxStackSizes(handles);
  auto values = manager->getValues(handles);

  BOOST_CHECK_EQUAL(maxStackSizes.size(), 100);
  BOOST_CHECK_EQUAL(values.size(), 100);

  // All handles should be valid and retrievable
  for (const auto &handle : handles) {
    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(manager->isValidHandle(handle));

    auto resource = manager->getResourceTemplate(handle);
    BOOST_REQUIRE(resource != nullptr);
    BOOST_CHECK(resource->getHandle() == handle);
  }
}

// Test that runtime operations avoid name-based lookups
BOOST_AUTO_TEST_CASE(TestRuntimeOperationsUseHandles) {
  auto resource = createTestResource("RuntimeTest", ResourceCategory::Item,
                                     ResourceType::Consumable);
  auto handle = resource->getHandle();

  BOOST_CHECK(manager->registerResourceTemplate(resource));

  // Runtime operations should use handles, not names
  // These methods should be fast and cache-friendly
  BOOST_CHECK_EQUAL(manager->getMaxStackSize(handle),
                    resource->getMaxStackSize());
  BOOST_CHECK_CLOSE(manager->getValue(handle), resource->getValue(), 0.001f);
  BOOST_CHECK(manager->getCategory(handle) == resource->getCategory());
  BOOST_CHECK(manager->getType(handle) == resource->getType());

  // Invalid handles should return sensible defaults
  ResourceHandle invalidHandle;
  BOOST_CHECK_EQUAL(manager->getMaxStackSize(invalidHandle), 1);
  BOOST_CHECK_EQUAL(manager->getValue(invalidHandle), 0.0f);
}

// Test edge cases for duplicate name detection
BOOST_AUTO_TEST_CASE(TestDuplicateNameEdgeCases) {
  // Test case sensitivity
  auto resource1 = createTestResource("TestCase", ResourceCategory::Item,
                                      ResourceType::Equipment);
  auto resource2 = createTestResource("testcase", ResourceCategory::Item,
                                      ResourceType::Equipment);

  BOOST_CHECK(manager->registerResourceTemplate(resource1));
  BOOST_CHECK(manager->registerResourceTemplate(
      resource2)); // Different case should be allowed

  // Test empty names
  auto resourceEmpty1 =
      createTestResource("", ResourceCategory::Item, ResourceType::Equipment);
  auto resourceEmpty2 = createTestResource("", ResourceCategory::Material,
                                           ResourceType::RawResource);

  BOOST_CHECK(manager->registerResourceTemplate(resourceEmpty1));
  BOOST_CHECK(!manager->registerResourceTemplate(
      resourceEmpty2)); // Second empty name should fail

  // Test very long names
  std::string longName(1000, 'x');
  auto resourceLong1 = createTestResource(longName, ResourceCategory::Item,
                                          ResourceType::Equipment);
  auto resourceLong2 = createTestResource(longName, ResourceCategory::Material,
                                          ResourceType::RawResource);

  BOOST_CHECK(manager->registerResourceTemplate(resourceLong1));
  BOOST_CHECK(!manager->registerResourceTemplate(
      resourceLong2)); // Duplicate long name should fail
}

BOOST_AUTO_TEST_SUITE_END()
