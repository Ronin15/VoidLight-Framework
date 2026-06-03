/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE EntityDataManagerTests
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "entities/Entity.hpp"  // For AnimationConfig
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include <cmath>
#include <limits>
#include <vector>

// Test tolerance for floating-point comparisons
constexpr float EPSILON = 0.001f;

// Helper to check if two floats are approximately equal
bool approxEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// Global Fixture — ThreadSystem lives for the entire test module
// ============================================================================

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~ThreadSystemFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

// ============================================================================
// Per-Test Fixture — managers reset between tests
// ============================================================================

class EntityDataManagerTestFixture {
public:
    EntityDataManagerTestFixture() {
        ResourceTemplateManager::Instance().init();
        edm = &EntityDataManager::Instance();
        BOOST_REQUIRE(edm->init());
        EventManager::Instance().init();
        CollisionManager::Instance().init();
        PathfinderManager::Instance().init();
        AIManager::Instance().init();
    }

    ~EntityDataManagerTestFixture() {
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EventManager::Instance().clean();
        edm->clean();
        ResourceTemplateManager::Instance().clean();
    }

protected:
    EntityDataManager* edm;
};

// ============================================================================
// SINGLETON PATTERN TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SingletonTests)

BOOST_AUTO_TEST_CASE(TestSingletonPattern) {
    EntityDataManager* instance1 = &EntityDataManager::Instance();
    EntityDataManager* instance2 = &EntityDataManager::Instance();

    BOOST_CHECK(instance1 == instance2);
    BOOST_CHECK(instance1 != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(LifecycleTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestInitialization) {
    // Manager should be initialized by fixture
    BOOST_CHECK(edm->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestDoubleInitialization) {
    // Second init should return true (already initialized)
    bool result = edm->init();
    BOOST_CHECK(result);
    BOOST_CHECK(edm->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestCleanAndReinit) {
    // Create an entity first
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_CHECK(handle.isValid());

    // Clean should clear everything
    edm->clean();
    BOOST_CHECK(!edm->isInitialized());
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);

    // Re-init should work
    bool result = edm->init();
    BOOST_CHECK(result);
    BOOST_CHECK(edm->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransition) {
    // Create some entities
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 2);

    // State transition should clear entities
    edm->prepareForStateTransition();
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);

    // Manager should still be initialized
    BOOST_CHECK(edm->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestDirectDestroyClearsBehaviorConfigForSlotReuse) {
    EntityHandle npc = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(npc.isValid());

    const size_t npcIdx = edm->getIndex(npc);
    BOOST_REQUIRE(npcIdx != SIZE_MAX);

    const BehaviorConfigRef initialRef = edm->getBehaviorConfigRef(npcIdx);
    BOOST_REQUIRE(initialRef.type == BehaviorType::Guard);
    BOOST_REQUIRE(initialRef.index != std::numeric_limits<uint32_t>::max());

    edm->destroyEntity(npc);
    edm->processDestructionQueue();

    const BehaviorConfigRef destroyedRef = edm->getBehaviorConfigRef(npcIdx);
    BOOST_CHECK(destroyedRef.type == BehaviorType::None);
    BOOST_CHECK_EQUAL(destroyedRef.index, std::numeric_limits<uint32_t>::max());
    BOOST_CHECK(!AIManager::Instance().hasBehavior(npc));

    EntityHandle player = edm->registerPlayer(1, Vector2D(200.0f, 200.0f));
    BOOST_REQUIRE(player.isValid());

    const size_t playerIdx = edm->getIndex(player);
    BOOST_REQUIRE_EQUAL(playerIdx, npcIdx);

    const BehaviorConfigRef playerRef = edm->getBehaviorConfigRef(playerIdx);
    BOOST_CHECK(playerRef.type == BehaviorType::None);
    BOOST_CHECK_EQUAL(playerRef.index, std::numeric_limits<uint32_t>::max());
}

BOOST_AUTO_TEST_CASE(TestStateTransitionClearsBehaviorStatePools) {
    EntityHandle firstGuard = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(firstGuard.isValid());

    const size_t firstIdx = edm->getIndex(firstGuard);
    BOOST_REQUIRE(firstIdx != SIZE_MAX);

    const BehaviorConfigRef firstRef = edm->getBehaviorConfigRef(firstIdx);
    BOOST_REQUIRE(firstRef.type == BehaviorType::Guard);
    edm->getGuardState(firstRef.index).currentAlertLevel = 3;

    AIManager::Instance().prepareForStateTransition();
    edm->prepareForStateTransition();
    BOOST_REQUIRE_EQUAL(edm->getEntityCount(), 0);

    EntityHandle secondGuard = edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    BOOST_REQUIRE(secondGuard.isValid());

    const size_t secondIdx = edm->getIndex(secondGuard);
    BOOST_REQUIRE(secondIdx != SIZE_MAX);

    const BehaviorConfigRef secondRef = edm->getBehaviorConfigRef(secondIdx);
    BOOST_REQUIRE(secondRef.type == BehaviorType::Guard);
    BOOST_CHECK_EQUAL(secondRef.index, 0u);
    BOOST_CHECK_EQUAL(edm->getGuardState(secondRef.index).currentAlertLevel, 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ENTITY CREATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EntityCreationTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestCreateNPC) {
    Vector2D position(100.0f, 200.0f);
    EntityHandle handle = edm->createNPCWithRaceClass(position, "Human", "Guard");

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(handle.isNPC());
    BOOST_CHECK_EQUAL(static_cast<int>(handle.kind), static_cast<int>(EntityKind::NPC));
    BOOST_CHECK(edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 1);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::NPC), 1);

    // Verify transform
    const auto& transform = edm->getTransform(handle);
    BOOST_CHECK(approxEqual(transform.position.getX(), 100.0f));
    BOOST_CHECK(approxEqual(transform.position.getY(), 200.0f));

    // Verify hot data
    const auto& hot = edm->getHotData(handle);
    BOOST_CHECK(approxEqual(hot.halfWidth, 16.0f));
    BOOST_CHECK(approxEqual(hot.halfHeight, 16.0f));
    BOOST_CHECK(hot.isAlive());
}

BOOST_AUTO_TEST_CASE(TestCreatePlayer) {
    Vector2D position(300.0f, 400.0f);
    EntityHandle handle = edm->registerPlayer(1,position);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(handle.isPlayer());
    BOOST_CHECK_EQUAL(static_cast<int>(handle.kind), static_cast<int>(EntityKind::Player));
    BOOST_CHECK(edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::Player), 1);

    // Verify character data
    const auto& charData = edm->getCharacterData(handle);
    BOOST_CHECK(approxEqual(charData.health, 100.0f));
    BOOST_CHECK(approxEqual(charData.maxHealth, 100.0f));
    BOOST_CHECK(edm->isValidHandle(handle));
}

BOOST_AUTO_TEST_CASE(TestCreateDroppedItem) {
    Vector2D position(500.0f, 600.0f);
    VoidLight::ResourceHandle resourceHandle{1, 1};
    EntityHandle handle = edm->createDroppedItem(position, resourceHandle, 5);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(handle.isItem());
    BOOST_CHECK_EQUAL(static_cast<int>(handle.kind), static_cast<int>(EntityKind::DroppedItem));
    BOOST_CHECK(edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::DroppedItem), 1);

    // Verify item data
    const auto& itemData = edm->getItemData(handle);
    BOOST_CHECK_EQUAL(itemData.quantity, 5);
    BOOST_CHECK(approxEqual(itemData.pickupTimer, 0.5f));
}

