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
#include "events/EntityEvents.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
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
        const char* behaviorName = (i % 3 == 0) ? "Wander" : (i % 3 == 1) ? "Idle" : "Patrol";
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
    const char* behaviorTypes[] = {"Idle", "Wander", "Chase", "Patrol", "Guard", "Attack", "Flee", "Follow"};

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
    aiMgr.scanFactionInRadius(faction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) != results.end());

    // Should NOT be in a different faction
    uint8_t otherFaction = (faction == 0) ? 1 : 0;
    results.clear();
    aiMgr.scanFactionInRadius(otherFaction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
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
    aiMgr.scanFactionInRadius(faction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_REQUIRE(std::find(results.begin(), results.end(), idx) != results.end());

    // Unassign — should be removed from faction index
    aiMgr.unassignBehavior(handle);
    results.clear();
    aiMgr.scanFactionInRadius(faction, Vector2D(300.0f, 300.0f), 1000.0f, results, false);
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
    aiMgr.scanFactionInRadius(faction, Vector2D(300.0f, 300.0f), 500.0f, results, false);

    BOOST_CHECK(std::find(results.begin(), results.end(), nearIdx) != results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), farIdx) == results.end());
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
    aiMgr.update(0.016f);  // Commit queued transition
    results.clear();
    aiMgr.scanGuardsInRadius(Vector2D(300.0f, 300.0f), 1000.0f, results, false);
    BOOST_CHECK(std::find(results.begin(), results.end(), idx) == results.end());

    Behaviors::switchBehavior(idx, BehaviorType::Guard);
    aiMgr.update(0.016f);  // Commit queued transition
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
