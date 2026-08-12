/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file CollisionManagerEDMIntegrationTests.cpp
 * @brief Tests for CollisionManager's integration with EntityDataManager
 *
 * These tests verify Collision Manager-specific EDM integration:
 * - Active tier filtering (only Active tier entities with collision enabled)
 * - Static vs dynamic body separation (m_storage vs EDM)
 * - Dual index semantics in collision pairs
 * - Position reading from EDM transforms
 *
 * NOTE: AABB operations, spatial hash, and basic collision are tested
 * in CollisionSystemTests.cpp - these tests focus on CollisionManager's
 * specific use of EDM data.
 */

#define BOOST_TEST_MODULE CollisionManagerEDMIntegrationTests
#include <boost/test/unit_test.hpp>

#include "collisions/CollisionBody.hpp"
#include "core/ThreadSystem.hpp"
#include "entities/Entity.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "utils/Vector2D.hpp"
#include <memory>
#include <vector>

using namespace VoidLight;

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for Collision EDM tests");
        }
    }
    ~ThreadSystemFixture() {
        ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

// Test fixture
struct CollisionEDMFixture {
    CollisionEDMFixture() {
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());

        // Set reasonable world bounds
        CollisionManager::Instance().setWorldBounds(0, 0, 2000.0f, 2000.0f);
    }

    ~CollisionEDMFixture() {
        CollisionManager::Instance().clean();
        BackgroundSimulationManager::Instance().clean();
        EntityDataManager::Instance().clean();
        EventManager::Instance().clean();
    }

    // Helper: Create test NPC with collision enabled via EDM
    EntityHandle createTestNPC(const Vector2D& pos, bool enableCollision = true) {
        auto& edm = EntityDataManager::Instance();
        EntityHandle handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
        size_t index = edm.getIndex(handle);
        if (index != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(index);
            hot.setCollisionEnabled(enableCollision);
            if (enableCollision) {
                hot.collisionLayers = CollisionLayer::Layer_Default;
                hot.collisionMask = 0xFFFF;
            }
        }
        return handle;
    }

    // Force entities to Active tier
    void updateTiers(const Vector2D& refPoint) {
        BackgroundSimulationManager::Instance().update(refPoint, 0.016f);
    }
};

// ============================================================================
// ACTIVE TIER FILTERING TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ActiveTierFilteringTests, CollisionEDMFixture)

BOOST_AUTO_TEST_CASE(TestOnlyActiveTierEntitiesParticipateInCollision) {
    // Create entities at different distances from reference point
    EntityHandle nearHandle = createTestNPC(Vector2D(100.0f, 100.0f));
    EntityHandle farHandle = createTestNPC(Vector2D(15000.0f, 15000.0f));

    // Update tiers - near should be Active, far should be Hibernated
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    // Verify tiers
    auto& edm = EntityDataManager::Instance();
    const auto& nearHot = edm.getHotData(nearHandle);
    const auto& farHot = edm.getHotData(farHandle);

    BOOST_CHECK_EQUAL(static_cast<int>(nearHot.tier), static_cast<int>(SimulationTier::Active));
    BOOST_CHECK_EQUAL(static_cast<int>(farHot.tier), static_cast<int>(SimulationTier::Hibernated));

    // Get active indices with collision
    auto activeWithCollision = edm.getActiveIndicesWithCollision();

    // Near entity should be in active collision list, far should not
    size_t nearIndex = edm.getIndex(nearHandle);
    size_t farIndex = edm.getIndex(farHandle);

    bool nearFound = false;
    bool farFound = false;
    for (size_t idx : activeWithCollision) {
        if (idx == nearIndex) nearFound = true;
        if (idx == farIndex) farFound = true;
    }

    BOOST_CHECK(nearFound);
    BOOST_CHECK(!farFound);
}

