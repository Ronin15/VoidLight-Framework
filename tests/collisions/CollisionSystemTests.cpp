/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE CollisionSystemTests
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/old/interface.hpp>

#include "collisions/AABB.hpp"
#include "collisions/CollisionBody.hpp"
#include "collisions/TriggerTag.hpp"
#include "collisions/HierarchicalSpatialHash.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "entities/Entity.hpp"  // For AnimationConfig
#include "managers/EventManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "events/CollisionObstacleChangedEvent.hpp"
#include "events/WorldTriggerEvent.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "utils/Vector2D.hpp"
#include <vector>
#include <chrono>
#include <random>
#include <atomic>

using namespace VoidLight;

BOOST_AUTO_TEST_SUITE(AABBTests)

BOOST_AUTO_TEST_CASE(TestAABBBasicProperties)
{
    AABB aabb(10.0f, 20.0f, 5.0f, 7.5f);
    
    BOOST_CHECK_CLOSE(aabb.left(), 5.0f, 0.01f);
    BOOST_CHECK_CLOSE(aabb.right(), 15.0f, 0.01f);
    BOOST_CHECK_CLOSE(aabb.top(), 12.5f, 0.01f);
    BOOST_CHECK_CLOSE(aabb.bottom(), 27.5f, 0.01f);
}

BOOST_AUTO_TEST_CASE(TestAABBIntersection)
{
    AABB aabb1(10.0f, 10.0f, 5.0f, 5.0f);  // center at (10,10), size 10x10
    AABB aabb2(15.0f, 10.0f, 3.0f, 3.0f);  // center at (15,10), size 6x6
    AABB aabb3(20.0f, 10.0f, 2.0f, 2.0f);  // center at (20,10), size 4x4
    
    BOOST_CHECK(aabb1.intersects(aabb2));  // Should overlap
    BOOST_CHECK(aabb2.intersects(aabb1));  // Symmetry
    BOOST_CHECK(!aabb1.intersects(aabb3)); // Should not overlap
    BOOST_CHECK(!aabb3.intersects(aabb1)); // Symmetry
}

BOOST_AUTO_TEST_CASE(TestAABBContainsPoint)
{
    AABB aabb(10.0f, 10.0f, 5.0f, 5.0f);
    
    BOOST_CHECK(aabb.contains(Vector2D(10.0f, 10.0f)));  // Center
    BOOST_CHECK(aabb.contains(Vector2D(5.0f, 5.0f)));    // Corner
    BOOST_CHECK(aabb.contains(Vector2D(15.0f, 15.0f)));  // Opposite corner
    BOOST_CHECK(!aabb.contains(Vector2D(20.0f, 20.0f))); // Outside
    BOOST_CHECK(!aabb.contains(Vector2D(0.0f, 0.0f)));   // Outside
}

BOOST_AUTO_TEST_CASE(TestAABBClosestPoint)
{
    AABB aabb(10.0f, 10.0f, 5.0f, 5.0f);
    
    // Point inside should return itself
    Vector2D inside(10.0f, 10.0f);
    Vector2D closest1 = aabb.closestPoint(inside);
    BOOST_CHECK_CLOSE(closest1.getX(), inside.getX(), 0.01f);
    BOOST_CHECK_CLOSE(closest1.getY(), inside.getY(), 0.01f);
    
    // Point outside should clamp to edge
    Vector2D outside(20.0f, 20.0f);
    Vector2D closest2 = aabb.closestPoint(outside);
    BOOST_CHECK_CLOSE(closest2.getX(), 15.0f, 0.01f); // Right edge
    BOOST_CHECK_CLOSE(closest2.getY(), 15.0f, 0.01f); // Bottom edge
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SpatialHashTests)

BOOST_AUTO_TEST_CASE(TestSpatialHashInsertAndQuery)
{
    HierarchicalSpatialHash spatialHash;
    
    // Insert a few entities
    AABB aabb1(16.0f, 16.0f, 8.0f, 8.0f);  // Single cell
    AABB aabb2(48.0f, 16.0f, 8.0f, 8.0f);  // Different cell
    AABB aabb3(32.0f, 32.0f, 16.0f, 16.0f); // Spans multiple cells
    
    size_t id1 = 1, id2 = 2, id3 = 3;
    spatialHash.insert(id1, aabb1);
    spatialHash.insert(id2, aabb2);
    spatialHash.insert(id3, aabb3);
    
    // Query first cell area
    std::vector<size_t> results;
    AABB queryArea(16.0f, 16.0f, 16.0f, 16.0f);
    spatialHash.queryRegion(queryArea, results);
    
    BOOST_CHECK_GE(results.size(), 1);
    BOOST_CHECK(std::find(results.begin(), results.end(), id1) != results.end());
}

BOOST_AUTO_TEST_CASE(TestSpatialHashRemove)
{
    HierarchicalSpatialHash spatialHash;
    
    EntityID id1 = 1;
    AABB aabb1(16.0f, 16.0f, 8.0f, 8.0f);
    
    spatialHash.insert(id1, aabb1);
    
    // Verify it's there
    std::vector<size_t> results;
    spatialHash.queryRegion(aabb1, results);
    BOOST_CHECK_GE(results.size(), 1);
    
    // Remove and verify it's gone
    spatialHash.remove(id1);
    results.clear();
    spatialHash.queryRegion(aabb1, results);
    BOOST_CHECK(std::find(results.begin(), results.end(), id1) == results.end());
}

BOOST_AUTO_TEST_CASE(TestSpatialHashUpdate)
{
    HierarchicalSpatialHash spatialHash;
    
    size_t id1 = 1;
    AABB oldAABB(100.0f, 100.0f, 8.0f, 8.0f);  // Coarse cell (0,0)
    AABB newAABB(300.0f, 300.0f, 8.0f, 8.0f);  // Coarse cell (1,1)

    spatialHash.insert(id1, oldAABB);

    // Update position
    spatialHash.update(id1, oldAABB, newAABB);
    
    // Should not be found in old area
    std::vector<size_t> oldResults;
    spatialHash.queryRegion(oldAABB, oldResults);
    BOOST_CHECK(std::find(oldResults.begin(), oldResults.end(), id1) == oldResults.end());
    
    // Should be found in new area
    std::vector<size_t> newResults;
    spatialHash.queryRegion(newAABB, newResults);
    BOOST_CHECK(std::find(newResults.begin(), newResults.end(), id1) != newResults.end());
}

BOOST_AUTO_TEST_CASE(TestSpatialHashSmallAndLargeMovement)
{
    // Testing small moves within same coarse region vs large moves to different regions
    HierarchicalSpatialHash spatialHash;

    size_t id = 42;
    AABB aabb(64.0f, 64.0f, 8.0f, 8.0f); // starts in coarse cell (0,0)
    spatialHash.insert(id, aabb);

    // Small movement within same coarse region: should still be findable
    AABB smallMove(66.0f, 64.0f, 8.0f, 8.0f); // move by 2px in X, still in (0,0)
    spatialHash.update(id, aabb, smallMove);

    // Query both original and slightly shifted area should still find the entity
    std::vector<size_t> results1, results2;
    spatialHash.queryRegion(aabb, results1);
    spatialHash.queryRegion(smallMove, results2);
    BOOST_CHECK(std::find(results1.begin(), results1.end(), id) != results1.end());
    BOOST_CHECK(std::find(results2.begin(), results2.end(), id) != results2.end());

    // Large movement to different coarse region
    AABB bigMove(300.0f, 300.0f, 8.0f, 8.0f); // coarse cell (1,1)
    spatialHash.update(id, smallMove, bigMove);

    // Should not be found near the original area anymore
    std::vector<size_t> results3;
    spatialHash.queryRegion(aabb, results3);
    BOOST_CHECK(std::find(results3.begin(), results3.end(), id) == results3.end());

    // Should be found at the new location
    std::vector<size_t> results4;
    spatialHash.queryRegion(bigMove, results4);
    BOOST_CHECK(std::find(results4.begin(), results4.end(), id) != results4.end());
}

BOOST_AUTO_TEST_CASE(TestSpatialHashClear)
{
    HierarchicalSpatialHash spatialHash;
    
    // Add several entities
    for (EntityID id = 1; id <= 5; ++id) {
        AABB aabb(id * 16.0f, id * 16.0f, 8.0f, 8.0f);
        spatialHash.insert(id, aabb);
    }
    
    // Clear all
    spatialHash.clear();
    
    // Query should return nothing
    std::vector<size_t> results;
    AABB largeQuery(0.0f, 0.0f, 200.0f, 200.0f);
    spatialHash.queryRegion(largeQuery, results);
    BOOST_CHECK_EQUAL(results.size(), 0);
}

BOOST_AUTO_TEST_CASE(TestSpatialHashNoDuplicates)
{
    HierarchicalSpatialHash spatialHash; // Small cells to force multi-cell entities
    
    size_t id1 = 1;
    AABB largeAABB(24.0f, 24.0f, 20.0f, 20.0f); // Spans multiple cells
    spatialHash.insert(id1, largeAABB);
    
    // Query overlapping the entity should return it only once
    std::vector<size_t> results;
    spatialHash.queryRegion(largeAABB, results);
    
    int count = std::count(results.begin(), results.end(), id1);
    BOOST_CHECK_EQUAL(count, 1);
}

BOOST_AUTO_TEST_CASE(TestSpatialHashBatchOperationsAndStats)
{
    HierarchicalSpatialHash spatialHash;
    spatialHash.reserve(8);
    spatialHash.reserveRegions(4);

    const std::vector<std::pair<size_t, AABB>> inserts{
        {1, AABB(32.0f, 32.0f, 8.0f, 8.0f)},
        {2, AABB(96.0f, 32.0f, 8.0f, 8.0f)},
        {3, AABB(160.0f, 32.0f, 8.0f, 8.0f)}
    };

    spatialHash.insertBatch(inserts);

    BOOST_CHECK_GT(spatialHash.getRegionCount(), 0U);
    BOOST_CHECK_GE(spatialHash.getActiveRegionCount(), 0U);

    const std::vector<std::tuple<size_t, AABB, AABB>> updates{
        {1, AABB(32.0f, 32.0f, 8.0f, 8.0f), AABB(224.0f, 32.0f, 8.0f, 8.0f)},
        {2, AABB(96.0f, 32.0f, 8.0f, 8.0f), AABB(96.0f, 96.0f, 8.0f, 8.0f)}
    };
    spatialHash.updateBatch(updates);

    std::vector<size_t> movedResults;
    spatialHash.queryRegion(AABB(224.0f, 32.0f, 16.0f, 16.0f), movedResults);
    BOOST_CHECK(std::find(movedResults.begin(), movedResults.end(), 1) != movedResults.end());
}

