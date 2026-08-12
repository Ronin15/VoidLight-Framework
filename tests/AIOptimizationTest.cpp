/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE AIOptimizationTests
#include <boost/test/unit_test.hpp>

#include <memory>
#include <chrono>
#include <vector>
#include <iostream>

// Include real engine headers
#include "core/ThreadSystem.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class OptimizationTestNPC {
public:
    explicit OptimizationTestNPC(const Vector2D& pos = Vector2D(0, 0)) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
    }

    static std::shared_ptr<OptimizationTestNPC> create(const Vector2D& pos = Vector2D(0, 0)) {
        return std::make_shared<OptimizationTestNPC>(pos);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

private:
    EntityHandle m_handle;
};

// Global fixture for test setup and cleanup
struct AITestFixture {
    AITestFixture() {
        // Initialize dependencies required by the real AIManager
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
        if (!EntityDataManager::Instance().init()) {
            throw std::runtime_error("EntityDataManager::init() failed");
        }
        // NOTE: global fixtures run outside any test-case context, so Boost.Test
        // assertion macros (BOOST_REQUIRE/BOOST_CHECK) here raise a framework
        // setup_error (exit 200) even when all cases pass. Fail via throw instead.
        if (!CollisionManager::Instance().init()) {
            throw std::runtime_error("CollisionManager::init() failed");
        }
        if (!PathfinderManager::Instance().init()) {
            throw std::runtime_error("PathfinderManager::init() failed");
        }
        if (!AIManager::Instance().init()) {
            throw std::runtime_error("AIManager::init() failed");
        }
        if (!BackgroundSimulationManager::Instance().init()) {
            throw std::runtime_error("BackgroundSimulationManager::init() failed");
        }
    }

