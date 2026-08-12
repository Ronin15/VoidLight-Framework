/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE PathfinderAIContentionTests
#include <boost/test/unit_test.hpp>

#include "managers/PathfinderManager.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "utils/Vector2D.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace VoidLight;

/**
 * PathfinderAIContentionTests
 *
 * Integration tests to verify that PathfinderManager and AIManager
 * can coexist under heavy load without starving each other for
 * ThreadSystem workers.
 *
 * Tests the WorkerBudget coordination between:
 * - PathfinderManager (all workers during its update window)
 * - AIManager (all workers during its update window)
 * Sequential execution model: each manager gets full worker access
 */

// Global ThreadSystem fixture (matching PathfinderManagerTests pattern)
struct ContentionThreadFixture {
    ContentionThreadFixture() {
        if (!ThreadSystem::Instance().init(4096)) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~ContentionThreadFixture() {
        if (!ThreadSystem::Instance().isShutdown()) {
            ThreadSystem::Instance().clean();
        }
    }
};

BOOST_GLOBAL_FIXTURE(ContentionThreadFixture);

BOOST_AUTO_TEST_SUITE(PathfinderAIContentionTestSuite)

struct ContentionFixture {
    ContentionFixture() {
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        PathfinderManager::Instance().resetStats();
        BOOST_REQUIRE(AIManager::Instance().init());
    }

    ~ContentionFixture() {
        for (const auto& handle : m_handles) {
            if (EntityDataManager::Instance().isValidHandle(handle)) {
                EntityDataManager::Instance().destroyEntity(handle);
            }
        }
        EntityDataManager::Instance().processDestructionQueue();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EventManager::Instance().clean();
        EntityDataManager::Instance().clean();
    }

    void createIdleNPCs(size_t count) {
        auto& edm = EntityDataManager::Instance();
        auto& ai = AIManager::Instance();
        for (size_t i = 0; i < count; ++i) {
            EntityHandle handle = edm.createNPCWithRaceClass(
                Vector2D(64.0f + static_cast<float>(i) * 16.0f, 64.0f),
                "Human",
                "Guard");
            ai.assignBehavior(handle, "Idle");
            m_handles.push_back(handle);
        }
    }

    std::vector<EntityHandle> m_handles;
};

BOOST_AUTO_TEST_CASE(TestWorkerBudgetAllocation) {
    auto& threadSystem = ThreadSystem::Instance();
    size_t availableWorkers = threadSystem.getThreadCount();

    BOOST_TEST_MESSAGE("Available workers: " << availableWorkers);

    // Get WorkerBudget from manager
    const auto& budget = WorkerBudgetManager::Instance().getBudget();

    BOOST_TEST_MESSAGE("Worker allocation (sequential execution model):");
    BOOST_TEST_MESSAGE("  Total workers: " << budget.totalWorkers);
    BOOST_TEST_MESSAGE("  Each manager gets ALL workers during its execution window");

    // Verify total workers matches available
    BOOST_CHECK_EQUAL(budget.totalWorkers, availableWorkers);
    BOOST_CHECK_GT(budget.totalWorkers, 0);
}

BOOST_FIXTURE_TEST_CASE(TestSimultaneousAIAndPathfindingLoad, ContentionFixture) {
    createIdleNPCs(32);

    const size_t pathRequests = 100;
    std::atomic<size_t> pathsCompleted{0};

    for (size_t i = 0; i < pathRequests; ++i) {
        Vector2D start(200.0f + i * 5.0f, 200.0f);
        Vector2D goal(800.0f + i * 5.0f, 800.0f);

        PathfinderManager::Instance().requestPath(
            static_cast<EntityID>(2000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&pathsCompleted](EntityID, const std::vector<Vector2D>&) {
                pathsCompleted.fetch_add(1, std::memory_order_relaxed);
            }
        );
    }

    const int numFrames = 10;

    for (int frame = 0; frame < numFrames; ++frame) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 40 && pathsCompleted.load() < pathRequests; ++i) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_TEST_MESSAGE("Completed: " << pathsCompleted.load() << " / " << pathRequests);

    BOOST_CHECK_EQUAL(pathsCompleted.load(), pathRequests);
    BOOST_CHECK(AIManager::Instance().isInitialized());
    BOOST_CHECK(PathfinderManager::Instance().isInitialized());
}

BOOST_FIXTURE_TEST_CASE(TestNoWorkerStarvation, ContentionFixture) {
    createIdleNPCs(64);

    const size_t burstRequests = 200;
    std::atomic<size_t> pathsCompleted{0};

    for (size_t i = 0; i < burstRequests; ++i) {
        Vector2D start(100.0f, 100.0f + i);
        Vector2D goal(500.0f, 500.0f + i);

        PathfinderManager::Instance().requestPath(
            static_cast<EntityID>(3000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&pathsCompleted](EntityID, const std::vector<Vector2D>&) {
                pathsCompleted.fetch_add(1, std::memory_order_relaxed);
            }
        );
    }

    const int stressFrames = 15;

    for (int frame = 0; frame < stressFrames; ++frame) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 60 && pathsCompleted.load() < burstRequests; ++i) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_TEST_MESSAGE("Completed: " << pathsCompleted.load() << " / " << burstRequests);

    BOOST_CHECK_EQUAL(pathsCompleted.load(), burstRequests);
    BOOST_CHECK(AIManager::Instance().isInitialized());
}

BOOST_FIXTURE_TEST_CASE(TestQueuePressureCoordination, ContentionFixture) {
    createIdleNPCs(48);

    const size_t pathRequests = 150;
    std::atomic<size_t> pathsCompleted{0};

    for (size_t i = 0; i < pathRequests; ++i) {
        Vector2D start(150.0f, 150.0f + i * 2.0f);
        Vector2D goal(600.0f, 600.0f + i * 2.0f);

        PathfinderManager::Instance().requestPath(
            static_cast<EntityID>(4000 + i),
            start,
            goal,
            PathfinderManager::Priority::Normal,
            [&pathsCompleted](EntityID, const std::vector<Vector2D>&) {
                pathsCompleted.fetch_add(1, std::memory_order_relaxed);
            }
        );
    }

    for (int frame = 0; frame < 10; ++frame) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 50 && pathsCompleted.load() < pathRequests; ++i) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_TEST_MESSAGE("Completed: " << pathsCompleted.load() << " / " << pathRequests);

    BOOST_CHECK_EQUAL(pathsCompleted.load(), pathRequests);
    const auto stats = PathfinderManager::Instance().getStats();
    BOOST_CHECK_EQUAL(stats.totalRequests, static_cast<uint64_t>(pathRequests));
}

BOOST_AUTO_TEST_SUITE_END()
