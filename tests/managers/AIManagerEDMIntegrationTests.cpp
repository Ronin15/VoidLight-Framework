/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file AIManagerEDMIntegrationTests.cpp
 * @brief Tests for AIManager's integration with EntityDataManager
 *
 * These tests verify AI Manager-specific EDM integration:
 * - Sparse behavior vector (m_behaviorsByEdmIndex) management
 * - EDM index caching in EntityStorage
 * - Batch processing using EDM indices
 * - State transition cleanup of AI-specific data
 *
 * NOTE: Handle generation, slot reuse, and tier management are tested
 * in EntityDataManagerTests.cpp - these tests focus on AI Manager's
 * specific use of EDM data.
 */

#define BOOST_TEST_MODULE AIManagerEDMIntegrationTests
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "ai/AICommandBus.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "collisions/CollisionBody.hpp"
#include "events/EntityEvents.hpp"
#include "events/StanceChangedEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for AIManager EDM tests");
        }
    }
    ~ThreadSystemFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class AITestNPC {
public:
    explicit AITestNPC(const Vector2D& pos = Vector2D(0, 0)) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
        m_initialPosition = pos;
    }

    static std::shared_ptr<AITestNPC> create(const Vector2D& pos = Vector2D(0, 0)) {
        return std::make_shared<AITestNPC>(pos);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

    // Check if position changed in EDM (AIManager writes directly to EDM)
    [[nodiscard]] bool hasPositionChanged() const {
        if (!m_handle.isValid()) return false;

        auto& edm = EntityDataManager::Instance();
        size_t index = edm.getIndex(m_handle);
        if (index == SIZE_MAX) return false;

        auto& transform = edm.getTransformByIndex(index);
        return (transform.position - m_initialPosition).length() > 0.01f ||
            transform.velocity.length() > 0.01f;
    }

    void resetInitialPosition() {
        if (m_handle.isValid()) {
            auto& edm = EntityDataManager::Instance();
            size_t index = edm.getIndex(m_handle);
            if (index != SIZE_MAX) {
                m_initialPosition = edm.getTransformByIndex(index).position;
            }
        }
    }

private:
    EntityHandle m_handle;
    Vector2D m_initialPosition;
};

// Test fixture that initializes all required managers
struct AIManagerEDMFixture {
    AIManagerEDMFixture() {
        BOOST_REQUIRE(GameTimeManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(AIManager::Instance().init());
        BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());
    }

    ~AIManagerEDMFixture() {
        BackgroundSimulationManager::Instance().clean();
        AIManager::Instance().clean();
        EventManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
    }
};

// ============================================================================
// SPARSE BEHAVIOR VECTOR TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SparseBehaviorVectorTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestBehaviorAssignmentCreatesEdmIndexMapping) {
    // Create entity and get its EDM index
    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    EntityHandle handle = entity->getHandle();
    BOOST_REQUIRE(handle.isValid());

    size_t edmIndex = EntityDataManager::Instance().getIndex(handle);
    BOOST_REQUIRE(edmIndex != SIZE_MAX);

    // Assign behavior using data-oriented API
    AIManager::Instance().assignBehavior(handle, "Idle");

    // Verify behavior is assigned
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
}

BOOST_AUTO_TEST_CASE(TestRangedAttackCommitFailureQueuesBehaviorOwnedReset) {
    auto& edm = EntityDataManager::Instance();
    auto& rtm = ResourceTemplateManager::Instance();
    auto& aiMgr = AIManager::Instance();
    VoidLight::AICommandBus::Instance().clearAll();

    auto attacker = AITestNPC::create(Vector2D(100.0f, 100.0f));
    const EntityHandle attackerHandle = attacker->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    aiMgr.unassignBehavior(attackerHandle);
    aiMgr.assignBehavior(attackerHandle, "Attack");
    BOOST_REQUIRE(aiMgr.hasBehavior(attackerHandle));
    const auto ref = edm.getBehaviorConfigRef(attackerIdx);
    BOOST_REQUIRE(ref.type == BehaviorType::Attack);

    const auto bow = rtm.getHandleById("bow");
    BOOST_REQUIRE(bow.isValid());
    auto& charData = edm.getCharacterDataByIndex(attackerIdx);
    BOOST_REQUIRE(charData.hasInventory());
    BOOST_REQUIRE(edm.addToInventory(charData.inventoryIndex, bow, 1));
    BOOST_REQUIRE(edm.equipCharacterItem(attackerHandle, bow));

    auto& attackState = edm.getAttackState(ref.index);
    attackState.currentState = 4;
    attackState.stateChangeTimer = 0.0f;
    attackState.canAttack = false;
    attackState.lastAttackHit = true;

    VoidLight::AICommandBus::Instance().enqueueRangedAttack(
        attackerHandle, attackerIdx, Vector2D(100.0f, 100.0f),
        Vector2D(140.0f, 100.0f), 10.0f, 160.0f, 180.0f);

    aiMgr.update(0.0f);
    BOOST_CHECK_EQUAL(static_cast<int>(attackState.currentState), 4);

    aiMgr.update(0.0f);
    BOOST_CHECK_EQUAL(static_cast<int>(attackState.currentState), 0);
    BOOST_CHECK(attackState.canAttack);
    BOOST_CHECK(!attackState.lastAttackHit);
}

BOOST_AUTO_TEST_CASE(TestSparseBehaviorVectorHandlesGaps) {
    // Create entities at different positions (will get different EDM indices)
    // Note: NPCs created via createNPCWithRaceClass auto-register with their
    // class's suggestedBehavior (e.g., "Guard"), so we must unassign first
    std::vector<std::shared_ptr<AITestNPC>> entities;
    std::vector<EntityHandle> handles;

    for (int i = 0; i < 10; ++i) {
        auto entity = AITestNPC::create(Vector2D(i * 100.0f, 0.0f));
        entities.push_back(entity);
        handles.push_back(entity->getHandle());
        // Unassign auto-registered behavior to start with clean slate
        AIManager::Instance().unassignBehavior(entity->getHandle());
    }

    // Assign behaviors to only odd-indexed entities (creates gaps)
    for (size_t i = 1; i < handles.size(); i += 2) {
        AIManager::Instance().assignBehavior(handles[i], "Wander");
    }

    // Verify correct entities have behaviors
    for (size_t i = 0; i < handles.size(); ++i) {
        bool shouldHaveBehavior = (i % 2 == 1);
        BOOST_CHECK_EQUAL(AIManager::Instance().hasBehavior(handles[i]), shouldHaveBehavior);
    }
}

BOOST_AUTO_TEST_CASE(TestBehaviorUnassignmentClearsSparseBehavior) {
    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    EntityHandle handle = entity->getHandle();

    // Assign then unassign
    AIManager::Instance().assignBehavior(handle, "Chase");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    AIManager::Instance().unassignBehavior(handle);
    BOOST_CHECK(!AIManager::Instance().hasBehavior(handle));
}