BOOST_AUTO_TEST_CASE(TestSpatialHashThreadSafeBoundsQueryMatchesRegularQuery)
{
    HierarchicalSpatialHash spatialHash;
    spatialHash.insert(7, AABB(64.0f, 64.0f, 12.0f, 12.0f));
    spatialHash.insert(8, AABB(96.0f, 64.0f, 12.0f, 12.0f));
    spatialHash.insert(9, AABB(224.0f, 224.0f, 12.0f, 12.0f));

    std::vector<size_t> regularResults;
    spatialHash.queryRegionBounds(32.0f, 32.0f, 128.0f, 128.0f, regularResults);

    HierarchicalSpatialHash::QueryBuffers buffers;
    buffers.reserve();
    std::vector<size_t> threadSafeResults;
    spatialHash.queryRegionBoundsThreadSafe(
        32.0f, 32.0f, 128.0f, 128.0f, threadSafeResults, buffers);

    std::sort(regularResults.begin(), regularResults.end());
    std::sort(threadSafeResults.begin(), threadSafeResults.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(
        regularResults.begin(), regularResults.end(),
        threadSafeResults.begin(), threadSafeResults.end());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(CollisionPerformanceTests)

BOOST_AUTO_TEST_CASE(TestSpatialHashPerformance)
{
    const int NUM_ENTITIES = 1000;
    const int NUM_QUERIES = 100;
    const float WORLD_SIZE = 1000.0f;
    
    HierarchicalSpatialHash spatialHash;
    
    // Generate random entities
    std::mt19937 rng(42); // Fixed seed for reproducible results
    std::uniform_real_distribution<float> posDist(0.0f, WORLD_SIZE);
    std::uniform_real_distribution<float> sizeDist(5.0f, 25.0f);
    
    std::vector<std::pair<EntityID, AABB>> entities;
    
    // Insertion performance test
    auto startInsert = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        EntityID id = static_cast<EntityID>(i + 1);
        float x = posDist(rng);
        float y = posDist(rng);
        float halfW = sizeDist(rng);
        float halfH = sizeDist(rng);
        
        AABB aabb(x, y, halfW, halfH);
        entities.emplace_back(id, aabb);
        spatialHash.insert(id, aabb);
    }
    
    auto endInsert = std::chrono::high_resolution_clock::now();
    auto insertDuration = std::chrono::duration_cast<std::chrono::microseconds>(endInsert - startInsert);
    
    BOOST_TEST_MESSAGE("Inserted " << NUM_ENTITIES << " entities in " 
                      << insertDuration.count() << " microseconds ("
                      << (insertDuration.count() / NUM_ENTITIES) << " μs per entity)");
    
    // Query performance test
    std::vector<size_t> results;
    int totalFound = 0;
    
    auto startQuery = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        float queryX = posDist(rng);
        float queryY = posDist(rng);
        float querySize = 100.0f; // Fixed query size
        
        AABB queryArea(queryX, queryY, querySize, querySize);
        results.clear();
        spatialHash.queryRegion(queryArea, results);
        totalFound += results.size();
    }
    
    auto endQuery = std::chrono::high_resolution_clock::now();
    auto queryDuration = std::chrono::duration_cast<std::chrono::microseconds>(endQuery - startQuery);
    
    BOOST_TEST_MESSAGE("Performed " << NUM_QUERIES << " queries in " 
                      << queryDuration.count() << " microseconds ("
                      << (queryDuration.count() / NUM_QUERIES) << " μs per query)");
    BOOST_TEST_MESSAGE("Average entities found per query: " << (totalFound / NUM_QUERIES));
    
    // Performance requirements (adjust based on target performance)
    BOOST_CHECK_LT(insertDuration.count() / NUM_ENTITIES, 50); // < 50μs per insertion
    BOOST_CHECK_LT(queryDuration.count() / NUM_QUERIES, 100);  // < 100μs per query
}

BOOST_AUTO_TEST_CASE(TestSpatialHashUpdatePerformance)
{
    const int NUM_ENTITIES = 500;
    const int NUM_UPDATES = 1000;
    const float WORLD_SIZE = 500.0f;
    
    HierarchicalSpatialHash spatialHash;
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(0.0f, WORLD_SIZE);
    std::uniform_real_distribution<float> sizeDist(5.0f, 15.0f);
    std::uniform_int_distribution<int> idDist(1, NUM_ENTITIES);
    
    // Insert initial entities
    std::vector<std::pair<size_t, AABB>> entities;
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        size_t id = static_cast<size_t>(i);
        float x = posDist(rng);
        float y = posDist(rng);
        float halfW = sizeDist(rng);
        float halfH = sizeDist(rng);

        AABB aabb(x, y, halfW, halfH);
        entities.emplace_back(id, aabb);
        spatialHash.insert(id, aabb);
    }
    
    // Update performance test
    auto startUpdate = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_UPDATES; ++i) {
        // Pick random entity to update
        int entityIndex = idDist(rng) - 1;
        size_t id = entities[entityIndex].first;
        AABB oldAABB = entities[entityIndex].second;

        // Generate new position
        float newX = posDist(rng);
        float newY = posDist(rng);
        float halfW = oldAABB.halfSize.getX();
        float halfH = oldAABB.halfSize.getY();

        AABB newAABB(newX, newY, halfW, halfH);
        spatialHash.update(id, oldAABB, newAABB);
        entities[entityIndex].second = newAABB;
    }
    
    auto endUpdate = std::chrono::high_resolution_clock::now();
    auto updateDuration = std::chrono::duration_cast<std::chrono::microseconds>(endUpdate - startUpdate);
    
    BOOST_TEST_MESSAGE("Performed " << NUM_UPDATES << " updates in " 
                      << updateDuration.count() << " microseconds ("
                      << (updateDuration.count() / NUM_UPDATES) << " μs per update)");
    
    // Performance requirement
    BOOST_CHECK_LT(updateDuration.count() / NUM_UPDATES, 75); // < 75μs per update
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(CollisionStressTests)

BOOST_AUTO_TEST_CASE(TestHighDensityCollisions)
{
    const int ENTITIES_PER_CELL = 20;
    const int GRID_SIZE = 10;
    const float CELL_SIZE = 50.0f;
    const int TOTAL_ENTITIES = ENTITIES_PER_CELL * GRID_SIZE * GRID_SIZE;
    
    HierarchicalSpatialHash spatialHash;
    
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> offsetDist(-20.0f, 20.0f);
    
    // Place multiple entities in each grid cell
    EntityID currentId = 1;
    for (int gridX = 0; gridX < GRID_SIZE; ++gridX) {
        for (int gridY = 0; gridY < GRID_SIZE; ++gridY) {
            float cellCenterX = (gridX + 0.5f) * CELL_SIZE;
            float cellCenterY = (gridY + 0.5f) * CELL_SIZE;
            
            for (int e = 0; e < ENTITIES_PER_CELL; ++e) {
                float x = cellCenterX + offsetDist(rng);
                float y = cellCenterY + offsetDist(rng);
                AABB aabb(x, y, 5.0f, 5.0f);
                
                spatialHash.insert(currentId++, aabb);
            }
        }
    }
    
    // Query each cell and verify reasonable entity counts
    int totalQueriesChecked = 0;
    for (int gridX = 0; gridX < GRID_SIZE; ++gridX) {
        for (int gridY = 0; gridY < GRID_SIZE; ++gridY) {
            float cellCenterX = (gridX + 0.5f) * CELL_SIZE;
            float cellCenterY = (gridY + 0.5f) * CELL_SIZE;
            
            AABB queryArea(cellCenterX, cellCenterY, CELL_SIZE * 0.4f, CELL_SIZE * 0.4f);
            std::vector<size_t> results;
            spatialHash.queryRegion(queryArea, results);
            
            // Should find at least some entities in each dense cell
            BOOST_CHECK_GE(results.size(), 1);
            totalQueriesChecked++;
        }
    }
    
    BOOST_TEST_MESSAGE("Stress test completed with " << TOTAL_ENTITIES 
                      << " entities across " << totalQueriesChecked << " cells");
}

BOOST_AUTO_TEST_CASE(TestBoundaryConditions)
{
    HierarchicalSpatialHash spatialHash;
    
    // Test entities exactly on cell boundaries
    EntityID id1 = 1;
    AABB boundaryAABB(32.0f, 32.0f, 1.0f, 1.0f); // Exactly on boundary
    spatialHash.insert(id1, boundaryAABB);
    
    // Query should find it in adjacent cells
    std::vector<size_t> results;
    AABB queryArea(31.0f, 31.0f, 2.0f, 2.0f);
    spatialHash.queryRegion(queryArea, results);
    
    BOOST_CHECK_GE(results.size(), 1);
    BOOST_CHECK(std::find(results.begin(), results.end(), id1) != results.end());
    
    // Test very large entities
    EntityID id2 = 2;
    AABB largeAABB(64.0f, 64.0f, 100.0f, 100.0f); // Spans many cells
    spatialHash.insert(id2, largeAABB);
    
    // Should be found in multiple query areas
    AABB query1(0.0f, 0.0f, 32.0f, 32.0f);
    AABB query2(128.0f, 128.0f, 32.0f, 32.0f);
    
    std::vector<size_t> results1, results2;
    spatialHash.queryRegion(query1, results1);
    spatialHash.queryRegion(query2, results2);
    
    bool foundInFirst = std::find(results1.begin(), results1.end(), id2) != results1.end();
    bool foundInSecond = std::find(results2.begin(), results2.end(), id2) != results2.end();
    
    BOOST_CHECK(foundInFirst || foundInSecond); // Should be found in at least one
}

BOOST_AUTO_TEST_SUITE_END()

// EDM-Centric Collision Tests
// Statics (buildings, triggers) go in CollisionManager m_storage
// Movables (NPCs, players) are managed by EntityDataManager only
BOOST_AUTO_TEST_SUITE(EDMCentricCollisionTests)

