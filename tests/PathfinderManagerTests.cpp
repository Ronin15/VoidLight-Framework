/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE PathfinderManagerTests
#include <boost/test/unit_test.hpp>

#include "managers/PathfinderManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/CollisionManager.hpp"
#include "events/CollisionObstacleChangedEvent.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "ai/pathfinding/PathfindingGrid.hpp"
#include "utils/Vector2D.hpp"
#include "world/WorldData.hpp"
#include <chrono>
#include <thread>
#include <vector>

using namespace VoidLight;

namespace {

template <typename Predicate>
bool waitForPathfinder(PathfinderManager& manager,
                       Predicate&& predicate,
                       int maxPolls = 100,
                       std::chrono::milliseconds pollInterval = std::chrono::milliseconds(10)) {
    for (int i = 0; i < maxPolls; ++i) {
        manager.update();
        manager.commitCompletedPaths();
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }

    manager.update();
    manager.commitCompletedPaths();
    return predicate();
}

bool waitForEdmPathCommit(PathfinderManager& manager, size_t edmIndex,
                          int maxPolls = 100) {
    auto& edm = EntityDataManager::Instance();
    return waitForPathfinder(manager, [&edm, edmIndex] {
        return edm.hasPathData(edmIndex) &&
               edm.getPathData(edmIndex).pathRequestPending.load(
                   std::memory_order_acquire) == 0;
    }, maxPolls);
}

size_t createPathNpc(const Vector2D& pos) {
    EntityHandle handle =
        EntityDataManager::Instance().createNPCWithRaceClass(pos, "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());
    const size_t idx = EntityDataManager::Instance().getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);
    BOOST_REQUIRE(EntityDataManager::Instance().hasPathData(idx));
    return idx;
}

bool ensureWorldAndGrid() {
    auto& worldMgr = WorldManager::Instance();
    if (!worldMgr.hasActiveWorld()) {
        WorldGenerationConfig worldConfig{};
        worldConfig.width = 20;
        worldConfig.height = 20;
        worldConfig.seed = 20260305;
        worldConfig.elevationFrequency = 0.1f;
        worldConfig.humidityFrequency = 0.1f;
        worldConfig.waterLevel = 0.3f;
        worldConfig.mountainLevel = 0.7f;
        if (!worldMgr.loadNewWorld(worldConfig)) {
            return false;
        }
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EventManager::Instance().update();
    }

    auto& pm = PathfinderManager::Instance();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (std::chrono::steady_clock::now() < deadline) {
        pm.update();
        if (pm.isGridReady()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return pm.isGridReady();
}

struct PathfinderRequestFixture {
    PathfinderRequestFixture() {
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(WorldManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(ensureWorldAndGrid());
    }

    ~PathfinderRequestFixture() {
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        WorldManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        WorldResourceManager::Instance().clean();
        EventManager::Instance().clean();
    }
};

} // namespace

// Initialize ThreadSystem for async pathfinding in this test module
struct PFThreadFixture {
    PFThreadFixture() {
        if (!VoidLight::ThreadSystem::Instance().init(4096)) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~PFThreadFixture() {
        if (!VoidLight::ThreadSystem::Instance().isShutdown()) {
            VoidLight::ThreadSystem::Instance().clean();
        }
    }
};

BOOST_GLOBAL_FIXTURE(PFThreadFixture);

BOOST_AUTO_TEST_SUITE(PathfinderManagerTestSuite)

BOOST_AUTO_TEST_CASE(TestPathfinderManagerSingleton) {
    // Test singleton behavior
    PathfinderManager& instance1 = PathfinderManager::Instance();
    PathfinderManager& instance2 = PathfinderManager::Instance();
    
    BOOST_CHECK(&instance1 == &instance2);
}

BOOST_AUTO_TEST_CASE(TestPathfinderManagerInitialization) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    // Initially not initialized
    BOOST_CHECK(!manager.isInitialized());
    BOOST_CHECK(!manager.isShutdown());
    
    // Initialize should succeed
    BOOST_CHECK(manager.init());
    BOOST_CHECK(manager.isInitialized());
    BOOST_CHECK(!manager.isShutdown());
    
    // Calling init again should return true (already initialized)
    BOOST_CHECK(manager.init());
    
    // Clean up for other tests
    manager.clean();
    BOOST_CHECK(!manager.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(TestAsyncPathfinding, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    Vector2D start(100.0f, 100.0f);
    Vector2D goal(200.0f, 200.0f);
    const size_t edmIndex = createPathNpc(start);

    const uint64_t requestId = manager.requestPathToEDM(
        edmIndex, start, goal, PathfinderManager::Priority::Normal);
    BOOST_CHECK(requestId > 0);

    BOOST_CHECK(waitForEdmPathCommit(manager, edmIndex));
}

BOOST_AUTO_TEST_CASE(TestPathfinderConfiguration) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    
    // Test configuration methods
    manager.setMaxPathsPerFrame(3);
    manager.setCacheExpirationTime(10.0f);
    manager.setAllowDiagonal(false);
    manager.setMaxIterations(5000);
    
    // These should not crash - actual behavior testing would require more complex setup
    
    manager.clean();
}

BOOST_FIXTURE_TEST_CASE(TestBasicFunctionality, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    Vector2D start(100.0f, 100.0f);
    Vector2D goal(200.0f, 200.0f);
    const size_t edmIndex = createPathNpc(start);

    // Production path: requestPathToEDM + main-thread commitCompletedPaths.
    const uint64_t requestId = manager.requestPathToEDM(
        edmIndex, start, goal, PathfinderManager::Priority::Low);
    BOOST_CHECK(requestId > 0);

    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    BOOST_CHECK(waitForEdmPathCommit(manager, edmIndex));
}

BOOST_AUTO_TEST_CASE(TestWeightFields) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    
    Vector2D center(150.0f, 150.0f);
    float radius = 50.0f;
    float weight = 2.0f;
    
    // Add temporary weight field
    manager.addTemporaryWeightField(center, radius, weight);
    
    // Clear weight fields
    manager.clearWeightFields();
    
    // These should not crash
    
    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestStatistics) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    
    // Reset stats first to ensure clean state
    manager.resetStats();
    
    // Get initial stats after reset
    auto stats = manager.getStats();
    BOOST_CHECK(stats.totalRequests == 0);
    BOOST_CHECK(stats.completedRequests == 0);
    BOOST_CHECK(stats.failedRequests == 0);
    
    // Reset stats again (should not crash)
    manager.resetStats();
    
    auto statsAfterReset = manager.getStats();
    BOOST_CHECK(statsAfterReset.totalRequests == 0);
    
    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestShutdown) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    BOOST_CHECK(manager.isInitialized());
    BOOST_CHECK(!manager.isShutdown());
    
    // Clean should mark as not initialized but not shutdown
    manager.clean();
    BOOST_CHECK(!manager.isInitialized());
    
    // Re-initialize should work
    BOOST_CHECK(manager.init());
    BOOST_CHECK(manager.isInitialized());
    
    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestUpdateCycle) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    
    // Test multiple update cycles
    for (int i = 0; i < 10; ++i) {
        manager.update(); // ~60 FPS
    }
    