BOOST_AUTO_TEST_CASE(TestBehaviorReassignmentUpdatesSparseBehavior) {
    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    EntityHandle handle = entity->getHandle();

    // Assign, unassign, then reassign
    AIManager::Instance().assignBehavior(handle, "Patrol");
    AIManager::Instance().unassignBehavior(handle);
    AIManager::Instance().assignBehavior(handle, "Guard");

    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// COMBAT EVENT ROUTING TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CombatEventRoutingTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestEventManagerCombatHandlerMutatesPlayerHealth) {
    auto& edm = EntityDataManager::Instance();
    EntityHandle playerHandle = edm.registerPlayer(9001, Vector2D(100.0f, 100.0f));
    BOOST_REQUIRE(playerHandle.isValid());

    auto& playerData = edm.getCharacterData(playerHandle);
    playerData.maxHealth = 100.0f;
    playerData.health = 100.0f;
    playerData.mass = 1.0f;

    auto damageEvent = std::make_shared<DamageEvent>(
        EntityEventType::DamageIntent,
        EntityHandle{},
        playerHandle,
        25.0f,
        Vector2D(5.0f, 0.0f));

    EventData data;
    data.typeId = EventTypeId::Combat;
    data.setActive(true);
    data.event = damageEvent;

    EventManager::Instance().dispatchEvent(damageEvent, EventManager::DispatchMode::Immediate);

    BOOST_CHECK_CLOSE(edm.getCharacterData(playerHandle).health, 75.0f, 0.01f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// BATCH PROCESSING WITH EDM INDICES TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(BatchProcessingEDMTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestBatchProcessingWritesToEDMTransform) {
    // Create entity and assign behavior
    auto entity = AITestNPC::create(Vector2D(500.0f, 500.0f));
    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Wander");

    // Get EDM index
    auto& edm = EntityDataManager::Instance();
    size_t index = edm.getIndex(handle);
    BOOST_REQUIRE(index != SIZE_MAX);

    // Verify entity is registered with EDM and has behavior
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Verify the entity's transform is accessible via EDM
    auto& transform = edm.getTransformByIndex(index);
    BOOST_CHECK_CLOSE(transform.position.getX(), 500.0f, 0.01f);
    BOOST_CHECK_CLOSE(transform.position.getY(), 500.0f, 0.01f);

    // Note: Actual batch processing depends on tier updates and threading
    // which is tested more thoroughly in AIScalingBenchmark and ThreadSafeAIManagerTests
}

BOOST_AUTO_TEST_CASE(TestMultipleEntitiesProcessedViaBatch) {
    const size_t ENTITY_COUNT = 50;
    std::vector<std::shared_ptr<AITestNPC>> entities;
    std::vector<EntityHandle> handles;

    // Create and assign behaviors to many entities
    for (size_t i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = AITestNPC::create(Vector2D(100.0f + i * 50.0f, 100.0f));
        // Alternate between different behavior types to test variety
        const char* behaviorName = (i % 3 == 0) ? "Wander" : (i % 3 == 1) ? "Idle"
                                                                          : "Patrol";
        AIManager::Instance().assignBehavior(entity->getHandle(), behaviorName);
        entities.push_back(entity);
        handles.push_back(entity->getHandle());
    }

    // Verify all entities are registered with behaviors
    auto& edm = EntityDataManager::Instance();
    size_t registeredCount = 0;
    for (const auto& handle : handles) {
        if (AIManager::Instance().hasBehavior(handle)) {
            size_t index = edm.getIndex(handle);
            if (index != SIZE_MAX) {
                registeredCount++;
            }
        }
    }

    // All entities should be registered with behaviors and have EDM indices
    BOOST_CHECK_EQUAL(registeredCount, ENTITY_COUNT);

    // Note: Actual batch processing execution is tested in AIScalingBenchmark
    // and ThreadSafeAIManagerTests which properly set up threading and tiers
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(StateTransitionTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionClearsAIData) {
    // Create entities with behaviors
    std::vector<std::shared_ptr<AITestNPC>> entities;
    for (int i = 0; i < 10; ++i) {
        auto entity = AITestNPC::create(Vector2D(i * 100.0f, 0.0f));
        AIManager::Instance().assignBehavior(entity->getHandle(), "Idle");
        entities.push_back(entity);
    }

    // Verify behaviors exist
    for (const auto& entity : entities) {
        BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));
    }

    // Trigger state transition
    AIManager::Instance().prepareForStateTransition();

    // Verify all AI data is cleared (behaviors should no longer exist)
    for (const auto& entity : entities) {
        BOOST_CHECK(!AIManager::Instance().hasBehavior(entity->getHandle()));
    }
}

BOOST_AUTO_TEST_CASE(TestStateTransitionWhileBatchProcessing) {
    // Create many entities to ensure batch processing is used
    std::vector<std::shared_ptr<AITestNPC>> entities;
    for (int i = 0; i < 100; ++i) {
        auto entity = AITestNPC::create(Vector2D(i * 50.0f, 100.0f));
        AIManager::Instance().assignBehavior(entity->getHandle(), "Wander");
        entities.push_back(entity);
    }

    // Set world bounds and trigger tier update
    CollisionManager::Instance().setWorldBounds(0, 0, 10000.0f, 10000.0f);
    BackgroundSimulationManager::Instance().update(Vector2D(500.0f, 500.0f), 0.016f);

    // Start an update (may trigger batch processing)
    AIManager::Instance().update(0.016f);

    // Immediately request state transition
    AIManager::Instance().prepareForStateTransition();

    // Should not crash and all data should be cleared
    for (const auto& entity : entities) {
        BOOST_CHECK(!AIManager::Instance().hasBehavior(entity->getHandle()));
    }
}

BOOST_AUTO_TEST_CASE(TestAIManagerReinitAfterStateTransition) {
    // Create entity and assign behavior
    auto entity1 = AITestNPC::create(Vector2D(100.0f, 100.0f));
    AIManager::Instance().assignBehavior(entity1->getHandle(), "Chase");
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity1->getHandle()));

    // State transition clears everything
    AIManager::Instance().prepareForStateTransition();
    EntityDataManager::Instance().prepareForStateTransition();

    // Create new entity after transition
    auto entity2 = AITestNPC::create(Vector2D(200.0f, 200.0f));
    AIManager::Instance().assignBehavior(entity2->getHandle(), "Flee");

    // New entity should have behavior
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity2->getHandle()));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// EDM INDEX CACHING TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EDMIndexCachingTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestEdmIndexCachedOnBehaviorAssignment) {
    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    EntityHandle handle = entity->getHandle();

    size_t expectedIndex = EntityDataManager::Instance().getIndex(handle);
    BOOST_REQUIRE(expectedIndex != SIZE_MAX);

    // Assign behavior
    AIManager::Instance().assignBehavior(handle, "Guard");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // The index should be cached internally (verified indirectly via successful batch processing)
}

BOOST_AUTO_TEST_CASE(TestEntityDestructionDoesNotAffectOtherEntities) {
    // Create multiple entities
    auto entity1 = AITestNPC::create(Vector2D(100.0f, 100.0f));
    auto entity2 = AITestNPC::create(Vector2D(200.0f, 200.0f));
    auto entity3 = AITestNPC::create(Vector2D(300.0f, 300.0f));

    EntityHandle handle1 = entity1->getHandle();
    EntityHandle handle2 = entity2->getHandle();
    EntityHandle handle3 = entity3->getHandle();

    // Assign behaviors to all
    AIManager::Instance().assignBehavior(handle1, "Follow");
    AIManager::Instance().assignBehavior(handle2, "Attack");
    AIManager::Instance().assignBehavior(handle3, "Patrol");

    // Unassign middle entity's behavior
    AIManager::Instance().unassignBehavior(handle2);

    // Other entities should still have behaviors
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle1));
    BOOST_CHECK(!AIManager::Instance().hasBehavior(handle2));
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle3));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// BEHAVIOR DATA-DRIVEN TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(BehaviorDataDrivenTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestMultipleEntitiesShareBehaviorType) {
    auto entity1 = AITestNPC::create(Vector2D(100.0f, 100.0f));
    auto entity2 = AITestNPC::create(Vector2D(200.0f, 200.0f));

    AIManager::Instance().assignBehavior(entity1->getHandle(), "Wander");
    AIManager::Instance().assignBehavior(entity2->getHandle(), "Wander");

    // Both should have behaviors (data-driven, not separate instances)
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity1->getHandle()));
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity2->getHandle()));

    // Unassigning one should not affect the other
    AIManager::Instance().unassignBehavior(entity1->getHandle());
    BOOST_CHECK(!AIManager::Instance().hasBehavior(entity1->getHandle()));
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity2->getHandle()));
}

BOOST_AUTO_TEST_CASE(TestAllBehaviorTypesCanBeAssigned) {
    // Test all available behavior types
    const char* behaviorTypes[] = {"Idle", "Wander", "Chase", "Patrol", "Guard", "Attack", "Flee", "Follow", "Forage"};

    std::vector<std::shared_ptr<AITestNPC>> entities;
    std::vector<EntityHandle> handles;

    for (const char* behaviorName : behaviorTypes) {
        auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
        handles.push_back(entity->getHandle());
        entities.push_back(entity);

        // Assign behavior
        AIManager::Instance().assignBehavior(entity->getHandle(), behaviorName);
        BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));
    }

    // All entities should have their behaviors
    for (const auto& handle : handles) {
        BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
    }
}