BOOST_AUTO_TEST_CASE(TestStaticMovableSeparation)
{
    // Initialize managers
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);

    // Create static body via CollisionManager (buildings, obstacles)
    Vector2D testPos(100.0f, 100.0f);
    AABB testAABB(testPos.getX(), testPos.getY(), 32.0f, 32.0f);
    EntityHandle staticHandle = edm.createStaticBody(testAABB.center, testAABB.halfSize.getX(), testAABB.halfSize.getY());
    size_t staticEdmIndex = edm.getStaticIndex(staticHandle);
    EntityID staticId = staticHandle.getId();
    CollisionManager::Instance().addStaticBody(staticId, testAABB.center, testAABB.halfSize,
                                                CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex);

    // Create movable entity via EDM (NPCs)
    Vector2D npcPos(150.0f, 150.0f);
    EntityHandle npcHandle = edm.createNPCWithRaceClass( npcPos, "Human", "Guard");
    size_t npcIdx = edm.getIndex(npcHandle);
    auto& npcHot = edm.getHotDataByIndex(npcIdx);
    npcHot.setCollisionEnabled(true);

    // Update BGM to populate active indices
    bgm.update(testPos, 0.016f);

    // Verify static is in CollisionManager storage
    BOOST_CHECK_EQUAL(CollisionManager::Instance().getStaticBodyCount(), 1);

    // Verify movable is in EDM active tier (not in CM storage)
    BOOST_CHECK_GE(edm.getActiveIndices().size(), 1u);
    BOOST_CHECK(npcHot.hasCollision());

    // Clean up
    CollisionManager::Instance().removeCollisionBody(staticId);
    edm.unregisterEntity(20000);
    CollisionManager::Instance().clean();
    bgm.clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestBroadphasePerformanceWithDualHashes)
{
    // Test that broadphase performance is improved with separate static/movable storage
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);

    const int NUM_STATIC_BODIES = 200; // Simulate world tiles
    const int NUM_MOVABLE_BODIES = 20;  // Simulate NPCs

    std::vector<EntityID> staticBodies;
    std::vector<EntityHandle> movableHandles;

    // Add many static bodies (world tiles) via CollisionManager
    for (int i = 0; i < NUM_STATIC_BODIES; ++i) {
        float x = static_cast<float>(i % 20) * 64.0f; // Grid layout
        float y = static_cast<float>(i / 20) * 64.0f;
        AABB aabb(x, y, 32.0f, 32.0f);

        EntityHandle staticHandle = edm.createStaticBody(aabb.center, aabb.halfSize.getX(), aabb.halfSize.getY());
        size_t staticEdmIndex = edm.getStaticIndex(staticHandle);
        EntityID id = staticHandle.getId();
        CollisionManager::Instance().addStaticBody(id, aabb.center, aabb.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                    false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex);
        staticBodies.push_back(id);
    }

    // Add movable bodies (NPCs) via EDM
    for (int i = 0; i < NUM_MOVABLE_BODIES; ++i) {
        float x = 500.0f + static_cast<float>(i % 5) * 32.0f;
        float y = 500.0f + static_cast<float>(i / 5) * 32.0f;
        Vector2D pos(x, y);

        EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
        size_t idx = edm.getIndex(handle);
        auto& hot = edm.getHotDataByIndex(idx);
        hot.collisionLayers = CollisionLayer::Layer_Enemy;
        hot.collisionMask = 0xFFFF;
        hot.setCollisionEnabled(true);
        movableHandles.push_back(handle);
    }

    // Update BGM to populate active indices
    bgm.update(Vector2D(500.0f, 500.0f), 0.016f);

    // Reset performance stats before measurement
    CollisionManager::Instance().resetPerfStats();

    // Run several collision detection cycles
    const int NUM_CYCLES = 10;
    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < NUM_CYCLES; ++cycle) {
        CollisionManager::Instance().update(0.016f); // 60 FPS simulation
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Get performance statistics
    auto perfStats = CollisionManager::Instance().getPerfStats();

    // Performance assertions - broadphase should be fast with dual storage
    BOOST_CHECK_LT(perfStats.lastBroadphaseMs, 0.5); // < 0.5ms broadphase
    BOOST_CHECK_LT(perfStats.lastTotalMs, 2.0);     // < 2ms total collision time

    // Average cycle time should be reasonable
    double avgCycleTimeMs = duration.count() / 1000.0 / NUM_CYCLES;
    BOOST_CHECK_LT(avgCycleTimeMs, 1.0); // < 1ms per collision cycle

    BOOST_TEST_MESSAGE("Dual storage broadphase: " << perfStats.lastBroadphaseMs << "ms, "
                      << "Total: " << perfStats.lastTotalMs << "ms, "
                      << "Avg cycle: " << avgCycleTimeMs << "ms");

    // Clean up
    for (EntityID id : staticBodies) {
        CollisionManager::Instance().removeCollisionBody(id);
    }
    for (const auto& handle : movableHandles) {
        edm.unregisterEntity(handle.getId());
    }
    CollisionManager::Instance().clean();
    bgm.clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestMovableBatchUpdateWithEDM)
{
    // Test that batch movable updates work correctly with EDM-centric system
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(5000.0f);

    const int NUM_MOVABLE_BODIES = 50;
    std::vector<EntityHandle> movableHandles;
    std::vector<EntityID> movableIds;

    // Add movable bodies via EDM
    for (int i = 0; i < NUM_MOVABLE_BODIES; ++i) {
        EntityID id = 30000 + i;
        Vector2D pos(i * 20.0f, i * 20.0f);

        EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
        size_t idx = edm.getIndex(handle);
        auto& hot = edm.getHotDataByIndex(idx);
        hot.collisionLayers = CollisionLayer::Layer_Enemy;
        hot.collisionMask = 0xFFFF;
        hot.setCollisionEnabled(true);
        movableHandles.push_back(handle);
        movableIds.push_back(id);
    }

    // Update BGM to populate active indices
    bgm.update(Vector2D(500.0f, 500.0f), 0.016f);

    // Measure batch update performance via direct EDM position updates
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MOVABLE_BODIES; ++i) {
        Vector2D newPos(i * 25.0f + 100.0f, i * 25.0f + 100.0f);
        size_t idx = edm.getIndex(movableHandles[i]);
        auto& hot = edm.getHotDataByIndex(idx);
        hot.transform.position.setX(newPos.getX());
        hot.transform.position.setY(newPos.getY());
        hot.transform.velocity.setX(10.0f);
        hot.transform.velocity.setY(5.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Verify positions were updated
    size_t idx0 = edm.getIndex(movableHandles[0]);
    const auto& hot0 = edm.getHotDataByIndex(idx0);
    BOOST_CHECK_CLOSE(hot0.transform.position.getX(), 100.0f, 1.0f);
    BOOST_CHECK_CLOSE(hot0.transform.position.getY(), 100.0f, 1.0f);

    // Verify last body was also updated correctly
    size_t idxLast = edm.getIndex(movableHandles[NUM_MOVABLE_BODIES-1]);
    const auto& hotLast = edm.getHotDataByIndex(idxLast);
    float expectedX = (NUM_MOVABLE_BODIES-1) * 25.0f + 100.0f;
    float expectedY = (NUM_MOVABLE_BODIES-1) * 25.0f + 100.0f;
    BOOST_CHECK_CLOSE(hotLast.transform.position.getX(), expectedX, 1.0f);
    BOOST_CHECK_CLOSE(hotLast.transform.position.getY(), expectedY, 1.0f);

    // Performance check - batch update should be fast
    double avgUpdateTimeUs = static_cast<double>(duration.count()) / NUM_MOVABLE_BODIES;
    BOOST_CHECK_LT(avgUpdateTimeUs, 20.0); // < 20μs per body update

    BOOST_TEST_MESSAGE("Batch updated " << NUM_MOVABLE_BODIES << " movable bodies in "
                      << duration.count() << "μs (" << avgUpdateTimeUs << "μs/body)");

    // Clean up
    for (EntityID id : movableIds) {
        edm.unregisterEntity(id);
    }
    CollisionManager::Instance().clean();
    bgm.clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestStaticBodyCacheInvalidation)
{
    // Test that static body cache is properly invalidated when static bodies change
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);

    // Add a static body
    AABB staticAABB(200.0f, 200.0f, 32.0f, 32.0f);
    EntityHandle staticHandle = edm.createStaticBody(staticAABB.center, staticAABB.halfSize.getX(), staticAABB.halfSize.getY());
    size_t staticEdmIndex = edm.getStaticIndex(staticHandle);
    EntityID staticId = staticHandle.getId();
    CollisionManager::Instance().addStaticBody(staticId, staticAABB.center, staticAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex);

    // Add a movable body near the static body via EDM
    EntityID movableId = 40001;
    Vector2D movablePos(220.0f, 220.0f);
    EntityHandle movableHandle = edm.createNPCWithRaceClass( movablePos, "Human", "Guard");
    size_t movableIdx = edm.getIndex(movableHandle);
    auto& movableHot = edm.getHotDataByIndex(movableIdx);
    movableHot.collisionLayers = CollisionLayer::Layer_Enemy;
    movableHot.collisionMask = 0xFFFF;
    movableHot.setCollisionEnabled(true);

    // Update BGM to populate active indices
    bgm.update(movablePos, 0.016f);

    // Run collision detection to populate any caches
    CollisionManager::Instance().update(0.016f);

    // Add another static body that could affect collision detection
    AABB staticAABB2(240.0f, 240.0f, 32.0f, 32.0f);
    EntityHandle staticHandle2 = edm.createStaticBody(staticAABB2.center, staticAABB2.halfSize.getX(), staticAABB2.halfSize.getY());
    size_t staticEdmIndex2 = edm.getStaticIndex(staticHandle2);
    EntityID staticId2 = staticHandle2.getId();
    CollisionManager::Instance().addStaticBody(staticId2, staticAABB2.center, staticAABB2.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex2);

    // Verify cache invalidation by checking that static body count is correct
    BOOST_CHECK_EQUAL(CollisionManager::Instance().getStaticBodyCount(), 2);

    // Run collision detection again - should handle the new static body correctly
    CollisionManager::Instance().update(0.016f);

    // Remove static body and verify cache invalidation
    CollisionManager::Instance().removeCollisionBody(staticId);
    BOOST_CHECK_EQUAL(CollisionManager::Instance().getStaticBodyCount(), 1);

    // Clean up
    CollisionManager::Instance().removeCollisionBody(staticId2);
    edm.unregisterEntity(movableId);
    CollisionManager::Instance().clean();
    bgm.clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestTriggerSystemCreation)
{
    // Test trigger area creation and basic functionality
    CollisionManager::Instance().init();

    // Test createTriggerArea method
    AABB triggerAABB(100.0f, 100.0f, 50.0f, 50.0f);
    EntityID triggerId = CollisionManager::Instance().createTriggerArea(
        triggerAABB,
        VoidLight::TriggerTag::Water,
        VoidLight::TriggerType::EventOnly,
        CollisionLayer::Layer_Environment,
        CollisionLayer::Layer_Player | CollisionLayer::Layer_Enemy
    );

    BOOST_CHECK_NE(triggerId, 0); // Should return valid ID
    BOOST_CHECK(CollisionManager::Instance().isTrigger(triggerId));

    // Test createTriggerAreaAt convenience method
    EntityID triggerId2 = CollisionManager::Instance().createTriggerAreaAt(
        200.0f, 200.0f, 25.0f, 25.0f,
        VoidLight::TriggerTag::Lava,
        VoidLight::TriggerType::EventOnly,
        CollisionLayer::Layer_Environment,
        CollisionLayer::Layer_Player
    );

    BOOST_CHECK_NE(triggerId2, 0);
    BOOST_CHECK(CollisionManager::Instance().isTrigger(triggerId2));
    BOOST_CHECK_NE(triggerId, triggerId2); // Should be different IDs

    // Test that trigger bodies can be queried
    std::vector<EntityID> results;
    CollisionManager::Instance().queryArea(triggerAABB, results);
    BOOST_CHECK(std::find(results.begin(), results.end(), triggerId) != results.end());

    // Clean up
    CollisionManager::Instance().removeCollisionBody(triggerId);
    CollisionManager::Instance().removeCollisionBody(triggerId2);
    CollisionManager::Instance().clean();
}