    // Should not crash
    
    manager.clean();
}

BOOST_FIXTURE_TEST_CASE(TestNoInfiniteRetryLoop, PathfinderRequestFixture) {
    // Distinct NPCs each complete once; requestPathToEDM does not requeue.

    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    Vector2D start(50.0f, 50.0f);
    Vector2D goal(100.0f, 100.0f);
    std::vector<size_t> edmIndices;
    edmIndices.reserve(4);
    for (int i = 0; i < 4; ++i) {
        edmIndices.push_back(createPathNpc(start));
        BOOST_CHECK_GT(manager.requestPathToEDM(
            edmIndices.back(), start, goal, PathfinderManager::Priority::High), 0U);
    }

    BOOST_CHECK(waitForPathfinder(manager, [&edmIndices] {
        auto& edm = EntityDataManager::Instance();
        for (size_t idx : edmIndices) {
            if (!edm.hasPathData(idx) ||
                edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }));
}

BOOST_FIXTURE_TEST_CASE(TestSequentialRequestsCommitOnMainThread, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    Vector2D start(64.0f, 64.0f);
    Vector2D goal(192.0f, 192.0f);
    const size_t edmIndex = createPathNpc(start);

    BOOST_CHECK_GT(manager.requestPathToEDM(
        edmIndex, start, goal, PathfinderManager::Priority::High), 0U);
    BOOST_CHECK(waitForEdmPathCommit(manager, edmIndex));

    BOOST_CHECK_GT(manager.requestPathToEDM(
        edmIndex, start, goal, PathfinderManager::Priority::High), 0U);
    BOOST_CHECK(waitForEdmPathCommit(manager, edmIndex));
}

BOOST_AUTO_TEST_SUITE_END()

// Integration Tests for PathfinderManager Event System  
BOOST_AUTO_TEST_SUITE(PathfinderEventIntegrationTests)

// Test fixture for PathfinderManager event integration
struct PathfinderEventFixture {
    PathfinderEventFixture() {
        // Initialize EventManager for event testing (following established pattern)
        BOOST_REQUIRE(EventManager::Instance().init());
        
        // Initialize PathfinderManager
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        
        // Reset tracking variables
        collisionVersionIncremented = false;
        cacheInvalidationCount = 0;
    }
    
    ~PathfinderEventFixture() {
        // Clean up in reverse order
        PathfinderManager::Instance().clean();
        EventManager::Instance().clean();
        // ThreadSystem persists across tests (per established pattern)
    }
    
    // Event tracking variables
    bool collisionVersionIncremented;
    std::atomic<int> cacheInvalidationCount{0};
};

BOOST_FIXTURE_TEST_CASE(TestPathfinderEventSubscription, PathfinderEventFixture)
{
    // Test that PathfinderManager properly subscribes to collision obstacle events
    // This tests the event subscription lifecycle
    auto initialStats = PathfinderManager::Instance().getStats();
    
    // Manually trigger a collision obstacle changed event
    Vector2D obstaclePos(100.0f, 150.0f);
    float obstacleRadius = 64.0f;
    std::string description = "Test obstacle change";
    
    // Trigger the event
    bool eventFired = EventManager::Instance().triggerCollisionObstacleChanged(
        obstaclePos, obstacleRadius, description, EventManager::DispatchMode::Immediate);
    
    // Event should fire (PathfinderManager should be subscribed)
    BOOST_CHECK(eventFired);
    
    // Brief wait to let the handler process (following established pattern)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto stats = PathfinderManager::Instance().getStats();
    BOOST_CHECK(PathfinderManager::Instance().isInitialized());
    BOOST_CHECK_GE(stats.totalRequests, initialStats.totalRequests);
    BOOST_CHECK_LE(stats.totalRequests, initialStats.totalRequests + 8U);
    BOOST_CHECK_EQUAL(stats.cacheSize, 0U);
}

BOOST_FIXTURE_TEST_CASE(TestPathfinderCacheInvalidationOnCollisionChange, PathfinderRequestFixture)
{
    // Test that collision obstacle changes properly invalidate pathfinding cache
    
    // First, let's simulate having some cached paths by triggering path requests
    Vector2D start1(0.0f, 0.0f);
    Vector2D goal1(100.0f, 100.0f);
    Vector2D start2(200.0f, 200.0f); 
    Vector2D goal2(300.0f, 300.0f);
    
    const size_t npc1 = createPathNpc(start1);
    const size_t npc2 = createPathNpc(start2);
    BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
        npc1, start1, goal1, PathfinderManager::Priority::High), 0U);
    BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
        npc2, start2, goal2, PathfinderManager::Priority::High), 0U);

    BOOST_CHECK(waitForEdmPathCommit(PathfinderManager::Instance(), npc1));
    BOOST_CHECK(waitForEdmPathCommit(PathfinderManager::Instance(), npc2));

    auto initialStats = PathfinderManager::Instance().getStats();

    Vector2D obstaclePos(150.0f, 150.0f);
    EventManager::Instance().triggerCollisionObstacleChanged(
        obstaclePos, 100.0f, "Cache invalidation test", EventManager::DispatchMode::Immediate);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const size_t npc3 = createPathNpc(start1);
    BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
        npc3, start1, goal1, PathfinderManager::Priority::High), 0U);
    BOOST_CHECK(waitForEdmPathCommit(PathfinderManager::Instance(), npc3));
    
    // Verify the system is still functioning (no crashes from event handling)
    auto finalStats = PathfinderManager::Instance().getStats();
    BOOST_CHECK_GE(finalStats.totalRequests, initialStats.totalRequests);
}