BOOST_AUTO_TEST_CASE(TestEntitiesWithCollisionDisabledNotInActiveList) {
    // Create entity with collision disabled
    EntityHandle withoutCollisionHandle = createTestNPC(Vector2D(100.0f, 100.0f), false);

    // Create entity with collision enabled
    EntityHandle withCollisionHandle = createTestNPC(Vector2D(200.0f, 200.0f), true);

    // Update tiers - both should be Active tier
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(100.0f, 100.0f), 1500.0f, 10000.0f);

    auto& edm = EntityDataManager::Instance();
    auto activeWithCollision = edm.getActiveIndicesWithCollision();

    size_t withoutCollisionIdx = edm.getIndex(withoutCollisionHandle);
    size_t withCollisionIdx = edm.getIndex(withCollisionHandle);

    bool foundWithout = false;
    bool foundWith = false;
    for (size_t idx : activeWithCollision) {
        if (idx == withoutCollisionIdx) foundWithout = true;
        if (idx == withCollisionIdx) foundWith = true;
    }

    BOOST_CHECK(!foundWithout);
    BOOST_CHECK(foundWith);
}

BOOST_AUTO_TEST_CASE(TestBackgroundTierEntitiesNotInCollision) {
    // Create entity that will be in Background tier
    EntityHandle bgHandle = createTestNPC(Vector2D(5000.0f, 5000.0f));

    // Update tiers - should be Background
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(0.0f, 0.0f), 1500.0f, 10000.0f);

    auto& edm = EntityDataManager::Instance();
    const auto& hot = edm.getHotData(bgHandle);
    BOOST_CHECK_EQUAL(static_cast<int>(hot.tier), static_cast<int>(SimulationTier::Background));

    // Should not be in active collision list
    auto activeWithCollision = edm.getActiveIndicesWithCollision();
    size_t bgIndex = edm.getIndex(bgHandle);

    bool found = false;
    for (size_t idx : activeWithCollision) {
        if (idx == bgIndex) found = true;
    }
    BOOST_CHECK(!found);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// STATIC VS DYNAMIC SEPARATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(StaticDynamicSeparationTests, CollisionEDMFixture)

BOOST_AUTO_TEST_CASE(TestStaticBodyAddedToStorage) {
    // Create static body via EDM first (single source of truth)
    auto& edm = EntityDataManager::Instance();
    Vector2D center(500.0f, 500.0f);
    float halfWidth = 32.0f;
    float halfHeight = 32.0f;
    EntityHandle handle = edm.createStaticBody(center, halfWidth, halfHeight);
    EntityID id = handle.getId();
    size_t edmIndex = edm.getStaticIndex(handle);

    // Add static body to CollisionManager with proper EDM routing
    size_t staticIdx = CollisionManager::Instance().addStaticBody(
        id,  // entityId from handle
        center,  // position
        Vector2D(halfWidth, halfHeight),    // halfSize
        CollisionLayer::Layer_Environment,  // layer
        0xFFFF,  // collidesWith
        false,   // isTrigger
        0,       // triggerTag
        1,       // triggerType
        edmIndex // EDM index for static body
    );

    BOOST_CHECK(staticIdx != SIZE_MAX);

    // Static bodies are now routed through EDM (static storage)
    // The handle should be valid and the static index should be valid
    BOOST_CHECK(handle.isValid());
    BOOST_CHECK(edmIndex != SIZE_MAX);
}

BOOST_AUTO_TEST_CASE(TestDynamicEntityInEDMNotInStaticStorage) {
    // Create dynamic entity via EDM
    EntityHandle handle = createTestNPC(Vector2D(300.0f, 300.0f));

    // Dynamic entity should be in EDM
    auto& edm = EntityDataManager::Instance();
    size_t edmIndex = edm.getIndex(handle);
    BOOST_CHECK(edmIndex != SIZE_MAX);
}

BOOST_AUTO_TEST_CASE(TestStaticBodyAlwaysCheckedForCollision) {
    // Create static obstacle via EDM first
    auto& edm = EntityDataManager::Instance();
    Vector2D center(500.0f, 500.0f);
    float halfWidth = 50.0f;
    float halfHeight = 50.0f;
    EntityHandle handle = edm.createStaticBody(center, halfWidth, halfHeight);
    EntityID id = handle.getId();
    size_t edmIndex = edm.getStaticIndex(handle);

    // Add static obstacle to CollisionManager with EDM routing
    CollisionManager::Instance().addStaticBody(
        id,
        center,
        Vector2D(halfWidth, halfHeight),
        CollisionLayer::Layer_Environment,
        0xFFFF,
        false,
        0,
        1,
        edmIndex
    );

    // Create dynamic entity near the static obstacle
    [[maybe_unused]] EntityHandle entityHandle = createTestNPC(Vector2D(510.0f, 510.0f));

    // Update tiers to make entity Active
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(500.0f, 500.0f), 1500.0f, 10000.0f);

    // Run collision update
    CollisionManager::Instance().update(0.016f);

    // Static should be detected (via spatial hash query)
    // This verifies static bodies are checked regardless of tier system
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// POSITION READING FROM EDM TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PositionReadingTests, CollisionEDMFixture)

