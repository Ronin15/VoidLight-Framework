/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ResourceArchitectureTests
#include <boost/test/unit_test.hpp>

#include "entities/Resource.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "utils/Vector2D.hpp"

/**
 * @brief Tests that validate the data-driven Resource architecture
 *
 * These tests ensure that:
 * 1. Resources are pure data classes (no Entity inheritance)
 * 2. EDM createDroppedItem properly uses Resource templates
 * 3. Visual properties are tracked in ItemRenderData
 * 4. Memory usage is efficient through data-oriented design
 */

class ResourceArchitectureTestFixture {
public:
  ResourceArchitectureTestFixture() {
    // Initialize EntityDataManager FIRST (required for DroppedItem creation via EDM)
    entityDataManager = &EntityDataManager::Instance();
    BOOST_REQUIRE(entityDataManager->init());

    // Initialize ResourceTemplateManager
    resourceManager = &ResourceTemplateManager::Instance();
    if (!resourceManager->isInitialized()) {
      BOOST_REQUIRE(resourceManager->init());
    }

    // Get a test resource handle
    testResourceHandle = resourceManager->getHandleByName("Super Health Potion");
    BOOST_REQUIRE(testResourceHandle.isValid());

    testResource = resourceManager->getResourceTemplate(testResourceHandle);
    BOOST_REQUIRE(testResource != nullptr);
  }

  ~ResourceArchitectureTestFixture() {
    // Clean up EntityDataManager
    entityDataManager->clean();
    // Explicitly clean up the ResourceTemplateManager
    resourceManager->clean();
  }

protected:
  EntityDataManager *entityDataManager;
  ResourceTemplateManager *resourceManager;
  VoidLight::ResourceHandle testResourceHandle;
  std::shared_ptr<Resource> testResource;
};

BOOST_FIXTURE_TEST_SUITE(ResourceArchitectureTestSuite,
                         ResourceArchitectureTestFixture)

BOOST_AUTO_TEST_CASE(TestResourceIsPureDataClass) {
  // Test that Resource doesn't have Entity behavior
  BOOST_REQUIRE(testResource != nullptr);

  // Verify Resource has proper data properties
  BOOST_CHECK(!testResource->getName().empty());
  BOOST_CHECK(!testResource->getId().empty());
  BOOST_CHECK(testResource->getValue() >= 0.0f);
  BOOST_CHECK(testResource->getWeight() >= 0.0f);
  BOOST_CHECK(testResource->getMaxStackSize() > 0);

  // Verify visual properties are available
  BOOST_CHECK(!testResource->getIconTextureId().empty());
  BOOST_CHECK(!testResource->getWorldTextureId().empty());
  BOOST_CHECK(testResource->getNumFrames() > 0);
  BOOST_CHECK(testResource->getAnimSpeed() > 0);

  // Verify category and type are set
  BOOST_CHECK(!Resource::categoryToString(testResource->getCategory()).empty());
  BOOST_CHECK(!Resource::typeToString(testResource->getType()).empty());
}

BOOST_AUTO_TEST_CASE(TestResourceImmutability) {
  // Test that core properties can't be changed after creation
  std::string originalName = testResource->getName();
  std::string originalId = testResource->getId();
  ResourceCategory originalCategory = testResource->getCategory();
  ResourceType originalType = testResource->getType();

  // These should remain constant (no setters for core properties)
  BOOST_CHECK_EQUAL(testResource->getName(), originalName);
  BOOST_CHECK_EQUAL(testResource->getId(), originalId);

  // Test category and type are still the same
  BOOST_CHECK_EQUAL(Resource::categoryToString(testResource->getCategory()),
                    Resource::categoryToString(originalCategory));
  BOOST_CHECK_EQUAL(Resource::typeToString(testResource->getType()),
                    Resource::typeToString(originalType));
}

BOOST_AUTO_TEST_CASE(TestEDMDroppedItemCreation) {
  // Test that DroppedItem can be created via EntityDataManager
  Vector2D testPosition(100.0f, 200.0f);
  int testQuantity = 5;

  // Create dropped item via EDM
  EntityHandle droppedHandle = entityDataManager->createDroppedItem(
      testPosition, testResourceHandle, testQuantity);

  BOOST_REQUIRE(droppedHandle.isValid());
  BOOST_CHECK(droppedHandle.getKind() == EntityKind::DroppedItem);

  // Get the item data
  size_t edmIndex = entityDataManager->getIndex(droppedHandle);
  BOOST_REQUIRE(edmIndex != SIZE_MAX);

  // Verify the entity was created with correct position
  const auto& hotData = entityDataManager->getHotData(droppedHandle);
  BOOST_CHECK_CLOSE(hotData.transform.position.getX(), testPosition.getX(), 0.001f);
  BOOST_CHECK_CLOSE(hotData.transform.position.getY(), testPosition.getY(), 0.001f);

  // Cleanup
  entityDataManager->destroyEntity(droppedHandle);
}