BOOST_AUTO_TEST_CASE(TestCreateProjectile) {
    Vector2D position(100.0f, 100.0f);
    Vector2D velocity(50.0f, 0.0f);
    EntityHandle owner = edm->registerPlayer(1,Vector2D(0.0f, 0.0f));
    EntityHandle handle = edm->createProjectile(position, velocity, owner, 25.0f, 3.0f);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(handle.isProjectile());
    BOOST_CHECK_EQUAL(static_cast<int>(handle.kind), static_cast<int>(EntityKind::Projectile));
    BOOST_CHECK(edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::Projectile), 1);

    // Verify projectile data
    const auto& projData = edm->getProjectileData(handle);
    BOOST_CHECK(approxEqual(projData.damage, 25.0f));
    BOOST_CHECK(approxEqual(projData.lifetime, 3.0f));
    BOOST_CHECK(projData.owner == owner);
}

BOOST_AUTO_TEST_CASE(TestCreateAreaEffect) {
    Vector2D position(200.0f, 200.0f);
    EntityHandle owner = edm->registerPlayer(1,Vector2D(0.0f, 0.0f));
    EntityHandle handle = edm->createAreaEffect(position, 50.0f, owner, 10.0f, 5.0f);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK_EQUAL(static_cast<int>(handle.kind), static_cast<int>(EntityKind::AreaEffect));
    BOOST_CHECK(edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::AreaEffect), 1);

    // Verify area effect data
    const auto& effectData = edm->getAreaEffectData(handle);
    BOOST_CHECK(approxEqual(effectData.radius, 50.0f));
    BOOST_CHECK(approxEqual(effectData.damage, 10.0f));
    BOOST_CHECK(approxEqual(effectData.duration, 5.0f));
}

BOOST_AUTO_TEST_CASE(TestCreateStaticBody) {
    Vector2D position(400.0f, 400.0f);
    EntityHandle handle = edm->createStaticBody(position, 32.0f, 32.0f);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK_EQUAL(static_cast<int>(handle.kind), static_cast<int>(EntityKind::StaticObstacle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::StaticObstacle), 1);

    // Static bodies use separate storage
    size_t staticIndex = edm->getStaticIndex(handle);
    BOOST_CHECK(staticIndex != SIZE_MAX);

    const auto& staticHot = edm->getStaticHotDataByIndex(staticIndex);
    BOOST_CHECK(approxEqual(staticHot.transform.position.getX(), 400.0f));
    BOOST_CHECK(approxEqual(staticHot.halfWidth, 32.0f));
}

BOOST_AUTO_TEST_CASE(TestCreateMultipleEntities) {
    // Create various entity types
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    edm->registerPlayer(1,Vector2D(300.0f, 300.0f));
    edm->createDroppedItem(Vector2D(400.0f, 400.0f), VoidLight::ResourceHandle{1, 1}, 1);

    BOOST_CHECK_EQUAL(edm->getEntityCount(), 4);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::NPC), 2);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::Player), 1);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::DroppedItem), 1);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ENTITY REGISTRATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EntityRegistrationTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestCreateNPCWithCharacterData) {
    // NPCs are created via createNPCWithRaceClass - stats = base × class multiplier
    // Human base health = 100, Guard healthMult = 1.2, so expected = 120
    Vector2D position(100.0f, 200.0f);

    EntityHandle handle = edm->createNPCWithRaceClass(position, "Human", "Guard");

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(handle.isNPC());

    // Verify character data has computed health (Human 100 × Guard 1.2 = 120)
    const auto& charData = edm->getCharacterData(handle);
    BOOST_CHECK(approxEqual(charData.health, 120.0f));
    BOOST_CHECK(approxEqual(charData.maxHealth, 120.0f));
}

BOOST_AUTO_TEST_CASE(TestRegisterPlayer) {
    EntityHandle::IDType entityId = 67890;
    Vector2D position(300.0f, 400.0f);

    EntityHandle handle = edm->registerPlayer(entityId, position, 32.0f, 32.0f);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK_EQUAL(handle.id, entityId);
    BOOST_CHECK(handle.isPlayer());
}

BOOST_AUTO_TEST_CASE(TestRegisterDroppedItem) {
    EntityHandle::IDType entityId = 11111;
    Vector2D position(500.0f, 600.0f);
    VoidLight::ResourceHandle resourceHandle{2, 3};

    EntityHandle handle = edm->registerDroppedItem(entityId, position, resourceHandle, 10);

    BOOST_CHECK(handle.isValid());
    BOOST_CHECK_EQUAL(handle.id, entityId);
    BOOST_CHECK(handle.isItem());

    const auto& itemData = edm->getItemData(handle);
    BOOST_CHECK_EQUAL(itemData.quantity, 10);
}

BOOST_AUTO_TEST_CASE(TestUnregisterEntity) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_CHECK(handle.isValid());
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 1);

    // Unregister by entity ID
    edm->unregisterEntity(handle.id);

    // Entity should be gone
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
    BOOST_CHECK(!edm->isValidHandle(handle));
}

BOOST_AUTO_TEST_CASE(TestUnregisterNonexistentEntity) {
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
    edm->unregisterEntity(99999999);
    edm->unregisterEntity(0);
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// DESTRUCTION QUEUE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(DestructionQueueTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestDestroyEntity) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_CHECK(edm->isValidHandle(handle));

    // Queue for destruction
    edm->destroyEntity(handle);

    // Still valid until processed
    BOOST_CHECK(edm->isValidHandle(handle));

    // Process destruction
    edm->processDestructionQueue();

    // Now invalid
    BOOST_CHECK(!edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
}

BOOST_AUTO_TEST_CASE(TestDestroyMultipleEntities) {
    EntityHandle handle1 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    EntityHandle handle2 = edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    EntityHandle handle3 = edm->createNPCWithRaceClass(Vector2D(300.0f, 300.0f), "Human", "Guard");
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 3);

    // Queue all for destruction
    edm->destroyEntity(handle1);
    edm->destroyEntity(handle2);
    edm->destroyEntity(handle3);

    // Process
    edm->processDestructionQueue();

    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
    BOOST_CHECK(!edm->isValidHandle(handle1));
    BOOST_CHECK(!edm->isValidHandle(handle2));
    BOOST_CHECK(!edm->isValidHandle(handle3));
}

BOOST_AUTO_TEST_CASE(TestDestroyInvalidHandle) {
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
    edm->destroyEntity(INVALID_ENTITY_HANDLE);
    edm->processDestructionQueue();
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
}

BOOST_AUTO_TEST_CASE(TestGenerationIncrementAfterDestruction) {
    // Create and destroy, then create again - should get different generation
    EntityHandle handle1 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    [[maybe_unused]] uint32_t gen1 = handle1.generation;

    edm->destroyEntity(handle1);
    edm->processDestructionQueue();

    // Create new entity - may reuse slot with new generation
    EntityHandle handle2 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");

    // The old handle should be stale
    BOOST_CHECK(!edm->isValidHandle(handle1));
    BOOST_CHECK(edm->isValidHandle(handle2));
}

BOOST_AUTO_TEST_CASE(TestProcessEmptyQueue) {
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
    edm->processDestructionQueue();
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// HANDLE VALIDATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(HandleValidationTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestValidHandle) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_CHECK(edm->isValidHandle(handle));
}

BOOST_AUTO_TEST_CASE(TestInvalidHandle) {
    BOOST_CHECK(!edm->isValidHandle(INVALID_ENTITY_HANDLE));
}

BOOST_AUTO_TEST_CASE(TestGetIndex) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->getIndex(handle);

    BOOST_CHECK(index != SIZE_MAX);

    // Access by index should work
    const auto& hot = edm->getHotDataByIndex(index);
    BOOST_CHECK(hot.isAlive());
}

BOOST_AUTO_TEST_CASE(TestGetIndexInvalidHandle) {
    size_t index = edm->getIndex(INVALID_ENTITY_HANDLE);
    BOOST_CHECK_EQUAL(index, SIZE_MAX);
}

BOOST_AUTO_TEST_CASE(TestFindIndexByEntityId) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->findIndexByEntityId(handle.id);

    BOOST_CHECK(index != SIZE_MAX);
    BOOST_CHECK_EQUAL(index, edm->getIndex(handle));
}