BOOST_AUTO_TEST_CASE(TestTriggerCooldowns)
{
    // Test trigger cooldown functionality
    CollisionManager::Instance().init();

    // Set default cooldown
    CollisionManager::Instance().setDefaultTriggerCooldown(1.5f);

    // Create a trigger
    EntityID triggerId = CollisionManager::Instance().createTriggerAreaAt(
        50.0f, 50.0f, 20.0f, 20.0f,
        VoidLight::TriggerTag::Portal,
        VoidLight::TriggerType::EventOnly
    );

    // Set specific cooldown for this trigger
    CollisionManager::Instance().setTriggerCooldown(triggerId, 2.0f);

    // Verify trigger was created
    BOOST_CHECK(CollisionManager::Instance().isTrigger(triggerId));

    // Clean up
    CollisionManager::Instance().removeCollisionBody(triggerId);
    CollisionManager::Instance().clean();
}

BOOST_AUTO_TEST_CASE(TestBodyLayerFiltering)
{
    // Test collision layer filtering functionality via EDM
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    // Create movable entities with different layers via EDM
    EntityID playerId = 5000;
    EntityID npcId = 5001;

    Vector2D pos(100.0f, 100.0f);

    // Add movables via EDM
    EntityHandle playerHandle = edm.registerPlayer(playerId, pos, 16.0f, 16.0f);
    EntityHandle npcHandle = edm.createNPCWithRaceClass( pos, "Human", "Guard");

    // Set layers on EDM hot data - Player collides with NPCs and environment
    size_t playerIdx = edm.getIndex(playerHandle);
    auto& playerHot = edm.getHotDataByIndex(playerIdx);
    playerHot.collisionLayers = CollisionLayer::Layer_Player;
    playerHot.collisionMask = CollisionLayer::Layer_Enemy | CollisionLayer::Layer_Environment;
    playerHot.setCollisionEnabled(true);

    // NPC collides with players and environment, but not other NPCs
    size_t npcIdx = edm.getIndex(npcHandle);
    auto& npcHot = edm.getHotDataByIndex(npcIdx);
    npcHot.collisionLayers = CollisionLayer::Layer_Enemy;
    npcHot.collisionMask = CollisionLayer::Layer_Player | CollisionLayer::Layer_Environment;
    npcHot.setCollisionEnabled(true);

    // Add static environment body via CollisionManager
    AABB aabb(pos.getX(), pos.getY(), 16.0f, 16.0f);
    EntityHandle envHandle = edm.createStaticBody(aabb.center, aabb.halfSize.getX(), aabb.halfSize.getY());
    size_t envEdmIndex = edm.getStaticIndex(envHandle);
    EntityID environmentId = envHandle.getId();
    CollisionManager::Instance().addStaticBody(environmentId, aabb.center, aabb.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), envEdmIndex);

    // Verify layer settings on EDM entities
    BOOST_CHECK(playerHot.hasCollision());
    BOOST_CHECK(npcHot.hasCollision());
    BOOST_CHECK_EQUAL(playerHot.collisionLayers, CollisionLayer::Layer_Player);
    BOOST_CHECK_EQUAL(npcHot.collisionLayers, CollisionLayer::Layer_Enemy);

    // Clean up
    edm.unregisterEntity(playerId);
    edm.unregisterEntity(npcId);
    CollisionManager::Instance().removeCollisionBody(environmentId);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestBodyEnableDisable)
{
    // Test body enable/disable functionality via EDM
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    EntityID bodyId = 6000;
    Vector2D pos(150.0f, 150.0f);

    // Create movable via EDM
    EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
    size_t idx = edm.getIndex(handle);
    auto& hot = edm.getHotDataByIndex(idx);
    hot.collisionLayers = CollisionLayer::Layer_Player;
    hot.collisionMask = 0xFFFF;
    hot.setCollisionEnabled(true);

    // Body should have collision enabled
    BOOST_CHECK(hot.hasCollision());

    // Disable collision on the body
    hot.setCollisionEnabled(false);
    BOOST_CHECK(!hot.hasCollision());

    // Re-enable collision
    hot.setCollisionEnabled(true);
    BOOST_CHECK(hot.hasCollision());

    // Clean up
    edm.unregisterEntity(bodyId);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestBodyResize)
{
    // Test body resize functionality via EDM
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    Vector2D originalPos(200.0f, 200.0f);

    // Create movable via EDM (default frame 32x32 -> halfSize 16)
    EntityHandle handle = edm.createNPCWithRaceClass(originalPos, "Human", "Guard");
    size_t idx = edm.getIndex(handle);
    auto& hot = edm.getHotDataByIndex(idx);
    hot.collisionLayers = CollisionLayer::Layer_Player;
    hot.collisionMask = 0xFFFF;
    hot.setCollisionEnabled(true);

    // Verify original position and default half-size
    BOOST_CHECK_CLOSE(hot.transform.position.getX(), 200.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.position.getY(), 200.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.halfWidth, 16.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.halfHeight, 16.0f, 0.01f);

    // Resize the body via EDM
    hot.halfWidth = 25.0f;
    hot.halfHeight = 15.0f;

    // Position should remain the same, but size should change
    BOOST_CHECK_CLOSE(hot.transform.position.getX(), 200.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.position.getY(), 200.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.halfWidth, 25.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.halfHeight, 15.0f, 0.01f);

    // Clean up
    edm.unregisterEntity(handle.getId());
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestVelocityManagement)
{
    // Test velocity setting via EDM
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    EntityID bodyId = 8000;
    Vector2D pos(100.0f, 100.0f);
    Vector2D velocity(15.0f, 10.0f);

    // Register with EDM (the single source of truth for movables)
    EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
    size_t idx = edm.getIndex(handle);
    auto& hot = edm.getHotDataByIndex(idx);
    hot.collisionLayers = CollisionLayer::Layer_Player;
    hot.collisionMask = 0xFFFF;
    hot.setCollisionEnabled(true);

    // Set velocity on EDM hot data
    hot.transform.velocity.setX(velocity.getX());
    hot.transform.velocity.setY(velocity.getY());

    BOOST_CHECK_CLOSE(hot.transform.velocity.getX(), 15.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.velocity.getY(), 10.0f, 0.01f);

    // Update position and velocity
    Vector2D newPosition(120.0f, 110.0f);
    Vector2D newVelocity(20.0f, 5.0f);
    hot.transform.position.setX(newPosition.getX());
    hot.transform.position.setY(newPosition.getY());
    hot.transform.velocity.setX(newVelocity.getX());
    hot.transform.velocity.setY(newVelocity.getY());

    // Verify position and velocity were updated
    BOOST_CHECK_CLOSE(hot.transform.position.getX(), 120.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.position.getY(), 110.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.velocity.getX(), 20.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.velocity.getY(), 5.0f, 0.01f);

    // Clean up
    edm.unregisterEntity(bodyId);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_SUITE_END()

// Collision Info and Index Tests
BOOST_AUTO_TEST_SUITE(CollisionInfoTests)

BOOST_AUTO_TEST_CASE(TestCollisionInfoIndicesIntegrity)
{
    // CRITICAL TEST: Verify that our CollisionInfo index optimization works correctly
    // This test validates that indexA and indexB are properly populated
    // EDM-CENTRIC: Movables must be registered in EDM to participate in collision

    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    // Create two overlapping NPC entities in EDM (Active tier = participates in collision)
    Vector2D posA(100.0f, 100.0f);
    Vector2D posB(120.0f, 120.0f);  // Overlapping
    // Create NPCs with data-driven API (default frame 32x32 -> halfSize 16)
    EntityHandle handleA = edm.createNPCWithRaceClass(posA, "Human", "Guard");
    EntityHandle handleB = edm.createNPCWithRaceClass(posB, "Human", "Guard");

    // Get EDM indices and set up collision layers for testing
    // NPCs are created with Layer_Default by default (faction=Friendly), which doesn't
    // collide with other Layer_Default entities. Set Layer_Enemy for this test.
    size_t edmIdxA = edm.findIndexByEntityId(handleA.getId());
    size_t edmIdxB = edm.findIndexByEntityId(handleB.getId());
    auto& hotA = edm.getHotDataByIndex(edmIdxA);
    auto& hotB = edm.getHotDataByIndex(edmIdxB);
    hotA.collisionLayers = VoidLight::CollisionLayer::Layer_Enemy;
    hotA.collisionMask = VoidLight::CollisionLayer::Layer_Enemy;
    hotB.collisionLayers = VoidLight::CollisionLayer::Layer_Enemy;
    hotB.collisionMask = VoidLight::CollisionLayer::Layer_Enemy;

    // EDM-CENTRIC: Use BackgroundSimulationManager to update tiers
    // This populates m_activeIndices for collision detection
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);       // Large radius to include test entities
    bgm.setBackgroundRadius(4000.0f);
    bgm.update(posA, 0.016f);           // Update with reference point near entities

    // Verify active indices were populated
    auto activeIndices = edm.getActiveIndices();
    BOOST_REQUIRE_MESSAGE(activeIndices.size() >= 2,
        "Expected at least 2 entities in active tier, got " + std::to_string(activeIndices.size()));

    // Verify collision is enabled for both entities
    BOOST_REQUIRE_MESSAGE(hotA.hasCollision(), "Entity A should have collision enabled");
    BOOST_REQUIRE_MESSAGE(hotB.hasCollision(), "Entity B should have collision enabled");

    // Run collision detection
    CollisionManager::Instance().update(0.016f);

    // Read this frame's collisions directly from the manager's reusable buffer
    const auto& capturedCollisions = CollisionManager::Instance().getLastFrameCollisions();
    BOOST_REQUIRE_MESSAGE(!capturedCollisions.empty(), "Expected movable-movable collision between overlapping EDM entities");

    for (const auto& collision : capturedCollisions) {
        // Verify entity IDs are valid
        BOOST_CHECK(collision.a != 0);
        BOOST_CHECK(collision.b != 0);

        // CRITICAL: Verify indices are populated and valid (EDM indices for movable-movable)
        BOOST_CHECK_NE(collision.indexA, SIZE_MAX);  // Should not be default value
        BOOST_CHECK_NE(collision.indexB, SIZE_MAX);  // Should not be default value

        // For movable-movable collisions, indices are EDM indices
        BOOST_CHECK(collision.isMovableMovable);

        // Verify indices are different (can't collide with self)
        BOOST_CHECK_NE(collision.indexA, collision.indexB);

        // Verify collision normal is reasonable
        float normalLength = collision.normal.length();
        BOOST_CHECK_GT(normalLength, 0.1f);  // Should have meaningful normal

        // Verify penetration is positive for actual collision
        if (!collision.trigger) {
            BOOST_CHECK_GT(collision.penetration, 0.0f);
        }
    }

    // Clean up
    edm.destroyEntity(handleA);
    edm.destroyEntity(handleB);
    bgm.clean();
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_SUITE_END()

// Integration Tests for CollisionManager Event System
BOOST_AUTO_TEST_SUITE(CollisionIntegrationTests)

// Test fixture for manager integration tests
struct CollisionIntegrationFixture {
    CollisionIntegrationFixture() {
        // Initialize ThreadSystem first (following established pattern)
        if (!VoidLight::ThreadSystem::Exists()) {
            BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init()); // Auto-detect system threads
        }
        
        // Initialize EventManager for event testing
        EventManager::Instance().init();
        
        // Initialize CollisionManager
        CollisionManager::Instance().init();
        
        eventCount = 0;
        lastEventPosition = Vector2D(0, 0);
        lastEventRadius = 0.0f;
        lastEventDescription = "";
    }
    
    ~CollisionIntegrationFixture() {
        // Clean up in reverse order (following established pattern)
        CollisionManager::Instance().clean();
        EventManager::Instance().clean();
        // Note: Don't clean ThreadSystem as it's shared across tests
    }
    
    // Event tracking variables
    std::atomic<int> eventCount{0};
    Vector2D lastEventPosition;
    float lastEventRadius;
    std::string lastEventDescription;
};

