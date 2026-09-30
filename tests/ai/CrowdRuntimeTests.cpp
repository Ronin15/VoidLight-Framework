/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE CrowdRuntimeTests
#include <boost/test/unit_test.hpp>

#include "ai/internal/Crowd.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "utils/Vector2D.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

using namespace VoidLight;

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for Crowd tests");
        }
    }
    ~ThreadSystemFixture() {
        ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

namespace {

// Crowd queries read AIManager's active-tier scan, so the fixture initializes
// the AIManager path (its init requires Collision and Pathfinder managers).
struct CrowdRuntimeFixture {
    CrowdRuntimeFixture() {
        BOOST_REQUIRE(GameTimeManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(AIManager::Instance().init());
        BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());
        BackgroundSimulationManager::Instance().setActiveRadius(2000.0f);

        VOIDLIGHT_STATS_ONLY(AIInternal::ResetCrowdStats();)
    }

    ~CrowdRuntimeFixture() {
        BackgroundSimulationManager::Instance().clean();
        AIManager::Instance().clean();
        EventManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
    }

    size_t createNPC(const Vector2D& pos) {
        auto& edm = EntityDataManager::Instance();
        EntityHandle handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
        size_t index = edm.getIndex(handle);
        BOOST_REQUIRE(index != SIZE_MAX);
        return index;
    }

    // Activates entities near referencePoint and runs one AIManager update
    // (dt 0: nothing moves). The update builds the active index buffer Crowd
    // reads and starts a new crowd-cache frame; stats are cleared after it.
    void runAIFrame(const Vector2D& referencePoint) {
        BackgroundSimulationManager::Instance().update(referencePoint, 0.016f);
        BOOST_REQUIRE(!EntityDataManager::Instance().getActiveIndices().empty());
        AIManager::Instance().update(0.0f);
        VOIDLIGHT_STATS_ONLY(AIInternal::ResetCrowdStats();)
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(CrowdQueryTests, CrowdRuntimeFixture)

BOOST_AUTO_TEST_CASE(TestEmptyQueriesReuseCacheAndUpdateStats) {
    createNPC(Vector2D(100.0f, 100.0f));
    runAIFrame(Vector2D(100.0f, 100.0f));

    const size_t excludeEdmIndex = SIZE_MAX;
    const Vector2D center(250.0f, 250.0f);
    const float radius = 64.0f;

    const int nearbyCount =
        AIInternal::CountNearbyEntities(excludeEdmIndex, center, radius);
    BOOST_CHECK_EQUAL(nearbyCount, 0);

    auto& reusableBuffer = AIInternal::GetNearbyPositionBuffer();
    reusableBuffer.clear();
    reusableBuffer.push_back(Vector2D(1.0f, 2.0f));
    std::vector<Vector2D> positions;
    const int positionCount = AIInternal::GetNearbyEntitiesWithPositions(
        excludeEdmIndex, center, radius, positions);
    BOOST_CHECK_EQUAL(positionCount, 0);
    BOOST_CHECK(positions.empty());
    BOOST_REQUIRE_EQUAL(reusableBuffer.size(), 1u);

    VOIDLIGHT_STATS_ONLY(
        const auto stats = AIInternal::GetCrowdStats();
        BOOST_CHECK_EQUAL(stats.queryCount, 2u);
        BOOST_CHECK_EQUAL(stats.cacheMisses, 1u);
        BOOST_CHECK_EQUAL(stats.cacheHits, 1u);
        BOOST_CHECK_EQUAL(stats.resultsCount, 0u);)
}

BOOST_AUTO_TEST_CASE(TestQueriesReturnNearbyActiveNpcs) {
    // Chase (cluster center) and Flee (crowd-size tactic) steer from these
    // queries. They return nearby active NPCs, excluding the querying entity,
    // non-NPC entities, and anything outside the radius.
    auto& edm = EntityDataManager::Instance();
    const size_t self = createNPC(Vector2D(500.0f, 500.0f));
    const size_t nearA = createNPC(Vector2D(540.0f, 500.0f));
    const size_t nearB = createNPC(Vector2D(500.0f, 540.0f));
    const size_t far = createNPC(Vector2D(1200.0f, 1200.0f));
    // A projectile in the crowd is not a crowd member.
    const EntityHandle projectile = edm.createProjectile(
        Vector2D(520.0f, 520.0f), Vector2D(0.0f, 0.0f), edm.getHandle(nearA), 1.0f);
    BOOST_REQUIRE(projectile.isValid());
    runAIFrame(Vector2D(500.0f, 500.0f));
    const auto active = edm.getActiveIndices();
    BOOST_REQUIRE(std::find(active.begin(), active.end(), edm.getIndex(projectile)) !=
        active.end());

    const Vector2D selfPos = edm.getHotDataByIndex(self).transform.position;
    constexpr float radius = 100.0f;

    BOOST_CHECK_EQUAL(AIInternal::CountNearbyEntities(self, selfPos, radius), 2);

    std::vector<Vector2D> positions;
    BOOST_REQUIRE_EQUAL(
        AIInternal::GetNearbyEntitiesWithPositions(self, selfPos, radius, positions), 2);
    const auto contains = [&positions](const Vector2D& p) {
        return std::any_of(positions.begin(), positions.end(), [&p](const Vector2D& q) {
            return Vector2D::distanceSquared(p, q) < 0.01f;
        });
    };
    BOOST_CHECK(contains(edm.getHotDataByIndex(nearA).transform.position));
    BOOST_CHECK(contains(edm.getHotDataByIndex(nearB).transform.position));

    // The far NPC is active but has no neighbours within the radius.
    const Vector2D farPos = edm.getHotDataByIndex(far).transform.position;
    BOOST_CHECK_EQUAL(AIInternal::CountNearbyEntities(far, farPos, radius), 0);
}

BOOST_AUTO_TEST_CASE(TestCacheDoesNotLeakAcrossStateTransition) {
    // AIManager's frame counter restarts at 0 on a state transition while the
    // thread_local crowd caches survive. The same query key at the same
    // AIManager frame in the next state must not return the old state's
    // EDM indices.
    // Query key unused by other cases, so only this case can seed it.
    const Vector2D center(700.0f, 700.0f);
    constexpr float radius = 100.0f;
    for (int i = 0; i < 4; ++i) {
        createNPC(Vector2D(680.0f + static_cast<float>(i) * 10.0f, 700.0f));
    }
    runAIFrame(center);
    BOOST_REQUIRE_EQUAL(AIInternal::CountNearbyEntities(SIZE_MAX, center, radius), 4);

    // State transition (AI-heavy cleanup order), then a smaller population.
    AIManager::Instance().prepareForStateTransition();
    CollisionManager::Instance().prepareForStateTransition();
    PathfinderManager::Instance().prepareForStateTransition();
    EntityDataManager::Instance().prepareForStateTransition();
    BOOST_REQUIRE(EntityDataManager::Instance().getActiveIndices().empty());

    createNPC(Vector2D(700.0f, 700.0f));
    runAIFrame(center);
    BOOST_CHECK_EQUAL(AIInternal::CountNearbyEntities(SIZE_MAX, center, radius), 1);
}

BOOST_AUTO_TEST_CASE(TestCacheInvalidationAndBufferReuse) {
    auto& bufferA = AIInternal::GetNearbyPositionBuffer();
    bufferA.clear();
    bufferA.push_back(Vector2D(1.0f, 2.0f));

    auto& bufferB = AIInternal::GetNearbyPositionBuffer();
    BOOST_CHECK_EQUAL(&bufferA, &bufferB);
    BOOST_REQUIRE_EQUAL(bufferB.size(), 1u);
    BOOST_CHECK_CLOSE(bufferB[0].getX(), 1.0f, 0.001f);

    VOIDLIGHT_STATS_ONLY(
        AIInternal::ResetCrowdStats();
        const auto cleared = AIInternal::GetCrowdStats();
        BOOST_CHECK_EQUAL(cleared.queryCount, 0u);
        BOOST_CHECK_EQUAL(cleared.cacheHits, 0u);
        BOOST_CHECK_EQUAL(cleared.cacheMisses, 0u);
        BOOST_CHECK_EQUAL(cleared.resultsCount, 0u);)
}

BOOST_AUTO_TEST_SUITE_END()