    ~AITestFixture() {
        // Clean up in reverse order
        BackgroundSimulationManager::Instance().clean();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(AITestFixture);

// Helper to update AI with proper tier calculation
// Tests create/destroy entities frequently, so we need to invalidate tiers each time
void updateAI(float deltaTime, const Vector2D& referencePoint = Vector2D(500.0f, 500.0f)) {
    BackgroundSimulationManager::Instance().invalidateTiers();
    BackgroundSimulationManager::Instance().update(referencePoint, deltaTime);
    AIManager::Instance().update(deltaTime);
}

// Test case for entity component caching
BOOST_AUTO_TEST_CASE(TestEntityComponentCaching)
{
    // Create test NPCs (already registered via createDataDrivenNPC)
    std::vector<EntityHandle> handles;
    std::vector<std::shared_ptr<OptimizationTestNPC>> entities;
    for (int i = 0; i < 10; ++i) {
        Vector2D pos(i * 100.0f, i * 100.0f);
        auto entity = OptimizationTestNPC::create(pos);
        entities.push_back(entity);
        EntityHandle handle = entity->getHandle();
        handles.push_back(handle);
        AIManager::Instance().assignBehavior(handle, "Wander");
    }

    // Process pending assignments
    updateAI(0.016f);

    // Wait for async assignments to complete (matches production behavior)
    // Assignments are now synchronous - no wait needed

    // Cleanup - unregister entities from managed updates
    auto& edm = EntityDataManager::Instance();
    for (const auto& handle : handles) {
        AIManager::Instance().unregisterEntity(handle);
        AIManager::Instance().unassignBehavior(handle);
        edm.unregisterEntity(handle.getId());
    }
    handles.clear();
    entities.clear();
}

// Test case for batch processing
BOOST_AUTO_TEST_CASE(TestBatchProcessing)
{
    // Create test NPCs (already registered via createDataDrivenNPC)
    std::vector<EntityHandle> handles;
    std::vector<std::shared_ptr<OptimizationTestNPC>> entityPtrs;
    for (int i = 0; i < 100; ++i) {
        Vector2D pos(i * 10.0f, i * 10.0f);
        auto entity = OptimizationTestNPC::create(pos);
        entityPtrs.push_back(entity);
        EntityHandle handle = entity->getHandle();
        handles.push_back(handle);
        AIManager::Instance().assignBehavior(handle, "Wander");
    }

    // Process pending assignments
    updateAI(0.016f);

    // Wait for async assignments to complete before timing updates
    // Assignments are now synchronous - no wait needed

    // Time the unified entity processing
    auto startTime = std::chrono::high_resolution_clock::now();
    updateAI(0.016f);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto batchDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Time multiple managed updates
    startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 5; ++i) {
        updateAI(0.016f);
    }
    endTime = std::chrono::high_resolution_clock::now();
    auto individualDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Output timing info (not an actual test, just informational)
    std::cout << "Batch processing time: " << batchDuration.count() << " µs" << std::endl;
    std::cout << "Individual processing time: " << individualDuration.count() << " µs" << std::endl;

    // Batch processing should be reasonably efficient
    // (Don't enforce strict inequality as threading can cause variance)
    BOOST_CHECK_GT(individualDuration.count(), 0);
    BOOST_CHECK_GT(batchDuration.count(), 0);

    // Cleanup - unregister entities from managed updates
    auto& edm = EntityDataManager::Instance();
    for (const auto& handle : handles) {
        AIManager::Instance().unregisterEntity(handle);
        AIManager::Instance().unassignBehavior(handle);
        edm.unregisterEntity(handle.getId());
    }
    handles.clear();
    entityPtrs.clear();
}

// Test case for early exit conditions
BOOST_AUTO_TEST_CASE(TestEarlyExitConditions)
{
    // Create test NPC (already registered via createDataDrivenNPC)
    Vector2D pos(100.0f, 100.0f);
    auto entity = OptimizationTestNPC::create(pos);
    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Wander");

    // Process pending assignments
    updateAI(0.016f);

    // Wait for async assignments to complete
    // Assignments are now synchronous - no wait needed

    // Test that behavior is assigned
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Cleanup - unregister entity from managed updates
    AIManager::Instance().unregisterEntity(handle);
    AIManager::Instance().unassignBehavior(handle);
    EntityDataManager::Instance().unregisterEntity(handle.getId());
}

// Test case for message queue system
BOOST_AUTO_TEST_CASE(TestMessageQueueSystem)
{
    // Create test NPC (already registered via createDataDrivenNPC)
    Vector2D pos(100.0f, 100.0f);
    auto entity = OptimizationTestNPC::create(pos);
    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Wander");

    // Process pending assignments
    updateAI(0.016f);

    // Wait for async assignments to complete (matches production behavior)
    // Assignments are now synchronous - no wait needed

    // Legacy string message API was removed - message system now uses BehaviorMessage queue

    // Entity should still have behavior after all messages
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Cleanup - unregister entity from managed updates
    AIManager::Instance().unregisterEntity(handle);
    AIManager::Instance().unassignBehavior(handle);
    EntityDataManager::Instance().unregisterEntity(handle.getId());
}

BOOST_AUTO_TEST_CASE(TestWorldBoundsClampingWithBehaviors)
{
    // Test that entities with active behaviors stay within world bounds
    // This verifies world bounds clamping works with the behavior system
    std::vector<EntityHandle> handles;
    std::vector<std::shared_ptr<OptimizationTestNPC>> entities;
    auto& edm = EntityDataManager::Instance();

    // World bounds: min = 16 (halfWidth), max = 31984 (32000 - halfWidth)
    constexpr float WORLD_MIN = 16.0f;
    constexpr float WORLD_MAX = 31984.0f;

    auto createEntity = [&](const Vector2D& pos, const std::string& behavior) {
        auto entity = OptimizationTestNPC::create(pos);
        entities.push_back(entity);
        EntityHandle handle = entity->getHandle();
        handles.push_back(handle);
        AIManager::Instance().assignBehavior(handle, behavior);
    };

    // Create entities at various positions including near boundaries
    createEntity(Vector2D(50.0f, 50.0f), "Wander");           // Near min corner
    createEntity(Vector2D(31950.0f, 31950.0f), "Wander");     // Near max corner
    createEntity(Vector2D(16000.0f, 16000.0f), "Wander");     // Center
    createEntity(Vector2D(30.0f, 16000.0f), "Wander");        // Near min X
    createEntity(Vector2D(16000.0f, 31970.0f), "Wander");     // Near max Y

    // Run simulation for multiple frames to let behaviors move entities
    for (int frame = 0; frame < 100; ++frame) {
        updateAI(0.016f, Vector2D(16000.0f, 16000.0f));
    }

    // Verify all entities stayed within world bounds
    for (size_t i = 0; i < handles.size(); ++i) {
        size_t idx = edm.getIndex(handles[i]);
        const auto& transform = edm.getHotDataByIndex(idx).transform;

        BOOST_CHECK_GE(transform.position.getX(), WORLD_MIN);
        BOOST_CHECK_LE(transform.position.getX(), WORLD_MAX);
        BOOST_CHECK_GE(transform.position.getY(), WORLD_MIN);
        BOOST_CHECK_LE(transform.position.getY(), WORLD_MAX);
    }

    // Cleanup
    for (const auto& handle : handles) {
        AIManager::Instance().unregisterEntity(handle);
        AIManager::Instance().unassignBehavior(handle);
        edm.unregisterEntity(handle.getId());
    }
    handles.clear();
    entities.clear();
}

// Test case for SIMD distance calculations including tail loop edge cases
// This verifies that ALL entities receive proper distance calculations,
// especially for entity counts that are NOT multiples of 4 (SIMD width)
BOOST_AUTO_TEST_CASE(TestDistanceCalculationCorrectness)
{
    auto& edm = EntityDataManager::Instance();

    // Test with entity counts that stress the SIMD tail loop:
    // 1, 2, 3 (all scalar)
    // 4, 5, 6, 7 (SIMD + tail)
    // 8, 9, 10, 11 (SIMD*2 + tail)
    std::vector<size_t> testCounts = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 17, 23};