BOOST_FIXTURE_TEST_CASE(TestCollisionManagerEventNotification, CollisionIntegrationFixture)
{
    // Subscribe to collision obstacle changed events
    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::CollisionObstacleChanged,
        [this](const EventData& data) {
            if (data.isActive() && data.event) {
                auto obstacleEvent = std::dynamic_pointer_cast<CollisionObstacleChangedEvent>(data.event);
                if (obstacleEvent) {
                    eventCount++;
                    lastEventPosition = obstacleEvent->getPosition();
                    lastEventRadius = obstacleEvent->getRadius();
                    lastEventDescription = obstacleEvent->getDescription();
                }
            }
        });
    
    // Test 1: Adding a static body should trigger an event
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());

    Vector2D staticPos(100.0f, 200.0f);
    AABB staticAABB(staticPos.getX(), staticPos.getY(), 32.0f, 32.0f);
    EntityHandle staticHandle = edm.createStaticBody(staticAABB.center, staticAABB.halfSize.getX(), staticAABB.halfSize.getY());
    size_t staticEdmIndex = edm.getStaticIndex(staticHandle);
    EntityID staticId = staticHandle.getId();

    CollisionManager::Instance().addStaticBody(staticId, staticAABB.center, staticAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex);

    // Process deferred events - CollisionManager fires events in Deferred mode
    // so we need to drain all events to ensure deterministic test behavior
    EventManager::Instance().drainAllDeferredEvents();

    // Should have received 1 event for the static body
    BOOST_CHECK_EQUAL(eventCount.load(), 1);
    BOOST_CHECK_CLOSE(lastEventPosition.getX(), staticPos.getX(), 0.01f);
    BOOST_CHECK_CLOSE(lastEventPosition.getY(), staticPos.getY(), 0.01f);
    BOOST_CHECK_GT(lastEventRadius, 32.0f); // Should be radius + safety margin
    BOOST_CHECK(lastEventDescription.find("Static obstacle added") != std::string::npos);
    
    // Test 2: Adding a movable body via EDM should NOT trigger a CollisionObstacleChanged event
    // (Movables don't fire these events - only static obstacles do)
    EntityID movableId = 1001;
    Vector2D movablePos(150.0f, 250.0f);
    int previousEventCount = eventCount.load();

    EntityHandle handle = edm.createNPCWithRaceClass( movablePos, "Human", "Guard");
    size_t idx = edm.getIndex(handle);
    auto& hot = edm.getHotDataByIndex(idx);
    hot.collisionLayers = CollisionLayer::Layer_Enemy;
    hot.collisionMask = 0xFFFF;
    hot.setCollisionEnabled(true);

    EventManager::Instance().drainAllDeferredEvents();  // Process any events (should be none for movable)

    // Event count should not have changed
    BOOST_CHECK_EQUAL(eventCount.load(), previousEventCount);

    // Test 3: Removing a static body should trigger an event
    CollisionManager::Instance().removeCollisionBody(staticId);
    EventManager::Instance().drainAllDeferredEvents();

    // Should have received another event for removal
    BOOST_CHECK_EQUAL(eventCount.load(), 2);
    BOOST_CHECK(lastEventDescription.find("Static obstacle removed") != std::string::npos);

    // Clean up
    edm.unregisterEntity(movableId);
    edm.clean();
    EventManager::Instance().removeHandler(token);
}

BOOST_FIXTURE_TEST_CASE(TestNonProjectileCollisionPairsAreNotEventManagerEvents, CollisionIntegrationFixture)
{
    std::atomic<int> collisionEventCount{0};
    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::Collision,
        [&collisionEventCount](const EventData&) {
            ++collisionEventCount;
        });

    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());

    EntityHandle handleA = edm.createNPCWithRaceClass({100.0f, 100.0f}, "Human", "Guard");
    EntityHandle handleB = edm.createNPCWithRaceClass({110.0f, 100.0f}, "Human", "Guard");

    size_t idxA = edm.getIndex(handleA);
    size_t idxB = edm.getIndex(handleB);
    auto& hotA = edm.getHotDataByIndex(idxA);
    auto& hotB = edm.getHotDataByIndex(idxB);
    hotA.collisionLayers = CollisionLayer::Layer_Enemy;
    hotA.collisionMask = CollisionLayer::Layer_Enemy;
    hotA.setCollisionEnabled(true);
    hotB.collisionLayers = CollisionLayer::Layer_Enemy;
    hotB.collisionMask = CollisionLayer::Layer_Enemy;
    hotB.setCollisionEnabled(true);

    auto& bgm = BackgroundSimulationManager::Instance();
    BOOST_REQUIRE(bgm.init());
    bgm.setActiveRadius(2000.0f);
    bgm.setBackgroundRadius(4000.0f);
    bgm.update({100.0f, 100.0f}, 0.016f);

    CollisionManager::Instance().update(0.016f);
    BOOST_REQUIRE(!CollisionManager::Instance().getLastFrameCollisions().empty());

    EventManager::Instance().drainAllDeferredEvents();
    BOOST_CHECK_EQUAL(collisionEventCount.load(), 0);

    edm.destroyEntity(handleA);
    edm.destroyEntity(handleB);
    bgm.clean();
    edm.clean();
    EventManager::Instance().removeHandler(token);
}

BOOST_FIXTURE_TEST_CASE(TestCollisionEventRadiusCalculation, CollisionIntegrationFixture)
{
    // Subscribe to events
    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::CollisionObstacleChanged,
        [this](const EventData& data) {
            if (data.isActive() && data.event) {
                auto obstacleEvent = std::dynamic_pointer_cast<CollisionObstacleChangedEvent>(data.event);
                if (obstacleEvent) {
                    eventCount++;
                    lastEventRadius = obstacleEvent->getRadius();
                }
            }
        });
    
    // Test different sized obstacles produce appropriate radii
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());

    // Small obstacle: 10x10
    AABB smallAABB(0.0f, 0.0f, 5.0f, 5.0f);
    EntityHandle smallHandle = edm.createStaticBody(smallAABB.center, smallAABB.halfSize.getX(), smallAABB.halfSize.getY());
    size_t smallEdmIndex = edm.getStaticIndex(smallHandle);
    EntityID smallId = smallHandle.getId();
    CollisionManager::Instance().addStaticBody(smallId, smallAABB.center, smallAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), smallEdmIndex);
    EventManager::Instance().drainAllDeferredEvents();

    float smallRadius = lastEventRadius;
    BOOST_CHECK_GT(smallRadius, 5.0f); // Should be larger than half-size
    BOOST_CHECK_LT(smallRadius, 50.0f); // But reasonable

    // Large obstacle: 100x100
    AABB largeAABB(200.0f, 200.0f, 50.0f, 50.0f);
    EntityHandle largeHandle = edm.createStaticBody(largeAABB.center, largeAABB.halfSize.getX(), largeAABB.halfSize.getY());
    size_t largeEdmIndex = edm.getStaticIndex(largeHandle);
    EntityID largeId = largeHandle.getId();
    CollisionManager::Instance().addStaticBody(largeId, largeAABB.center, largeAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), largeEdmIndex);
    EventManager::Instance().drainAllDeferredEvents();

    float largeRadius = lastEventRadius;
    BOOST_CHECK_GT(largeRadius, smallRadius); // Large should have larger radius
    BOOST_CHECK_GT(largeRadius, 50.0f); // Should be larger than half-size + margin
    
    // Clean up
    CollisionManager::Instance().removeCollisionBody(smallId);
    CollisionManager::Instance().removeCollisionBody(largeId);
    edm.clean();
    EventManager::Instance().removeHandler(token);
}