BOOST_AUTO_TEST_CASE(TestBehaviorSwitching) {
    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    EntityHandle handle = entity->getHandle();

    // Assign initial behavior
    AIManager::Instance().assignBehavior(handle, "Idle");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Switch to different behavior (unassign + reassign)
    AIManager::Instance().unassignBehavior(handle);
    AIManager::Instance().assignBehavior(handle, "Chase");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Switch again
    AIManager::Instance().unassignBehavior(handle);
    AIManager::Instance().assignBehavior(handle, "Flee");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GUARD AND FACTION INDEX TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(GuardFactionIndexTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(GuardIndexPopulatedOnAssignment) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto guard = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = guard->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Unassign auto-assigned behavior, verify guard index is empty for this entity
    aiMgr.unassignBehavior(handle);
    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());

    // Assign Guard — should appear in guard index
    aiMgr.assignBehavior(handle, "Guard");
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) != results.end());
}

BOOST_AUTO_TEST_CASE(GuardIndexNotPopulatedForNonGuards) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto npc = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = npc->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Assign non-Guard behavior
    aiMgr.unassignBehavior(handle);
    aiMgr.assignBehavior(handle, "Idle");

    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());
}

BOOST_AUTO_TEST_CASE(GuardIndexRemovedOnUnassign) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto guard = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = guard->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    aiMgr.unassignBehavior(handle);
    aiMgr.assignBehavior(handle, "Guard");

    // Verify present
    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(std::find(results.begin(), results.end(), idx) != results.end());

    // Unassign — should be removed
    aiMgr.unassignBehavior(handle);
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());
}

BOOST_AUTO_TEST_CASE(GuardIndexRemovedOnUnregister) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto guard = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = guard->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Auto-assigned Guard from createNPCWithRaceClass — verify present
    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(std::find(results.begin(), results.end(), idx) != results.end());

    // Unregister — should be removed
    aiMgr.unregisterEntity(handle);
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());
}

BOOST_AUTO_TEST_CASE(GuardRadiusFilterExcludesDistantGuards) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto near = AITestNPC::create(Vector2D(300.0f, 300.0f));
    auto far = AITestNPC::create(Vector2D(2000.0f, 2000.0f));

    // Both auto-assigned Guard from "Human"/"Guard" class
    size_t nearIdx = edm.getIndex(near->getHandle());
    size_t farIdx = edm.getIndex(far->getHandle());
    BOOST_REQUIRE(nearIdx != SIZE_MAX);
    BOOST_REQUIRE(farIdx != SIZE_MAX);

    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 500.0f, results, false);

    BOOST_CHECK(std::find(results.begin(), results.end(), nearIdx) != results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), farIdx) == results.end());
}

BOOST_AUTO_TEST_CASE(FactionIndexPopulatedOnAssignment) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto npc = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = npc->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Read the faction assigned by createNPCWithRaceClass
    uint8_t faction = edm.getCharacterDataByIndex(idx).faction;

    // Should be in faction index after auto-assignment
    std::vector<size_t> results;
    aiMgr.scanAlliedInRadius(faction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) != results.end());

    // Should NOT be in a different faction's Allied scan (default Neutral off-diagonal)
    uint8_t otherFaction = (faction == 0) ? 1 : 0;
    results.clear();
    aiMgr.scanAlliedInRadius(otherFaction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());
}

BOOST_AUTO_TEST_CASE(FactionIndexRemovedOnUnassign) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto npc = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = npc->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    uint8_t faction = edm.getCharacterDataByIndex(idx).faction;

    // Verify present
    std::vector<size_t> results;
    aiMgr.scanAlliedInRadius(faction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(std::find(results.begin(), results.end(), idx) != results.end());

    // Unassign — should be removed from faction index
    aiMgr.unassignBehavior(handle);
    results.clear();
    aiMgr.scanAlliedInRadius(faction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());
}

BOOST_AUTO_TEST_CASE(FactionRadiusFilterExcludesDistantEntities) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto near = AITestNPC::create(Vector2D(300.0f, 300.0f));
    auto far = AITestNPC::create(Vector2D(2000.0f, 2000.0f));

    size_t nearIdx = edm.getIndex(near->getHandle());
    size_t farIdx = edm.getIndex(far->getHandle());
    BOOST_REQUIRE(nearIdx != SIZE_MAX);
    BOOST_REQUIRE(farIdx != SIZE_MAX);

    // Both should have same faction (same race/class)
    uint8_t faction = edm.getCharacterDataByIndex(nearIdx).faction;
    BOOST_REQUIRE(edm.getCharacterDataByIndex(farIdx).faction == faction);

    std::vector<size_t> results;
    aiMgr.scanAlliedInRadius(faction, Vector2D(300.0f, 300.0f), 500.0f, results, false);

    BOOST_CHECK(std::find(results.begin(), results.end(), nearIdx) != results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), farIdx) == results.end());
}

BOOST_AUTO_TEST_CASE(ScanHostileInRadiusReturnsOnlyHostileFactionMembers) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto self = AITestNPC::create(Vector2D(300.0f, 300.0f));
    auto hostile = AITestNPC::create(Vector2D(340.0f, 300.0f));
    auto farHostile = AITestNPC::create(Vector2D(2000.0f, 2000.0f));
    auto neutral = AITestNPC::create(Vector2D(360.0f, 300.0f));
    auto allied = AITestNPC::create(Vector2D(380.0f, 300.0f));
    const size_t selfIdx = edm.getIndex(self->getHandle());
    const size_t hostileIdx = edm.getIndex(hostile->getHandle());
    const size_t farHostileIdx = edm.getIndex(farHostile->getHandle());
    const size_t neutralIdx = edm.getIndex(neutral->getHandle());
    const size_t alliedIdx = edm.getIndex(allied->getHandle());
    BOOST_REQUIRE(selfIdx != SIZE_MAX);
    BOOST_REQUIRE(hostileIdx != SIZE_MAX);
    BOOST_REQUIRE(farHostileIdx != SIZE_MAX);
    BOOST_REQUIRE(neutralIdx != SIZE_MAX);
    BOOST_REQUIRE(alliedIdx != SIZE_MAX);

    edm.setFaction(self->getHandle(), 3);
    edm.setFaction(hostile->getHandle(), 4);
    edm.setFaction(farHostile->getHandle(), 4);
    edm.setFaction(neutral->getHandle(), 5);
    edm.setFaction(allied->getHandle(), 6);
    aiMgr.update(0.016f); // Commit queued faction changes into the faction index

    const Vector2D center(300.0f, 300.0f);
    std::vector<size_t> results;

    // No Hostile cell in the row: early return, empty result.
    BOOST_REQUIRE(!aiMgr.factionRowHasHostile(3));
    results.push_back(selfIdx); // Stale content must be cleared
    aiMgr.scanHostileInRadius(3, center, 1000.0f, results);
    BOOST_CHECK(results.empty());

    aiMgr.setStance(3, 4, FactionStance::Hostile);
    aiMgr.setStance(3, 6, FactionStance::Allied);
    aiMgr.scanHostileInRadius(3, center, 500.0f, results);
    BOOST_CHECK_EQUAL(results.size(), 1u);
    BOOST_CHECK(std::find(results.begin(), results.end(), hostileIdx) != results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), farHostileIdx) == results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), neutralIdx) == results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), alliedIdx) == results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), selfIdx) == results.end());

    // Directed: faction 4's row is still Neutral toward 3.
    aiMgr.scanHostileInRadius(4, center, 1000.0f, results);
    BOOST_CHECK(results.empty());

    // Out-of-range faction is a no-op.
    aiMgr.scanHostileInRadius(AIManager::MAX_FACTIONS, center, 1000.0f, results);
    BOOST_CHECK(results.empty());
}

BOOST_AUTO_TEST_CASE(IndicesClearedOnStateTransition) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto guard = AITestNPC::create(Vector2D(300.0f, 300.0f));
    size_t idx = edm.getIndex(guard->getHandle());
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Verify guard is in indices
    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(!results.empty());

    // State transition should clear all indices
    aiMgr.prepareForStateTransition();
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(results.empty());
}

BOOST_AUTO_TEST_CASE(BehaviorReassignmentUpdatesGuardIndex) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto npc = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle handle = npc->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Auto-assigned Guard — should be in guard index
    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(std::find(results.begin(), results.end(), idx) != results.end());

    // Reassign to Idle — should be removed from guard index
    aiMgr.assignBehavior(handle, "Idle");
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());

    // Reassign back to Guard — should be back in guard index
    aiMgr.assignBehavior(handle, "Guard");
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) != results.end());
}