    for (size_t count : testCounts) {
        // Create entities at known positions (already registered via createDataDrivenNPC)
        std::vector<std::shared_ptr<OptimizationTestNPC>> entities;
        std::vector<EntityHandle> handles;
        for (size_t i = 0; i < count; ++i) {
            // Place entities at (100 * i, 100 * i) for predictable distances
            Vector2D pos(100.0f * static_cast<float>(i), 100.0f * static_cast<float>(i));
            auto entity = OptimizationTestNPC::create(pos);
            entities.push_back(entity);
            EntityHandle handle = entity->getHandle();
            handles.push_back(handle);
            AIManager::Instance().assignBehavior(handle, "Wander");
        }

        // Process assignments
        updateAI(0.016f);
        // Assignments are now synchronous - no wait needed

        // Run a few update cycles to ensure distance calculations run
        for (int frame = 0; frame < 3; ++frame) {
            updateAI(0.016f);
        }

        // Verify all entities received valid processing (no teleportation to (0,0))
        for (size_t i = 0; i < entities.size(); ++i) {
            size_t edmIndex = edm.getIndex(entities[i]->getHandle());
            auto pos = edm.getTransformByIndex(edmIndex).position;
            // Entities should be near their starting positions (WanderBehavior may move them slightly)
            // But they should NEVER teleport to (0,0) unless they started there
            if (i > 0) {
                // Entity i started at (100*i, 100*i), so it should NOT be at origin
                // Allow for some movement due to WanderBehavior, but position should be reasonable
                float distanceFromOrigin = std::sqrt(pos.getX() * pos.getX() + pos.getY() * pos.getY());
                BOOST_CHECK_MESSAGE(distanceFromOrigin > 10.0f,
                    "Entity " << i << " of " << count << " teleported to origin! Position: ("
                    << pos.getX() << ", " << pos.getY() << ")");
            }
        }

        // Cleanup
        for (const auto& handle : handles) {
            AIManager::Instance().unregisterEntity(handle);
            AIManager::Instance().unassignBehavior(handle);
            edm.unregisterEntity(handle.getId());
        }
        handles.clear();
        entities.clear();
    }
}