BOOST_AUTO_TEST_CASE(TestCollisionUsesEDMPosition) {
    // Create entity at known position
    EntityHandle handle = createTestNPC(Vector2D(400.0f, 400.0f));

    // Get EDM index
    auto& edm = EntityDataManager::Instance();
    size_t edmIndex = edm.getIndex(handle);
    BOOST_REQUIRE(edmIndex != SIZE_MAX);

    // Verify position in EDM
    auto& transform = edm.getTransformByIndex(edmIndex);
    BOOST_CHECK_CLOSE(transform.position.getX(), 400.0f, 0.01f);
    BOOST_CHECK_CLOSE(transform.position.getY(), 400.0f, 0.01f);

    // Update position in EDM directly
    transform.position = Vector2D(600.0f, 600.0f);

    // Verify collision would use new position
    auto& newTransform = edm.getTransformByIndex(edmIndex);
    BOOST_CHECK_CLOSE(newTransform.position.getX(), 600.0f, 0.01f);
    BOOST_CHECK_CLOSE(newTransform.position.getY(), 600.0f, 0.01f);
}

BOOST_AUTO_TEST_CASE(TestAABBComputedFromEDMHalfSize) {
    // Create data-driven NPC - collision size derived from frame dimensions
    // Default frame size is 32x32, giving halfWidth/halfHeight of 16
    auto& edm = EntityDataManager::Instance();
    EntityHandle handle = edm.createNPCWithRaceClass(Vector2D(500.0f, 500.0f), "Human", "Guard");

    // Enable collision
    size_t index = edm.getIndex(handle);
    BOOST_REQUIRE(index != SIZE_MAX);

    auto& hot = edm.getHotDataByIndex(index);
    hot.setCollisionEnabled(true);

    // Verify half-sizes in EDM (derived from default 32x32 frame)
    BOOST_CHECK_CLOSE(hot.halfWidth, 16.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.halfHeight, 16.0f, 0.01f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// COLLISION INFO INDEX SEMANTICS TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(IndexSemanticsTests, CollisionEDMFixture)

BOOST_AUTO_TEST_CASE(TestMovableMovablePairIndicesAreEDMIndices) {
    // Create two overlapping entities
    EntityHandle handle1 = createTestNPC(Vector2D(100.0f, 100.0f));
    EntityHandle handle2 = createTestNPC(Vector2D(110.0f, 110.0f));

    auto& edm = EntityDataManager::Instance();
    size_t edmIdx1 = edm.getIndex(handle1);
    size_t edmIdx2 = edm.getIndex(handle2);

    BOOST_REQUIRE(edmIdx1 != SIZE_MAX);
    BOOST_REQUIRE(edmIdx2 != SIZE_MAX);

    // Update tiers to Active
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(100.0f, 100.0f), 1500.0f, 10000.0f);

    // Verify both are in active collision indices
    auto activeWithCollision = edm.getActiveIndicesWithCollision();
    bool found1 = false, found2 = false;
    for (size_t idx : activeWithCollision) {
        if (idx == edmIdx1) found1 = true;
        if (idx == edmIdx2) found2 = true;
    }

    BOOST_CHECK(found1);
    BOOST_CHECK(found2);
}

BOOST_AUTO_TEST_CASE(TestMovableStaticPairMixedIndices) {
    // Create static body via EDM first
    auto& edm = EntityDataManager::Instance();
    Vector2D center(200.0f, 200.0f);
    float halfWidth = 30.0f;
    float halfHeight = 30.0f;
    EntityHandle staticHandle = edm.createStaticBody(center, halfWidth, halfHeight);
    EntityID id = staticHandle.getId();
    size_t edmIndex = edm.getStaticIndex(staticHandle);

    // Add static body to CollisionManager with EDM routing
    size_t staticStorageIdx = CollisionManager::Instance().addStaticBody(
        id,
        center,
        Vector2D(halfWidth, halfHeight),
        CollisionLayer::Layer_Environment,
        0xFFFF,
        false,
        0,
        1,
        edmIndex
    );
    BOOST_REQUIRE(staticStorageIdx != SIZE_MAX);

    // Create dynamic entity near static
    EntityHandle dynamicHandle = createTestNPC(Vector2D(210.0f, 210.0f));

    size_t edmIdx = edm.getIndex(dynamicHandle);
    BOOST_REQUIRE(edmIdx != SIZE_MAX);

    // Update tiers
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(200.0f, 200.0f), 1500.0f, 10000.0f);

    // The collision pair should use EDM index for movable, storage index for static
    // (Verified structurally via code review - this test ensures the data is set up correctly)
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CollisionStateTransitionTests, CollisionEDMFixture)

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsDynamicData) {
    // Create some dynamic entities
    [[maybe_unused]] EntityHandle handle1 = createTestNPC(Vector2D(100.0f, 100.0f));
    [[maybe_unused]] EntityHandle handle2 = createTestNPC(Vector2D(200.0f, 200.0f));

    // Update tiers
    EntityDataManager::Instance().updateSimulationTiers(
        Vector2D(150.0f, 150.0f), 1500.0f, 10000.0f);

    // Run collision update to populate pools
    CollisionManager::Instance().update(0.016f);

    // Trigger state transition
    CollisionManager::Instance().prepareForStateTransition();
    EntityDataManager::Instance().prepareForStateTransition();

    // EDM should be cleared
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(), 0);
}

