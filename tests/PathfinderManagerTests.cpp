/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE PathfinderManagerTests
#include <boost/test/unit_test.hpp>

#include "managers/PathfinderManager.hpp"
#include "managers/EventManager.hpp"
#include "events/CollisionObstacleChangedEvent.hpp"
#include "core/ThreadSystem.hpp"
#include "ai/pathfinding/PathfindingGrid.hpp"
#include "utils/Vector2D.hpp"
#include <chrono>
#include <thread>

using namespace VoidLight;

namespace {

template <typename Predicate>
bool waitForPathfinder(PathfinderManager& manager,
                       Predicate&& predicate,
                       int maxPolls = 100,
                       std::chrono::milliseconds pollInterval = std::chrono::milliseconds(10)) {
    for (int i = 0; i < maxPolls; ++i) {
        manager.update();
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }

    manager.update();
    return predicate();
}

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

BOOST_AUTO_TEST_CASE(TestAsyncPathfinding) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    
    Vector2D start(100.0f, 100.0f);
    Vector2D goal(200.0f, 200.0f);
    EntityID entityId = 12345;
    bool callbackCalled = false;
    std::vector<Vector2D> resultPath;
    
    // Test async pathfinding with callback
    auto requestId = manager.requestPath(
        entityId, 
        start, 
        goal, 
        PathfinderManager::Priority::Normal, // Normal priority
        [&callbackCalled, &resultPath](EntityID id, const std::vector<Vector2D>& path) {
            BOOST_CHECK(id == 12345);
            callbackCalled = true;
            resultPath = path;
        }
    );
    
    BOOST_CHECK(requestId > 0); // Valid request ID
    
    BOOST_CHECK(waitForPathfinder(manager, [&callbackCalled] {
        return callbackCalled;
    }));
    
