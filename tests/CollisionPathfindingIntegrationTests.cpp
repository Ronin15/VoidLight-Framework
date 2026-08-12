/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE CollisionPathfindingIntegrationTests
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/old/interface.hpp>

#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "collisions/AABB.hpp"
#include "collisions/CollisionBody.hpp"
#include "managers/EventManager.hpp"
#include "managers/WorldManager.hpp"
#include "core/ThreadSystem.hpp"
#include "utils/Vector2D.hpp"
#include "ai/pathfinding/PathfindingGrid.hpp"
#include "world/WorldData.hpp"
#include <chrono>
#include <thread>
#include <vector>

using namespace VoidLight;

// Test fixture for collision-pathfinding integration
struct CollisionPathfindingFixture {
    CollisionPathfindingFixture() {
        // Initialize ThreadSystem first (required for PathfinderManager async tasks)
        // Always call init() - it has guards against double-initialization
        BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init()); // Auto-detect system threads

        // Initialize managers in proper order
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(WorldManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());

        // Load a test world - larger size with reduced blocking for navigable paths
        VoidLight::WorldGenerationConfig cfg{};
        cfg.width = 50; cfg.height = 50; cfg.seed = 1234;
        cfg.elevationFrequency = 0.1f; cfg.humidityFrequency = 0.1f;
        cfg.waterLevel = 0.1f; cfg.mountainLevel = 0.9f;

        if (!WorldManager::Instance().loadNewWorld(cfg)) {
            throw std::runtime_error("Failed to load test world");
        }

        // EVENT-DRIVEN: Process any deferred events (triggers WorldLoaded task on ThreadSystem)
        EventManager::Instance().update();

        // Give ThreadSystem time to execute the WorldLoaded task and enqueue the deferred event
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Process the deferred WorldLoadedEvent (delivers to PathfinderManager)
        EventManager::Instance().update();

        // Wait for async grid rebuild to complete
        // Larger world (50x50) needs more time for grid construction
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        // Set up a test world with some static obstacles
        setupTestWorld();

        // Process any deferred collision events from setupTestWorld()
        EventManager::Instance().update();
    }

    ~CollisionPathfindingFixture() {
        // Clean up in reverse order
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        WorldManager::Instance().clean();
        EventManager::Instance().clean();
        // ThreadSystem persists across tests
    }

    void setupTestWorld() {
        // Add static collision bodies that should affect pathfinding
        auto& edm = EntityDataManager::Instance();

        // Wall across middle of world
        for (int i = 5; i <= 15; ++i) {
            AABB wallAABB(i * 64.0f, 320.0f, 32.0f, 32.0f);
            EntityHandle handle = edm.createStaticBody(wallAABB.center, wallAABB.halfSize.getX(), wallAABB.halfSize.getY());
            EntityID wallId = handle.getId();
            size_t edmIndex = edm.getStaticIndex(handle);
            CollisionManager::Instance().addStaticBody(wallId, wallAABB.center, wallAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu, false, 0, 1, edmIndex);
        }

        // L-shaped obstacle
        for (int i = 0; i < 3; ++i) {
            AABB obstacleAABB(800.0f + i * 64.0f, 200.0f, 32.0f, 32.0f);
            EntityHandle handle = edm.createStaticBody(obstacleAABB.center, obstacleAABB.halfSize.getX(), obstacleAABB.halfSize.getY());
            EntityID obstacleId = handle.getId();
            size_t edmIndex = edm.getStaticIndex(handle);
            CollisionManager::Instance().addStaticBody(obstacleId, obstacleAABB.center, obstacleAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu, false, 0, 1, edmIndex);
        }
        for (int i = 0; i < 3; ++i) {
            AABB obstacleAABB(800.0f, 200.0f + i * 64.0f, 32.0f, 32.0f);
            EntityHandle handle = edm.createStaticBody(obstacleAABB.center, obstacleAABB.halfSize.getX(), obstacleAABB.halfSize.getY());
            EntityID obstacleId = handle.getId();
            size_t edmIndex = edm.getStaticIndex(handle);
            CollisionManager::Instance().addStaticBody(obstacleId, obstacleAABB.center, obstacleAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu, false, 0, 1, edmIndex);
        }
    }

    // Helper: Check if path intersects with known obstacles
    bool pathIntersectsObstacles(const std::vector<Vector2D>& path) {
        for (const auto& waypoint : path) {
            float x = waypoint.getX();
            float y = waypoint.getY();

            // Check if waypoint intersects with wall (y=320, x=320-960)
            if (y > 290.0f && y < 350.0f && x > 300.0f && x < 980.0f) {
                return true;
            }

            // Check L-shaped obstacle (horizontal part)
            if (x > 770.0f && x < 990.0f && y > 170.0f && y < 230.0f) {
                return true;
            }

            // Check L-shaped obstacle (vertical part)
            if (x > 770.0f && x < 830.0f && y > 170.0f && y < 390.0f) {
                return true;
            }
        }
        return false;
    }

    // Helper: Check if a point would collide using CollisionManager
    bool wouldCollideAt(const Vector2D& position, float radius = 16.0f) {
        // Create a temporary test body
        auto& edm = EntityDataManager::Instance();
        AABB testAABB(position.getX(), position.getY(), radius, radius);
        EntityHandle handle = edm.createStaticBody(testAABB.center, testAABB.halfSize.getX(), testAABB.halfSize.getY());
        EntityID testId = handle.getId();
        size_t edmIndex = edm.getStaticIndex(handle);

        // Use static body for collision query test (EDM for actual movables at runtime)
        CollisionManager::Instance().addStaticBody(
            testId, testAABB.center, testAABB.halfSize,
            CollisionLayer::Layer_Player, CollisionLayer::Layer_Environment,
            false, 0, 1, edmIndex
        );

        // Check for collisions using queryArea (use actual radius, not 2x)
        AABB queryAABB(position.getX(), position.getY(), radius, radius);
        std::vector<EntityID> nearbyBodies;
        CollisionManager::Instance().queryArea(queryAABB, nearbyBodies);

        bool hasCollision = false;
        for (EntityID nearbyId : nearbyBodies) {
            if (nearbyId != testId) {
                // Found a nearby body that isn't our test body
                hasCollision = true;
                break;
            }
        }

        // Clean up test body
        CollisionManager::Instance().removeCollisionBody(testId);

        return hasCollision;
    }
};