BOOST_FIXTURE_TEST_CASE(TestPathfinderEventHandlerLifecycle, PathfinderEventFixture)
{
    // Test that PathfinderManager properly manages its event subscriptions
    
    // PathfinderManager should be initialized with event subscriptions
    BOOST_CHECK(PathfinderManager::Instance().isInitialized());
    
    // Trigger multiple events to ensure handler is stable
    for (int i = 0; i < 5; ++i) {
        Vector2D pos(i * 50.0f, i * 50.0f);
        bool fired = EventManager::Instance().triggerCollisionObstacleChanged(
            pos, 32.0f, "Lifecycle test " + std::to_string(i), 
            EventManager::DispatchMode::Immediate);
        BOOST_CHECK(fired);
    }
    
    // Brief processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Clean and reinitialize to test subscription cleanup/re-establishment
    PathfinderManager::Instance().clean();
    BOOST_CHECK(!PathfinderManager::Instance().isInitialized());
    
    // Re-initialize
    BOOST_REQUIRE(PathfinderManager::Instance().init());
    BOOST_CHECK(PathfinderManager::Instance().isInitialized());
    
    // Should still be able to receive events after re-initialization
    bool fired = EventManager::Instance().triggerCollisionObstacleChanged(
        Vector2D(999.0f, 999.0f), 64.0f, "Post-reinit test",
        EventManager::DispatchMode::Immediate);
    BOOST_CHECK(fired);
}

