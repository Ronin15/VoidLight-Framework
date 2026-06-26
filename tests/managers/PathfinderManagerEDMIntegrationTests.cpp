/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file PathfinderManagerEDMIntegrationTests.cpp
 * @brief Tests for PathfinderManager's integration with EntityDataManager
 *
 * These tests verify PathfinderManager-specific EDM integration:
 * - requestPathToEDM() EDM path data access
 * - Path data lifecycle with entity creation/destruction
 *
 * NOTE: State transition tests are covered in EntityDataManagerTests.cpp
 * and CollisionManagerEDMIntegrationTests.cpp. This file focuses on
 * PathfinderManager-specific EDM data flow.
 */

#define BOOST_TEST_MODULE PathfinderManagerEDMIntegrationTests
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace VoidLight;

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for Pathfinder EDM tests");
        }
    }
    ~ThreadSystemFixture() {
        ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

namespace {

bool ensureActiveWorldForPathfindingTests() {
    auto& worldResMgr = WorldResourceManager::Instance();
    if (!worldResMgr.isInitialized()) {
        if (!worldResMgr.init()) { return false; }
    }

    auto& resourceMgr = ResourceTemplateManager::Instance();
    if (!resourceMgr.isInitialized()) {
        if (!resourceMgr.init()) { return false; }
    }

    auto& worldMgr = WorldManager::Instance();
    if (!worldMgr.isInitialized()) {
        if (!worldMgr.init()) {
            return false;
        }
    }

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

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return worldMgr.hasActiveWorld();
}

bool waitForGridReady(PathfinderManager& pm, int maxWaitMs = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxWaitMs);
    while (std::chrono::steady_clock::now() < deadline) {
        pm.update();
        if (pm.isGridReady()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return pm.isGridReady();
}

} // namespace

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class PathfindingTestNPC {
public:
    explicit PathfindingTestNPC(const Vector2D& pos) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
    }

    static std::shared_ptr<PathfindingTestNPC> create(const Vector2D& pos) {
        return std::make_shared<PathfindingTestNPC>(pos);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

    [[nodiscard]] size_t getEdmIndex() const {
        if (!m_handle.isValid()) return SIZE_MAX;
        return EntityDataManager::Instance().getIndex(m_handle);
    }

private:
    EntityHandle m_handle;
};

// Test fixture
struct PathfinderEDMFixture {
    PathfinderEDMFixture() {
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(WorldManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());
        BOOST_REQUIRE(ensureActiveWorldForPathfindingTests());
    }

    ~PathfinderEDMFixture() {
        BackgroundSimulationManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        WorldManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        WorldResourceManager::Instance().clean();
        EntityDataManager::Instance().clean();
    }

    void waitForPathCompletion(int maxWaitMs = 100) {
        auto& pm = PathfinderManager::Instance();
        int waited = 0;
        while (waited < maxWaitMs) {
            pm.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waited += 10;
        }
    }
};

// ============================================================================
// PATH DATA EXISTENCE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PathDataExistenceTests, PathfinderEDMFixture)

BOOST_AUTO_TEST_CASE(TestPathDataExistsForNewEntity) {
    auto entity = PathfindingTestNPC::create(Vector2D(100.0f, 100.0f));
    size_t edmIndex = entity->getEdmIndex();
    BOOST_REQUIRE(edmIndex != SIZE_MAX);

    auto& edm = EntityDataManager::Instance();
    BOOST_CHECK(edm.hasPathData(edmIndex));
}

BOOST_AUTO_TEST_CASE(TestPathDataAccessible) {
    auto entity = PathfindingTestNPC::create(Vector2D(100.0f, 100.0f));
    size_t edmIndex = entity->getEdmIndex();
    BOOST_REQUIRE(edmIndex != SIZE_MAX);

    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.hasPathData(edmIndex));

    // Access path data - should not crash
    auto& pathData = edm.getPathData(edmIndex);
    [[maybe_unused]] bool hasPath = pathData.hasPath;
    [[maybe_unused]] size_t navIndex = pathData.navIndex;
}

BOOST_AUTO_TEST_CASE(TestMultipleEntitiesHavePathData) {
    auto& edm = EntityDataManager::Instance();

    std::vector<std::shared_ptr<PathfindingTestNPC>> entities;
    std::vector<size_t> edmIndices;

    for (int i = 0; i < 20; ++i) {
        auto pos = Vector2D(static_cast<float>(i * 50), 0.0f);
        entities.push_back(PathfindingTestNPC::create(pos));
        edmIndices.push_back(entities.back()->getEdmIndex());
    }

    // All should have path data
    for (size_t idx : edmIndices) {
        BOOST_CHECK(idx != SIZE_MAX);
        BOOST_CHECK(edm.hasPathData(idx));
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// REQUEST PATH TO EDM TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(RequestPathToEDMTests, PathfinderEDMFixture)

BOOST_AUTO_TEST_CASE(TestRequestPathToEDMDoesNotCrash) {
    auto entity = PathfindingTestNPC::create(Vector2D(100.0f, 100.0f));
    size_t edmIndex = entity->getEdmIndex();
    BOOST_REQUIRE(edmIndex != SIZE_MAX);

    auto& pm = PathfinderManager::Instance();

    // Request path (may return 0 without grid, but shouldn't crash)
    [[maybe_unused]] uint64_t requestId = pm.requestPathToEDM(edmIndex,
        Vector2D(100.0f, 100.0f), Vector2D(500.0f, 500.0f),
        PathfinderManager::Priority::Normal);

    waitForPathCompletion();

    // Path data should still be accessible
    auto& edm = EntityDataManager::Instance();
    BOOST_CHECK(edm.hasPathData(edmIndex));
}

BOOST_AUTO_TEST_CASE(TestMultiplePathRequestsDoNotCrash) {
    auto entity = PathfindingTestNPC::create(Vector2D(100.0f, 100.0f));
    size_t edmIndex = entity->getEdmIndex();
    BOOST_REQUIRE(edmIndex != SIZE_MAX);

    auto& pm = PathfinderManager::Instance();

    // Multiple requests shouldn't crash
    for (int i = 0; i < 5; ++i) {
        Vector2D goal(200.0f + i * 100.0f, 200.0f + i * 100.0f);
        pm.requestPathToEDM(edmIndex, Vector2D(100.0f, 100.0f), goal,
            PathfinderManager::Priority::Normal);
    }

    waitForPathCompletion();

    auto& edm = EntityDataManager::Instance();
    BOOST_CHECK(edm.hasPathData(edmIndex));
}

BOOST_AUTO_TEST_CASE(TestRequestPathWithInvalidIndex) {
    auto& pm = PathfinderManager::Instance();
    pm.resetStats();

    // Request with invalid index - should handle gracefully
    const uint64_t requestId = pm.requestPathToEDM(SIZE_MAX,
        Vector2D(0, 0), Vector2D(100, 100), PathfinderManager::Priority::Normal);

    waitForPathCompletion();
    const auto stats = pm.getStats();
    BOOST_CHECK_EQUAL(requestId, 0U);
    BOOST_CHECK_EQUAL(stats.totalRequests, 0U);
    BOOST_CHECK_EQUAL(stats.failedRequests, 0U);
    BOOST_CHECK_EQUAL(stats.completedRequests, 0U);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ENTITY DESTRUCTION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EntityDestructionTests, PathfinderEDMFixture)

BOOST_AUTO_TEST_CASE(TestPathDataInvalidAfterEntityDestruction) {
    auto& edm = EntityDataManager::Instance();

    auto entity = PathfindingTestNPC::create(Vector2D(100.0f, 100.0f));
    EntityHandle handle = entity->getHandle();
    size_t edmIndex = entity->getEdmIndex();
    BOOST_REQUIRE(edmIndex != SIZE_MAX);
    BOOST_CHECK(edm.hasPathData(edmIndex));

    // Destroy entity
    edm.destroyEntity(handle);
    edm.processDestructionQueue();

    // Handle should be invalid
    BOOST_CHECK(!edm.isValidHandle(handle));
}

BOOST_AUTO_TEST_CASE(TestPathRequestAfterStateTransition) {
    auto& edm = EntityDataManager::Instance();
    auto& pm = PathfinderManager::Instance();

    // Phase 1: Create entities
    {
        std::vector<std::shared_ptr<PathfindingTestNPC>> entities;
        for (int i = 0; i < 10; ++i) {
            entities.push_back(PathfindingTestNPC::create(
                Vector2D(static_cast<float>(i * 50), 0.0f)));
        }

        // State transition
        pm.prepareForStateTransition();
        edm.prepareForStateTransition();
        entities.clear();
    }

    // Phase 2: New entities should work
    auto entity = PathfindingTestNPC::create(Vector2D(100.0f, 100.0f));
    size_t edmIndex = entity->getEdmIndex();
    BOOST_REQUIRE(edmIndex != SIZE_MAX);
    BOOST_CHECK(edm.hasPathData(edmIndex));

    // Path request should work
    pm.requestPathToEDM(edmIndex, Vector2D(100.0f, 100.0f),
        Vector2D(500.0f, 500.0f), PathfinderManager::Priority::Normal);

    waitForPathCompletion();

    BOOST_CHECK(edm.hasPathData(edmIndex));
}

BOOST_AUTO_TEST_CASE(StaleCompletionFromReusedSlotDoesNotOverwriteNewEntityPath) {
    auto& edm = EntityDataManager::Instance();
    auto& pm = PathfinderManager::Instance();
    auto& cm = CollisionManager::Instance();

    BOOST_REQUIRE_MESSAGE(ensureActiveWorldForPathfindingTests(),
                          "Expected active world for grid rebuild");
    cm.setWorldBounds(0.0f, 0.0f, 2048.0f, 2048.0f);
    pm.rebuildGrid(false);

    const bool gridReady = waitForGridReady(pm);
    BOOST_REQUIRE_MESSAGE(gridReady, "Expected grid rebuild to complete for stale completion test");

    auto original = PathfindingTestNPC::create(Vector2D(64.0f, 64.0f));
    EntityHandle staleHandle = original->getHandle();
    const size_t reusedIndex = edm.getIndex(staleHandle);
    BOOST_REQUIRE(reusedIndex != SIZE_MAX);

    // Older request on original occupant (token=1 for this slot).
    const uint64_t oldReq = pm.requestPathToEDM(
        reusedIndex, Vector2D(64.0f, 64.0f), Vector2D(1900.0f, 1900.0f),
        PathfinderManager::Priority::Normal);
    BOOST_REQUIRE(oldReq > 0);

    // Destroy original occupant before completion commit.
    edm.destroyEntity(staleHandle);
    edm.processDestructionQueue();
    BOOST_REQUIRE(edm.getIndex(staleHandle) == SIZE_MAX);

    // Reuse same slot for new entity and issue a new request (token resets and
    // starts at 1 again on slot reuse, so handle-generation validation matters).
    std::vector<std::shared_ptr<PathfindingTestNPC>> keepAlive;
    keepAlive.reserve(12);
    EntityHandle currentHandle{};
    bool slotReused = false;
    for (int i = 0; i < 12; ++i) {
        auto spawned = PathfindingTestNPC::create(Vector2D(96.0f + i * 8.0f, 96.0f));
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

    const Vector2D expectedGoal(160.0f, 160.0f);
    const uint64_t newReq = pm.requestPathToEDM(
        reusedIndex, Vector2D(96.0f, 96.0f), expectedGoal, PathfinderManager::Priority::High);
    BOOST_REQUIRE(newReq > 0);

    auto& pd = edm.getPathData(reusedIndex);
    for (int i = 0; i < 200 && pd.pathRequestPending.load(std::memory_order_acquire) != 0; ++i) {
        pm.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_CHECK_EQUAL(pd.pathRequestPending.load(std::memory_order_acquire), 0);
    BOOST_REQUIRE(pd.hasPath);
    BOOST_REQUIRE(pd.pathLength > 0);

    auto waypoints = edm.getWaypointSlot(reusedIndex);
    const Vector2D finalWaypoint = waypoints[pd.pathLength - 1];
    const float distToExpected = (finalWaypoint - expectedGoal).length();
    const float distToOldGoal = (finalWaypoint - Vector2D(1900.0f, 1900.0f)).length();
    BOOST_CHECK_LT(distToExpected, distToOldGoal);
}

BOOST_AUTO_TEST_CASE(StaleCompletionFilteringStressLoop) {
    auto& edm = EntityDataManager::Instance();
    auto& pm = PathfinderManager::Instance();
    auto& cm = CollisionManager::Instance();

    BOOST_REQUIRE_MESSAGE(ensureActiveWorldForPathfindingTests(),
                          "Expected active world for grid rebuild");
    cm.setWorldBounds(0.0f, 0.0f, 2048.0f, 2048.0f);
    pm.rebuildGrid(false);

    const bool gridReady = waitForGridReady(pm);
    BOOST_REQUIRE_MESSAGE(gridReady, "Expected grid rebuild to complete for stale completion stress loop");

    for (int iter = 0; iter < 5; ++iter) {
        auto original = PathfindingTestNPC::create(Vector2D(64.0f + iter * 8.0f, 64.0f));
        EntityHandle staleHandle = original->getHandle();
        const size_t reusedIndex = edm.getIndex(staleHandle);
        BOOST_REQUIRE(reusedIndex != SIZE_MAX);

        const uint64_t oldReq = pm.requestPathToEDM(
            reusedIndex, Vector2D(64.0f, 64.0f), Vector2D(1900.0f, 1900.0f),
            PathfinderManager::Priority::Normal);
        BOOST_REQUIRE(oldReq > 0);

        edm.destroyEntity(staleHandle);
        edm.processDestructionQueue();
        BOOST_REQUIRE(edm.getIndex(staleHandle) == SIZE_MAX);

        std::vector<std::shared_ptr<PathfindingTestNPC>> keepAlive;
        keepAlive.reserve(20);
        EntityHandle currentHandle{};
        bool slotReused = false;
        for (int i = 0; i < 20; ++i) {
            auto spawned = PathfindingTestNPC::create(
                Vector2D(96.0f + i * 8.0f, 96.0f + iter * 5.0f));
            keepAlive.push_back(spawned);
            if (edm.getIndex(spawned->getHandle()) == reusedIndex) {
                currentHandle = spawned->getHandle();
                slotReused = true;
                break;
            }
        }
        BOOST_REQUIRE(slotReused);

        const Vector2D expectedGoal(160.0f + iter * 10.0f, 160.0f + iter * 10.0f);
        const uint64_t newReq = pm.requestPathToEDM(
            reusedIndex, Vector2D(96.0f, 96.0f), expectedGoal, PathfinderManager::Priority::High);
        BOOST_REQUIRE(newReq > 0);

        auto& pd = edm.getPathData(reusedIndex);
        for (int i = 0; i < 200 && pd.pathRequestPending.load(std::memory_order_acquire) != 0; ++i) {
            pm.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        BOOST_CHECK_EQUAL(pd.pathRequestPending.load(std::memory_order_acquire), 0);
        BOOST_REQUIRE(pd.hasPath);
        BOOST_REQUIRE(pd.pathLength > 0);

        auto waypoints = edm.getWaypointSlot(reusedIndex);
        const Vector2D finalWaypoint = waypoints[pd.pathLength - 1];
        const float distToExpected = (finalWaypoint - expectedGoal).length();
        const float distToOldGoal = (finalWaypoint - Vector2D(1900.0f, 1900.0f)).length();
        BOOST_CHECK_LT(distToExpected, distToOldGoal);
    }
}

BOOST_AUTO_TEST_SUITE_END()
