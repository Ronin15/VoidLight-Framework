/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE ThreadSafeAIIntegrationTest
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/Logger.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class IntegrationTestNPC {
public:
    explicit IntegrationTestNPC(int id = 0, const Vector2D& pos = Vector2D(0, 0)) : m_id(id) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
        m_initialPosition = pos;
    }

    static std::shared_ptr<IntegrationTestNPC> create(int id = 0, const Vector2D& pos = Vector2D(0, 0)) {
        return std::make_shared<IntegrationTestNPC>(id, pos);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

    int getId() const { return m_id; }

    // Check if entity was updated (position changed or has velocity)
    int getUpdateCount() const {
        if (!m_handle.isValid()) return 0;

        auto& edm = EntityDataManager::Instance();
        size_t index = edm.getIndex(m_handle);
        if (index == SIZE_MAX) return 0;

        auto& transform = edm.getTransformByIndex(index);
        bool positionMoved = (transform.position - m_initialPosition).length() > 0.01f;
        bool hasVelocity = transform.velocity.length() > 0.01f;
        return (positionMoved || hasVelocity) ? 1 : 0;
    }

private:
    EntityHandle m_handle;
    Vector2D m_initialPosition;
    int m_id;
};

// Global test fixture for setting up and tearing down the system once for all tests
struct GlobalTestFixture {
    GlobalTestFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
        if (!EntityDataManager::Instance().init()) {
            throw std::runtime_error("EntityDataManager::init() failed");
        }
        if (!CollisionManager::Instance().init()) {
            throw std::runtime_error("CollisionManager::init() failed");
        }
        if (!PathfinderManager::Instance().init()) {
            throw std::runtime_error("PathfinderManager::init() failed");
        }
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
        if (!AIManager::Instance().init()) {
            throw std::runtime_error("AIManager::init() failed");
        }
        VOIDLIGHT_DEBUG_ONLY(AIManager::Instance().enableThreading(true);)
    }

    ~GlobalTestFixture() {
        AIManager::Instance().clean();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();
    }
};