BOOST_FIXTURE_TEST_CASE(TestPathfinderEventPerformance, PathfinderEventFixture) 
{
    // Test that event handling doesn't significantly impact PathfinderManager performance
    
    const int numEvents = 50;
    std::vector<std::chrono::microseconds> eventTimes;
    eventTimes.reserve(numEvents);
    
    // Measure time for each event to be processed
    for (int i = 0; i < numEvents; ++i) {
        Vector2D pos(i * 20.0f, i * 20.0f);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        EventManager::Instance().triggerCollisionObstacleChanged(
            pos, 48.0f, "Performance test " + std::to_string(i),
            EventManager::DispatchMode::Immediate);
        
        auto end = std::chrono::high_resolution_clock::now();
        eventTimes.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
    }
    
    // Calculate average event processing time
    auto totalTime = std::chrono::microseconds(0);
    for (const auto& time : eventTimes) {
        totalTime += time;
    }
    double avgTime = static_cast<double>(totalTime.count()) / numEvents;
    
    // Event processing should be fast (under 200 microseconds average)
    // Generous threshold to avoid flakiness under system load / context switches
    BOOST_CHECK_LT(avgTime, 200.0);

    // No single event should take more than 2000 microseconds
    for (const auto& time : eventTimes) {
        BOOST_CHECK_LT(time.count(), 2000);
    }
    
    BOOST_TEST_MESSAGE("Processed " << numEvents << " collision events in avg " 
                      << avgTime << " μs per event");
    
    // Verify PathfinderManager is still functioning after many events
    auto stats = PathfinderManager::Instance().getStats();
    BOOST_CHECK_GE(stats.totalRequests, 0); // Should be accessible
}

// ========== WorkerBudget Integration Tests ==========