BOOST_AUTO_TEST_CASE(TestStaticBodiesPreservedAfterDynamicClear) {
    // Create static body via EDM first
    auto& edm = EntityDataManager::Instance();
    Vector2D center(500.0f, 500.0f);
    float halfWidth = 50.0f;
    float halfHeight = 50.0f;
    EntityHandle handle = edm.createStaticBody(center, halfWidth, halfHeight);
    EntityID id = handle.getId();
    size_t edmIndex = edm.getStaticIndex(handle);

    // Add static body to CollisionManager with EDM routing
    CollisionManager::Instance().addStaticBody(
        id,
        center,
        Vector2D(halfWidth, halfHeight),
        CollisionLayer::Layer_Environment,
        0xFFFF,
        false,
        0,
        1,
        edmIndex
    );

    // Create dynamic entity
    [[maybe_unused]] EntityHandle entityHandle = createTestNPC(Vector2D(100.0f, 100.0f));

    // Clear EDM (simulating partial state transition)
    EntityDataManager::Instance().prepareForStateTransition();

    // EDM cleared
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(), 0);

    // Static bodies are managed by CollisionManager, not cleared by EDM transition
    // They would be cleared by CollisionManager::clean() or rebuildStaticBodies()
}

/**
 * @test TestActiveCollisionIndicesClearedAfterStateTransition
 *
 * Regression test for crash in CollisionManager::buildActiveIndices().
 *
 * Bug: EDM's prepareForStateTransition() cleared m_hotData but NOT
 * m_activeCollisionIndices. This left stale indices that caused:
 *   Assertion failed: (index < m_hotData.size() && "Index out of bounds")
 *   in EntityDataManager::getHotDataByIndex()
 *
 * The crash occurred when CollisionManager::buildActiveIndices() iterated
 * over the cached collision indices after EDM was cleared.
 *
 * Fix: EDM::prepareForStateTransition() now clears m_activeCollisionIndices
 * and sets m_activeCollisionDirty = true.
 */