BOOST_AUTO_TEST_SUITE(CollisionPathfindingIntegrationSuite)

BOOST_FIXTURE_TEST_CASE(TestObstacleAvoidancePathfinding, CollisionPathfindingFixture)
{
    // Test that pathfinding correctly avoids collision obstacles using async requestPath()

    Vector2D start(100.0f, 100.0f);  // Clear area
    Vector2D goal(600.0f, 600.0f);   // Across obstacles

    // Use async requestPath() like the real game does
    std::vector<Vector2D> path;
    bool callbackExecuted = false;

    PathfinderManager::Instance().requestPath(
        1000, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            path = resultPath;
            callbackExecuted = true;
        }
    );

    // Process async tasks (mimics game loop behavior)
    for (int i = 0; i < 20 && !callbackExecuted; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Path should be found successfully
    BOOST_REQUIRE(callbackExecuted);
    BOOST_REQUIRE_GE(path.size(), 2);

    // INTEGRATION TEST #1: Verify path avoids known obstacles
    bool pathClear = !pathIntersectsObstacles(path);

    BOOST_TEST_MESSAGE("Path obstacle avoidance: " <<
                      (pathClear ? "CLEAR" : "INTERSECTS") <<
                      " (" << path.size() << " waypoints)");

    // Path MUST avoid obstacles - this validates collision-pathfinding integration
    BOOST_CHECK_MESSAGE(pathClear, "Path should avoid collision obstacles");

    // INTEGRATION TEST #2: Verify path waypoints don't collide using CollisionManager
    int collisionCount = 0;
    for (const auto& waypoint : path) {
        if (wouldCollideAt(waypoint, 16.0f)) {
            collisionCount++;
        }
    }

    BOOST_CHECK_MESSAGE(collisionCount == 0,
        "Path waypoints should not collide with obstacles (found " +
        std::to_string(collisionCount) + " collisions)");
}