BOOST_AUTO_TEST_CASE(RuntimeSwitchBehaviorUpdatesGuardQueries) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto npc = AITestNPC::create(Vector2D(300.0f, 300.0f));
    size_t idx = edm.getIndex(npc->getHandle());
    BOOST_REQUIRE(idx != SIZE_MAX);

    std::vector<size_t> results;
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(std::find(results.begin(), results.end(), idx) != results.end());

    // Simulate runtime transition from behavior execution path (bypasses AIManager assignment APIs)
    Behaviors::switchBehavior(idx, BehaviorType::Attack);
    aiMgr.update(0.016f); // Commit queued transition
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());

    Behaviors::switchBehavior(idx, BehaviorType::Guard);
    aiMgr.update(0.016f); // Commit queued transition
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) != results.end());
}

BOOST_AUTO_TEST_CASE(StaleHigherSequenceTransitionDoesNotSuppressValidTransition) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto original = AITestNPC::create(Vector2D(300.0f, 300.0f));
    EntityHandle staleHandle = original->getHandle();
    const size_t reusedIndex = edm.getIndex(staleHandle);
    BOOST_REQUIRE(reusedIndex != SIZE_MAX);

    aiMgr.assignBehavior(staleHandle, "Idle");
    BOOST_REQUIRE(edm.getBehaviorConfigRef(reusedIndex).type == BehaviorType::Idle);

    aiMgr.unregisterEntity(staleHandle);
    edm.destroyEntity(staleHandle);
    edm.processDestructionQueue();
    BOOST_REQUIRE(edm.getIndex(staleHandle) == SIZE_MAX);

    std::vector<std::shared_ptr<AITestNPC>> keepAlive;
    keepAlive.reserve(8);

    EntityHandle currentHandle{};
    bool slotReused = false;
    for (int i = 0; i < 8; ++i) {
        auto spawned = AITestNPC::create(Vector2D(400.0f + i * 10.0f, 300.0f));
        const size_t idx = edm.getIndex(spawned->getHandle());
        keepAlive.push_back(spawned);
        if (idx == reusedIndex) {
            currentHandle = spawned->getHandle();
            slotReused = true;
            break;
        }
    }
    BOOST_REQUIRE(slotReused);
    BOOST_REQUIRE(currentHandle.isValid());
    BOOST_REQUIRE(edm.getIndex(currentHandle) == reusedIndex);

    aiMgr.assignBehavior(currentHandle, "Idle");
    BOOST_REQUIRE(edm.getBehaviorConfigRef(reusedIndex).type == BehaviorType::Idle);

    // Valid transition first (older sequence): should still apply even if a stale
    // command with newer sequence is enqueued after it.
    Behaviors::switchBehavior(reusedIndex, BehaviorType::Attack);

    VoidLight::BehaviorConfigData staleConfig{};
    staleConfig.type = BehaviorType::Flee;
    VoidLight::AICommandBus::Instance().enqueueBehaviorTransition(
        staleHandle, reusedIndex, staleConfig);

    aiMgr.update(0.016f);
    BOOST_CHECK(edm.getBehaviorConfigRef(reusedIndex).type == BehaviorType::Attack);
}

BOOST_AUTO_TEST_CASE(StaleTransitionSuppressionStressLoop) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    for (int iter = 0; iter < 8; ++iter) {
        auto original = AITestNPC::create(Vector2D(300.0f + iter * 5.0f, 300.0f));
        EntityHandle staleHandle = original->getHandle();
        const size_t reusedIndex = edm.getIndex(staleHandle);
        BOOST_REQUIRE(reusedIndex != SIZE_MAX);

        aiMgr.assignBehavior(staleHandle, "Idle");
        BOOST_REQUIRE(edm.getBehaviorConfigRef(reusedIndex).type == BehaviorType::Idle);

        aiMgr.unregisterEntity(staleHandle);
        edm.destroyEntity(staleHandle);
        edm.processDestructionQueue();
        BOOST_REQUIRE(edm.getIndex(staleHandle) == SIZE_MAX);

        std::vector<std::shared_ptr<AITestNPC>> keepAlive;
        keepAlive.reserve(16);
        EntityHandle currentHandle{};
        bool slotReused = false;
        for (int i = 0; i < 16; ++i) {
            auto spawned = AITestNPC::create(Vector2D(420.0f + i * 10.0f, 320.0f + iter * 5.0f));
            keepAlive.push_back(spawned);
            if (edm.getIndex(spawned->getHandle()) == reusedIndex) {
                currentHandle = spawned->getHandle();
                slotReused = true;
                break;
            }
        }
        BOOST_REQUIRE(slotReused);

        aiMgr.assignBehavior(currentHandle, "Idle");
        BOOST_REQUIRE(edm.getBehaviorConfigRef(reusedIndex).type == BehaviorType::Idle);

        Behaviors::switchBehavior(reusedIndex, BehaviorType::Attack);

        VoidLight::BehaviorConfigData staleConfig{};
        staleConfig.type = BehaviorType::Flee;
        VoidLight::AICommandBus::Instance().enqueueBehaviorTransition(
            staleHandle, reusedIndex, staleConfig);

        aiMgr.update(0.016f);
        BOOST_CHECK(edm.getBehaviorConfigRef(reusedIndex).type == BehaviorType::Attack);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FACTION STANCE TABLE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(FactionStanceTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestFactionStanceDefaultsSameAlliedOthersNeutral) {
    auto& aiMgr = AIManager::Instance();

    for (uint8_t from = 0; from < AIManager::MAX_FACTIONS; ++from) {
        BOOST_CHECK(!aiMgr.factionRowHasHostile(from));
        for (uint8_t toward = 0; toward < AIManager::MAX_FACTIONS; ++toward) {
            if (from == toward) {
                BOOST_CHECK(aiMgr.getStance(from, toward) == FactionStance::Allied);
                BOOST_CHECK(aiMgr.isAlliedTo(from, toward));
                BOOST_CHECK(!aiMgr.isHostileTo(from, toward));
            } else {
                BOOST_CHECK(aiMgr.getStance(from, toward) == FactionStance::Neutral);
                BOOST_CHECK(!aiMgr.isAlliedTo(from, toward));
                BOOST_CHECK(!aiMgr.isHostileTo(from, toward));
            }
        }
    }
    BOOST_CHECK(!aiMgr.factionRowHasHostile(AIManager::MAX_FACTIONS));
}

BOOST_AUTO_TEST_CASE(TestFactionStanceSetGetAndBounds) {
    auto& aiMgr = AIManager::Instance();

    aiMgr.setStance(0, 1, FactionStance::Hostile);
    BOOST_CHECK(aiMgr.getStance(0, 1) == FactionStance::Hostile);
    BOOST_CHECK(aiMgr.getStance(1, 0) == FactionStance::Neutral);
    BOOST_CHECK(aiMgr.isHostileTo(0, 1));
    BOOST_CHECK(!aiMgr.isHostileTo(1, 0));
    BOOST_CHECK(aiMgr.factionRowHasHostile(0));
    BOOST_CHECK(!aiMgr.factionRowHasHostile(1));

    aiMgr.setStance(0, 1, FactionStance::Neutral);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(0));

    aiMgr.setStance(0, 1, FactionStance::Hostile);
    aiMgr.setStance(0, 2, FactionStance::Hostile);
    BOOST_CHECK(aiMgr.factionRowHasHostile(0));
    aiMgr.setStance(0, 1, FactionStance::Neutral);
    BOOST_CHECK(aiMgr.factionRowHasHostile(0));
    aiMgr.setStance(0, 2, FactionStance::Allied);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(0));

    aiMgr.setStance(0, 0, FactionStance::Hostile);
    BOOST_CHECK(aiMgr.getStance(0, 0) == FactionStance::Allied);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(0));

    BOOST_CHECK(aiMgr.getStance(AIManager::MAX_FACTIONS, 0) == FactionStance::Neutral);
    BOOST_CHECK(aiMgr.getStance(0, AIManager::MAX_FACTIONS) == FactionStance::Neutral);
    BOOST_CHECK(!aiMgr.isHostileTo(AIManager::MAX_FACTIONS, 1));
    BOOST_CHECK(!aiMgr.isAlliedTo(AIManager::MAX_FACTIONS, 1));
    BOOST_CHECK(!aiMgr.factionRowHasHostile(AIManager::MAX_FACTIONS));

    aiMgr.setStance(AIManager::MAX_FACTIONS, 1, FactionStance::Hostile);
    BOOST_CHECK(aiMgr.getStance(AIManager::MAX_FACTIONS, 1) == FactionStance::Neutral);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(AIManager::MAX_FACTIONS));

    aiMgr.setStance(2, 3, FactionStance::Allied);
    aiMgr.worsenStance(2, 3);
    BOOST_CHECK(aiMgr.getStance(2, 3) == FactionStance::Neutral);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(2));
    aiMgr.worsenStance(2, 3);
    BOOST_CHECK(aiMgr.getStance(2, 3) == FactionStance::Hostile);
    BOOST_CHECK(aiMgr.factionRowHasHostile(2));
    aiMgr.worsenStance(2, 3);
    BOOST_CHECK(aiMgr.getStance(2, 3) == FactionStance::Hostile);
    aiMgr.setStance(2, 3, FactionStance::Neutral);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(2));
    aiMgr.worsenStance(2, 2);
    BOOST_CHECK(aiMgr.getStance(2, 2) == FactionStance::Allied);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(2));
}