BOOST_AUTO_TEST_CASE(TestFindIndexByInvalidEntityId) {
    size_t index = edm->findIndexByEntityId(0);
    BOOST_CHECK_EQUAL(index, SIZE_MAX);

    index = edm->findIndexByEntityId(99999999);
    BOOST_CHECK_EQUAL(index, SIZE_MAX);
}

BOOST_AUTO_TEST_CASE(TestStaleHandleDetection) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_CHECK(edm->isValidHandle(handle));

    // Destroy the entity
    edm->destroyEntity(handle);
    edm->processDestructionQueue();

    // Old handle should be stale
    BOOST_CHECK(!edm->isValidHandle(handle));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TRANSFORM ACCESS TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TransformAccessTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestGetTransform) {
    Vector2D position(100.0f, 200.0f);
    EntityHandle handle = edm->createNPCWithRaceClass(position, "Human", "Guard");

    const auto& transform = edm->getTransform(handle);
    BOOST_CHECK(approxEqual(transform.position.getX(), 100.0f));
    BOOST_CHECK(approxEqual(transform.position.getY(), 200.0f));
}

BOOST_AUTO_TEST_CASE(TestModifyTransform) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(0.0f, 0.0f), "Human", "Guard");

    auto& transform = edm->getTransform(handle);
    transform.position = Vector2D(500.0f, 600.0f);
    transform.velocity = Vector2D(10.0f, 20.0f);

    const auto& readTransform = edm->getTransform(handle);
    BOOST_CHECK(approxEqual(readTransform.position.getX(), 500.0f));
    BOOST_CHECK(approxEqual(readTransform.position.getY(), 600.0f));
    BOOST_CHECK(approxEqual(readTransform.velocity.getX(), 10.0f));
    BOOST_CHECK(approxEqual(readTransform.velocity.getY(), 20.0f));
}

BOOST_AUTO_TEST_CASE(TestGetTransformByIndex) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 200.0f), "Human", "Guard");
    size_t index = edm->getIndex(handle);

    const auto& transform = edm->getTransformByIndex(index);
    BOOST_CHECK(approxEqual(transform.position.getX(), 100.0f));
    BOOST_CHECK(approxEqual(transform.position.getY(), 200.0f));
}

BOOST_AUTO_TEST_CASE(TestGetStaticTransformByIndex) {
    EntityHandle handle = edm->createStaticBody(Vector2D(400.0f, 500.0f), 32.0f, 32.0f);
    size_t index = edm->getStaticIndex(handle);

    const auto& transform = edm->getStaticTransformByIndex(index);
    BOOST_CHECK(approxEqual(transform.position.getX(), 400.0f));
    BOOST_CHECK(approxEqual(transform.position.getY(), 500.0f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// HOT DATA ACCESS TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(HotDataAccessTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestGetHotData) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");

    const auto& hot = edm->getHotData(handle);
    BOOST_CHECK(hot.isAlive());
    BOOST_CHECK_EQUAL(static_cast<int>(hot.kind), static_cast<int>(EntityKind::NPC));
    // Default frame size is 32x32, so halfWidth/halfHeight = 16
    BOOST_CHECK(approxEqual(hot.halfWidth, 16.0f));
    BOOST_CHECK(approxEqual(hot.halfHeight, 16.0f));
}

BOOST_AUTO_TEST_CASE(TestGetHotDataByIndex) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->getIndex(handle);

    const auto& hot = edm->getHotDataByIndex(index);
    BOOST_CHECK(hot.isAlive());
}

BOOST_AUTO_TEST_CASE(TestGetHotDataArray) {
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");

    auto hotArray = edm->getHotDataArray();
    BOOST_CHECK(hotArray.size() >= 2);

    // Count alive entities in array
    size_t aliveCount = 0;
    for (const auto& hot : hotArray) {
        if (hot.isAlive()) aliveCount++;
    }
    BOOST_CHECK_EQUAL(aliveCount, 2);
}

BOOST_AUTO_TEST_CASE(TestGetStaticHotDataArray) {
    edm->createStaticBody(Vector2D(100.0f, 100.0f), 16.0f, 16.0f);
    edm->createStaticBody(Vector2D(200.0f, 200.0f), 16.0f, 16.0f);

    auto staticArray = edm->getStaticHotDataArray();
    BOOST_CHECK(staticArray.size() >= 2);
}

BOOST_AUTO_TEST_CASE(TestHotDataFlags) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");

    auto& hot = edm->getHotData(handle);
    BOOST_CHECK(hot.isAlive());
    BOOST_CHECK(!hot.isDirty());
    BOOST_CHECK(!hot.isPendingDestroy());

    // Modify flags
    hot.setDirty(true);
    BOOST_CHECK(hot.isDirty());

    hot.setDirty(false);
    BOOST_CHECK(!hot.isDirty());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TYPE-SPECIFIC DATA TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TypeSpecificDataTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestGetCharacterData) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");

    auto& charData = edm->getCharacterData(handle);
    BOOST_CHECK(edm->isValidHandle(handle));

    // Modify health
    charData.health = 50.0f;
    const auto& readData = edm->getCharacterData(handle);
    BOOST_CHECK(approxEqual(readData.health, 50.0f));
}

BOOST_AUTO_TEST_CASE(TestGetCharacterDataByIndex) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->getIndex(handle);

    BOOST_CHECK_NO_THROW(static_cast<void>(edm->getCharacterDataByIndex(index)));
    BOOST_CHECK(edm->isValidHandle(handle));
}

BOOST_AUTO_TEST_CASE(TestGetItemData) {
    VoidLight::ResourceHandle resourceHandle{1, 2};
    EntityHandle handle = edm->createDroppedItem(Vector2D(100.0f, 100.0f), resourceHandle, 5);

    auto& itemData = edm->getItemData(handle);
    BOOST_CHECK_EQUAL(itemData.quantity, 5);

    // Modify quantity
    itemData.quantity = 10;
    const auto& readData = edm->getItemData(handle);
    BOOST_CHECK_EQUAL(readData.quantity, 10);
}

BOOST_AUTO_TEST_CASE(TestGetProjectileData) {
    EntityHandle owner = edm->registerPlayer(1,Vector2D(0.0f, 0.0f));
    EntityHandle handle = edm->createProjectile(Vector2D(100.0f, 100.0f),
                                                 Vector2D(50.0f, 0.0f),
                                                 owner, 25.0f, 5.0f);

    const auto& projData = edm->getProjectileData(handle);
    BOOST_CHECK(approxEqual(projData.damage, 25.0f));
    BOOST_CHECK(approxEqual(projData.lifetime, 5.0f));
    BOOST_CHECK(projData.owner == owner);
}

BOOST_AUTO_TEST_CASE(TestGetAreaEffectData) {
    EntityHandle owner = edm->registerPlayer(1,Vector2D(0.0f, 0.0f));
    EntityHandle handle = edm->createAreaEffect(Vector2D(200.0f, 200.0f),
                                                 100.0f, owner, 15.0f, 10.0f);

    const auto& effectData = edm->getAreaEffectData(handle);
    BOOST_CHECK(approxEqual(effectData.radius, 100.0f));
    BOOST_CHECK(approxEqual(effectData.damage, 15.0f));
    BOOST_CHECK(approxEqual(effectData.duration, 10.0f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SIMULATION TIER TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SimulationTierTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestDefaultTierIsActive) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    const auto& hot = edm->getHotData(handle);
    BOOST_CHECK_EQUAL(static_cast<int>(hot.tier), static_cast<int>(SimulationTier::Active));
}

BOOST_AUTO_TEST_CASE(TestSetSimulationTier) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");

    edm->setSimulationTier(handle, SimulationTier::Background);
    const auto& hot = edm->getHotData(handle);
    BOOST_CHECK_EQUAL(static_cast<int>(hot.tier), static_cast<int>(SimulationTier::Background));

    edm->setSimulationTier(handle, SimulationTier::Hibernated);
    BOOST_CHECK_EQUAL(static_cast<int>(edm->getHotData(handle).tier),
                      static_cast<int>(SimulationTier::Hibernated));
}

