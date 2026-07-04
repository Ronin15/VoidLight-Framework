/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE CrowdRuntimeTests
#include <boost/test/unit_test.hpp>

#include "ai/internal/Crowd.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "utils/Vector2D.hpp"

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

struct CrowdRuntimeFixture
{
    CrowdRuntimeFixture()
    {
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        CollisionManager::Instance().setWorldBounds(0.0f, 0.0f, 2000.0f, 2000.0f);

        VOIDLIGHT_STATS_ONLY(AIInternal::ResetCrowdStats();)
    }

    ~CrowdRuntimeFixture()
    {
        CollisionManager::Instance().clean();
        BackgroundSimulationManager::Instance().clean();
        EntityDataManager::Instance().clean();
        EventManager::Instance().clean();
    }

    EntityHandle createCollidableNPC(const Vector2D& pos)
    {
        auto& edm = EntityDataManager::Instance();
        EntityHandle handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
        size_t index = edm.getIndex(handle);
        BOOST_REQUIRE(index != SIZE_MAX);

        auto& hot = edm.getHotDataByIndex(index);
        hot.setCollisionEnabled(true);
        hot.collisionLayers = CollisionLayer::Layer_Default;
        hot.collisionMask = 0xFFFFu;
        return handle;
    }

    void activateCollisionAt(const Vector2D& referencePoint)
    {
        EntityDataManager::Instance().updateSimulationTiers(referencePoint, 1500.0f, 10000.0f);
        BackgroundSimulationManager::Instance().update(referencePoint, 0.016f);
        CollisionManager::Instance().update(0.016f);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(CrowdQueryTests, CrowdRuntimeFixture)

BOOST_AUTO_TEST_CASE(TestEmptyQueriesReuseCacheAndUpdateStats)
{
    createCollidableNPC(Vector2D(100.0f, 100.0f));
    activateCollisionAt(Vector2D(100.0f, 100.0f));
    AIInternal::InvalidateSpatialCache(1);

    const EntityID excludeId = 999999;
    const Vector2D center(250.0f, 250.0f);
    const float radius = 64.0f;

    const int nearbyCount =
        AIInternal::CountNearbyEntities(excludeId, center, radius);
    BOOST_CHECK_EQUAL(nearbyCount, 0);

    auto& reusableBuffer = AIInternal::GetNearbyPositionBuffer();
    reusableBuffer.clear();
    reusableBuffer.push_back(Vector2D(1.0f, 2.0f));
    std::vector<Vector2D> positions;
    const int positionCount = AIInternal::GetNearbyEntitiesWithPositions(
        excludeId, center, radius, positions);
    BOOST_CHECK_EQUAL(positionCount, 0);
    BOOST_CHECK(positions.empty());
    BOOST_REQUIRE_EQUAL(reusableBuffer.size(), 1u);

    VOIDLIGHT_STATS_ONLY(
    const auto stats = AIInternal::GetCrowdStats();
    BOOST_CHECK_EQUAL(stats.queryCount, 2u);
    BOOST_CHECK_EQUAL(stats.cacheMisses, 1u);
    BOOST_CHECK_EQUAL(stats.cacheHits, 1u);
    BOOST_CHECK_EQUAL(stats.resultsCount, 0u);
    )
}

BOOST_AUTO_TEST_CASE(TestCacheInvalidationAndBufferReuse)
{
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
    BOOST_CHECK_EQUAL(cleared.resultsCount, 0u);
    )
}

BOOST_AUTO_TEST_SUITE_END()