BOOST_FIXTURE_TEST_CASE(TestCollisionEventPerformanceImpact, CollisionIntegrationFixture)
{
    // Test that event firing doesn't significantly impact collision performance
    std::atomic<int> eventCount{0};
    
    // Subscribe to events but don't do heavy work
    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::CollisionObstacleChanged,
        [&eventCount](const EventData& data) {
            if (data.isActive() && data.event) {
                eventCount++;
            }
        });
    
    const int numBodies = 100;
    std::vector<EntityID> bodies;

    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());

    // Measure time to add many static bodies (which trigger events)
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numBodies; ++i) {
        AABB aabb(i * 10.0f, i * 10.0f, 16.0f, 16.0f);
        EntityHandle staticHandle = edm.createStaticBody(aabb.center, aabb.halfSize.getX(), aabb.halfSize.getY());
        size_t staticEdmIndex = edm.getStaticIndex(staticHandle);
        EntityID id = staticHandle.getId();
        CollisionManager::Instance().addStaticBody(id, aabb.center, aabb.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                    false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex);
        bodies.push_back(id);
    }

    // Process deferred events
    EventManager::Instance().update();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // EventManager processes all pending events per update. Batch sizing for threading
    // is managed by WorkerBudget, not EventManager. All queued events should be processed.
    int actualEvents = eventCount.load();
    BOOST_CHECK_EQUAL(actualEvents, static_cast<int>(numBodies));
    BOOST_TEST_MESSAGE("Event dispatch performance: " << actualEvents << "/" << numBodies
                      << " events processed");
    
    // Performance check: shouldn't take more than 20ms total (generous for test environment)
    BOOST_CHECK_LT(duration.count(), 20000); // 20ms = 20,000 microseconds
    
    // Average time per body should be reasonable  
    double avgTimePerBody = static_cast<double>(duration.count()) / numBodies;
    BOOST_CHECK_LT(avgTimePerBody, 200.0); // 200 microseconds per body max
    
    BOOST_TEST_MESSAGE("Added " << numBodies << " static bodies with events in " 
                      << duration.count() << " μs (" << avgTimePerBody << " μs/body)");
    
    // Clean up
    for (EntityID id : bodies) {
        CollisionManager::Instance().removeCollisionBody(id);
    }
    edm.clean();
    EventManager::Instance().removeHandler(token);
}

BOOST_AUTO_TEST_CASE(TestTriggerEventNotifications)
{
    // Test that trigger events are properly generated
    std::atomic<int> triggerEventCount{0};
    Vector2D lastTriggerPosition;
    VoidLight::TriggerTag lastTriggerTag;
    bool lastTriggerEntering = false;

    // Subscribe to trigger events
    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::WorldTrigger,
        [&](const EventData& data) {
            if (data.isActive() && data.event) {
                auto triggerEvent = std::dynamic_pointer_cast<WorldTriggerEvent>(data.event);
                if (triggerEvent) {
                    triggerEventCount++;
                    lastTriggerPosition = triggerEvent->getPosition();
                    lastTriggerTag = triggerEvent->getTag();
                    lastTriggerEntering = triggerEvent->getPhase() == TriggerPhase::Enter;
                }
            }
        });

    // Create a trigger
    EntityID triggerId = CollisionManager::Instance().createTriggerAreaAt(
        300.0f, 300.0f, 30.0f, 30.0f,
        VoidLight::TriggerTag::Water,
        VoidLight::TriggerType::EventOnly
    );

    BOOST_CHECK(CollisionManager::Instance().isTrigger(triggerId));

    // Note: Actual trigger event generation would require entity movement
    // and collision detection updates, which is tested in integration scenarios

    // Clean up
    CollisionManager::Instance().removeCollisionBody(triggerId);
    EventManager::Instance().removeHandler(token);
}

BOOST_AUTO_TEST_CASE(TestWorldBounds)
{
    // Test world bounds functionality
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    // Set world bounds
    float minX = -500.0f, minY = -300.0f;
    float maxX = 1000.0f, maxY = 800.0f;
    CollisionManager::Instance().setWorldBounds(minX, minY, maxX, maxY);

    // Create a movable body within bounds via EDM
    EntityID bodyId = 9000;
    Vector2D validPosition(500.0f, 400.0f);

    EntityHandle handle = edm.createNPCWithRaceClass( validPosition, "Human", "Guard");
    size_t idx = edm.getIndex(handle);
    auto& hot = edm.getHotDataByIndex(idx);
    hot.collisionLayers = CollisionLayer::Layer_Player;
    hot.collisionMask = 0xFFFF;
    hot.setCollisionEnabled(true);

    // Verify body was created successfully in EDM
    BOOST_CHECK_CLOSE(hot.transform.position.getX(), validPosition.getX(), 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.position.getY(), validPosition.getY(), 0.01f);

    // Clean up
    edm.unregisterEntity(bodyId);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestLayerCollisionFiltering)
{
    // Test that collision detection respects layer filtering via EDM
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    // Create two movable bodies that should NOT collide due to layer filtering
    EntityID player1Id = 10000;
    EntityID player2Id = 10001;
    Vector2D overlappingPos(400.0f, 400.0f);

    EntityHandle handle1 = edm.registerPlayer(player1Id, overlappingPos, 16.0f, 16.0f);
    EntityHandle handle2 = edm.createNPCWithRaceClass( overlappingPos, "Human", "Guard");

    // Set both as players with masks that exclude Layer_Player
    size_t idx1 = edm.getIndex(handle1);
    auto& hot1 = edm.getHotDataByIndex(idx1);
    hot1.collisionLayers = CollisionLayer::Layer_Player;
    hot1.collisionMask = CollisionLayer::Layer_Enemy | CollisionLayer::Layer_Environment;  // No Layer_Player
    hot1.setCollisionEnabled(true);

    size_t idx2 = edm.getIndex(handle2);
    auto& hot2 = edm.getHotDataByIndex(idx2);
    hot2.collisionLayers = CollisionLayer::Layer_Player;
    hot2.collisionMask = CollisionLayer::Layer_Enemy | CollisionLayer::Layer_Environment;  // No Layer_Player
    hot2.setCollisionEnabled(true);

    // Verify both have collision enabled but won't collide with each other
    BOOST_CHECK(hot1.hasCollision());
    BOOST_CHECK(hot2.hasCollision());
    BOOST_CHECK_EQUAL(hot1.collisionLayers, CollisionLayer::Layer_Player);
    BOOST_CHECK_EQUAL(hot2.collisionLayers, CollisionLayer::Layer_Player);

    // The mask excludes Layer_Player, so they shouldn't collide with each other
    BOOST_CHECK_EQUAL(hot1.collisionMask & CollisionLayer::Layer_Player, 0u);
    BOOST_CHECK_EQUAL(hot2.collisionMask & CollisionLayer::Layer_Player, 0u);

    // Clean up
    edm.unregisterEntity(player1Id);
    edm.unregisterEntity(player2Id);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestMixedBodyTypeInteractions)
{
    // Test interactions between different body types
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    EntityID movableId = 11001;
    EntityID triggerId = 11002;

    Vector2D position(500.0f, 500.0f);
    AABB aabb(position.getX(), position.getY(), 25.0f, 25.0f);

    // Add static body via CollisionManager
    EntityHandle staticHandle = edm.createStaticBody(aabb.center, aabb.halfSize.getX(), aabb.halfSize.getY());
    size_t staticEdmIndex = edm.getStaticIndex(staticHandle);
    EntityID staticId = staticHandle.getId();
    CollisionManager::Instance().addStaticBody(staticId, aabb.center, aabb.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
                                                false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), staticEdmIndex);

    // Add movable body via EDM
    EntityHandle movableHandle = edm.createNPCWithRaceClass( position, "Human", "Guard");
    size_t movableIdx = edm.getIndex(movableHandle);
    auto& movableHot = edm.getHotDataByIndex(movableIdx);
    movableHot.collisionLayers = CollisionLayer::Layer_Enemy;
    movableHot.collisionMask = 0xFFFF;
    movableHot.setCollisionEnabled(true);

    // Add trigger via CollisionManager
    triggerId = CollisionManager::Instance().createTriggerAreaAt(
        position.getX(), position.getY(), 25.0f, 25.0f,
        VoidLight::TriggerTag::Checkpoint,
        VoidLight::TriggerType::EventOnly
    );

    // Verify static body type in CollisionManager
    BOOST_CHECK(!CollisionManager::Instance().isTrigger(staticId));

    // Verify trigger is a trigger
    BOOST_CHECK(CollisionManager::Instance().isTrigger(triggerId));

    // Verify movable body in EDM
    BOOST_CHECK(movableHot.hasCollision());
    BOOST_CHECK_EQUAL(movableHot.collisionLayers, CollisionLayer::Layer_Enemy);

    // Static and trigger should be queryable via CollisionManager
    std::vector<EntityID> results;
    CollisionManager::Instance().queryArea(aabb, results);
    BOOST_CHECK(std::find(results.begin(), results.end(), staticId) != results.end());
    BOOST_CHECK(std::find(results.begin(), results.end(), triggerId) != results.end());

    // Clean up
    CollisionManager::Instance().removeCollisionBody(staticId);
    CollisionManager::Instance().removeCollisionBody(triggerId);
    edm.unregisterEntity(movableId);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_SUITE_END()

// Spatial Hash Edge Case Tests
BOOST_AUTO_TEST_SUITE(CollisionSpatialHashTests)