BOOST_AUTO_TEST_CASE(TestFactionStanceResetsOnPrepareForStateTransition) {
    auto& aiMgr = AIManager::Instance();
    aiMgr.setStance(0, 1, FactionStance::Hostile);
    BOOST_REQUIRE(aiMgr.isHostileTo(0, 1));

    aiMgr.prepareForStateTransition();

    BOOST_CHECK(aiMgr.getStance(0, 1) == FactionStance::Neutral);
    BOOST_CHECK(aiMgr.getStance(0, 0) == FactionStance::Allied);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(0));
}

BOOST_AUTO_TEST_CASE(TestFactionStanceResetsOnResetBehaviors) {
    auto& aiMgr = AIManager::Instance();
    aiMgr.setStance(4, 5, FactionStance::Hostile);
    BOOST_REQUIRE(aiMgr.isHostileTo(4, 5));

    aiMgr.resetBehaviors();

    BOOST_CHECK(aiMgr.getStance(4, 5) == FactionStance::Neutral);
    BOOST_CHECK(aiMgr.getStance(4, 4) == FactionStance::Allied);
    BOOST_CHECK(!aiMgr.factionRowHasHostile(4));
}

BOOST_AUTO_TEST_CASE(TestSetStanceNoOpDoesNotEmitAndRealChangeDoes) {
    auto& aiMgr = AIManager::Instance();
    auto& eventMgr = EventManager::Instance();

    int stanceEvents = 0;
    uint8_t fromFaction = 255;
    uint8_t towardFaction = 255;
    FactionStance oldStance = FactionStance::Allied;
    FactionStance newStance = FactionStance::Allied;
    uint32_t settlementId = 99;
    bool towardPlayer = true;
    eventMgr.registerHandler(EventTypeId::StanceChanged,
        [&](const EventData& data) {
            const auto* event =
                dynamic_cast<const StanceChangedEvent*>(data.event.get());
            if (!event) {
                return;
            }
            ++stanceEvents;
            fromFaction = event->getFromFaction();
            towardFaction = event->getTowardFaction();
            oldStance = event->getOldStance();
            newStance = event->getNewStance();
            settlementId = event->getSettlementId();
            towardPlayer = event->isTowardPlayer();
        });

    aiMgr.setStance(0, 1, FactionStance::Neutral);
    BOOST_CHECK_EQUAL(stanceEvents, 0);

    aiMgr.setStance(0, 1, FactionStance::Hostile);
    BOOST_CHECK_EQUAL(stanceEvents, 1);
    BOOST_CHECK_EQUAL(fromFaction, 0);
    BOOST_CHECK_EQUAL(towardFaction, 1);
    BOOST_CHECK(oldStance == FactionStance::Neutral);
    BOOST_CHECK(newStance == FactionStance::Hostile);
    BOOST_CHECK_EQUAL(settlementId, 0u);
    BOOST_CHECK(!towardPlayer);

    aiMgr.setStance(0, 1, FactionStance::Hostile);
    BOOST_CHECK_EQUAL(stanceEvents, 1);

    aiMgr.setStance(0, 0, FactionStance::Hostile);
    aiMgr.setStance(AIManager::MAX_FACTIONS, 1, FactionStance::Hostile);
    aiMgr.worsenStance(2, 2);
    BOOST_CHECK_EQUAL(stanceEvents, 1);

    aiMgr.resetFactionStances();
    BOOST_CHECK_EQUAL(stanceEvents, 1);
}

BOOST_AUTO_TEST_CASE(TestCollisionFollowsPlayerRelation) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    EntityHandle player = edm.registerPlayer(9101, Vector2D(0.0f, 0.0f));
    BOOST_REQUIRE(player.isValid());
    aiMgr.setPlayerHandle(player);

    EntityHandle warrior = edm.createNPCWithRaceClass(
        Vector2D(100.0f, 100.0f), "Human", "Warrior");
    BOOST_REQUIRE(warrior.isValid());
    const size_t warriorIdx = edm.getIndex(warrior);
    BOOST_REQUIRE(warriorIdx != SIZE_MAX);
    BOOST_CHECK_EQUAL(edm.getCharacterDataByIndex(warriorIdx).faction, 1);

    const auto& hot = edm.getHotDataByIndex(warriorIdx);
    BOOST_CHECK_NE(hot.collisionLayers, CollisionLayer::Layer_Enemy);

    // The stance table is NPC-faction only: it never drives player collision.
    aiMgr.setStance(1, 0, FactionStance::Hostile);
    BOOST_CHECK_NE(hot.collisionLayers, CollisionLayer::Layer_Enemy);
    aiMgr.resetFactionStances();

    // Standing crossing the Hostile threshold groups the faction as Enemy.
    aiMgr.adjustPlayerStanding(player, 1, AIManager::PLAYER_STANDING_HOSTILE_AT);
    BOOST_REQUIRE(aiMgr.getPlayerRelation(1) == FactionStance::Hostile);
    BOOST_CHECK_EQUAL(hot.collisionLayers, CollisionLayer::Layer_Enemy);

    // A gift de-escalates below Hostile and clears it.
    aiMgr.recordPlayerIncident(AIManager::PlayerIncident::Gift, player, warrior);
    BOOST_CHECK(aiMgr.getPlayerRelation(1) == FactionStance::Neutral);
    BOOST_CHECK_NE(hot.collisionLayers, CollisionLayer::Layer_Enemy);

    // setPlayerHandle resyncs every faction toward the new player.
    aiMgr.adjustPlayerStanding(player, 1, AIManager::PLAYER_STANDING_MIN);
    BOOST_REQUIRE_EQUAL(hot.collisionLayers, CollisionLayer::Layer_Enemy);
    EntityHandle otherPlayer = edm.registerPlayer(9102, Vector2D(0.0f, 0.0f));
    BOOST_REQUIRE(otherPlayer.isValid());
    aiMgr.setPlayerHandle(otherPlayer);
    BOOST_CHECK_NE(hot.collisionLayers, CollisionLayer::Layer_Enemy);
    aiMgr.setPlayerHandle(player);
    BOOST_CHECK_EQUAL(hot.collisionLayers, CollisionLayer::Layer_Enemy);
}