BOOST_AUTO_TEST_CASE(TestUpdateSimulationTiers) {
    // Create entities at various distances
    EntityHandle near = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");     // Close
    EntityHandle mid = edm->createNPCWithRaceClass(Vector2D(2000.0f, 2000.0f), "Human", "Guard");    // Medium
    EntityHandle far = edm->createNPCWithRaceClass(Vector2D(15000.0f, 15000.0f), "Human", "Guard");  // Far

    // Update tiers with reference point at origin
    Vector2D refPoint(0.0f, 0.0f);
    edm->updateSimulationTiers(refPoint, 1500.0f, 10000.0f);

    // Check tiers
    const auto& nearHot = edm->getHotData(near);
    const auto& midHot = edm->getHotData(mid);
    const auto& farHot = edm->getHotData(far);

    BOOST_CHECK_EQUAL(static_cast<int>(nearHot.tier), static_cast<int>(SimulationTier::Active));
    BOOST_CHECK_EQUAL(static_cast<int>(midHot.tier), static_cast<int>(SimulationTier::Background));
    BOOST_CHECK_EQUAL(static_cast<int>(farHot.tier), static_cast<int>(SimulationTier::Hibernated));
}

BOOST_AUTO_TEST_CASE(TestGetActiveIndices) {
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");

    // Force tier update
    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    auto activeIndices = edm->getActiveIndices();
    BOOST_CHECK_EQUAL(activeIndices.size(), 2);
}

BOOST_AUTO_TEST_CASE(TestGetBackgroundIndices) {
    // Create entities at background distance
    edm->createNPCWithRaceClass(Vector2D(5000.0f, 5000.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(6000.0f, 6000.0f), "Human", "Guard");

    // Update tiers
    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    auto bgIndices = edm->getBackgroundIndices();
    BOOST_CHECK_EQUAL(bgIndices.size(), 2);
}

BOOST_AUTO_TEST_CASE(TestEntityCountByTier) {
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");      // Will be active
    edm->createNPCWithRaceClass(Vector2D(5000.0f, 5000.0f), "Human", "Guard");    // Will be background
    edm->createNPCWithRaceClass(Vector2D(15000.0f, 15000.0f), "Human", "Guard");  // Will be hibernated

    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    BOOST_CHECK_EQUAL(edm->getEntityCount(SimulationTier::Active), 1);
    BOOST_CHECK_EQUAL(edm->getEntityCount(SimulationTier::Background), 1);
    BOOST_CHECK_EQUAL(edm->getEntityCount(SimulationTier::Hibernated), 1);
}

BOOST_AUTO_TEST_CASE(TestPlayerAlwaysActive) {
    // Player should stay active regardless of distance
    EntityHandle player = edm->registerPlayer(1,Vector2D(50000.0f, 50000.0f));

    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    const auto& hot = edm->getHotData(player);
    BOOST_CHECK_EQUAL(static_cast<int>(hot.tier), static_cast<int>(SimulationTier::Active));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// QUERY TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(QueryTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestQueryEntitiesInRadius) {
    // Create entities at known positions
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");   // In radius
    edm->createNPCWithRaceClass(Vector2D(150.0f, 150.0f), "Human", "Guard");   // In radius
    edm->createNPCWithRaceClass(Vector2D(1000.0f, 1000.0f), "Human", "Guard"); // Out of radius

    std::vector<EntityHandle> found;
    edm->queryEntitiesInRadius(Vector2D(100.0f, 100.0f), 200.0f, found);

    BOOST_CHECK_EQUAL(found.size(), 2);
}

BOOST_AUTO_TEST_CASE(TestQueryEntitiesWithKindFilter) {
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->registerPlayer(1,Vector2D(150.0f, 150.0f));
    edm->createDroppedItem(Vector2D(120.0f, 120.0f), VoidLight::ResourceHandle{1, 1}, 1);

    std::vector<EntityHandle> found;
    edm->queryEntitiesInRadius(Vector2D(100.0f, 100.0f), 500.0f, found, EntityKind::NPC);

    BOOST_CHECK_EQUAL(found.size(), 1);
    BOOST_CHECK(found[0].isNPC());
}

BOOST_AUTO_TEST_CASE(TestQueryEmptyResult) {
    edm->createNPCWithRaceClass(Vector2D(1000.0f, 1000.0f), "Human", "Guard");

    std::vector<EntityHandle> found;
    edm->queryEntitiesInRadius(Vector2D(0.0f, 0.0f), 100.0f, found);

    BOOST_CHECK(found.empty());
}

BOOST_AUTO_TEST_CASE(TestGetEntityCount) {
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);

    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 1);

    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 2);
}

BOOST_AUTO_TEST_CASE(TestGetEntityCountByKind) {
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    edm->registerPlayer(1,Vector2D(300.0f, 300.0f));
    edm->createDroppedItem(Vector2D(400.0f, 400.0f), VoidLight::ResourceHandle{1, 1}, 1);

    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::NPC), 2);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::Player), 1);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::DroppedItem), 1);
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::Projectile), 0);
}

BOOST_AUTO_TEST_CASE(TestGetIndicesByKind) {
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    edm->registerPlayer(1,Vector2D(300.0f, 300.0f));

    auto npcIndices = edm->getIndicesByKind(EntityKind::NPC);
    BOOST_CHECK_EQUAL(npcIndices.size(), 2);

    auto playerIndices = edm->getIndicesByKind(EntityKind::Player);
    BOOST_CHECK_EQUAL(playerIndices.size(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ENTITY LOOKUP TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EntityLookupTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestGetEntityId) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->getIndex(handle);

    EntityHandle::IDType id = edm->getEntityId(index);
    BOOST_CHECK_EQUAL(id, handle.id);
}

BOOST_AUTO_TEST_CASE(TestGetEntityIdInvalidIndex) {
    EntityHandle::IDType id = edm->getEntityId(SIZE_MAX);
    BOOST_CHECK_EQUAL(id, 0);
}

BOOST_AUTO_TEST_CASE(TestGetHandle) {
    EntityHandle original = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->getIndex(original);

    EntityHandle retrieved = edm->getHandle(index);
    BOOST_CHECK(retrieved.isValid());
    BOOST_CHECK_EQUAL(retrieved.id, original.id);
    BOOST_CHECK_EQUAL(retrieved.generation, original.generation);
    BOOST_CHECK_EQUAL(static_cast<int>(retrieved.kind), static_cast<int>(original.kind));
}

BOOST_AUTO_TEST_CASE(TestGetHandleInvalidIndex) {
    EntityHandle handle = edm->getHandle(SIZE_MAX);
    BOOST_CHECK(!handle.isValid());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SLOT REUSE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SlotReuseTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestSlotReuseAfterDestruction) {
    // Create and destroy entities to test slot reuse
    std::vector<EntityHandle> handles;
    for (int i = 0; i < 10; ++i) {
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(static_cast<float>(i * 100), 0.0f), "Human", "Guard"));
    }
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 10);

    // Destroy half
    for (int i = 0; i < 5; ++i) {
        edm->destroyEntity(handles[i]);
    }
    edm->processDestructionQueue();
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 5);

    // Create new entities - should reuse slots
    for (int i = 0; i < 5; ++i) {
        edm->createNPCWithRaceClass(Vector2D(static_cast<float>(i * 100 + 50), 100.0f), "Human", "Guard");
    }
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 10);

    // Verify all handles are valid
    for (int i = 5; i < 10; ++i) {
        BOOST_CHECK(edm->isValidHandle(handles[i]));
    }
}