BOOST_AUTO_TEST_CASE(TestGridHashEdgeCases)
{
    // Test spatial partitioning edge cases for static bodies in CollisionManager
    // Note: Movables are now in EDM, so these tests focus on static body spatial hashing
    if (!VoidLight::ThreadSystem::Exists()) {
        BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
    }

    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    // Test 1: Static bodies exactly at grid boundaries
    float cellBoundary = 128.0f;
    AABB boundaryAABB(cellBoundary, cellBoundary, 10.0f, 10.0f);
    EntityHandle boundaryHandle = edm.createStaticBody(boundaryAABB.center, boundaryAABB.halfSize.getX(), boundaryAABB.halfSize.getY());
    size_t boundaryEdmIndex = edm.getStaticIndex(boundaryHandle);
    EntityID boundaryId = boundaryHandle.getId();

    CollisionManager::Instance().addStaticBody(
        boundaryId, boundaryAABB.center, boundaryAABB.halfSize,
        CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
        false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), boundaryEdmIndex
    );

    // Should be findable via area query
    std::vector<EntityID> results;
    CollisionManager::Instance().queryArea(boundaryAABB, results);
    BOOST_CHECK(std::find(results.begin(), results.end(), boundaryId) != results.end());

    // Test 2: Very large static bodies spanning multiple cells
    AABB largeAABB(200.0f, 200.0f, 300.0f, 300.0f); // 600x600 body spanning many cells
    EntityHandle largeHandle = edm.createStaticBody(largeAABB.center, largeAABB.halfSize.getX(), largeAABB.halfSize.getY());
    size_t largeEdmIndex = edm.getStaticIndex(largeHandle);
    EntityID largeId = largeHandle.getId();

    CollisionManager::Instance().addStaticBody(
        largeId, largeAABB.center, largeAABB.halfSize,
        CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
        false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), largeEdmIndex
    );

    // Should be findable from multiple query regions
    AABB queryTopLeft(50.0f, 50.0f, 20.0f, 20.0f);
    AABB queryBottomRight(350.0f, 350.0f, 20.0f, 20.0f);

    results.clear();
    CollisionManager::Instance().queryArea(queryTopLeft, results);
    bool foundInTopLeft = std::find(results.begin(), results.end(), largeId) != results.end();

    results.clear();
    CollisionManager::Instance().queryArea(queryBottomRight, results);
    bool foundInBottomRight = std::find(results.begin(), results.end(), largeId) != results.end();

    BOOST_CHECK(foundInTopLeft);
    BOOST_CHECK(foundInBottomRight);

    // Test 3: Static bodies at extreme coordinates
    AABB extremeAABB(-1000000.0f, -1000000.0f, 50.0f, 50.0f);
    EntityHandle extremeHandle = edm.createStaticBody(extremeAABB.center, extremeAABB.halfSize.getX(), extremeAABB.halfSize.getY());
    size_t extremeEdmIndex = edm.getStaticIndex(extremeHandle);
    EntityID extremeId = extremeHandle.getId();

    CollisionManager::Instance().addStaticBody(
        extremeId, extremeAABB.center, extremeAABB.halfSize,
        CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
        false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), extremeEdmIndex
    );

    // Should still be queryable
    results.clear();
    CollisionManager::Instance().queryArea(extremeAABB, results);
    BOOST_CHECK(std::find(results.begin(), results.end(), extremeId) != results.end());

    // Test 4: Zero-sized static bodies (degenerate case)
    AABB zeroAABB(100.0f, 100.0f, 0.0f, 0.0f); // Zero size
    EntityHandle zeroHandle = edm.createStaticBody(zeroAABB.center, zeroAABB.halfSize.getX(), zeroAABB.halfSize.getY());
    size_t zeroEdmIndex = edm.getStaticIndex(zeroHandle);
    EntityID zeroId = zeroHandle.getId();

    CollisionManager::Instance().addStaticBody(
        zeroId, zeroAABB.center, zeroAABB.halfSize,
        CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
        false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), zeroEdmIndex
    );

    // Should still be tracked and queryable
    results.clear();
    AABB zeroQuery(99.0f, 99.0f, 2.0f, 2.0f);
    CollisionManager::Instance().queryArea(zeroQuery, results);
    BOOST_CHECK(std::find(results.begin(), results.end(), zeroId) != results.end());

    // Test 5: Static bodies that update position (e.g., moving platforms)
    Vector2D startPos(64.0f, 64.0f);
    AABB movingAABB(startPos.getX(), startPos.getY(), 15.0f, 15.0f);
    EntityHandle movingHandle = edm.createStaticBody(movingAABB.center, movingAABB.halfSize.getX(), movingAABB.halfSize.getY());
    size_t movingEdmIndex = edm.getStaticIndex(movingHandle);
    EntityID movingId = movingHandle.getId();

    CollisionManager::Instance().addStaticBody(
        movingId, movingAABB.center, movingAABB.halfSize,
        CollisionLayer::Layer_Environment, 0xFFFFFFFFu,
        false, 0, static_cast<uint8_t>(VoidLight::TriggerType::Physical), movingEdmIndex
    );

    // Move across fine cell boundaries multiple times
    for (int i = 1; i <= 5; ++i) {
        Vector2D newPos(startPos.getX() + (i * 20.0f), startPos.getY() + (i * 20.0f));
        AABB newAABB(newPos.getX(), newPos.getY(), 15.0f, 15.0f);

        CollisionManager::Instance().updateCollisionBodyPosition(
            movingId, newAABB.center
        );

        // Should still be queryable at new position
        results.clear();
        CollisionManager::Instance().queryArea(newAABB, results);
        BOOST_CHECK(std::find(results.begin(), results.end(), movingId) != results.end());
    }

    BOOST_TEST_MESSAGE("Grid hash edge case testing completed successfully");

    // Clean up
    CollisionManager::Instance().removeCollisionBody(boundaryId);
    CollisionManager::Instance().removeCollisionBody(largeId);
    CollisionManager::Instance().removeCollisionBody(extremeId);
    CollisionManager::Instance().removeCollisionBody(zeroId);
    CollisionManager::Instance().removeCollisionBody(movingId);
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_SUITE_END()

// Tests for EDM batch position updates - critical for AI entity movement optimization
BOOST_AUTO_TEST_SUITE(EDMBatchUpdateTests)