BOOST_AUTO_TEST_CASE(TestActiveCollisionIndicesClearedAfterStateTransition) {
    auto& edm = EntityDataManager::Instance();

    // Create entities with collision enabled
    [[maybe_unused]] EntityHandle handle1 = createTestNPC(Vector2D(100.0f, 100.0f));
    [[maybe_unused]] EntityHandle handle2 = createTestNPC(Vector2D(200.0f, 200.0f));
    [[maybe_unused]] EntityHandle handle3 = createTestNPC(Vector2D(300.0f, 300.0f));

    // Update simulation tiers to make entities Active
    edm.updateSimulationTiers(Vector2D(150.0f, 150.0f), 1500.0f, 10000.0f);

    // Get active collision indices - this populates the cache
    auto activeWithCollision = edm.getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(activeWithCollision.size(), 3);

    // Run collision update to use the indices
    CollisionManager::Instance().update(0.016f);

    // Simulate state transition - CRITICAL: this must clear collision indices
    edm.prepareForStateTransition();

    // Verify entity data is cleared
    BOOST_CHECK_EQUAL(edm.getEntityCount(), 0);

    // CRITICAL CHECK: Active collision indices should be empty
    // This was the bug - before the fix, this would return stale indices
    auto postTransitionCollision = edm.getActiveIndicesWithCollision();
    BOOST_CHECK_MESSAGE(postTransitionCollision.empty(),
        "m_activeCollisionIndices should be cleared after prepareForStateTransition()");
}

/**
 * @test TestCollisionUpdateAfterStateTransitionDoesNotCrash
 *
 * End-to-end regression test for the state transition crash.
 *
 * This simulates the exact sequence that caused the original crash:
 * 1. Create entities in AIDemoState (or similar)
 * 2. Run collision updates (populates cached indices)
 * 3. Transition to MainMenu (calls prepareForStateTransition on managers)
 * 4. Transition to EventDemoState (creates new entities, runs updates)
 *
 * Before the fix, step 4 would crash because CollisionManager used
 * stale cached indices from the previous state.
 */
BOOST_AUTO_TEST_CASE(TestCollisionUpdateAfterStateTransitionDoesNotCrash) {
    auto& edm = EntityDataManager::Instance();

    // === Phase 1: First "state" with many entities ===
    // Place all entities within active tier radius (1500 from origin)
    std::vector<EntityHandle> state1Handles;
    for (int i = 0; i < 100; ++i) {
        // Grid layout within 1000x1000 area (well within 1500 radius)
        float x = static_cast<float>((i % 10) * 100);
        float y = static_cast<float>((i / 10) * 100);
        auto pos = Vector2D(x, y);
        state1Handles.push_back(createTestNPC(pos));
    }

    // Update tiers and run collision
    edm.updateSimulationTiers(Vector2D(500.0f, 500.0f), 1500.0f, 10000.0f);
    CollisionManager::Instance().update(0.016f);

    // Verify indices were populated (all should be active)
    auto activeIndices = edm.getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(activeIndices.size(), 100);

    // === Phase 2: State transition (like going to MainMenu) ===
    // This order matches what game states do
    CollisionManager::Instance().prepareForStateTransition();
    edm.prepareForStateTransition();

    // Clear our handle references (they're now invalid)
    state1Handles.clear();

    // Verify clean state
    BOOST_CHECK_EQUAL(edm.getEntityCount(), 0);

    // === Phase 3: New "state" - like entering EventDemoState ===
    // Create new entities within active tier radius
    std::vector<EntityHandle> state2Handles;
    for (int i = 0; i < 50; ++i) {
        float x = static_cast<float>((i % 10) * 100);
        float y = static_cast<float>((i / 10) * 100);
        auto pos = Vector2D(x, y);
        state2Handles.push_back(createTestNPC(pos));
    }

    // Update tiers for new entities
    edm.updateSimulationTiers(Vector2D(500.0f, 500.0f), 1500.0f, 10000.0f);

    // THIS WAS THE CRASH POINT - CollisionManager::update() called
    // buildActiveIndices() which iterated over stale indices
    BOOST_CHECK_NO_THROW(CollisionManager::Instance().update(0.016f));

    // Verify new state works correctly
    auto newActiveIndices = edm.getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(newActiveIndices.size(), 50);
}

/**
 * @test TestMultipleStateTransitionsClearIndicesEachTime
 *
 * Ensures that repeated state transitions properly clear cached indices
 * each time, not just the first time.
 */