BOOST_AUTO_TEST_CASE(TestPlayerFactionStandingSidecarLifetime) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    EntityHandle player = edm.registerPlayer(9001, Vector2D(100.0f, 100.0f));
    BOOST_REQUIRE(player.isValid());
    const size_t playerIdx = edm.getIndex(player);
    BOOST_REQUIRE(playerIdx != SIZE_MAX);

    aiMgr.adjustPlayerStanding(player, 1, AIManager::PLAYER_STANDING_THEFT_DELTA);
    BOOST_CHECK_EQUAL(edm.getPlayerFactionStanding(playerIdx, 1),
        AIManager::PLAYER_STANDING_THEFT_DELTA);

    edm.destroyEntity(player);
    edm.processDestructionQueue();
    BOOST_CHECK_EQUAL(edm.getPlayerFactionStanding(playerIdx, 1), 0);

    EntityHandle reused = edm.registerPlayer(9002, Vector2D(120.0f, 120.0f));
    BOOST_REQUIRE(reused.isValid());
    BOOST_CHECK_EQUAL(aiMgr.getPlayerStanding(reused, 1), 0);

    aiMgr.adjustPlayerStanding(reused, 1, AIManager::PLAYER_STANDING_GIFT_DELTA);
    BOOST_CHECK_EQUAL(aiMgr.getPlayerStanding(reused, 1),
        AIManager::PLAYER_STANDING_GIFT_DELTA);
    aiMgr.resetFactionStances();
    BOOST_CHECK_EQUAL(aiMgr.getPlayerStanding(reused, 1),
        AIManager::PLAYER_STANDING_GIFT_DELTA);

    const size_t reusedIdx = edm.getIndex(reused);
    BOOST_REQUIRE(reusedIdx != SIZE_MAX);
    edm.prepareForStateTransition();
    BOOST_CHECK_EQUAL(edm.getPlayerFactionStanding(reusedIdx, 1), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// NPC NEED SIDECAR TESTS (Slice 6)
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(NpcNeedSidecarTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(NpcNeedGetterDefaultsWhenAbsent) {
    auto& edm = EntityDataManager::Instance();
    auto npc = AITestNPC::create(Vector2D(100.0f, 100.0f));
    BOOST_REQUIRE(npc->getHandle().isValid());
    const size_t idx = edm.getIndex(npc->getHandle());
    BOOST_REQUIRE(idx != SIZE_MAX);

    BOOST_CHECK(!edm.hasNpcNeed(idx));
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(idx), 0.0f);
    BOOST_CHECK(edm.npcNeedSidecar().get(static_cast<uint32_t>(idx)) == nullptr);
    // Out-of-range index reads as absent, never grows storage.
    BOOST_CHECK(!edm.hasNpcNeed(idx + 100000));
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(idx + 100000), 0.0f);

    NpcNeedData& need = edm.ensureNpcNeed(idx);
    BOOST_CHECK(edm.hasNpcNeed(idx));
    BOOST_CHECK_EQUAL(need.pressure, 0.0f);
    BOOST_CHECK_EQUAL(need.retryCooldown, 0.0f);
    BOOST_CHECK_EQUAL(need.failCount, 0);
    BOOST_CHECK(need.returnBehavior == BehaviorType::Idle);

    edm.setNpcNeedPressure(idx, 0.4f);
    BOOST_CHECK_CLOSE(edm.getNpcNeedPressure(idx), 0.4f, 0.001f);
    edm.setNpcNeedPressure(idx, 2.0f);
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(idx), 1.0f);
    edm.setNpcNeedPressure(idx, -1.0f);
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(idx), 0.0f);

    // ensureNpcNeed returns the existing entry rather than resetting it.
    edm.setNpcNeedPressure(idx, 0.6f);
    BOOST_CHECK_CLOSE(edm.ensureNpcNeed(idx).pressure, 0.6f, 0.001f);
    BOOST_CHECK_EQUAL(edm.npcNeedSidecar().activeCount(), 1u);
}

BOOST_AUTO_TEST_CASE(NpcNeedSidecarClearedOnDestroyAndReuse) {
    auto& edm = EntityDataManager::Instance();
    auto npc = AITestNPC::create(Vector2D(100.0f, 100.0f));
    const EntityHandle handle = npc->getHandle();
    BOOST_REQUIRE(handle.isValid());
    const size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    edm.setNpcNeedPressure(idx, 0.8f);
    BOOST_REQUIRE(edm.hasNpcNeed(idx));

    edm.destroyEntity(handle);
    edm.processDestructionQueue();
    BOOST_CHECK(!edm.hasNpcNeed(idx));
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(idx), 0.0f);
    BOOST_CHECK_EQUAL(edm.npcNeedSidecar().activeCount(), 0u);

    // A new entity in the (possibly reused) slot starts without a need entry.
    auto reused = AITestNPC::create(Vector2D(120.0f, 120.0f));
    BOOST_REQUIRE(reused->getHandle().isValid());
    const size_t reusedIdx = edm.getIndex(reused->getHandle());
    BOOST_REQUIRE(reusedIdx != SIZE_MAX);
    BOOST_CHECK(!edm.hasNpcNeed(reusedIdx));
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(reusedIdx), 0.0f);
}

BOOST_AUTO_TEST_CASE(NpcNeedSidecarClearedOnPrepareForStateTransition) {
    auto& edm = EntityDataManager::Instance();
    auto npcA = AITestNPC::create(Vector2D(100.0f, 100.0f));
    auto npcB = AITestNPC::create(Vector2D(200.0f, 200.0f));
    const size_t idxA = edm.getIndex(npcA->getHandle());
    const size_t idxB = edm.getIndex(npcB->getHandle());
    BOOST_REQUIRE(idxA != SIZE_MAX);
    BOOST_REQUIRE(idxB != SIZE_MAX);

    edm.setNpcNeedPressure(idxA, 0.5f);
    edm.setNpcNeedPressure(idxB, 0.9f);
    BOOST_REQUIRE_EQUAL(edm.npcNeedSidecar().activeCount(), 2u);

    edm.prepareForStateTransition();
    BOOST_CHECK_EQUAL(edm.npcNeedSidecar().activeCount(), 0u);
    BOOST_CHECK(!edm.hasNpcNeed(idxA));
    BOOST_CHECK(!edm.hasNpcNeed(idxB));
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(idxA), 0.0f);
}

BOOST_AUTO_TEST_CASE(ForageAssignmentCreatesNeedEntry) {
    auto& edm = EntityDataManager::Instance();
    auto npc = AITestNPC::create(Vector2D(100.0f, 100.0f));
    const EntityHandle handle = npc->getHandle();
    const size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    AIManager::Instance().assignBehavior(handle, "Forage");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
    BOOST_CHECK(edm.getBehaviorConfigRef(idx).type == BehaviorType::Forage);
    BOOST_CHECK(edm.hasNpcNeed(idx));
}

BOOST_AUTO_TEST_CASE(NeedEnabledOnlyForCivilianRoles) {
    auto& edm = EntityDataManager::Instance();
    auto& ai = AIManager::Instance();

    // Civilian NPC roles from classes.json suggestedBehavior.
    const EntityHandle villager =
        edm.createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Villager");
    const EntityHandle blacksmith =
        edm.createNPCWithRaceClass(Vector2D(150.0f, 100.0f), "Human", "Blacksmith");
    // Non-civilian roles and a Wander animal.
    const EntityHandle guard =
        edm.createNPCWithRaceClass(Vector2D(200.0f, 100.0f), "Human", "Guard");
    const EntityHandle warrior =
        edm.createNPCWithRaceClass(Vector2D(250.0f, 100.0f), "Human", "Warrior");
    const EntityHandle deer = edm.createAnimal(Vector2D(300.0f, 100.0f), "Deer", "Adult");
    for (const EntityHandle h : {villager, blacksmith, guard, warrior, deer}) {
        BOOST_REQUIRE(h.isValid());
        BOOST_REQUIRE(edm.getIndex(h) != SIZE_MAX);
    }

    const size_t villagerIdx = edm.getIndex(villager);
    const size_t blacksmithIdx = edm.getIndex(blacksmith);
    const size_t guardIdx = edm.getIndex(guard);
    const size_t warriorIdx = edm.getIndex(warrior);
    const size_t deerIdx = edm.getIndex(deer);

    BOOST_REQUIRE(edm.getBehaviorConfigRef(villagerIdx).type == BehaviorType::Wander);
    BOOST_REQUIRE(edm.getBehaviorConfigRef(blacksmithIdx).type == BehaviorType::Idle);
    BOOST_REQUIRE(edm.getBehaviorConfigRef(guardIdx).type == BehaviorType::Guard);
    BOOST_REQUIRE(edm.getBehaviorConfigRef(warriorIdx).type == BehaviorType::Chase);
    BOOST_REQUIRE(edm.getBehaviorConfigRef(deerIdx).type == BehaviorType::Wander);
    BOOST_REQUIRE(edm.getCharacterDataByIndex(deerIdx).category == CreatureCategory::Animal);

    BOOST_CHECK(edm.hasNpcNeed(villagerIdx));
    BOOST_CHECK(edm.hasNpcNeed(blacksmithIdx));
    BOOST_CHECK(!edm.hasNpcNeed(guardIdx));
    BOOST_CHECK(!edm.hasNpcNeed(warriorIdx));
    BOOST_CHECK(!edm.hasNpcNeed(deerIdx));

    // Explicit role assignment follows the same rule: Patrol never gets a need,
    // Idle does.
    ai.assignBehavior(warrior, "Patrol");
    BOOST_CHECK(!edm.hasNpcNeed(warriorIdx));
    ai.assignBehavior(guard, "Idle");
    BOOST_CHECK(edm.hasNpcNeed(guardIdx));
    ai.assignBehavior(deer, "Idle");
    BOOST_CHECK(!edm.hasNpcNeed(deerIdx));

    // Preset and explicit configs are assigned intent: Forage would return through
    // the default config and discard them, so they carry no need entry.
    ai.assignBehavior(villager, "SmallWander");
    BOOST_REQUIRE(edm.getBehaviorConfigRef(villagerIdx).type == BehaviorType::Wander);
    BOOST_CHECK(!edm.hasNpcNeed(villagerIdx));
    ai.assignBehavior(blacksmith, Behaviors::getDefaultConfig(BehaviorType::Idle));
    BOOST_CHECK(!edm.hasNpcNeed(blacksmithIdx));
    // Back to the base role by name: need re-enabled with a fresh, staggered entry.
    ai.assignBehavior(villager, "Wander");
    BOOST_CHECK(edm.hasNpcNeed(villagerIdx));
    BOOST_CHECK_LT(edm.getNpcNeedPressure(villagerIdx), Behaviors::FORAGE_ENTER_THRESHOLD);
}