BOOST_AUTO_TEST_CASE(TestTypeSpecificSlotReuse) {
    // Create character entities
    EntityHandle npc1 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    EntityHandle npc2 = edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");

    // Destroy first NPC
    edm->destroyEntity(npc1);
    edm->processDestructionQueue();

    // Create new NPC - should reuse character data slot
    EntityHandle npc3 = edm->createNPCWithRaceClass(Vector2D(300.0f, 300.0f), "Human", "Guard");

    // Both remaining NPCs should be valid
    BOOST_CHECK(!edm->isValidHandle(npc1));
    BOOST_CHECK(edm->isValidHandle(npc2));
    BOOST_CHECK(edm->isValidHandle(npc3));

    // Verify character data is accessible
    BOOST_CHECK_NO_THROW(static_cast<void>(edm->getCharacterData(npc2)));
    BOOST_CHECK_NO_THROW(static_cast<void>(edm->getCharacterData(npc3)));
    BOOST_CHECK(edm->isValidHandle(npc2));
    BOOST_CHECK(edm->isValidHandle(npc3));
}

BOOST_AUTO_TEST_CASE(TestMassCreationAndDestruction) {
    const size_t COUNT = 1000;

    // Create many entities
    std::vector<EntityHandle> handles;
    handles.reserve(COUNT);
    for (size_t i = 0; i < COUNT; ++i) {
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(static_cast<float>(i), 0.0f), "Human", "Guard"));
    }
    BOOST_CHECK_EQUAL(edm->getEntityCount(), COUNT);

    // Destroy all
    for (const auto& handle : handles) {
        edm->destroyEntity(handle);
    }
    edm->processDestructionQueue();
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);

    // Create again - should reuse all slots
    handles.clear();
    for (size_t i = 0; i < COUNT; ++i) {
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(static_cast<float>(i), 0.0f), "Human", "Guard"));
    }
    BOOST_CHECK_EQUAL(edm->getEntityCount(), COUNT);

    // All handles should be valid
    for (const auto& handle : handles) {
        BOOST_CHECK(edm->isValidHandle(handle));
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// STATE TRANSITION CACHED INDICES TESTS
// ============================================================================
/**
 * @brief Comprehensive regression tests for state transition cleanup.
 *
 * These tests verify that prepareForStateTransition() properly clears
 * ALL cached index vectors. Stale cached indices can cause crashes when:
 * - A new state is entered
 * - Managers iterate over the cached indices
 * - The indices point to cleared/invalid data
 *
 * Bug pattern: m_hotData was cleared but cached index vectors were not,
 * leading to assertion failures in getHotDataByIndex() when the stale
 * indices were used.
 */
BOOST_FIXTURE_TEST_SUITE(StateTransitionCachedIndicesTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsActiveIndices) {
    // Create entities that will be in Active tier
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(300.0f, 300.0f), "Human", "Guard");

    // Update tiers to populate active indices
    edm->updateSimulationTiers(Vector2D(150.0f, 150.0f), 1500.0f, 10000.0f);

    // Verify active indices are populated
    auto activeIndices = edm->getActiveIndices();
    BOOST_CHECK_EQUAL(activeIndices.size(), 3);

    // State transition
    edm->prepareForStateTransition();

    // Active indices should be empty
    BOOST_CHECK(edm->getActiveIndices().empty());
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsBackgroundIndices) {
    // Create entities at background distance
    edm->createNPCWithRaceClass(Vector2D(5000.0f, 5000.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(6000.0f, 6000.0f), "Human", "Guard");

    // Update tiers - should be Background
    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    // Verify background indices are populated
    auto bgIndices = edm->getBackgroundIndices();
    BOOST_CHECK_EQUAL(bgIndices.size(), 2);

    // State transition
    edm->prepareForStateTransition();

    // Background indices should be empty
    BOOST_CHECK(edm->getBackgroundIndices().empty());
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsHibernatedIndices) {
    // Create entities at hibernation distance
    edm->createNPCWithRaceClass(Vector2D(15000.0f, 15000.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(20000.0f, 20000.0f), "Human", "Guard");

    // Update tiers - should be Hibernated
    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    // State transition
    edm->prepareForStateTransition();

    // Entity count should be 0
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsActiveCollisionIndices) {
    // Create entities with collision enabled
    EntityHandle h1 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    EntityHandle h2 = edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");

    // Enable collision on entities
    auto& hot1 = edm->getHotData(h1);
    auto& hot2 = edm->getHotData(h2);
    hot1.setCollisionEnabled(true);
    hot2.setCollisionEnabled(true);

    // Update tiers to make them Active
    edm->updateSimulationTiers(Vector2D(100.0f, 100.0f), 1500.0f, 10000.0f);

    // Get active collision indices - this populates the cache
    auto collisionIndices = edm->getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(collisionIndices.size(), 2);

    // State transition
    edm->prepareForStateTransition();

    // Collision indices should be empty
    BOOST_CHECK(edm->getActiveIndicesWithCollision().empty());
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsTriggerDetectionIndices) {
    // Create entities that need trigger detection (e.g., Player)
    EntityHandle h1 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    EntityHandle h2 = edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");

    // Set trigger detection flag (distinct from isTrigger - this is for entities
    // that need to DETECT triggers, like the player)
    auto& hot1 = edm->getHotData(h1);
    auto& hot2 = edm->getHotData(h2);
    hot1.setTriggerDetection(true);
    hot2.setTriggerDetection(true);

    // Update tiers to make entities Active (trigger detection only works on active)
    edm->updateSimulationTiers(Vector2D(100.0f, 100.0f), 1500.0f, 10000.0f);

    // Get trigger detection indices - this populates the cache
    auto triggerIndices = edm->getTriggerDetectionIndices();
    BOOST_CHECK_EQUAL(triggerIndices.size(), 2);

    // State transition
    edm->prepareForStateTransition();

    // Trigger detection indices should be empty
    BOOST_CHECK(edm->getTriggerDetectionIndices().empty());
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsKindIndices) {
    // Create entities of different kinds
    edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    edm->registerPlayer(99999, Vector2D(300.0f, 300.0f));  // Use unique ID to avoid collision
    // Note: DroppedItems are now in static pool, not tracked by getIndicesByKind()
    edm->createDroppedItem(Vector2D(400.0f, 400.0f), VoidLight::ResourceHandle{1, 1}, 1);

    // Get kind indices - this populates the cache (dynamic pool only)
    auto npcIndices = edm->getIndicesByKind(EntityKind::NPC);
    auto playerIndices = edm->getIndicesByKind(EntityKind::Player);

    BOOST_CHECK_EQUAL(npcIndices.size(), 2);
    BOOST_CHECK_EQUAL(playerIndices.size(), 1);
    // DroppedItems are in static pool - getIndicesByKind returns 0 for them
    BOOST_CHECK(edm->getIndicesByKind(EntityKind::DroppedItem).empty());

    // State transition
    edm->prepareForStateTransition();

    // All dynamic pool kind indices should be empty
    BOOST_CHECK(edm->getIndicesByKind(EntityKind::NPC).empty());
    BOOST_CHECK(edm->getIndicesByKind(EntityKind::Player).empty());
    BOOST_CHECK(edm->getIndicesByKind(EntityKind::DroppedItem).empty());
}

/**
 * @test TestAllCachedIndicesClearedComprehensive
 *
 * Master test that populates ALL cached index types and verifies
 * they are all cleared after prepareForStateTransition().
 */
BOOST_AUTO_TEST_CASE(TestAllCachedIndicesClearedComprehensive) {
    // Create diverse entity set
    std::vector<EntityHandle> handles;

    // NPCs at various distances
    for (int i = 0; i < 5; ++i) {
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(100.0f + i * 50, 100.0f), "Human", "Guard"));
    }

    // Background distance
    handles.push_back(edm->createNPCWithRaceClass(Vector2D(5000.0f, 5000.0f), "Human", "Guard"));

    // Hibernated distance
    handles.push_back(edm->createNPCWithRaceClass(Vector2D(15000.0f, 15000.0f), "Human", "Guard"));

    // Player (always active)
    handles.push_back(edm->registerPlayer(1,Vector2D(300.0f, 300.0f)));

    // Items
    handles.push_back(edm->createDroppedItem(Vector2D(400.0f, 400.0f),
        VoidLight::ResourceHandle{1, 1}, 5));

    // Enable collision on some
    for (size_t i = 0; i < 3; ++i) {
        auto& hot = edm->getHotData(handles[i]);
        hot.setCollisionEnabled(true);
    }

    // Set trigger detection on some (entities that DETECT triggers)
    for (size_t i = 3; i < 5; ++i) {
        auto& hot = edm->getHotData(handles[i]);
        hot.setTriggerDetection(true);
    }

    // Update tiers to populate all tier-based caches
    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    // Force population of all caches
    [[maybe_unused]] auto activeIndices = edm->getActiveIndices();
    [[maybe_unused]] auto bgIndices = edm->getBackgroundIndices();
    [[maybe_unused]] auto collisionIndices = edm->getActiveIndicesWithCollision();
    [[maybe_unused]] auto triggerIndices = edm->getTriggerDetectionIndices();
    [[maybe_unused]] auto npcIndices = edm->getIndicesByKind(EntityKind::NPC);
    [[maybe_unused]] auto playerIndices = edm->getIndicesByKind(EntityKind::Player);
    [[maybe_unused]] auto itemIndices = edm->getIndicesByKind(EntityKind::DroppedItem);

    // Verify caches are populated
    BOOST_CHECK(!edm->getActiveIndices().empty());
    BOOST_CHECK_GT(edm->getEntityCount(), 0);

    // State transition - MUST clear ALL cached indices
    edm->prepareForStateTransition();

    // Verify entity count is zero
    BOOST_CHECK_EQUAL(edm->getEntityCount(), 0);

    // Verify ALL cached index vectors are empty
    BOOST_CHECK_MESSAGE(edm->getActiveIndices().empty(),
        "m_activeIndices not cleared");
    BOOST_CHECK_MESSAGE(edm->getBackgroundIndices().empty(),
        "m_backgroundIndices not cleared");
    BOOST_CHECK_MESSAGE(edm->getActiveIndicesWithCollision().empty(),
        "m_activeCollisionIndices not cleared");
    BOOST_CHECK_MESSAGE(edm->getTriggerDetectionIndices().empty(),
        "m_triggerDetectionIndices not cleared");
    BOOST_CHECK_MESSAGE(edm->getIndicesByKind(EntityKind::NPC).empty(),
        "m_kindIndices[NPC] not cleared");
    BOOST_CHECK_MESSAGE(edm->getIndicesByKind(EntityKind::Player).empty(),
        "m_kindIndices[Player] not cleared");
    BOOST_CHECK_MESSAGE(edm->getIndicesByKind(EntityKind::DroppedItem).empty(),
        "m_kindIndices[DroppedItem] not cleared");
}