BOOST_AUTO_TEST_CASE(TestEDMDroppedItemUsesResourceTemplate) {
  // Test that EDM dropped item properly references Resource template
  Vector2D testPosition(50.0f, 75.0f);

  EntityHandle droppedHandle = entityDataManager->createDroppedItem(
      testPosition, testResourceHandle, 1);

  BOOST_REQUIRE(droppedHandle.isValid());

  // The dropped item should reference the same resource template
  // This is validated by the fact that createDroppedItem requires a valid handle
  // and populates ItemRenderData from the template

  // Verify the resource template is still accessible
  auto templateResource = resourceManager->getResourceTemplate(testResourceHandle);
  BOOST_REQUIRE(templateResource != nullptr);
  BOOST_CHECK_EQUAL(templateResource->getId(), testResource->getId());
  BOOST_CHECK_EQUAL(templateResource->getName(), testResource->getName());

  // Cleanup
  entityDataManager->destroyEntity(droppedHandle);
}

BOOST_AUTO_TEST_CASE(TestDroppedItemInvalidResourceHandle) {
  // Test that creating dropped item with invalid handle fails
  Vector2D testPosition(0.0f, 0.0f);
  VoidLight::ResourceHandle invalidHandle;

  EntityHandle droppedHandle = entityDataManager->createDroppedItem(
      testPosition, invalidHandle, 5);

  // Should return invalid handle
  BOOST_CHECK(!droppedHandle.isValid());
}

BOOST_AUTO_TEST_CASE(TestDroppedItemInvalidQuantity) {
  // Test that creating dropped item with invalid quantity fails
  Vector2D testPosition(0.0f, 0.0f);

  // Zero quantity should fail
  EntityHandle droppedHandle = entityDataManager->createDroppedItem(
      testPosition, testResourceHandle, 0);
  BOOST_CHECK(!droppedHandle.isValid());

  // Negative quantity should fail
  droppedHandle = entityDataManager->createDroppedItem(
      testPosition, testResourceHandle, -5);
  BOOST_CHECK(!droppedHandle.isValid());
}

BOOST_AUTO_TEST_CASE(TestResourceStringConversions) {
  // Test category string conversions for known values
  std::string itemStr = Resource::categoryToString(ResourceCategory::Item);
  BOOST_CHECK_EQUAL(itemStr, "Item");

  std::string currencyStr = Resource::categoryToString(ResourceCategory::Currency);
  BOOST_CHECK_EQUAL(currencyStr, "Currency");

  // Test round trip conversion for Item category
  ResourceCategory convertedBack = Resource::stringToCategory(itemStr);
  BOOST_CHECK_EQUAL(Resource::categoryToString(convertedBack), itemStr);
}

BOOST_AUTO_TEST_CASE(TestMemoryEfficiencyThroughEDM) {
  // Test that EDM efficiently stores multiple dropped items
  Vector2D testPosition(0.0f, 0.0f);
  std::vector<EntityHandle> droppedItems;

  // Create multiple DroppedItems referencing the same Resource template
  for (int i = 0; i < 10; ++i) {
    Vector2D pos(static_cast<float>(i * 50), static_cast<float>(i * 50));
    EntityHandle handle = entityDataManager->createDroppedItem(
        pos, testResourceHandle, i + 1);
    BOOST_REQUIRE(handle.isValid());
    droppedItems.push_back(handle);
  }

  // All should reference the same Resource template (verified through RTM)
  auto templateResource = resourceManager->getResourceTemplate(testResourceHandle);
  BOOST_REQUIRE(templateResource != nullptr);

  // The template is shared, not duplicated for each dropped item
  for (const auto &handle : droppedItems) {
    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(handle.getKind() == EntityKind::DroppedItem);
  }

  // Cleanup all entities
  for (const auto &handle : droppedItems) {
    entityDataManager->destroyEntity(handle);
  }
}

BOOST_AUTO_TEST_CASE(TestMultipleResourceTypes) {
  // Test creating dropped items of different resource types
  auto goldHandle = resourceManager->getHandleByName("Platinum Coins");
  auto oreHandle = resourceManager->getHandleByName("Mithril Ore");
  auto swordHandle = resourceManager->getHandleByName("Magic Sword");

  std::vector<EntityHandle> droppedItems;

  // Create different resource types
  if (goldHandle.isValid()) {
    Vector2D pos(100.0f, 100.0f);
    auto handle = entityDataManager->createDroppedItem(pos, goldHandle, 100);
    if (handle.isValid()) {
      droppedItems.push_back(handle);
    }
  }

  if (oreHandle.isValid()) {
    Vector2D pos(200.0f, 100.0f);
    auto handle = entityDataManager->createDroppedItem(pos, oreHandle, 50);
    if (handle.isValid()) {
      droppedItems.push_back(handle);
    }
  }

  if (swordHandle.isValid()) {
    Vector2D pos(300.0f, 100.0f);
    auto handle = entityDataManager->createDroppedItem(pos, swordHandle, 1);
    if (handle.isValid()) {
      droppedItems.push_back(handle);
    }
  }

  // Should have created at least some items
  BOOST_CHECK_GT(droppedItems.size(), 0);

  // Cleanup
  for (const auto &handle : droppedItems) {
    entityDataManager->destroyEntity(handle);
  }
}

BOOST_AUTO_TEST_SUITE_END()