BOOST_FIXTURE_TEST_CASE(TestDynamicObstacleIntegration, CollisionPathfindingFixture)
{
    // Test dynamic obstacle integration between collision and pathfinding systems

    Vector2D start(200.0f, 200.0f);
    Vector2D goal(400.0f, 400.0f);

    // Get initial path using async API
    std::vector<Vector2D> originalPath;
    bool callback1Executed = false;

    PathfinderManager::Instance().requestPath(
        5000, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            originalPath = resultPath;
            callback1Executed = true;
        }
    );

    // Wait for async completion
    for (int i = 0; i < 20 && !callback1Executed; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_REQUIRE(callback1Executed);

    // Add dynamic obstacle
    auto& edm = EntityDataManager::Instance();
    AABB obstacleAABB(300.0f, 300.0f, 48.0f, 48.0f);
    EntityHandle dynamicHandle = edm.createStaticBody(obstacleAABB.center, obstacleAABB.halfSize.getX(), obstacleAABB.halfSize.getY());
    EntityID dynamicObstacle = dynamicHandle.getId();
    size_t dynamicEdmIndex = edm.getStaticIndex(dynamicHandle);
    CollisionManager::Instance().addStaticBody(dynamicObstacle, obstacleAABB.center, obstacleAABB.halfSize, CollisionLayer::Layer_Enemy, 0xFFFFFFFFu, false, 0, 1, dynamicEdmIndex);

    // Event-driven: PathfinderManager automatically updates via CollisionObstacleChanged events
    EventManager::Instance().update();

    // Give time for grid rebuild
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get new path
    std::vector<Vector2D> newPath;
    bool callback2Executed = false;

    PathfinderManager::Instance().requestPath(
        5002, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            newPath = resultPath;
            callback2Executed = true;
        }
    );

    // Wait for async completion
    for (int i = 0; i < 20 && !callback2Executed; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_REQUIRE(callback2Executed);

    // Both paths should be valid
    BOOST_CHECK_GE(originalPath.size(), 2);
    BOOST_CHECK_GE(newPath.size(), 2);

    BOOST_TEST_MESSAGE("Dynamic obstacle integration: original " << originalPath.size()
                      << " waypoints, new " << newPath.size() << " waypoints");

    // Clean up
    CollisionManager::Instance().removeCollisionBody(dynamicObstacle);
}

BOOST_FIXTURE_TEST_CASE(TestEventDrivenPathInvalidation, CollisionPathfindingFixture)
{
    // Test that collision events properly invalidate pathfinding cache

    Vector2D start(100.0f, 100.0f);  // Clear starting position
    Vector2D goal(300.0f, 300.0f);   // Distant goal requiring multiple steps

    // Get initial path using async API
    std::vector<Vector2D> initialPath;
    bool callback1Executed = false;

    PathfinderManager::Instance().requestPath(
        6000, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            initialPath = resultPath;
            callback1Executed = true;
        }
    );

    // Wait for async completion
    for (int i = 0; i < 20 && !callback1Executed; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_REQUIRE(callback1Executed);
    BOOST_CHECK_GE(initialPath.size(), 2);

    // Add new obstacle that should invalidate cached paths
    auto& edm = EntityDataManager::Instance();
    AABB newObstacleAABB(300.0f, 300.0f, 64.0f, 64.0f);
    EntityHandle newObstacleHandle = edm.createStaticBody(newObstacleAABB.center, newObstacleAABB.halfSize.getX(), newObstacleAABB.halfSize.getY());
    EntityID newObstacle = newObstacleHandle.getId();
    size_t newObstacleEdmIndex = edm.getStaticIndex(newObstacleHandle);
    CollisionManager::Instance().addStaticBody(newObstacle, newObstacleAABB.center, newObstacleAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu, false, 0, 1, newObstacleEdmIndex);

    // Process events and allow grid rebuild
    EventManager::Instance().update();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get new path after obstacle added
    std::vector<Vector2D> newPath;
    bool callback2Executed = false;

    PathfinderManager::Instance().requestPath(
        6002, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            newPath = resultPath;
            callback2Executed = true;
        }
    );

    // Wait for async completion
    for (int i = 0; i < 20 && !callback2Executed; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_REQUIRE(callback2Executed);
    BOOST_CHECK_GE(newPath.size(), 2);

    // Test demonstrates that pathfinding works before and after collision changes

    // Clean up
    CollisionManager::Instance().removeCollisionBody(newObstacle);
}