/**
 * @test TestNoStaleIndicesAfterStateTransitionReuse
 *
 * Tests that after state transition, creating new entities
 * produces fresh indices that don't conflict with stale cached data.
 */
BOOST_AUTO_TEST_CASE(TestNoStaleIndicesAfterStateTransitionReuse) {
    // Phase 1: Create and populate caches
    std::vector<EntityHandle> phase1Handles;
    for (int i = 0; i < 20; ++i) {
        phase1Handles.push_back(edm->createNPCWithRaceClass(
            Vector2D(static_cast<float>(i * 50), 0.0f), "Human", "Guard"));
    }

    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 2000.0f, 10000.0f);

    // Enable collision
    for (auto& h : phase1Handles) {
        edm->getHotData(h).setCollisionEnabled(true);
    }

    auto phase1Collision = edm->getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(phase1Collision.size(), 20);

    // Phase 2: State transition
    edm->prepareForStateTransition();
    phase1Handles.clear();

    // Phase 3: Create new entities
    std::vector<EntityHandle> phase2Handles;
    for (int i = 0; i < 10; ++i) {
        phase2Handles.push_back(edm->createNPCWithRaceClass(
            Vector2D(static_cast<float>(i * 100), 0.0f), "Human", "Guard"));
    }

    edm->updateSimulationTiers(Vector2D(0.0f, 0.0f), 2000.0f, 10000.0f);

    for (auto& h : phase2Handles) {
        edm->getHotData(h).setCollisionEnabled(true);
    }

    // Get new collision indices
    auto phase2Collision = edm->getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(phase2Collision.size(), 10);

    // Verify all indices are valid and accessible
    for (size_t idx : phase2Collision) {
        BOOST_CHECK_NO_THROW({
            [[maybe_unused]] const auto& hot = edm->getHotDataByIndex(idx);
        });
    }
}

/**
 * @test TestAccessAfterClearDoesNotCrash
 *
 * Regression test: After clearing, any attempt to access data via
 * stale indices should be caught, not cause undefined behavior.
 */
BOOST_AUTO_TEST_CASE(TestAccessAfterClearDoesNotCrash) {
    // Create entity and get its index
    EntityHandle h = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    size_t index = edm->getIndex(h);
    BOOST_REQUIRE(index != SIZE_MAX);

    // State transition
    edm->prepareForStateTransition();

    // Handle should now be invalid
    BOOST_CHECK(!edm->isValidHandle(h));

    // getIndex on stale handle should return SIZE_MAX
    BOOST_CHECK_EQUAL(edm->getIndex(h), SIZE_MAX);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// NPC RENDER DATA TESTS
// ============================================================================
/**
 * @brief Tests for NPCRenderData initialization and lifecycle.
 *
 * Verifies that createNPCWithRaceClass() correctly populates NPCRenderData
 * from AnimationConfig parameters, and that the data is properly cleared
 * on entity destruction.
 */
BOOST_FIXTURE_TEST_SUITE(NPCRenderDataTests, EntityDataManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestNPCRenderDataInitialization) {
    // Create NPC using data-driven approach (config loaded from races.json)
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());

    // Get render data via handle
    const auto& renderData = edm->getNPCRenderData(handle);

    // Verify animation config was loaded from JSON (Guard uses idle row 0, move row 1)
    // Values come from races.json, so we verify they were loaded
    BOOST_CHECK_GE(renderData.numIdleFrames, 1);
    BOOST_CHECK_GE(renderData.numMoveFrames, 1);
    BOOST_CHECK_GE(renderData.idleSpeedMs, 1);
    BOOST_CHECK_GE(renderData.moveSpeedMs, 1);

    // Verify initial state
    BOOST_CHECK_EQUAL(renderData.currentFrame, 0);
    BOOST_CHECK(approxEqual(renderData.animationAccumulator, 0.0f));
    BOOST_CHECK_EQUAL(renderData.flipMode, 0);  // SDL_FLIP_NONE

    // Verify atlas coordinates were loaded
    BOOST_CHECK_GE(renderData.atlasX, 0);
    BOOST_CHECK_GE(renderData.atlasY, 0);
}

BOOST_AUTO_TEST_CASE(TestNPCRenderDataDefaultsWithoutTexture) {
    // Create NPC - in test environment without renderer, atlas texture won't exist
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());

    const auto& renderData = edm->getNPCRenderData(handle);

    // Frame dimensions should be set from JSON config
    BOOST_CHECK_GT(renderData.frameWidth, 0);
    BOOST_CHECK_GT(renderData.frameHeight, 0);
}

BOOST_AUTO_TEST_CASE(TestNPCRenderDataMinimumValues) {
    // Create NPC using data-driven approach
    // EDM should enforce minimum values regardless of JSON config
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());

    const auto& renderData = edm->getNPCRenderData(handle);

    // Should always have at least 1 frame and 1ms speed (enforced by EDM)
    BOOST_CHECK_GE(renderData.numIdleFrames, 1);
    BOOST_CHECK_GE(renderData.numMoveFrames, 1);
    BOOST_CHECK_GE(renderData.idleSpeedMs, 1);
    BOOST_CHECK_GE(renderData.moveSpeedMs, 1);
}