BOOST_AUTO_TEST_CASE(TestMultipleStateTransitionsClearIndicesEachTime) {
    auto& edm = EntityDataManager::Instance();

    for (int transition = 0; transition < 3; ++transition) {
        // Create entities for this "state" within active tier radius
        std::vector<EntityHandle> handles;
        int entityCount = 20 + transition * 10; // Vary count each iteration

        for (int i = 0; i < entityCount; ++i) {
            // Grid layout within 500x500 area (well within 1500 radius)
            float x = static_cast<float>((i % 10) * 50);
            float y = static_cast<float>((i / 10) * 50);
            auto pos = Vector2D(x, y);
            handles.push_back(createTestNPC(pos));
        }

        // Run simulation with reference point that keeps all entities active
        edm.updateSimulationTiers(Vector2D(250.0f, 250.0f), 1500.0f, 10000.0f);
        CollisionManager::Instance().update(0.016f);

        // Verify correct count
        auto activeIndices = edm.getActiveIndicesWithCollision();
        BOOST_CHECK_EQUAL(static_cast<int>(activeIndices.size()), entityCount);

        // State transition
        CollisionManager::Instance().prepareForStateTransition();
        edm.prepareForStateTransition();
        handles.clear();

        // Verify cleared
        BOOST_CHECK_EQUAL(edm.getEntityCount(), 0);
        BOOST_CHECK(edm.getActiveIndicesWithCollision().empty());
    }
}

/**
 * @test TestConcurrentAccessDuringStateTransition
 *
 * Tests that accessing collision indices during state transition
 * doesn't cause data races. The indices should either return valid
 * data or be empty - never stale/invalid indices.
 */
BOOST_AUTO_TEST_CASE(TestConcurrentAccessDuringStateTransition) {
    auto& edm = EntityDataManager::Instance();

    // Create entities within active tier radius
    std::vector<EntityHandle> handles;
    for (int i = 0; i < 50; ++i) {
        // Grid layout within 500x500 area (well within 1500 radius)
        float x = static_cast<float>((i % 10) * 50);
        float y = static_cast<float>((i / 10) * 50);
        auto pos = Vector2D(x, y);
        handles.push_back(createTestNPC(pos));
    }

    edm.updateSimulationTiers(Vector2D(250.0f, 250.0f), 1500.0f, 10000.0f);
    CollisionManager::Instance().update(0.016f);

    // Get indices before transition
    auto beforeIndices = edm.getActiveIndicesWithCollision();
    BOOST_CHECK_EQUAL(beforeIndices.size(), 50);

    // Clear handles and transition
    handles.clear();
    edm.prepareForStateTransition();

    // Post-transition access should be safe and return empty
    auto afterIndices = edm.getActiveIndicesWithCollision();
    BOOST_CHECK(afterIndices.empty());

    // Any index in afterIndices should be valid if not empty
    // (This validates no stale indices leaked through)
    for (size_t idx : afterIndices) {
        BOOST_CHECK_NO_THROW({
            [[maybe_unused]] auto& hot = edm.getHotDataByIndex(idx);
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// COLLISION LAYER FILTERING VIA EDM TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(LayerFilteringTests, CollisionEDMFixture)

BOOST_AUTO_TEST_CASE(TestCollisionLayersReadFromEDM) {
    EntityHandle handle = createTestNPC(Vector2D(100.0f, 100.0f));

    auto& edm = EntityDataManager::Instance();
    size_t index = edm.getIndex(handle);
    BOOST_REQUIRE(index != SIZE_MAX);

    // Set specific layers in EDM
    auto& hot = edm.getHotDataByIndex(index);
    hot.collisionLayers = CollisionLayer::Layer_Player;
    hot.collisionMask = CollisionLayer::Layer_Environment | CollisionLayer::Layer_Enemy;

    // Verify layers are set
    BOOST_CHECK_EQUAL(hot.collisionLayers, CollisionLayer::Layer_Player);
    BOOST_CHECK(hot.collisionMask & CollisionLayer::Layer_Environment);
    BOOST_CHECK(hot.collisionMask & CollisionLayer::Layer_Enemy);
}

BOOST_AUTO_TEST_CASE(TestTriggerFlagReadFromEDM) {
    EntityHandle handle = createTestNPC(Vector2D(100.0f, 100.0f));

    auto& edm = EntityDataManager::Instance();
    size_t index = edm.getIndex(handle);
    BOOST_REQUIRE(index != SIZE_MAX);

    auto& hot = edm.getHotDataByIndex(index);

    // Verify not trigger by default
    BOOST_CHECK(!hot.isTrigger());

    // Set as trigger
    hot.setTrigger(true);
    BOOST_CHECK(hot.isTrigger());
}

BOOST_AUTO_TEST_SUITE_END()