BOOST_AUTO_TEST_CASE(NeedEntrySeedStaggersSameFrameCivilians) {
    auto& edm = EntityDataManager::Instance();

    // Two civilians created on the same frame: role assignment seeds each need
    // entry from its entity id, so their entry pressures differ.
    const EntityHandle first =
        edm.createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Villager");
    const EntityHandle second =
        edm.createNPCWithRaceClass(Vector2D(150.0f, 100.0f), "Human", "Villager");
    BOOST_REQUIRE(first.isValid());
    BOOST_REQUIRE(second.isValid());
    const size_t firstIdx = edm.getIndex(first);
    const size_t secondIdx = edm.getIndex(second);
    BOOST_REQUIRE(firstIdx != SIZE_MAX);
    BOOST_REQUIRE(secondIdx != SIZE_MAX);
    BOOST_REQUIRE(edm.hasNpcNeed(firstIdx));
    BOOST_REQUIRE(edm.hasNpcNeed(secondIdx));
    BOOST_REQUIRE_NE(first.getId() % 1024, second.getId() % 1024);

    const auto expectedSeed = [](EntityHandle::IDType id) {
        return Behaviors::NEED_PRESSURE_PER_SECOND * Behaviors::NEED_ENTRY_STAGGER_SECONDS *
            static_cast<float>(id % 1024) / 1024.0f;
    };
    const float firstPressure = edm.getNpcNeedPressure(firstIdx);
    const float secondPressure = edm.getNpcNeedPressure(secondIdx);
    BOOST_CHECK_CLOSE(firstPressure, expectedSeed(first.getId()), 0.01f);
    BOOST_CHECK_CLOSE(secondPressure, expectedSeed(second.getId()), 0.01f);
    BOOST_CHECK_NE(firstPressure, secondPressure);
    BOOST_CHECK_LT(firstPressure, Behaviors::FORAGE_ENTER_THRESHOLD);
    BOOST_CHECK_LT(secondPressure, Behaviors::FORAGE_ENTER_THRESHOLD);
}