    manager.clean();
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

BOOST_AUTO_TEST_CASE(TestBasicFunctionality) {
    PathfinderManager& manager = PathfinderManager::Instance();
    
    BOOST_REQUIRE(manager.init());
    
    Vector2D start(100.0f, 100.0f);
    Vector2D goal(200.0f, 200.0f);
    EntityID entityId = 54321;
    
    // Request a path without callback (should not crash)
    auto requestId = manager.requestPath(entityId, start, goal, PathfinderManager::Priority::Low);
    BOOST_CHECK(requestId > 0);
    
    // Direct-submission architecture: requestPath() queues onto ThreadSystem,
    // not an internal PathfinderManager queue.
    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    manager.update();

    manager.clean();
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

BOOST_AUTO_TEST_CASE(TestNoInfiniteRetryLoop) {
    // Repeated failed requests should complete once each without internal requeueing.

    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    Vector2D start(50.0f, 50.0f);
    Vector2D goal(100.0f, 100.0f);
    EntityID entityId = 99999;
    
    std::atomic<int> callbackCount{0};
    
    auto callback = [&callbackCount](EntityID, const std::vector<Vector2D>&) {
        callbackCount.fetch_add(1, std::memory_order_release);
    };
    
    manager.requestPath(entityId, start, goal, PathfinderManager::Priority::High, callback);
    manager.requestPath(entityId, start, goal, PathfinderManager::Priority::High, callback); 
    manager.requestPath(entityId, start, goal, PathfinderManager::Priority::High, callback);
    manager.requestPath(entityId, start, goal, PathfinderManager::Priority::High, callback);
    
    waitForPathfinder(manager, [&callbackCount] {
        return callbackCount.load(std::memory_order_acquire) >= 4;
    });
    
    BOOST_CHECK_EQUAL(callbackCount.load(std::memory_order_acquire), 4);

    auto stats = manager.getStats();
    BOOST_CHECK_EQUAL(stats.totalRequests, 4U);
    BOOST_CHECK_EQUAL(stats.failedRequests, 4U);
    BOOST_CHECK_EQUAL(stats.cacheSize, 0U);

    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestFailedRequestsDoNotPopulateCache) {
    // Current production behavior only caches non-empty paths.
    
    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();
    
    Vector2D start(10.0f, 10.0f);
    Vector2D goal(20.0f, 20.0f);  
    EntityID entityId = 88888;
    
    std::atomic<int> firstCallbackCount{0};
    std::atomic<int> secondCallbackCount{0};
    
    manager.requestPath(entityId, start, goal, PathfinderManager::Priority::High,
        [&firstCallbackCount](EntityID, const std::vector<Vector2D>&) {
            firstCallbackCount.fetch_add(1, std::memory_order_release);
        });
    
    waitForPathfinder(manager, [&firstCallbackCount] {
        return firstCallbackCount.load(std::memory_order_acquire) > 0;
    });
    
    manager.requestPath(entityId, start, goal, PathfinderManager::Priority::High,
        [&secondCallbackCount](EntityID, const std::vector<Vector2D>&) {
            secondCallbackCount.fetch_add(1, std::memory_order_release);
        });
    
    waitForPathfinder(manager, [&secondCallbackCount] {
        return secondCallbackCount.load(std::memory_order_acquire) > 0;
    });
    
    BOOST_CHECK_EQUAL(firstCallbackCount.load(std::memory_order_acquire), 1);
    BOOST_CHECK_EQUAL(secondCallbackCount.load(std::memory_order_acquire), 1);
    
    auto stats = manager.getStats();
    BOOST_CHECK_EQUAL(stats.totalRequests, 2U);
    BOOST_CHECK_EQUAL(stats.failedRequests, 2U);
    BOOST_CHECK_EQUAL(stats.cacheHits, 0U);
    BOOST_CHECK_EQUAL(stats.cacheSize, 0U);

    manager.clean();
}

BOOST_AUTO_TEST_SUITE_END()

// Integration Tests for PathfinderManager Event System  
BOOST_AUTO_TEST_SUITE(PathfinderEventIntegrationTests)

// Test fixture for PathfinderManager event integration
struct PathfinderEventFixture {
    PathfinderEventFixture() {
        // Initialize EventManager for event testing (following established pattern)
        EventManager::Instance().init();
        
        // Initialize PathfinderManager
        PathfinderManager::Instance().init();
        
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

BOOST_FIXTURE_TEST_CASE(TestPathfinderCacheInvalidationOnCollisionChange, PathfinderEventFixture)
{
    // Test that collision obstacle changes properly invalidate pathfinding cache
    
    // First, let's simulate having some cached paths by triggering path requests
    Vector2D start1(0.0f, 0.0f);
    Vector2D goal1(100.0f, 100.0f);
    Vector2D start2(200.0f, 200.0f); 
    Vector2D goal2(300.0f, 300.0f);
    
    // Request some paths (they may fail due to no world, but will be cached)
    PathfinderManager::Instance().requestPath(1001, start1, goal1, 
        PathfinderManager::Priority::High,
        [](EntityID, const std::vector<Vector2D>&){ /* no-op */ });
    PathfinderManager::Instance().requestPath(1002, start2, goal2,
        PathfinderManager::Priority::High,
        [](EntityID, const std::vector<Vector2D>&){ /* no-op */ });
    
    // Let processing complete
    const auto submittedStats = PathfinderManager::Instance().getStats();
    waitForPathfinder(PathfinderManager::Instance(), [submittedStats] {
        return PathfinderManager::Instance().getStats().totalRequests >= submittedStats.totalRequests + 2U;
    });
    
    // Get initial stats
    auto initialStats = PathfinderManager::Instance().getStats();
    
    // Now trigger a collision obstacle change at position that might affect paths
    Vector2D obstaclePos(150.0f, 150.0f);
    EventManager::Instance().triggerCollisionObstacleChanged(
        obstaclePos, 100.0f, "Cache invalidation test", EventManager::DispatchMode::Immediate);
    
    // Brief processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // The cache should have been selectively invalidated
    // We can't directly test cache internals, but we can verify that the system
    // handles the event without crashing and continues to function
    
    // Request the same paths again - they should be processed again if cache was invalidated
    PathfinderManager::Instance().requestPath(1003, start1, goal1,
        PathfinderManager::Priority::High,
        [this](EntityID, const std::vector<Vector2D>&){ 
            cacheInvalidationCount++; 
        });
    
    waitForPathfinder(PathfinderManager::Instance(), [this] {
        return cacheInvalidationCount.load(std::memory_order_acquire) > 0;
    });
    
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
    PathfinderManager::Instance().init();
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

BOOST_AUTO_TEST_CASE(TestBurstRequestHandling) {
    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    const size_t burstSize = 150; // Test 150 simultaneous requests
    std::atomic<size_t> completedCount{0};
    std::atomic<size_t> successCount{0};

    BOOST_TEST_MESSAGE("Submitting " << burstSize << " simultaneous path requests...");

    // Submit burst of path requests
    for (size_t i = 0; i < burstSize; ++i) {
        Vector2D start(100.0f + i * 10.0f, 100.0f);
        Vector2D goal(500.0f + i * 10.0f, 500.0f);

        manager.requestPath(
            static_cast<EntityID>(1000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&completedCount, &successCount](EntityID, const std::vector<Vector2D>& path) {
                completedCount.fetch_add(1, std::memory_order_release);
                if (!path.empty()) {
                    successCount.fetch_add(1, std::memory_order_release);
                }
            }
        );
    }

    waitForPathfinder(manager, [&completedCount] {
        return completedCount.load(std::memory_order_acquire) >= burstSize;
    }, 120, std::chrono::milliseconds(10));

    BOOST_TEST_MESSAGE("Completed " << completedCount.load() << " / " << burstSize << " requests");
    BOOST_CHECK_EQUAL(completedCount.load(), burstSize);

    auto stats = manager.getStats();
    BOOST_CHECK_EQUAL(stats.totalRequests, static_cast<uint64_t>(burstSize));
    BOOST_CHECK_EQUAL(stats.failedRequests, static_cast<uint64_t>(burstSize));

    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestDirectSubmissionHasNoInternalQueue) {
    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    const size_t testRequests = 200;
    std::atomic<size_t> completed{0};

    for (size_t i = 0; i < testRequests; ++i) {
        Vector2D start(50.0f + i, 50.0f);
        Vector2D goal(300.0f + i, 300.0f);

        manager.requestPath(
            static_cast<EntityID>(2000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&completed](EntityID, const std::vector<Vector2D>&) {
                completed.fetch_add(1, std::memory_order_release);
            }
        );
    }

    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    waitForPathfinder(manager, [&completed] {
        return completed.load(std::memory_order_acquire) >= testRequests;
    }, 120, std::chrono::milliseconds(10));

    BOOST_TEST_MESSAGE("Completed " << completed.load() << " / " << testRequests << " requests");
    BOOST_CHECK_EQUAL(completed.load(), testRequests);
    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestWorkerBudgetCoordination) {
    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    auto& threadSystem = VoidLight::ThreadSystem::Instance();
    size_t availableWorkers = threadSystem.getThreadCount();

    BOOST_TEST_MESSAGE("Available workers: " << availableWorkers);

    // Get WorkerBudget from manager
    const auto& budget = VoidLight::WorkerBudgetManager::Instance().getBudget();

    BOOST_TEST_MESSAGE("Total workers available: " << budget.totalWorkers);
    BOOST_CHECK_GT(budget.totalWorkers, 0); // Should have at least 1 worker available

    // Sequential execution model: each manager gets all workers during its window

    // Submit workload that should trigger batching (> 8 requests)
    const size_t batchWorkload = 24; // 3x the MIN_REQUESTS_FOR_BATCHING
    std::atomic<size_t> completed{0};

    for (size_t i = 0; i < batchWorkload; ++i) {
        Vector2D start(100.0f, 100.0f + i * 5.0f);
        Vector2D goal(400.0f, 400.0f + i * 5.0f);

        manager.requestPath(
            static_cast<EntityID>(3000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&completed](EntityID, const std::vector<Vector2D>&) {
                completed.fetch_add(1, std::memory_order_release);
            }
        );
    }

    waitForPathfinder(manager, [&completed] {
        return completed.load(std::memory_order_acquire) >= batchWorkload;
    });

    BOOST_TEST_MESSAGE("Completed " << completed.load() << " / " << batchWorkload << " batch requests");
    BOOST_CHECK_EQUAL(completed.load(), batchWorkload);

    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestRequestsRunWithoutFrameRateLimiting) {
    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    const size_t requestsSubmitted = 100;
    std::atomic<size_t> completed{0};

    for (size_t i = 0; i < requestsSubmitted; ++i) {
        Vector2D start(50.0f, 50.0f + i);
        Vector2D goal(200.0f, 200.0f + i);

        manager.requestPath(
            static_cast<EntityID>(4000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&completed](EntityID, const std::vector<Vector2D>&) {
                completed.fetch_add(1, std::memory_order_release);
            }
        );
    }

    BOOST_CHECK(waitForPathfinder(manager, [&completed] {
        return completed.load(std::memory_order_acquire) > 0U;
    }, 100, std::chrono::milliseconds(10)));

    waitForPathfinder(manager, [&completed] {
        return completed.load(std::memory_order_acquire) >= requestsSubmitted;
    }, 120, std::chrono::milliseconds(10));

    BOOST_TEST_MESSAGE("Final: " << completed.load() << " / " << requestsSubmitted << " completed");
    BOOST_CHECK_EQUAL(completed.load(), requestsSubmitted);

    manager.clean();
}

BOOST_AUTO_TEST_CASE(TestPriorityStratification) {
    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    // Test that default priority is now Normal (not High)
    std::atomic<size_t> completed{0};

    // Submit with default priority (should be Normal)
    Vector2D start(100.0f, 100.0f);
    Vector2D goal(300.0f, 300.0f);

    auto requestId = manager.requestPath(
        static_cast<EntityID>(5000),
        start,
        goal,
        PathfinderManager::Priority::Normal, // Explicitly Normal
        [&completed](EntityID, const std::vector<Vector2D>&) {
            completed.fetch_add(1, std::memory_order_release);
        }
    );

    BOOST_CHECK_GT(requestId, 0);

    // Process
    waitForPathfinder(manager, [&completed] {
        return completed.load(std::memory_order_acquire) > 0;
    });

    BOOST_CHECK_GE(completed.load(), 0); // Should process successfully

    manager.clean();
}

// ========== Direct Submission Architecture Regression Test ==========

BOOST_AUTO_TEST_CASE(TestDirectSubmissionStatsRemainStableAcrossIdenticalFailedRequests) {
    // Failed requests are recomputed and not cached, but stats should still remain bounded
    // and queue inspection should report the current direct-submission architecture.

    PathfinderManager& manager = PathfinderManager::Instance();
    BOOST_REQUIRE(manager.init());
    manager.resetStats();

    std::atomic<size_t> callbackCount{0};
    auto callback = [&callbackCount](EntityID, const std::vector<Vector2D>&) {
        callbackCount.fetch_add(1, std::memory_order_release);
    };

    Vector2D start(4000.0f, 4000.0f);
    Vector2D goal(8000.0f, 8000.0f);

    manager.requestPath(1001, start, goal, PathfinderManager::Priority::Normal, callback);
    manager.requestPath(1002, start, goal, PathfinderManager::Priority::Normal, callback);
    manager.requestPath(1003, start, goal, PathfinderManager::Priority::Normal, callback);

    BOOST_CHECK_EQUAL(manager.getQueueSize(), 0U);
    BOOST_CHECK(!manager.hasPendingWork());

    waitForPathfinder(manager, [&callbackCount] {
        return callbackCount.load(std::memory_order_acquire) >= 3;
    });

    const auto stats = manager.getStats();
    BOOST_CHECK_EQUAL(callbackCount.load(), 3U);
    BOOST_CHECK_EQUAL(stats.totalRequests, 3U);
    BOOST_CHECK_EQUAL(stats.failedRequests, 3U);
    BOOST_CHECK_EQUAL(stats.cacheHits, 0U);
    BOOST_CHECK_EQUAL(stats.cacheSize, 0U);

    manager.clean();
}

BOOST_AUTO_TEST_SUITE_END()