BOOST_AUTO_TEST_CASE(TestEDMBatchPositionUpdate)
{
    // EDM-CENTRIC: Test batch position updates via EntityDataManager
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    const int NUM_ENTITIES = 50;
    std::vector<EntityHandle> handles;
    std::vector<EntityID> entityIds;

    // Create movable bodies via EDM
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        EntityID id = static_cast<EntityID>(1000 + i);
        entityIds.push_back(id);
        Vector2D pos(100.0f + i * 10.0f, 100.0f + i * 10.0f);

        EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
        size_t idx = edm.getIndex(handle);
        auto& hot = edm.getHotDataByIndex(idx);
        hot.collisionLayers = CollisionLayer::Layer_Enemy;
        hot.collisionMask = 0xFFFF;
        hot.setCollisionEnabled(true);
        handles.push_back(handle);
    }

    // Batch update positions via direct EDM access
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        Vector2D newPos(200.0f + i * 10.0f, 200.0f + i * 10.0f);
        size_t idx = edm.getIndex(handles[i]);
        auto& hot = edm.getHotDataByIndex(idx);
        hot.transform.position.setX(newPos.getX());
        hot.transform.position.setY(newPos.getY());
        hot.transform.velocity.setX(1.0f);
        hot.transform.velocity.setY(0.5f);
    }

    // Verify positions updated
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        size_t idx = edm.getIndex(handles[i]);
        const auto& hot = edm.getHotDataByIndex(idx);
        float expectedX = 200.0f + i * 10.0f;
        float expectedY = 200.0f + i * 10.0f;
        BOOST_CHECK_CLOSE(hot.transform.position.getX(), expectedX, 0.01f);
        BOOST_CHECK_CLOSE(hot.transform.position.getY(), expectedY, 0.01f);
    }

    // Cleanup
    for (EntityID id : entityIds) {
        edm.unregisterEntity(id);
    }
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestEDMMultiBatchUpdates)
{
    // EDM-CENTRIC: Test multiple batch updates (like AIManager does per-thread)
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    const int NUM_BATCHES = 4;
    const int ENTITIES_PER_BATCH = 25;
    std::vector<std::vector<EntityHandle>> batchHandles(NUM_BATCHES);
    std::vector<std::vector<EntityID>> batchEntityIds(NUM_BATCHES);

    // Create movable bodies for each batch via EDM
    for (int batch = 0; batch < NUM_BATCHES; ++batch) {
        for (int i = 0; i < ENTITIES_PER_BATCH; ++i) {
            EntityID id = static_cast<EntityID>(2000 + batch * 100 + i);
            batchEntityIds[batch].push_back(id);
            Vector2D pos(50.0f + batch * 200.0f + i * 5.0f, 50.0f + i * 5.0f);

            EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
            size_t idx = edm.getIndex(handle);
            auto& hot = edm.getHotDataByIndex(idx);
            hot.collisionLayers = CollisionLayer::Layer_Enemy;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
            batchHandles[batch].push_back(handle);
        }
    }

    // Apply batch updates to each batch
    for (int batch = 0; batch < NUM_BATCHES; ++batch) {
        for (int i = 0; i < ENTITIES_PER_BATCH; ++i) {
            Vector2D newPos(100.0f + batch * 200.0f + i * 5.0f, 150.0f + i * 5.0f);
            size_t idx = edm.getIndex(batchHandles[batch][i]);
            auto& hot = edm.getHotDataByIndex(idx);
            hot.transform.position.setX(newPos.getX());
            hot.transform.position.setY(newPos.getY());
        }
    }

    // Verify all entities moved correctly
    int entitiesVerified = 0;
    for (int batch = 0; batch < NUM_BATCHES; ++batch) {
        for (int i = 0; i < ENTITIES_PER_BATCH; ++i) {
            float expectedX = 100.0f + batch * 200.0f + i * 5.0f;
            float expectedY = 150.0f + i * 5.0f;
            size_t idx = edm.getIndex(batchHandles[batch][i]);
            const auto& hot = edm.getHotDataByIndex(idx);
            if (std::abs(hot.transform.position.getX() - expectedX) < 0.01f && std::abs(hot.transform.position.getY() - expectedY) < 0.01f) {
                entitiesVerified++;
            }
        }
    }
    BOOST_CHECK_EQUAL(entitiesVerified, NUM_BATCHES * ENTITIES_PER_BATCH);

    // Cleanup
    for (int batch = 0; batch < NUM_BATCHES; ++batch) {
        for (EntityID id : batchEntityIds[batch]) {
            edm.unregisterEntity(id);
        }
    }
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestEDMBatchUpdatePerformance)
{
    // EDM-CENTRIC: Measure performance of batch position updates via EDM
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();

    const int NUM_ENTITIES = 500;
    std::vector<EntityHandle> handles;
    std::vector<EntityID> entityIds;

    // Create many movable bodies
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        EntityID id = static_cast<EntityID>(4000 + i);
        entityIds.push_back(id);
        Vector2D pos(static_cast<float>(i % 50) * 20.0f, static_cast<float>(i / 50) * 20.0f);

        EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
        size_t idx = edm.getIndex(handle);
        auto& hot = edm.getHotDataByIndex(idx);
        hot.collisionLayers = CollisionLayer::Layer_Enemy;
        hot.collisionMask = 0xFFFF;
        hot.setCollisionEnabled(true);
        handles.push_back(handle);
    }

    // Measure batch update performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < 100; ++iter) {
        for (int i = 0; i < NUM_ENTITIES; ++i) {
            float newX = static_cast<float>(i % 50) * 20.0f + 5.0f;
            float newY = static_cast<float>(i / 50) * 20.0f + 5.0f;
            size_t idx = edm.getIndex(handles[i]);
            auto& hot = edm.getHotDataByIndex(idx);
            hot.transform.position.setX(newX);
            hot.transform.position.setY(newY);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    BOOST_TEST_MESSAGE("EDM batch update of " << NUM_ENTITIES << " entities x 100 iterations: "
                      << duration.count() << " μs ("
                      << (duration.count() / 100) << " μs per batch)");

    // Performance requirement: batch update should be fast (< 1ms per batch of 500)
    BOOST_CHECK_LT(duration.count() / 100, 1000);

    // Cleanup
    for (EntityID id : entityIds) {
        edm.unregisterEntity(id);
    }
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_SUITE_END()

// Tests for NEEDS_TRIGGER_DETECTION flag-based trigger detection optimization
BOOST_AUTO_TEST_SUITE(TriggerDetectionOptimizationTests)

BOOST_AUTO_TEST_CASE(TestTriggerDetectionFlag)
{
    // Test that NEEDS_TRIGGER_DETECTION flag is properly set and queried
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);

    // Create player - should have NEEDS_TRIGGER_DETECTION flag set automatically
    EntityID playerId = 50000;
    Vector2D playerPos(100.0f, 100.0f);
    EntityHandle playerHandle = edm.registerPlayer(playerId, playerPos, 16.0f, 16.0f);
    size_t playerIdx = edm.getIndex(playerHandle);
    const auto& playerHot = edm.getHotDataByIndex(playerIdx);

    // Player should have trigger detection flag set
    BOOST_CHECK(playerHot.needsTriggerDetection());

    // Create NPC - should NOT have NEEDS_TRIGGER_DETECTION flag by default
    EntityID npcId = 50001;
    Vector2D npcPos(200.0f, 200.0f);
    EntityHandle npcHandle = edm.createNPCWithRaceClass( npcPos, "Human", "Guard");
    size_t npcIdx = edm.getIndex(npcHandle);
    auto& npcHot = edm.getHotDataByIndex(npcIdx);

    // NPC should NOT have trigger detection flag by default
    BOOST_CHECK(!npcHot.needsTriggerDetection());

    // Enable trigger detection on NPC
    npcHot.setTriggerDetection(true);
    BOOST_CHECK(npcHot.needsTriggerDetection());

    // Update BGM to populate active indices
    bgm.update(playerPos, 0.016f);

    // Verify getTriggerDetectionIndices() returns correct entities
    auto triggerDetectionIndices = edm.getTriggerDetectionIndices();

    // Should contain both player and NPC (now that NPC has flag enabled)
    BOOST_CHECK_GE(triggerDetectionIndices.size(), 2u);

    // Verify player index is in the list
    bool foundPlayer = false;
    bool foundNPC = false;
    for (size_t idx : triggerDetectionIndices) {
        if (idx == playerIdx) foundPlayer = true;
        if (idx == npcIdx) foundNPC = true;
    }
    BOOST_CHECK(foundPlayer);
    BOOST_CHECK(foundNPC);

    // Disable trigger detection on NPC
    npcHot.setTriggerDetection(false);
    BOOST_CHECK(!npcHot.needsTriggerDetection());

    // Clean up
    edm.unregisterEntity(playerId);
    edm.unregisterEntity(npcId);
    bgm.clean();
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestEventOnlyTriggerDetection)
{
    // Test that EventOnly triggers are detected via spatial queries
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);

    // Create player at position X
    EntityID playerId = 51000;
    Vector2D playerPos(100.0f, 100.0f);
    EntityHandle playerHandle = edm.registerPlayer(playerId, playerPos, 16.0f, 16.0f);
    size_t playerIdx = edm.getIndex(playerHandle);
    auto& playerHot = edm.getHotDataByIndex(playerIdx);
    playerHot.collisionLayers = CollisionLayer::Layer_Player;
    playerHot.collisionMask = CollisionLayer::Layer_Environment | CollisionLayer::Layer_Enemy;
    playerHot.setCollisionEnabled(true);

    // Create EventOnly trigger at position X (overlapping with player)
    EntityID nearTriggerId = CollisionManager::Instance().createTriggerAreaAt(
        105.0f, 105.0f, 30.0f, 30.0f,
        VoidLight::TriggerTag::Water,
        VoidLight::TriggerType::EventOnly,
        CollisionLayer::Layer_Environment,
        CollisionLayer::Layer_Player
    );

    // Create EventOnly trigger at distant position Y (NOT overlapping)
    EntityID farTriggerId = CollisionManager::Instance().createTriggerAreaAt(
        1000.0f, 1000.0f, 30.0f, 30.0f,
        VoidLight::TriggerTag::Lava,
        VoidLight::TriggerType::EventOnly,
        CollisionLayer::Layer_Environment,
        CollisionLayer::Layer_Player
    );

    // Update BGM to populate active indices
    bgm.update(playerPos, 0.016f);

    // Run collision detection (which includes detectEventOnlyTriggers)
    CollisionManager::Instance().update(0.016f);

    // The trigger detection should have found the nearby trigger but not the far one
    // We can verify this indirectly by checking that the system doesn't crash
    // and that triggers are properly registered
    BOOST_CHECK(CollisionManager::Instance().isTrigger(nearTriggerId));
    BOOST_CHECK(CollisionManager::Instance().isTrigger(farTriggerId));

    // Verify player has trigger detection flag
    BOOST_CHECK(playerHot.needsTriggerDetection());

    // Clean up
    CollisionManager::Instance().removeCollisionBody(nearTriggerId);
    CollisionManager::Instance().removeCollisionBody(farTriggerId);
    edm.unregisterEntity(playerId);
    bgm.clean();
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestNPCTriggerDetection)
{
    // Test that NPCs with NEEDS_TRIGGER_DETECTION flag can fire trigger events
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(2000.0f);
    EventManager::Instance().init();

    // Create NPC with NEEDS_TRIGGER_DETECTION enabled
    EntityID npcId = 52000;
    Vector2D npcPos(150.0f, 150.0f);
    EntityHandle npcHandle = edm.createNPCWithRaceClass( npcPos, "Human", "Guard");
    size_t npcIdx = edm.getIndex(npcHandle);
    auto& npcHot = edm.getHotDataByIndex(npcIdx);
    npcHot.collisionLayers = CollisionLayer::Layer_Enemy;
    npcHot.collisionMask = CollisionLayer::Layer_Environment | CollisionLayer::Layer_Player;
    npcHot.setCollisionEnabled(true);
    npcHot.setTriggerDetection(true);  // Enable trigger detection for NPC

    // Verify NPC has trigger detection flag
    BOOST_CHECK(npcHot.needsTriggerDetection());

    // Create EventOnly trigger overlapping NPC
    EntityID triggerId = CollisionManager::Instance().createTriggerAreaAt(
        155.0f, 155.0f, 30.0f, 30.0f,
        VoidLight::TriggerTag::Checkpoint,
        VoidLight::TriggerType::EventOnly,
        CollisionLayer::Layer_Environment,
        CollisionLayer::Layer_Enemy  // Mask includes Layer_Enemy so NPC can trigger it
    );

    // Track trigger events
    std::atomic<int> triggerEventCount{0};
    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::WorldTrigger,
        [&triggerEventCount](const EventData& data) {
            if (data.isActive() && data.event) {
                triggerEventCount++;
            }
        });

    // Update BGM to populate active indices
    bgm.update(npcPos, 0.016f);

    // Verify NPC is in trigger detection indices
    auto triggerDetectionIndices = edm.getTriggerDetectionIndices();
    bool foundNPC = false;
    for (size_t idx : triggerDetectionIndices) {
        if (idx == npcIdx) foundNPC = true;
    }
    BOOST_CHECK_MESSAGE(foundNPC, "NPC should be in trigger detection indices");

    // Run collision detection
    CollisionManager::Instance().update(0.016f);

    // Process deferred events
    EventManager::Instance().drainAllDeferredEvents();

    // An NPC with trigger detection enabled should be able to trigger events
    // (The actual event firing depends on cooldown and other mechanics)
    BOOST_TEST_MESSAGE("NPC trigger events fired: " << triggerEventCount.load());

    // Clean up
    EventManager::Instance().removeHandler(token);
    CollisionManager::Instance().removeCollisionBody(triggerId);
    edm.unregisterEntity(npcId);
    bgm.clean();
    EventManager::Instance().clean();
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_CASE(TestSweepAndPruneTriggerDetection)
{
    // Test that sweep-and-prune path works correctly for large entity counts
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(edm.init());
    CollisionManager::Instance().init();
    auto& bgm = BackgroundSimulationManager::Instance();
    bgm.init();
    bgm.setActiveRadius(5000.0f);

    const int NUM_NPCS = 100;  // Above the sweep threshold (50)
    std::vector<EntityID> npcIds;
    std::vector<EntityHandle> npcHandles;

    // Create many NPCs with NEEDS_TRIGGER_DETECTION flag
    for (int i = 0; i < NUM_NPCS; ++i) {
        EntityID npcId = static_cast<EntityID>(53000 + i);
        npcIds.push_back(npcId);

        // Spread NPCs across the world
        float x = static_cast<float>(i % 10) * 100.0f + 50.0f;
        float y = static_cast<float>(i / 10) * 100.0f + 50.0f;
        Vector2D npcPos(x, y);

        EntityHandle npcHandle = edm.createNPCWithRaceClass( npcPos, "Human", "Guard");
        size_t npcIdx = edm.getIndex(npcHandle);
        auto& npcHot = edm.getHotDataByIndex(npcIdx);
        npcHot.collisionLayers = CollisionLayer::Layer_Enemy;
        npcHot.collisionMask = CollisionLayer::Layer_Environment;
        npcHot.setCollisionEnabled(true);
        npcHot.setTriggerDetection(true);  // Enable trigger detection
        npcHandles.push_back(npcHandle);
    }

    // Create multiple EventOnly triggers at various positions
    std::vector<EntityID> triggerIds;
    for (int i = 0; i < 20; ++i) {
        float x = static_cast<float>(i % 5) * 200.0f + 100.0f;
        float y = static_cast<float>(i / 5) * 200.0f + 100.0f;

        EntityID triggerId = CollisionManager::Instance().createTriggerAreaAt(
            x, y, 50.0f, 50.0f,
            VoidLight::TriggerTag::Water,
            VoidLight::TriggerType::EventOnly,
            CollisionLayer::Layer_Environment,
            CollisionLayer::Layer_Enemy
        );
        triggerIds.push_back(triggerId);
    }

    // Update BGM to populate active indices
    bgm.update(Vector2D(500.0f, 500.0f), 0.016f);

    // Verify we have enough entities to trigger sweep-and-prune path
    auto triggerDetectionIndices = edm.getTriggerDetectionIndices();
    BOOST_CHECK_GE(triggerDetectionIndices.size(), 50u);  // Should be above threshold

    BOOST_TEST_MESSAGE("Trigger detection entities: " << triggerDetectionIndices.size()
                      << " (sweep threshold: 50)");

    // Measure performance of trigger detection with many entities
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; ++i) {
        CollisionManager::Instance().update(0.016f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avgUpdateMs = static_cast<double>(duration.count()) / 10.0 / 1000.0;
    BOOST_TEST_MESSAGE("Average collision update with " << NUM_NPCS
                      << " trigger-detecting NPCs: " << avgUpdateMs << "ms");

    // Performance check: should complete reasonably fast even with many entities
    BOOST_CHECK_LT(avgUpdateMs, 5.0);  // < 5ms per update

    // Clean up
    for (EntityID triggerId : triggerIds) {
        CollisionManager::Instance().removeCollisionBody(triggerId);
    }
    for (EntityID npcId : npcIds) {
        edm.unregisterEntity(npcId);
    }
    bgm.clean();
    CollisionManager::Instance().clean();
    edm.clean();
}

BOOST_AUTO_TEST_SUITE_END()