BOOST_AUTO_TEST_CASE(MerchantNeedEntryCarriesHomeLeash) {
    auto& edm = EntityDataManager::Instance();
    auto& ai = AIManager::Instance();
    const auto samePosition = [](const Vector2D& a, const Vector2D& b) {
        return a.getX() == b.getX() && a.getY() == b.getY();
    };

    // Role assignment (AIManager::syncNeedForRole) anchors at the creation position.
    const Vector2D merchantPos(150.0f, 100.0f);
    const Vector2D villagerPos(100.0f, 100.0f);
    const EntityHandle merchant =
        edm.createNPCWithRaceClass(merchantPos, "Human", "Blacksmith");
    const EntityHandle villager = edm.createNPCWithRaceClass(villagerPos, "Human", "Villager");
    BOOST_REQUIRE(merchant.isValid());
    BOOST_REQUIRE(villager.isValid());
    const size_t merchantIdx = edm.getIndex(merchant);
    const size_t villagerIdx = edm.getIndex(villager);
    BOOST_REQUIRE(edm.getCharacterDataByIndex(merchantIdx).isMerchant());
    BOOST_REQUIRE(!edm.getCharacterDataByIndex(villagerIdx).isMerchant());

    const NpcNeedData* merchantNeed = edm.npcNeedSidecar().get(static_cast<uint32_t>(merchantIdx));
    const NpcNeedData* villagerNeed = edm.npcNeedSidecar().get(static_cast<uint32_t>(villagerIdx));
    BOOST_REQUIRE(merchantNeed != nullptr);
    BOOST_REQUIRE(villagerNeed != nullptr);
    BOOST_CHECK_EQUAL(merchantNeed->leashRadius, Behaviors::MERCHANT_FORAGE_LEASH_RADIUS);
    BOOST_CHECK(samePosition(merchantNeed->home, merchantPos));
    BOOST_CHECK_EQUAL(villagerNeed->leashRadius, 0.0f);
    BOOST_CHECK(samePosition(villagerNeed->home, villagerPos));

    // Reassigning the same role keeps the existing entry and its anchor.
    edm.getTransformByIndex(merchantIdx).position = Vector2D(900.0f, 900.0f);
    ai.assignBehavior(merchant, "Idle");
    merchantNeed = edm.npcNeedSidecar().get(static_cast<uint32_t>(merchantIdx));
    BOOST_REQUIRE(merchantNeed != nullptr);
    BOOST_CHECK(samePosition(merchantNeed->home, merchantPos));

    // initForage seeds a new entry the same way (merchant without a prior entry).
    const Vector2D guardPos(300.0f, 200.0f);
    const EntityHandle guard = edm.createNPCWithRaceClass(guardPos, "Human", "Guard");
    BOOST_REQUIRE(guard.isValid());
    const size_t guardIdx = edm.getIndex(guard);
    BOOST_REQUIRE(!edm.hasNpcNeed(guardIdx));
    BOOST_REQUIRE(edm.initNPCAsMerchant(guard));
    ai.assignBehavior(guard, "Forage");
    const NpcNeedData* guardNeed = edm.npcNeedSidecar().get(static_cast<uint32_t>(guardIdx));
    BOOST_REQUIRE(guardNeed != nullptr);
    BOOST_CHECK_EQUAL(guardNeed->leashRadius, Behaviors::MERCHANT_FORAGE_LEASH_RADIUS);
    BOOST_CHECK(samePosition(guardNeed->home, guardPos));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// HARVESTABLE SNAPSHOT TESTS (Slice 6)
// ============================================================================

namespace {

struct AIManagerHarvestFixture : AIManagerEDMFixture {
    inline static const std::string WORLD_ID = "ai_snapshot_world";

    AIManagerHarvestFixture() {
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        auto& wrm = WorldResourceManager::Instance();
        BOOST_REQUIRE(wrm.createWorld(WORLD_ID));
        wrm.setActiveWorld(WORLD_ID);
        oreHandle = ResourceTemplateManager::Instance().getHandleById("iron_ore");
        BOOST_REQUIRE(oreHandle.isValid());
        CollisionManager::Instance().setWorldBounds(0, 0, 4000.0f, 4000.0f);
    }

    ~AIManagerHarvestFixture() {
        WorldResourceManager::Instance().clean();
    }

    AIManagerHarvestFixture(const AIManagerHarvestFixture&) = delete;
    AIManagerHarvestFixture& operator=(const AIManagerHarvestFixture&) = delete;

    EntityHandle createNode(const Vector2D& position) {
        EntityHandle handle = EntityDataManager::Instance().createHarvestable(
            position, oreHandle, 2, 2, 30.0f, WORLD_ID);
        BOOST_REQUIRE(handle.isValid());
        return handle;
    }

    static bool isDepleted(EntityHandle node) {
        const auto& edm = EntityDataManager::Instance();
        const size_t index = edm.getIndex(node);
        BOOST_REQUIRE(index != SIZE_MAX);
        return edm.getHarvestableData(edm.getStaticHotDataByIndex(index).typeLocalIndex).isDepleted;
    }

    static void step() {
        BackgroundSimulationManager::Instance().invalidateTiers();
        BackgroundSimulationManager::Instance().update(Vector2D(100.0f, 100.0f), 0.016f);
        AIManager::Instance().update(0.016f);
    }

    VoidLight::ResourceHandle oreHandle{};
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(HarvestableSnapshotTests, AIManagerHarvestFixture)

BOOST_AUTO_TEST_CASE(HarvestableSnapshotClearedOnStateTransition) {
    auto& edm = EntityDataManager::Instance();
    auto& ai = AIManager::Instance();
    auto& bus = VoidLight::AICommandBus::Instance();

    const EntityHandle nodeNear = createNode(Vector2D(130.0f, 100.0f));
    const EntityHandle nodeFar = createNode(Vector2D(400.0f, 100.0f));
    const EntityHandle villager =
        edm.createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Villager");
    BOOST_REQUIRE(villager.isValid());
    const size_t villagerIdx = edm.getIndex(villager);
    BOOST_REQUIRE(villagerIdx != SIZE_MAX);
    const size_t nearIdx = edm.getIndex(nodeNear);
    BOOST_REQUIRE(nearIdx != SIZE_MAX);

    // First update builds the snapshot; a second update with an unchanged WRM
    // version does not rebuild.
    const size_t rebuilds0 = ai.getHarvestableSnapshotRebuildCount();
    step();
    BOOST_CHECK_EQUAL(ai.getHarvestableSnapshot().size(), 2u);
    BOOST_CHECK_EQUAL(ai.getHarvestableSnapshotRebuildCount(), rebuilds0 + 1);
    step();
    BOOST_CHECK_EQUAL(ai.getHarvestableSnapshotRebuildCount(), rebuilds0 + 1);

    // A harvest command pending across the transition is dropped with the snapshot.
    bus.enqueueHarvest(villager, villagerIdx, nodeNear, static_cast<uint32_t>(nearIdx));
    ai.prepareForStateTransition();
    BOOST_CHECK(ai.getHarvestableSnapshot().empty());

    // The next update rebuilds even though the WRM version did not change.
    ai.assignBehavior(villager, "Wander");
    edm.getTransformByIndex(villagerIdx).position = Vector2D(100.0f, 100.0f);
    step();
    BOOST_CHECK_EQUAL(ai.getHarvestableSnapshot().size(), 2u);
    BOOST_CHECK_EQUAL(ai.getHarvestableSnapshotRebuildCount(), rebuilds0 + 2);
    BOOST_CHECK(!isDepleted(nodeNear));
    BOOST_CHECK(!isDepleted(nodeFar));

    // Positive control: the same command committed without a transition depletes
    // the node, resets need, and returns the forager to its origin behavior.
    // Pressure stays below FORAGE_ENTER_THRESHOLD so the Wander executor does not
    // start foraging (and overwrite returnBehavior) in the same frame.
    NpcNeedData& need = edm.ensureNpcNeed(villagerIdx);
    need.pressure = 0.5f;
    need.failCount = 2;
    need.returnBehavior = BehaviorType::Idle;
    edm.getTransformByIndex(villagerIdx).position = Vector2D(100.0f, 100.0f);
    bus.enqueueHarvest(villager, villagerIdx, nodeNear, static_cast<uint32_t>(nearIdx));
    step();
    BOOST_CHECK(isDepleted(nodeNear));
    BOOST_CHECK(!isDepleted(nodeFar));
    BOOST_CHECK_EQUAL(edm.getNpcNeedPressure(villagerIdx), 0.0f);
    BOOST_CHECK_EQUAL(edm.npcNeedSidecar().get(static_cast<uint32_t>(villagerIdx))->failCount, 0);
    BOOST_CHECK(edm.getBehaviorConfigRef(villagerIdx).type == BehaviorType::Idle);
    const uint32_t invIdx = edm.getCharacterDataByIndex(villagerIdx).inventoryIndex;
    BOOST_REQUIRE(invIdx != INVALID_INVENTORY_INDEX);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(invIdx, oreHandle), 2);

    // Depletion bumps the WRM version: next update rebuilds and excludes the node.
    step();
    BOOST_CHECK_EQUAL(ai.getHarvestableSnapshotRebuildCount(), rebuilds0 + 3);
    BOOST_REQUIRE_EQUAL(ai.getHarvestableSnapshot().size(), 1u);
    BOOST_CHECK(ai.getHarvestableSnapshot()[0].handle == nodeFar);
}

BOOST_AUTO_TEST_SUITE_END()

namespace {

void dispatchStormyVisibilityOne() {
    auto weather = std::make_shared<WeatherEvent>("test_storm", WeatherType::Clear);
    weather->setWeatherType(WeatherType::Stormy);
    WeatherParams params = weather->getWeatherParams();
    params.visibility = 1.0f;
    params.intensity = 1.0f;
    weather->setWeatherParams(params);
    EventManager::Instance().dispatchEvent(weather, EventManager::DispatchMode::Immediate);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(EnvironmentSnapshotTests, AIManagerEDMFixture)

BOOST_AUTO_TEST_CASE(TestWeatherHandlerFillsSnapshotBeforeBatch) {
    auto& aiMgr = AIManager::Instance();
    GameTimeManager::Instance().setGameHour(12.0f);

    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    aiMgr.assignBehavior(entity->getHandle(), "Wander");
    dispatchStormyVisibilityOne();

    aiMgr.update(0.016f);

    const auto& snap = aiMgr.getEnvironmentSnapshot();
    BOOST_CHECK_CLOSE(snap.moveSpeedScale, 0.75f, 0.01);
    BOOST_CHECK_CLOSE(snap.detectionScale, 0.55f, 0.01);
}

BOOST_AUTO_TEST_CASE(TestNightSnapshotFromGameHour) {
    auto& aiMgr = AIManager::Instance();
    GameTimeManager::Instance().setGameHour(22.0f);

    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    aiMgr.assignBehavior(entity->getHandle(), "Wander");

    aiMgr.update(0.016f);

    const auto& snap = aiMgr.getEnvironmentSnapshot();
    BOOST_CHECK_CLOSE(snap.detectionScale, 0.55f, 0.01);
    BOOST_CHECK_CLOSE(snap.moveSpeedScale, 0.90f, 0.01);
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransitionResetsWeatherKeepsHandler) {
    auto& aiMgr = AIManager::Instance();
    GameTimeManager::Instance().setGameHour(12.0f);

    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    aiMgr.assignBehavior(entity->getHandle(), "Wander");
    dispatchStormyVisibilityOne();
    aiMgr.update(0.016f);
    BOOST_CHECK_CLOSE(aiMgr.getEnvironmentSnapshot().moveSpeedScale, 0.75f, 0.01);

    aiMgr.prepareForStateTransition();
    const auto& resetSnap = aiMgr.getEnvironmentSnapshot();
    BOOST_CHECK_CLOSE(resetSnap.detectionScale, 1.0f, 0.01);
    BOOST_CHECK_CLOSE(resetSnap.moveSpeedScale, 1.0f, 0.01);
    BOOST_CHECK_CLOSE(resetSnap.cautionScale, 1.0f, 0.01);
    BOOST_CHECK_CLOSE(resetSnap.visibility, 1.0f, 0.01);

    dispatchStormyVisibilityOne();
    auto entityAfter = AITestNPC::create(Vector2D(200.0f, 200.0f));
    aiMgr.assignBehavior(entityAfter->getHandle(), "Wander");
    aiMgr.update(0.016f);
    BOOST_CHECK_CLOSE(aiMgr.getEnvironmentSnapshot().moveSpeedScale, 0.75f, 0.01);
    BOOST_CHECK_CLOSE(aiMgr.getEnvironmentSnapshot().detectionScale, 0.55f, 0.01);
}

BOOST_AUTO_TEST_CASE(TestChangeWeatherStormyIsNotCustom) {
    auto& aiMgr = AIManager::Instance();
    GameTimeManager::Instance().setGameHour(12.0f);

    auto entity = AITestNPC::create(Vector2D(100.0f, 100.0f));
    aiMgr.assignBehavior(entity->getHandle(), "Wander");
    BOOST_REQUIRE(EventManager::Instance().changeWeather(
        "Stormy", 1.0f, EventManager::DispatchMode::Immediate));

    aiMgr.update(0.016f);

    const auto& snap = aiMgr.getEnvironmentSnapshot();
    BOOST_CHECK_CLOSE(snap.moveSpeedScale, 0.75f, 0.01);
    BOOST_CHECK_CLOSE(snap.detectionScale, 0.55f, 0.01);
    BOOST_CHECK_CLOSE(snap.cautionScale, 1.40f, 0.01);
}

BOOST_AUTO_TEST_SUITE_END()