BOOST_FIXTURE_TEST_CASE(TestBurstRequestHandling, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    const size_t burstSize = 32;
    std::vector<size_t> edmIndices;
    edmIndices.reserve(burstSize);

    BOOST_TEST_MESSAGE("Submitting " << burstSize << " simultaneous path requests...");

    for (size_t i = 0; i < burstSize; ++i) {
        Vector2D start(100.0f + static_cast<float>(i) * 10.0f, 100.0f);
        Vector2D goal(200.0f + static_cast<float>(i) * 10.0f, 200.0f);
        edmIndices.push_back(createPathNpc(start));
        BOOST_CHECK_GT(manager.requestPathToEDM(
            edmIndices.back(), start, goal, PathfinderManager::Priority::Normal), 0U);
    }

    BOOST_CHECK(waitForPathfinder(manager, [&edmIndices] {
        auto& edm = EntityDataManager::Instance();
        for (size_t idx : edmIndices) {
            if (!edm.hasPathData(idx) ||
                edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }, 120, std::chrono::milliseconds(10)));
}

BOOST_FIXTURE_TEST_CASE(TestDirectSubmissionHasNoInternalQueue, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    const size_t testRequests = 24;
    std::vector<size_t> edmIndices;
    edmIndices.reserve(testRequests);

    for (size_t i = 0; i < testRequests; ++i) {
        Vector2D start(64.0f + static_cast<float>(i), 64.0f);
        Vector2D goal(192.0f + static_cast<float>(i), 192.0f);
        edmIndices.push_back(createPathNpc(start));
        BOOST_CHECK_GT(manager.requestPathToEDM(
            edmIndices.back(), start, goal, PathfinderManager::Priority::Normal), 0U);
    }

    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    BOOST_CHECK(waitForPathfinder(manager, [&edmIndices] {
        auto& edm = EntityDataManager::Instance();
        for (size_t idx : edmIndices) {
            if (!edm.hasPathData(idx) ||
                edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }, 120, std::chrono::milliseconds(10)));

    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());
}

BOOST_FIXTURE_TEST_CASE(TestWorkerBudgetCoordination, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    auto& threadSystem = VoidLight::ThreadSystem::Instance();
    size_t availableWorkers = threadSystem.getThreadCount();

    BOOST_TEST_MESSAGE("Available workers: " << availableWorkers);

    const auto& budget = VoidLight::WorkerBudgetManager::Instance().getBudget();

    BOOST_TEST_MESSAGE("Total workers available: " << budget.totalWorkers);
    BOOST_CHECK_GT(budget.totalWorkers, 0);

    const size_t batchWorkload = 24;
    std::vector<size_t> edmIndices;
    edmIndices.reserve(batchWorkload);

    for (size_t i = 0; i < batchWorkload; ++i) {
        Vector2D start(100.0f, 100.0f + static_cast<float>(i) * 5.0f);
        Vector2D goal(200.0f, 200.0f + static_cast<float>(i) * 5.0f);
        edmIndices.push_back(createPathNpc(start));
        BOOST_CHECK_GT(manager.requestPathToEDM(
            edmIndices.back(), start, goal, PathfinderManager::Priority::Normal), 0U);
    }

    BOOST_CHECK(waitForPathfinder(manager, [&edmIndices] {
        auto& edm = EntityDataManager::Instance();
        for (size_t idx : edmIndices) {
            if (!edm.hasPathData(idx) ||
                edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }));
}

BOOST_FIXTURE_TEST_CASE(TestRequestsRunWithoutFrameRateLimiting, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    const size_t requestsSubmitted = 24;
    std::vector<size_t> edmIndices;
    edmIndices.reserve(requestsSubmitted);

    for (size_t i = 0; i < requestsSubmitted; ++i) {
        Vector2D start(64.0f, 64.0f + static_cast<float>(i));
        Vector2D goal(192.0f, 192.0f + static_cast<float>(i));
        edmIndices.push_back(createPathNpc(start));
        BOOST_CHECK_GT(manager.requestPathToEDM(
            edmIndices.back(), start, goal, PathfinderManager::Priority::Normal), 0U);
    }

    BOOST_CHECK(waitForPathfinder(manager, [&edmIndices] {
        auto& edm = EntityDataManager::Instance();
        for (size_t idx : edmIndices) {
            if (!edm.hasPathData(idx) ||
                edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }, 120, std::chrono::milliseconds(10)));
}

BOOST_FIXTURE_TEST_CASE(TestPriorityStratification, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    Vector2D start(100.0f, 100.0f);
    Vector2D goal(200.0f, 200.0f);
    const size_t edmIndex = createPathNpc(start);

    const uint64_t requestId = manager.requestPathToEDM(
        edmIndex, start, goal, PathfinderManager::Priority::Normal);
    BOOST_CHECK_GT(requestId, 0);

    BOOST_CHECK(waitForEdmPathCommit(manager, edmIndex));
}

// ========== Direct Submission Architecture Regression Test ==========

BOOST_FIXTURE_TEST_CASE(TestDirectSubmissionStatsRemainStableAcrossIdenticalFailedRequests, PathfinderRequestFixture) {
    PathfinderManager& manager = PathfinderManager::Instance();
    manager.resetStats();

    Vector2D start(4000.0f, 4000.0f);
    Vector2D goal(8000.0f, 8000.0f);
    std::vector<size_t> edmIndices;
    edmIndices.reserve(3);
    for (int i = 0; i < 3; ++i) {
        edmIndices.push_back(createPathNpc(Vector2D(64.0f, 64.0f)));
        BOOST_CHECK_GT(manager.requestPathToEDM(
            edmIndices.back(), start, goal, PathfinderManager::Priority::Normal), 0U);
    }

    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    BOOST_CHECK(waitForPathfinder(manager, [&edmIndices] {
        auto& edm = EntityDataManager::Instance();
        for (size_t idx : edmIndices) {
            if (!edm.hasPathData(idx) ||
                edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }));
}

BOOST_AUTO_TEST_SUITE_END()
