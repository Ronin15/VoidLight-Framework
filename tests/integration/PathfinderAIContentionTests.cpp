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
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "utils/Vector2D.hpp"
#include "world/WorldData.hpp"
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
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(WorldManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        PathfinderManager::Instance().resetStats();
        BOOST_REQUIRE(AIManager::Instance().init());

        WorldGenerationConfig worldConfig{};
        worldConfig.width = 20;
        worldConfig.height = 20;
        worldConfig.seed = 20260305;
        worldConfig.elevationFrequency = 0.1f;
        worldConfig.humidityFrequency = 0.1f;
        worldConfig.waterLevel = 0.3f;
        worldConfig.mountainLevel = 0.7f;
        BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(worldConfig));
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EventManager::Instance().update();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
        while (std::chrono::steady_clock::now() < deadline &&
               !PathfinderManager::Instance().isGridReady()) {
            PathfinderManager::Instance().update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        BOOST_REQUIRE(PathfinderManager::Instance().isGridReady());
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
        WorldManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        WorldResourceManager::Instance().clean();
        EventManager::Instance().clean();
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

    void requestPathsOnNpcs(const Vector2D& startBase, const Vector2D& goalBase, float startStep) {
        auto& edm = EntityDataManager::Instance();
        auto& pm = PathfinderManager::Instance();
        for (size_t i = 0; i < m_handles.size(); ++i) {
            const size_t idx = edm.getIndex(m_handles[i]);
            BOOST_REQUIRE(idx != SIZE_MAX);
            Vector2D start(startBase.getX() + static_cast<float>(i) * startStep, startBase.getY());
            Vector2D goal(goalBase.getX() + static_cast<float>(i) * startStep, goalBase.getY());
            BOOST_CHECK_GT(pm.requestPathToEDM(
                idx, start, goal, PathfinderManager::Priority::Normal), 0U);
        }
    }

    bool allNpcPathsCommitted() const {
        auto& edm = EntityDataManager::Instance();
        for (const auto& handle : m_handles) {
            const size_t idx = edm.getIndex(handle);
            if (idx == SIZE_MAX || !edm.hasPathData(idx)) {
                return false;
            }
            if (edm.getPathData(idx).pathRequestPending.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }
        return true;
    }
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
    requestPathsOnNpcs(Vector2D(200.0f, 200.0f), Vector2D(400.0f, 400.0f), 5.0f);

    const int numFrames = 10;
    for (int frame = 0; frame < numFrames; ++frame) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        PathfinderManager::Instance().commitCompletedPaths();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 40 && !allNpcPathsCommitted(); ++i) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        PathfinderManager::Instance().commitCompletedPaths();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_CHECK(allNpcPathsCommitted());
    BOOST_CHECK(AIManager::Instance().isInitialized());
    BOOST_CHECK(PathfinderManager::Instance().isInitialized());
}

BOOST_FIXTURE_TEST_CASE(TestNoWorkerStarvation, ContentionFixture) {
    createIdleNPCs(64);
    requestPathsOnNpcs(Vector2D(100.0f, 100.0f), Vector2D(300.0f, 300.0f), 1.0f);

    const int stressFrames = 15;
    for (int frame = 0; frame < stressFrames; ++frame) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        PathfinderManager::Instance().commitCompletedPaths();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 60 && !allNpcPathsCommitted(); ++i) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        PathfinderManager::Instance().commitCompletedPaths();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_CHECK(allNpcPathsCommitted());
    BOOST_CHECK(AIManager::Instance().isInitialized());
}

BOOST_FIXTURE_TEST_CASE(TestQueuePressureCoordination, ContentionFixture) {
    createIdleNPCs(48);
    requestPathsOnNpcs(Vector2D(150.0f, 150.0f), Vector2D(350.0f, 350.0f), 2.0f);

    for (int frame = 0; frame < 10; ++frame) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        PathfinderManager::Instance().commitCompletedPaths();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 50 && !allNpcPathsCommitted(); ++i) {
        AIManager::Instance().update(0.016f);
        PathfinderManager::Instance().update();
        PathfinderManager::Instance().commitCompletedPaths();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_CHECK(allNpcPathsCommitted());
}

BOOST_AUTO_TEST_SUITE_END()