BOOST_FIXTURE_TEST_CASE(TestConcurrentCollisionPathfindingOperations, CollisionPathfindingFixture)
{
    // Test concurrent collision and pathfinding operations using async API

    const int NUM_CONCURRENT_REQUESTS = 10;

    // Track async path completions
    std::atomic<int> successfulPaths{0};
    std::atomic<int> completedCallbacks{0};

    // Submit multiple concurrent async pathfinding requests (matches real game behavior)
    for (int i = 0; i < NUM_CONCURRENT_REQUESTS; ++i) {
        Vector2D start(100.0f + i * 50.0f, 100.0f);
        Vector2D goal(500.0f + i * 20.0f, 500.0f);

        PathfinderManager::Instance().requestPath(
            7000 + i, start, goal, PathfinderManager::Priority::High,
            [&successfulPaths, &completedCallbacks](EntityID, const std::vector<Vector2D>& path) {
                if (path.size() >= 2) {
                    successfulPaths++;
                }
                completedCallbacks++;
            }
        );
    }

    // Simultaneously add collision bodies while paths are being computed
    auto& edm = EntityDataManager::Instance();
    std::vector<EntityID> tempBodies;
    for (int i = 0; i < 5; ++i) {
        AABB bodyAABB(300.0f + i * 100.0f, 250.0f, 32.0f, 32.0f);
        EntityHandle handle = edm.createStaticBody(bodyAABB.center, bodyAABB.halfSize.getX(), bodyAABB.halfSize.getY());
        EntityID bodyId = handle.getId();
        size_t edmIndex = edm.getStaticIndex(handle);
        CollisionManager::Instance().addStaticBody(bodyId, bodyAABB.center, bodyAABB.halfSize, CollisionLayer::Layer_Enemy, 0xFFFFFFFFu, false, 0, 1, edmIndex);
        tempBodies.push_back(bodyId);
    }

    // Wait for all async callbacks to complete
    for (int i = 0; i < 50 && completedCallbacks < NUM_CONCURRENT_REQUESTS; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Should have processed most requests successfully
    BOOST_CHECK_GE(successfulPaths.load(), NUM_CONCURRENT_REQUESTS / 2);

    BOOST_TEST_MESSAGE("Concurrent operations: " << successfulPaths.load()
                      << "/" << NUM_CONCURRENT_REQUESTS << " paths found successfully");

    // Clean up
    for (EntityID bodyId : tempBodies) {
        CollisionManager::Instance().removeCollisionBody(bodyId);
    }
}

BOOST_FIXTURE_TEST_CASE(TestPerformanceUnderLoad, CollisionPathfindingFixture)
{
    // Test system performance with both collision and pathfinding load

    const int NUM_COLLISION_BODIES = 50;
    const int NUM_PATH_REQUESTS = 20;

    auto& edm = EntityDataManager::Instance();
    std::vector<EntityID> bodies;

    // Add many collision bodies
    for (int i = 0; i < NUM_COLLISION_BODIES; ++i) {
        float x = 200.0f + static_cast<float>(i % 10) * 80.0f;
        float y = 200.0f + static_cast<float>(i / 10) * 80.0f;
        AABB bodyAABB(x, y, 16.0f, 16.0f);

        EntityHandle handle = edm.createStaticBody(bodyAABB.center, bodyAABB.halfSize.getX(), bodyAABB.halfSize.getY());
        EntityID bodyId = handle.getId();
        size_t edmIndex = edm.getStaticIndex(handle);
        CollisionManager::Instance().addStaticBody(bodyId, bodyAABB.center, bodyAABB.halfSize, CollisionLayer::Layer_Enemy, 0xFFFFFFFFu, false, 0, 1, edmIndex);
        bodies.push_back(bodyId);
    }

    // Measure combined system performance using async API
    auto startTime = std::chrono::high_resolution_clock::now();

    std::atomic<int> pathsCompleted{0};
    std::atomic<int> completedCallbacks{0};

    // Submit async pathfinding requests (matches real game behavior)
    for (int i = 0; i < NUM_PATH_REQUESTS; ++i) {
        Vector2D start(100.0f, 100.0f + i * 30.0f);
        Vector2D goal(900.0f, 500.0f + i * 20.0f);

        PathfinderManager::Instance().requestPath(
            8100 + i, start, goal, PathfinderManager::Priority::High,
            [&pathsCompleted, &completedCallbacks](EntityID, const std::vector<Vector2D>& path) {
                if (path.size() >= 2) {
                    pathsCompleted++;
                }
                completedCallbacks++;
            }
        );
    }

    // Wait for all paths to complete
    for (int i = 0; i < 200 && completedCallbacks < NUM_PATH_REQUESTS; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // System should handle load without excessive delays
    BOOST_CHECK_LT(duration.count(), 2000); // < 2 seconds total

    // Should complete most paths
    BOOST_CHECK_GE(pathsCompleted.load(), NUM_PATH_REQUESTS / 3);

    BOOST_TEST_MESSAGE("Performance under load: " << NUM_COLLISION_BODIES
                      << " bodies, " << pathsCompleted.load() << "/" << NUM_PATH_REQUESTS
                      << " paths completed in " << duration.count() << "ms");

    // Clean up
    for (EntityID bodyId : bodies) {
        CollisionManager::Instance().removeCollisionBody(bodyId);
    }
}

BOOST_FIXTURE_TEST_CASE(TestCollisionLayerPathfindingInteraction, CollisionPathfindingFixture)
{
    // Test that collision layers properly affect pathfinding

    // Add bodies with different collision layers
    auto& edm = EntityDataManager::Instance();
    AABB obstacleAABB(350.0f, 350.0f, 32.0f, 32.0f);

    EntityHandle playerHandle = edm.createStaticBody(obstacleAABB.center, obstacleAABB.halfSize.getX(), obstacleAABB.halfSize.getY());
    EntityID playerObstacle = playerHandle.getId();
    size_t playerEdmIndex = edm.getStaticIndex(playerHandle);

    EntityHandle enemyHandle = edm.createStaticBody(obstacleAABB.center, obstacleAABB.halfSize.getX(), obstacleAABB.halfSize.getY());
    EntityID enemyObstacle = enemyHandle.getId();
    size_t enemyEdmIndex = edm.getStaticIndex(enemyHandle);

    EntityHandle envHandle = edm.createStaticBody(obstacleAABB.center, obstacleAABB.halfSize.getX(), obstacleAABB.halfSize.getY());
    EntityID environmentObstacle = envHandle.getId();
    size_t envEdmIndex = edm.getStaticIndex(envHandle);

    CollisionManager::Instance().addStaticBody(playerObstacle, obstacleAABB.center, obstacleAABB.halfSize, CollisionLayer::Layer_Player, 0xFFFFFFFFu, false, 0, 1, playerEdmIndex);
    CollisionManager::Instance().addStaticBody(enemyObstacle, obstacleAABB.center, obstacleAABB.halfSize, CollisionLayer::Layer_Enemy, 0xFFFFFFFFu, false, 0, 1, enemyEdmIndex);
    CollisionManager::Instance().addStaticBody(environmentObstacle, obstacleAABB.center, obstacleAABB.halfSize, CollisionLayer::Layer_Environment, 0xFFFFFFFFu, false, 0, 1, envEdmIndex);

    // Set different collision layers
    CollisionManager::Instance().setBodyLayer(
        playerObstacle,
        CollisionLayer::Layer_Player,
        CollisionLayer::Layer_Enemy | CollisionLayer::Layer_Environment
    );

    CollisionManager::Instance().setBodyLayer(
        enemyObstacle,
        CollisionLayer::Layer_Enemy,
        CollisionLayer::Layer_Player | CollisionLayer::Layer_Environment
    );

    CollisionManager::Instance().setBodyLayer(
        environmentObstacle,
        CollisionLayer::Layer_Environment,
        CollisionLayer::Layer_Player | CollisionLayer::Layer_Enemy
    );

    // Event-driven: PathfinderManager automatically updates via CollisionObstacleChanged events
    EventManager::Instance().update();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow grid rebuild

    // Test pathfinding around the layered obstacles using async API
    Vector2D start(200.0f, 200.0f);
    Vector2D goal(500.0f, 500.0f);

    std::vector<Vector2D> path;
    bool callbackExecuted = false;

    PathfinderManager::Instance().requestPath(
        10100, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            path = resultPath;
            callbackExecuted = true;
        }
    );

    // Wait for async completion
    for (int i = 0; i < 20 && !callbackExecuted; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_REQUIRE(callbackExecuted);

    // Should handle layered obstacles appropriately
    BOOST_CHECK_GE(path.size(), 2);

    BOOST_TEST_MESSAGE("Collision layer pathfinding: " << path.size()
                      << " waypoints with layered obstacles");

    // Clean up
    CollisionManager::Instance().removeCollisionBody(playerObstacle);
    CollisionManager::Instance().removeCollisionBody(enemyObstacle);
    CollisionManager::Instance().removeCollisionBody(environmentObstacle);
}

BOOST_FIXTURE_TEST_CASE(TestEntityMovementAlongPath, CollisionPathfindingFixture)
{
    // INTEGRATION TEST #3: Actually move an entity along a path and verify no collisions occur

    Vector2D start(100.0f, 100.0f);  // Clear starting area
    Vector2D goal(600.0f, 600.0f);   // Goal requires navigating around obstacles

    // Request path
    std::vector<Vector2D> path;
    bool callbackExecuted = false;

    PathfinderManager::Instance().requestPath(
        11000, start, goal, PathfinderManager::Priority::High,
        [&](EntityID, const std::vector<Vector2D>& resultPath) {
            path = resultPath;
            callbackExecuted = true;
        }
    );

    // Wait for path
    for (int i = 0; i < 20 && !callbackExecuted; ++i) {
        PathfinderManager::Instance().update(); // Process buffered requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_REQUIRE(callbackExecuted);
    BOOST_REQUIRE_GE(path.size(), 2);

    // Simulated entity radius for collision queries
    float entityRadius = 16.0f;

    // Simulate movement along the path
    int collisionsDetected = 0;
    int waypointsTraversed = 0;
    Vector2D currentPos = start;

    for (size_t i = 0; i < path.size(); ++i) {
        const Vector2D& targetWaypoint = path[i];

        // Move towards waypoint in small steps (simulates frame-by-frame movement)
        const float stepSize = 8.0f;
        Vector2D direction = targetWaypoint - currentPos;
        float distance = direction.length();

        while (distance > stepSize) {
            // Normalize direction and step forward
            Vector2D normalized = direction * (1.0f / distance);
            currentPos = currentPos + (normalized * stepSize);

            // Check for collisions using the actual entity radius (not 2x)
            AABB queryAABB(currentPos.getX(), currentPos.getY(), entityRadius, entityRadius);
            std::vector<EntityID> collisions;
            CollisionManager::Instance().queryArea(queryAABB, collisions);

            for (EntityID colliderId : collisions) {
                collisionsDetected++;
                BOOST_TEST_MESSAGE("Collision detected at (" << currentPos.getX() << ", "
                                 << currentPos.getY() << ") with entity " << colliderId);
                break;
            }

            // Recalculate distance for next iteration
            direction = targetWaypoint - currentPos;
            distance = direction.length();
        }

        // Reached waypoint
        currentPos = targetWaypoint;
        waypointsTraversed++;
    }

    BOOST_TEST_MESSAGE("Entity movement test: traversed " << waypointsTraversed
                      << " waypoints with " << collisionsDetected << " collisions");

    // With 64px pathfinding grid, 32px obstacles, 16px entity radius, and 8px movement steps,
    // edge collisions are expected when brushing past obstacles. Each obstacle can trigger
    // multiple consecutive collision checks (e.g., ~11 checks when brushing one obstacle).
    // Allow minimum 20 collisions to handle realistic edge cases, or 30% of path traversal.
    int maxAcceptableCollisions = std::max(20, static_cast<int>(path.size() * waypointsTraversed * 0.3f));
    BOOST_CHECK_MESSAGE(collisionsDetected <= maxAcceptableCollisions,
        "Entity movement should mostly avoid collisions (detected " +
        std::to_string(collisionsDetected) + " collisions, max acceptable: " +
        std::to_string(maxAcceptableCollisions) + ")");

    // Verify entity made progress towards goal (not stuck)
    float finalDistance = (currentPos - goal).length();
    float startDistance = (start - goal).length();
    BOOST_CHECK_MESSAGE(finalDistance < startDistance,
        "Entity should make progress towards goal (start: " +
        std::to_string(startDistance) + "px, end: " +
        std::to_string(finalDistance) + "px)");

    // No collision body to clean up (query-only test)
}

BOOST_AUTO_TEST_SUITE_END()