BOOST_AUTO_TEST_CASE(TestContainerRenderDataUsesMappedAtlasForKnownContainerTypes) {
    EntityHandle handle = edm->createContainer(Vector2D(100.0f, 100.0f), ContainerType::Chest, 12, 0);
    BOOST_REQUIRE(handle.isValid());

    const auto& renderData = edm->getContainerRenderDataByTypeIndex(edm->getHotData(handle).typeLocalIndex);
    BOOST_CHECK(renderData.atlasX != 0 || renderData.atlasY != 0);
    BOOST_CHECK(renderData.openAtlasX != 0 || renderData.openAtlasY != 0);
    BOOST_CHECK_GT(renderData.frameWidth, 0);
    BOOST_CHECK_GT(renderData.frameHeight, 0);
}

BOOST_AUTO_TEST_CASE(TestContainerRenderDataPreservesOpenVariantDimensions) {
    EntityHandle handle = edm->createContainer(Vector2D(100.0f, 100.0f), ContainerType::Chest, 12, 0);
    BOOST_REQUIRE(handle.isValid());

    const auto& renderData = edm->getContainerRenderDataByTypeIndex(edm->getHotData(handle).typeLocalIndex);
    BOOST_CHECK_EQUAL(renderData.frameWidth, 30);
    BOOST_CHECK_EQUAL(renderData.frameHeight, 28);
    BOOST_CHECK_EQUAL(renderData.openFrameWidth, 30);
    BOOST_CHECK_EQUAL(renderData.openFrameHeight, 31);
}

BOOST_AUTO_TEST_CASE(TestMultipleNPCsGetSeparateRenderData) {
    // Create two NPCs of same type at different positions
    EntityHandle h1 = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    EntityHandle h2 = edm->createNPCWithRaceClass(Vector2D(200.0f, 200.0f), "Human", "Guard");
    BOOST_REQUIRE(h1.isValid());
    BOOST_REQUIRE(h2.isValid());

    auto& rd1 = edm->getNPCRenderData(h1);
    auto& rd2 = edm->getNPCRenderData(h2);

    // Both NPCs should have same config from Guard type
    BOOST_CHECK_EQUAL(rd1.idleRow, rd2.idleRow);
    BOOST_CHECK_EQUAL(rd1.moveRow, rd2.moveRow);
    BOOST_CHECK_EQUAL(rd1.numIdleFrames, rd2.numIdleFrames);
    BOOST_CHECK_EQUAL(rd1.numMoveFrames, rd2.numMoveFrames);

    // But they should have separate instances (can modify independently)
    rd1.currentFrame = 1;
    rd2.currentFrame = 2;
    BOOST_CHECK_NE(rd1.currentFrame, rd2.currentFrame);

    // Verify they point to different memory
    BOOST_CHECK_NE(&rd1, &rd2);
}

BOOST_AUTO_TEST_CASE(TestNPCRenderDataClearedOnDestroy) {
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());

    // Destroy the entity
    edm->destroyEntity(handle);
    edm->processDestructionQueue();

    // Handle should be invalid
    BOOST_CHECK(!edm->isValidHandle(handle));
    BOOST_CHECK_EQUAL(edm->getEntityCount(EntityKind::NPC), 0);
}

BOOST_AUTO_TEST_CASE(TestCharacterEquipmentRecalculatesCachedStats) {
    EntityHandle player = edm->registerPlayer(40001, Vector2D(10.0f, 10.0f));
    BOOST_REQUIRE(player.isValid());
    edm->setCharacterBaseStats(player, 100.0f, 100.0f, 25.0f, 50.0f, 120.0f);

    const uint32_t inventory = edm->createInventory(10, false);
    BOOST_REQUIRE_NE(inventory, INVALID_INVENTORY_INDEX);
    edm->setCharacterInventoryIndex(player, inventory);

    auto ironSword = ResourceTemplateManager::Instance().getHandleById("iron_sword");
    auto ironArmor = ResourceTemplateManager::Instance().getHandleById("iron_armor");
    BOOST_REQUIRE(ironSword.isValid());
    BOOST_REQUIRE(ironArmor.isValid());
    BOOST_REQUIRE(edm->addToInventory(inventory, ironSword, 1));
    BOOST_REQUIRE(edm->addToInventory(inventory, ironArmor, 1));

    BOOST_REQUIRE(edm->equipCharacterItem(player, ironSword));
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).attackDamage, 35.0f, 0.001f);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(inventory, ironSword), 0);

    BOOST_REQUIRE(edm->equipCharacterItem(player, ironArmor));
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).armorDefense, 20.0f, 0.001f);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).moveSpeed, 115.0f, 0.001f);

    BOOST_REQUIRE(edm->unequipCharacterItem(player, "weapon"));
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).attackDamage, 25.0f, 0.001f);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(inventory, ironSword), 1);
}

BOOST_AUTO_TEST_CASE(TestInventoryTransferMovesFullQuantityAtomically) {
    const auto ironSword = ResourceTemplateManager::Instance().getHandleById("iron_sword");
    BOOST_REQUIRE(ironSword.isValid());

    const uint32_t sourceInventory = edm->createInventory(2, false);
    const uint32_t targetInventory = edm->createInventory(2, false);
    BOOST_REQUIRE_NE(sourceInventory, INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE_NE(targetInventory, INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE(edm->addToInventory(sourceInventory, ironSword, 1));

    const auto transfer =
        edm->transferInventoryItem(sourceInventory, targetInventory, ironSword, 1);
    BOOST_REQUIRE(transfer.has_value());

    BOOST_CHECK_EQUAL(transfer->sourceChange.oldQuantity, 1);
    BOOST_CHECK_EQUAL(transfer->sourceChange.newQuantity, 0);
    BOOST_CHECK_EQUAL(transfer->targetChange.oldQuantity, 0);
    BOOST_CHECK_EQUAL(transfer->targetChange.newQuantity, 1);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(sourceInventory, ironSword), 0);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(targetInventory, ironSword), 1);
}

BOOST_AUTO_TEST_CASE(TestInventoryTransferRejectsFullTargetWithoutMutation) {
    const auto ironSword = ResourceTemplateManager::Instance().getHandleById("iron_sword");
    const auto ironArmor = ResourceTemplateManager::Instance().getHandleById("iron_armor");
    BOOST_REQUIRE(ironSword.isValid());
    BOOST_REQUIRE(ironArmor.isValid());

    const uint32_t sourceInventory = edm->createInventory(2, false);
    const uint32_t targetInventory = edm->createInventory(1, false);
    BOOST_REQUIRE_NE(sourceInventory, INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE_NE(targetInventory, INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE(edm->addToInventory(sourceInventory, ironSword, 1));
    BOOST_REQUIRE(edm->addToInventory(targetInventory, ironArmor, 1));

    const auto transfer =
        edm->transferInventoryItem(sourceInventory, targetInventory, ironSword, 1);
    BOOST_CHECK(!transfer.has_value());
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(sourceInventory, ironSword), 1);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(targetInventory, ironSword), 0);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(targetInventory, ironArmor), 1);
}

BOOST_AUTO_TEST_CASE(TestInventoryTransferRejectsSourceShortageWithoutMutation) {
    const auto ironSword = ResourceTemplateManager::Instance().getHandleById("iron_sword");
    BOOST_REQUIRE(ironSword.isValid());

    const uint32_t sourceInventory = edm->createInventory(2, false);
    const uint32_t targetInventory = edm->createInventory(2, false);
    BOOST_REQUIRE_NE(sourceInventory, INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE_NE(targetInventory, INVALID_INVENTORY_INDEX);
    BOOST_REQUIRE(edm->addToInventory(sourceInventory, ironSword, 1));

    const auto transfer =
        edm->transferInventoryItem(sourceInventory, targetInventory, ironSword, 2);
    BOOST_CHECK(!transfer.has_value());
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(sourceInventory, ironSword), 1);
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(targetInventory, ironSword), 0);
}