// Individual test fixture
struct AIIntegrationTestFixture {
    AIIntegrationTestFixture() {
        // Standard behavior names available: "Idle", "Wander", "Chase", "Patrol", "Guard", "Attack", "Flee", "Follow"
        behaviorNames = {"Idle", "Wander", "Chase", "Patrol", "Guard"};

        // Create test entities with different behaviors
        for (int i = 0; i < NUM_ENTITIES; ++i) {
            auto entity = IntegrationTestNPC::create(i, Vector2D(i * 10.0f, i * 10.0f));
            entities.push_back(entity);

            // Assign standard behavior
            int behaviorIdx = i % behaviorNames.size();
            AIManager::Instance().assignBehavior(entity->getHandle(), behaviorNames[behaviorIdx]);
        }

        // Process queued assignments (need tier indices for update to work)
        auto& edm = EntityDataManager::Instance();
        for (int i = 0; i < 5; ++i) {
            edm.updateSimulationTiers(Vector2D(100.0f, 100.0f), 3000.0f, 5000.0f);
            AIManager::Instance().update(0.016f);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    ~AIIntegrationTestFixture() {
        auto& edm = EntityDataManager::Instance();
        for (auto& entity : entities) {
            if (entity) {
                const EntityHandle handle = entity->getHandle();
                AIManager::Instance().unassignBehavior(handle);
                edm.destroyEntity(handle);
            }
        }
        edm.processDestructionQueue();
        entities.clear();
        behaviorNames.clear();
    }

    static constexpr int NUM_ENTITIES = 128;
    static constexpr int NUM_UPDATES = 10;

    std::vector<std::string> behaviorNames;
    std::vector<std::shared_ptr<IntegrationTestNPC>> entities;
};

// Apply the global fixture to the entire test module
BOOST_GLOBAL_FIXTURE(GlobalTestFixture);

// Helper to update AI with proper tier calculation
void updateAI(float deltaTime, const Vector2D& referencePoint = Vector2D(100.0f, 100.0f)) {
    auto& edm = EntityDataManager::Instance();
    edm.updateSimulationTiers(referencePoint, 3000.0f, 5000.0f);
    AIManager::Instance().update(deltaTime);
}

void primeAIThreadingDecision(size_t workloadSize) {
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    budgetMgr.prepareForStateTransition();
    for (int sample = 0; sample < 10; ++sample) {
        budgetMgr.reportExecution(VoidLight::SystemType::AI, workloadSize, false, 1, 5.0);
    }
    BOOST_REQUIRE(budgetMgr.shouldUseThreading(VoidLight::SystemType::AI, workloadSize).shouldThread);
}

BOOST_FIXTURE_TEST_SUITE(AIIntegrationTests, AIIntegrationTestFixture)

// Test that updates work properly
BOOST_AUTO_TEST_CASE(TestConcurrentUpdates) {
    // Get initial behavior execution count
    size_t initialCount = AIManager::Instance().getBehaviorUpdateCount();

    // Update the AI system multiple times
    primeAIThreadingDecision(entities.size());
    for (int i = 0; i < NUM_UPDATES; ++i) {
        updateAI(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Verify behaviors were executed (data-oriented pattern: AIManager processes behavior data
    // via executeLogic(), which operates on EntityDataManager data)
    size_t finalCount = AIManager::Instance().getBehaviorUpdateCount();
    bool behaviorsExecuted = (finalCount > initialCount);

    BOOST_CHECK_MESSAGE(behaviorsExecuted,
        "Expected behavior executions to increase. Initial: " << initialCount
        << ", Final: " << finalCount);

    // Note: In data-oriented architecture, AIManager processes behavior data directly
    // via executeLogic(ctx), which operates on EntityDataManager SoA data.
    // The getBehaviorUpdateCount() tracks these executions.
}

// Test concurrent assignment and update
BOOST_AUTO_TEST_CASE(TestConcurrentAssignmentAndUpdate) {
    // Get a test entity
    BOOST_REQUIRE(!entities.empty());
    auto entity = entities[0];
    const size_t initialAssignments = AIManager::Instance().getTotalAssignmentCount();

    // Queue a behavior assignment and process it
    AIManager::Instance().assignBehavior(entity->getHandle(), "Wander");
    updateAI(0.016f);

    BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));
    BOOST_CHECK_GT(AIManager::Instance().getTotalAssignmentCount(), initialAssignments);
}

// Test message delivery
BOOST_AUTO_TEST_CASE(TestMessageDelivery) {
    // Get a test entity
    BOOST_REQUIRE(!entities.empty());
    auto testEntity = entities[0];

    // Ensure the behavior is assigned before messaging
    BOOST_REQUIRE(AIManager::Instance().hasBehavior(testEntity->getHandle()));

    // Legacy string message API was removed - this test now verifies the entity
    // remains assigned and continues participating in AI updates.
    const size_t initialUpdates = AIManager::Instance().getBehaviorUpdateCount();
    updateAI(0.016f);

    BOOST_CHECK(AIManager::Instance().hasBehavior(testEntity->getHandle()));
    BOOST_CHECK_GT(AIManager::Instance().getBehaviorUpdateCount(), initialUpdates);
}

// Test behavior switching
BOOST_AUTO_TEST_CASE(TestBehaviorSwitching) {
    BOOST_REQUIRE(!entities.empty());
    auto entity = entities[0];
    const size_t initialAssignments = AIManager::Instance().getTotalAssignmentCount();

    // Switch between different behaviors
    AIManager::Instance().assignBehavior(entity->getHandle(), "Idle");
    updateAI(0.016f);
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));

    AIManager::Instance().assignBehavior(entity->getHandle(), "Chase");
    updateAI(0.016f);
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));

    AIManager::Instance().assignBehavior(entity->getHandle(), "Patrol");
    updateAI(0.016f);
    BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));

    BOOST_CHECK_GE(AIManager::Instance().getTotalAssignmentCount(), initialAssignments + 3);
}

// Test multiple entities with same behavior
BOOST_AUTO_TEST_CASE(TestMultipleSameBehavior) {
    const size_t initialUpdates = AIManager::Instance().getBehaviorUpdateCount();

    // Assign all entities the same behavior
    for (auto& entity : entities) {
        AIManager::Instance().assignBehavior(entity->getHandle(), "Wander");
        BOOST_CHECK(AIManager::Instance().hasBehavior(entity->getHandle()));
    }

    // Process multiple updates
    primeAIThreadingDecision(entities.size());
    for (int i = 0; i < 5; ++i) {
        updateAI(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    BOOST_CHECK_GT(AIManager::Instance().getBehaviorUpdateCount(), initialUpdates);
}

// Test cache invalidation - simplest possible implementation
BOOST_AUTO_TEST_CASE(TestCacheInvalidation) {
    BOOST_REQUIRE(!entities.empty());
    auto entity = entities[0];
    BOOST_REQUIRE(AIManager::Instance().hasBehavior(entity->getHandle()));

    AIManager::Instance().resetBehaviors();

    BOOST_CHECK_EQUAL(AIManager::Instance().getBehaviorUpdateCount(), 0U);
    BOOST_CHECK(!AIManager::Instance().hasBehavior(entity->getHandle()));
}

BOOST_AUTO_TEST_SUITE_END()