BOOST_AUTO_TEST_CASE(TestRangedWeaponConsumesCompatibleAmmunition) {
    EntityHandle player = edm->registerPlayer(40004, Vector2D(10.0f, 10.0f));
    BOOST_REQUIRE(player.isValid());
    edm->setCharacterBaseStats(player, 100.0f, 100.0f, 25.0f, 50.0f, 120.0f);

    const uint32_t inventory = edm->createInventory(10, false);
    BOOST_REQUIRE_NE(inventory, INVALID_INVENTORY_INDEX);
    edm->setCharacterInventoryIndex(player, inventory);

    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    const auto arrows = ResourceTemplateManager::Instance().getHandleById("arrows");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(arrows.isValid());
    BOOST_REQUIRE(edm->addToInventory(inventory, bow, 1));
    BOOST_REQUIRE(edm->addToInventory(inventory, arrows, 2));

    BOOST_REQUIRE(edm->equipCharacterItem(player, bow));
    BOOST_CHECK_EQUAL(edm->getCharacterData(player).combatStyle,
                      CharacterData::CombatStyle::Ranged);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).attackRange, 400.0f, 0.001f);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).projectileSpeed, 300.0f, 0.001f);

    InventoryResourceChange ammoChange{};
    BOOST_CHECK(edm->consumeRequiredAmmoForRangedAttack(player, &ammoChange));
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(inventory, arrows), 1);
    BOOST_CHECK(ammoChange.resourceHandle == arrows);
    BOOST_CHECK_EQUAL(ammoChange.oldQuantity, 2);
    BOOST_CHECK_EQUAL(ammoChange.newQuantity, 1);
}

BOOST_AUTO_TEST_CASE(TestRangedWeaponWithoutAmmoDoesNotConsumeInventory) {
    EntityHandle player = edm->registerPlayer(40005, Vector2D(10.0f, 10.0f));
    BOOST_REQUIRE(player.isValid());
    edm->setCharacterBaseStats(player, 100.0f, 100.0f, 25.0f, 50.0f, 120.0f);

    const uint32_t inventory = edm->createInventory(10, false);
    BOOST_REQUIRE_NE(inventory, INVALID_INVENTORY_INDEX);
    edm->setCharacterInventoryIndex(player, inventory);

    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(edm->addToInventory(inventory, bow, 1));

    BOOST_REQUIRE(edm->equipCharacterItem(player, bow));
    InventoryResourceChange ammoChange{};
    BOOST_CHECK(!edm->consumeRequiredAmmoForRangedAttack(player, &ammoChange));

    BOOST_CHECK_EQUAL(edm->getCharacterData(player).combatStyle,
                      CharacterData::CombatStyle::Ranged);
    BOOST_CHECK(!ammoChange.isValid());
}

BOOST_AUTO_TEST_CASE(TestTwoHandedWeaponClearsShieldSlot) {
    EntityHandle player = edm->registerPlayer(40002, Vector2D(10.0f, 10.0f));
    BOOST_REQUIRE(player.isValid());
    edm->setCharacterBaseStats(player, 100.0f, 100.0f, 25.0f, 50.0f, 120.0f);

    const uint32_t inventory = edm->createInventory(10, false);
    BOOST_REQUIRE_NE(inventory, INVALID_INVENTORY_INDEX);
    edm->setCharacterInventoryIndex(player, inventory);

    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    const auto ironShield = ResourceTemplateManager::Instance().getHandleById("iron_shield");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(ironShield.isValid());
    BOOST_REQUIRE(edm->addToInventory(inventory, ironShield, 1));
    BOOST_REQUIRE(edm->addToInventory(inventory, bow, 1));

    BOOST_REQUIRE(edm->equipCharacterItem(player, ironShield));
    BOOST_CHECK(edm->getEquippedCharacterItem(player, "shield") == ironShield);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).armorDefense, 15.0f, 0.001f);

    BOOST_REQUIRE(edm->equipCharacterItem(player, bow));
    BOOST_CHECK(edm->getEquippedCharacterItem(player, "weapon") == bow);
    BOOST_CHECK(!edm->getEquippedCharacterItem(player, "shield").isValid());
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(inventory, ironShield), 1);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).attackDamage, 33.0f, 0.001f);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).armorDefense, 0.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestShieldCannotEquipOverTwoHandedWeapon) {
    EntityHandle player = edm->registerPlayer(40003, Vector2D(10.0f, 10.0f));
    BOOST_REQUIRE(player.isValid());
    edm->setCharacterBaseStats(player, 100.0f, 100.0f, 25.0f, 50.0f, 120.0f);

    const uint32_t inventory = edm->createInventory(10, false);
    BOOST_REQUIRE_NE(inventory, INVALID_INVENTORY_INDEX);
    edm->setCharacterInventoryIndex(player, inventory);

    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    const auto ironShield = ResourceTemplateManager::Instance().getHandleById("iron_shield");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(ironShield.isValid());
    BOOST_REQUIRE(edm->addToInventory(inventory, bow, 1));
    BOOST_REQUIRE(edm->addToInventory(inventory, ironShield, 1));

    BOOST_REQUIRE(edm->equipCharacterItem(player, bow));
    BOOST_CHECK(!edm->equipCharacterItem(player, ironShield));
    BOOST_CHECK(edm->getEquippedCharacterItem(player, "weapon") == bow);
    BOOST_CHECK(!edm->getEquippedCharacterItem(player, "shield").isValid());
    BOOST_CHECK_EQUAL(edm->getInventoryQuantity(inventory, ironShield), 1);
    BOOST_CHECK_CLOSE(edm->getCharacterData(player).armorDefense, 0.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestNPCStartingEquipmentAutoEquipsNonMerchants) {
    EntityHandle guard = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(guard.isValid());

    const auto ironSword = ResourceTemplateManager::Instance().getHandleById("iron_sword");
    const auto ironShield = ResourceTemplateManager::Instance().getHandleById("iron_shield");
    BOOST_REQUIRE(ironSword.isValid());
    BOOST_REQUIRE(ironShield.isValid());

    BOOST_CHECK(edm->getEquippedCharacterItem(guard, "weapon") == ironSword);
    BOOST_CHECK(edm->getEquippedCharacterItem(guard, "shield") == ironShield);
    BOOST_CHECK_CLOSE(edm->getCharacterData(guard).attackDamage, 22.0f, 0.001f);
    BOOST_CHECK_CLOSE(edm->getCharacterData(guard).armorDefense, 15.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestNPCStartingEquipmentPreservesClassOrderForSameSlot) {
    EntityHandle ranger = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Ranger");
    BOOST_REQUIRE(ranger.isValid());

    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    const auto dagger = ResourceTemplateManager::Instance().getHandleById("dagger");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(dagger.isValid());

    BOOST_CHECK(edm->getEquippedCharacterItem(ranger, "weapon") == bow);
    BOOST_CHECK_EQUAL(
        edm->getInventoryQuantity(edm->getCharacterData(ranger).inventoryIndex, dagger),
        1);
    BOOST_CHECK_CLOSE(edm->getCharacterData(ranger).attackDamage, 21.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestMerchantStartingEquipmentRemainsInventoryStock) {
    EntityHandle blacksmith = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Blacksmith");
    BOOST_REQUIRE(blacksmith.isValid());

    const auto ironSword = ResourceTemplateManager::Instance().getHandleById("iron_sword");
    BOOST_REQUIRE(ironSword.isValid());

    BOOST_CHECK(!edm->getEquippedCharacterItem(blacksmith, "weapon").isValid());
    BOOST_CHECK_EQUAL(
        edm->getInventoryQuantity(edm->getNPCInventoryIndex(blacksmith), ironSword),
        3);
}

BOOST_AUTO_TEST_SUITE_END()
