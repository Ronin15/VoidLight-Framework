/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE AICollisionIntegrationTests
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <memory>

#include "core/Logger.hpp"
#include "ai/FactionStance.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "core/ThreadSystem.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include "world/WorldData.hpp"

/**
 * AICollisionIntegrationTests
 *
 * Production collision grouping follows the player relation: NPCs whose
 * faction is not Hostile toward the player (standing) are Layer_Default and
 * do not pair with each other. When player standing with their faction
 * crosses the Hostile threshold they become Layer_Enemy and pair with other
 * Enemy bodies. Default NPCs still collide with Layer_Environment.
 *
 * Wander crowd steering uses AIInternal nearby queries, not CollisionManager
 * pair generation. Do not force collisionMask = 0xFFFF to inflate lastPairs.
 *
 * These tests verify:
 * 1. AI wanderers vs Environment obstacles (production Default mask)
 * 2. Relation remap: Neutral Guards do not NPC-NPC pair; Hostile Warriors do
 * 3. AI entities stay within world boundaries
 * 4. Performance remains acceptable under load (1000+ entities)
 */

// Data-driven test entity helper
// Creates entities via EntityDataManager for collision testing
struct TestEntityHelper {
    // Create a data-driven NPC for testing
    static EntityHandle createTestEntity(const Vector2D& pos) {
        auto& edm = EntityDataManager::Instance();
        return edm.createNPCWithRaceClass(pos, "Human", "Guard");
    }

    // Get entity position from EDM
    static Vector2D getPosition(EntityHandle handle) {
        auto& edm = EntityDataManager::Instance();
        size_t idx = edm.getIndex(handle);
        if (idx != SIZE_MAX) {
            return edm.getHotDataByIndex(idx).transform.position;
        }
        return Vector2D(0, 0);
    }

    // Get entity ID from handle
    static EntityID getID(EntityHandle handle) {
        return handle.getId();
    }
};

// Global test fixture
struct AICollisionGlobalFixture {
    AICollisionGlobalFixture() {
        std::cout << "=== AICollisionIntegrationTests Global Setup ===" << std::endl;

        // Initialize core systems in dependency order
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem initialization failed");
        }

        if (!EventManager::Instance().init()) {
            throw std::runtime_error("EventManager initialization failed");
        }

        // EntityDataManager must be initialized before entities can register
        if (!EntityDataManager::Instance().init()) {
            throw std::runtime_error("EntityDataManager initialization failed");
        }

        if (!CollisionManager::Instance().init()) {
            throw std::runtime_error("CollisionManager initialization failed");
        }

        if (!WorldManager::Instance().init()) {
            throw std::runtime_error("WorldManager initialization failed");
        }

        if (!PathfinderManager::Instance().init()) {
            throw std::runtime_error("PathfinderManager initialization failed");
        }

        if (!AIManager::Instance().init()) {
            throw std::runtime_error("AIManager initialization failed");
        }

        if (!BackgroundSimulationManager::Instance().init()) {
            throw std::runtime_error("BackgroundSimulationManager initialization failed");
        }

        // Enable threading for AI
        VOIDLIGHT_DEBUG_ONLY(AIManager::Instance().enableThreading(true);)

        std::cout << "=== Global Setup Complete ===" << std::endl;
    }

    ~AICollisionGlobalFixture() {
        std::cout << "=== AICollisionIntegrationTests Global Teardown ===" << std::endl;

        // Wait for pending operations
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Clean up managers in reverse order
        BackgroundSimulationManager::Instance().clean();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        WorldManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        EventManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();

        std::cout << "=== Global Teardown Complete ===" << std::endl;
    }
};

BOOST_GLOBAL_FIXTURE(AICollisionGlobalFixture);

// Individual test fixture
struct AICollisionTestFixture {
    struct Obstacle {
        EntityID id;
        VoidLight::AABB box;
    };

    AICollisionTestFixture() {
        std::cout << "\n--- Test Setup ---" << std::endl;

        // Clear any previous state in production transition order. EventManager
        // drops deferred events a previous test left queued (e.g. an undrained
        // WorldLoaded that would rebuild statics under this test's grid rebuild).
        AIManager::Instance().prepareForStateTransition();
        EventManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
        PathfinderManager::Instance().prepareForStateTransition();
        if (!WorldManager::Instance().getCurrentWorldId().empty()) {
            WorldManager::Instance().unloadWorld();
        }

        // Set fixed RNG seed for reproducibility
        m_rng.seed(42);

        m_entityHandles.clear();
    }

    ~AICollisionTestFixture() {
        std::cout << "--- Test Teardown ---" << std::endl;

        // Clean up entities
        auto& edm = EntityDataManager::Instance();
        for (auto& handle : m_entityHandles) {
            if (handle.isValid()) {
                AIManager::Instance().unregisterEntity(handle);
                edm.destroyEntity(handle);
            }
        }
        m_entityHandles.clear();
        edm.processDestructionQueue();

        // Prepare for next test (production transition order)
        AIManager::Instance().prepareForStateTransition();
        EventManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
        PathfinderManager::Instance().prepareForStateTransition();

        // Wait for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EntityHandle createEntity(const Vector2D& pos) {
        EntityHandle handle = TestEntityHelper::createTestEntity(pos);
        m_entityHandles.push_back(handle);
        return handle;
    }

    EntityHandle createWarrior(const Vector2D& pos) {
        EntityHandle handle = EntityDataManager::Instance().createNPCWithRaceClass(
            pos, "Human", "Warrior");
        m_entityHandles.push_back(handle);
        return handle;
    }

    // Helper: Create static obstacle with proper EDM routing
    void createObstacle(const Vector2D& pos, float halfW, float halfH) {
        auto& edm = EntityDataManager::Instance();
        EntityHandle handle = edm.createStaticBody(pos, halfW, halfH);
        BOOST_REQUIRE(handle.isValid());
        size_t edmIndex = edm.getStaticIndex(handle);
        EntityID edmId = handle.getId();

        CollisionManager::Instance().addStaticBody(
            edmId,
            pos,
            Vector2D(halfW, halfH),
            VoidLight::CollisionLayer::Layer_Environment,
            0xFFFFFFFFu,
            false,
            0,
            1,
            edmIndex);
        m_obstacles.push_back({edmId, VoidLight::AABB(pos.getX(), pos.getY(), halfW, halfH)});
    }

    void loadBareWorld(int widthTiles, int heightTiles, int seed) {
        VoidLight::WorldGenerationConfig worldConfig{};
        worldConfig.width = widthTiles;
        worldConfig.height = heightTiles;
        worldConfig.seed = seed;
        worldConfig.elevationFrequency = 0.05f;
        worldConfig.humidityFrequency = 0.05f;
        worldConfig.waterLevel = 0.3f;
        worldConfig.mountainLevel = 0.7f;
        worldConfig.populate = false; // Tests spawn their own NPCs
        BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(worldConfig));
        // Wait for the event-driven grid rebuild. The deferred WorldLoaded event
        // builds collision statics on the main thread, then StaticCollidersReady
        // rebuilds the grid; a manual rebuildGrid() here would read collision
        // storage while the statics are still being built.
        waitForGridReady();
    }

    // Drains deferred events and polls until every grid rebuild future is done.
    // Collision storage is not mutated while waiting (no CollisionManager::update).
    void waitForGridReady() {
        EventManager::Instance().update();
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
        while (std::chrono::steady_clock::now() < deadline &&
            !PathfinderManager::Instance().isGridReady()) {
            PathfinderManager::Instance().update();
            EventManager::Instance().update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        BOOST_REQUIRE(PathfinderManager::Instance().isGridReady());
    }

    void tickCollision(int frames, float deltaTime = 0.016f) {
        for (int i = 0; i < frames; ++i) {
            CollisionManager::Instance().update(deltaTime);
        }
    }

    // Helper: Update simulation for N frames
    void updateSimulation(int frames, float deltaTime = 0.016f) {
        // Reference point for tier calculation (center of test area)
        Vector2D referencePoint(500.0f, 500.0f);

        for (int i = 0; i < frames; ++i) {
            // Update simulation tiers first (required for AIManager to find Active entities)
            BackgroundSimulationManager::Instance().update(referencePoint, deltaTime);

            // Update AI (processes entity behaviors)
            AIManager::Instance().update(deltaTime);

            // Update collision system
            CollisionManager::Instance().update(deltaTime);

            // Small sleep to allow async processing
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Helper: Check if an NPC's EDM collision box penetrates any obstacle.
    // NPCs are EDM-managed movables with no CollisionManager storage entry, so
    // CollisionManager::overlaps() cannot see them. Penetration means positive
    // interior overlap on both axes; a body resolved flush against an obstacle
    // edge is contact, which AABB::intersects() (edge-inclusive) would count.
    bool isEntityOverlappingObstacles(EntityHandle handle) const {
        const auto& edm = EntityDataManager::Instance();
        const size_t idx = edm.getIndex(handle);
        BOOST_REQUIRE(idx != SIZE_MAX);
        const auto& hot = edm.getHotDataByIndex(idx);
        const VoidLight::AABB entityBox(hot.transform.position.getX(),
            hot.transform.position.getY(), hot.halfWidth, hot.halfHeight);
        return std::any_of(m_obstacles.begin(), m_obstacles.end(),
            [&entityBox](const Obstacle& obstacle) {
                const float penX = std::min(entityBox.right(), obstacle.box.right()) -
                    std::max(entityBox.left(), obstacle.box.left());
                const float penY = std::min(entityBox.bottom(), obstacle.box.bottom()) -
                    std::max(entityBox.top(), obstacle.box.top());
                return penX > 0.0f && penY > 0.0f;
            });
    }

    std::mt19937 m_rng;
    std::vector<EntityHandle> m_entityHandles;
    std::vector<Obstacle> m_obstacles;
};

BOOST_FIXTURE_TEST_SUITE(AICollisionIntegrationTestSuite, AICollisionTestFixture)

/**
 * TEST 1: TestAINavigatesObstacleField
 *
 * Wanderers spawned in a gap of a static obstacle field stay out of the
 * obstacles. The world loads first (its WorldLoaded rebuild replaces every
 * STATIC body), then the obstacles are added; their CollisionObstacleChanged
 * events mark pathfinding dirty cells, which PathfinderManager::update() applies. The test
 * asserts the obstacles exist in CollisionManager and block the pathfinding
 * grid for the whole run, so the overlap check is against live obstacles.
 */
BOOST_AUTO_TEST_CASE(TestAINavigatesObstacleField) {
    std::cout << "\n=== TEST 1: AI Navigates Obstacle Field ===" << std::endl;

    loadBareWorld(50, 50, 12345);

    // Create a grid of static obstacles (5x5 grid with gaps)
    const float OBSTACLE_SIZE = 64.0f;
    const float GRID_SPACING = 200.0f;
    const Vector2D GRID_ORIGIN(500.0f, 500.0f);

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            // Create gaps for pathfinding (skip some positions)
            if ((row == 2 && col == 2) || (row == 0 && col == 4) || (row == 4 && col == 0)) {
                continue; // Leave gaps
            }

            Vector2D obstaclePos(
                GRID_ORIGIN.getX() + col * GRID_SPACING,
                GRID_ORIGIN.getY() + row * GRID_SPACING);

            createObstacle(obstaclePos, OBSTACLE_SIZE / 2.0f, OBSTACLE_SIZE / 2.0f);
        }
    }
    BOOST_REQUIRE_EQUAL(m_obstacles.size(), 22u);

    auto& collisionMgr = CollisionManager::Instance();
    auto requireObstaclesPresent = [&]() {
        for (const auto& obstacle : m_obstacles) {
            BOOST_REQUIRE(collisionMgr.isStatic(obstacle.id));
            BOOST_REQUIRE(collisionMgr.queryAreaHasStaticOverlap(obstacle.box));
        }
    };
    requireObstaclesPresent();

    // Deferred CollisionObstacleChanged events mark dirty cells; the next
    // PathfinderManager::update() rebuilds them and publishes the grid.
    EventManager::Instance().update();
    PathfinderManager::Instance().update();

    // The grid blocks every obstacle cell: snapping an obstacle center to an
    // open cell moves it off the obstacle (an open cell would snap in place).
    auto& pathfinder = PathfinderManager::Instance();
    for (const auto& obstacle : m_obstacles) {
        const Vector2D& center = obstacle.box.center;
        const Vector2D open = pathfinder.adjustSpawnToNavigable(center, 16.0f, 16.0f, 0.0f);
        BOOST_CHECK_GT((open - center).length(), OBSTACLE_SIZE);
    }

    // Create AI entities with wander behavior (will navigate around obstacles)
    const int NUM_ENTITIES = 10;

    // Spawn entities in the center gap (row=2, col=2) to avoid spawning on obstacles
    const Vector2D SPAWN_CENTER(
        GRID_ORIGIN.getX() + 2 * GRID_SPACING,
        GRID_ORIGIN.getY() + 2 * GRID_SPACING);

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        // Spawn in small cluster around center gap
        Vector2D startPos(
            SPAWN_CENTER.getX() + (i % 3 - 1) * 30.0f,
            SPAWN_CENTER.getY() + (i / 3 - 1) * 30.0f);

        auto entity = createEntity(startPos);
        AIManager::Instance().assignBehavior(entity, "Wander");
    }
    for (const auto& handle : m_entityHandles) {
        BOOST_REQUIRE(!isEntityOverlappingObstacles(handle));
    }

    std::cout << "Created " << NUM_ENTITIES << " AI entities with wander behavior" << std::endl;

    // Capture initial behavior execution count (DOD: AIManager tracks executions)
    size_t initialBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();

    // Run simulation for 200 frames (3.3 seconds at 60 FPS)
    std::cout << "Running simulation for 200 frames..." << std::endl;
    updateSimulation(200, 0.016f);

    // The obstacles are still live for the overlap check.
    requireObstaclesPresent();

    // VERIFICATION: Check that entities are NOT overlapping obstacles
    int entitiesOverlappingObstacles = 0;
    for (const auto& handle : m_entityHandles) {
        if (isEntityOverlappingObstacles(handle)) {
            entitiesOverlappingObstacles++;
            std::cout << "FAILURE: Entity " << handle.getId() << " is overlapping an obstacle!" << std::endl;
        }
    }

    std::cout << "Entities overlapping obstacles: " << entitiesOverlappingObstacles << " / " << NUM_ENTITIES << std::endl;

    // CRITICAL: Pathfinding should prevent most overlaps (allow 1 entity for edge cases)
    // Note: Tight obstacle grid with dynamic wandering can occasionally cause brief overlaps
    // This validates pathfinding is working while being realistic about edge cases
    BOOST_CHECK_LE(entitiesOverlappingObstacles, 1);

    // Verify behaviors were executed (DOD: AIManager doesn't call Entity::update() anymore)
    size_t finalBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();
    size_t behaviorExecutions = finalBehaviorCount - initialBehaviorCount;
    std::cout << "Behavior executions: " << behaviorExecutions << std::endl;
    BOOST_CHECK_GT(behaviorExecutions, 0);

    std::cout << "=== TEST 1: PASSED ===" << std::endl;
}

/**
 * TEST 2: TestAIStanceCollisionGrouping
 *
 * Neutral Guards stay Layer_Default and do not generate NPC-NPC pairs.
 * Warriors whose faction's player standing is Hostile remap to Layer_Enemy
 * and do pair.
 * Wander crowd steering is not CollisionManager pair generation.
 */
BOOST_AUTO_TEST_CASE(TestAIStanceCollisionGrouping) {
    std::cout << "\n=== TEST 2: AI Stance Collision Grouping ===" << std::endl;

    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();
    auto& collision = CollisionManager::Instance();

    const int NUM_ENTITIES = 16;
    const Vector2D SPAWN_CENTER(500.0f, 500.0f);
    const float SPAWN_RADIUS = 20.0f;

    auto spawnRing = [&](auto createFn) {
        for (int i = 0; i < NUM_ENTITIES; ++i) {
            const float angle = (i / static_cast<float>(NUM_ENTITIES)) * 2.0f * 3.14159f;
            Vector2D spawnPos(
                SPAWN_CENTER.getX() + std::cos(angle) * SPAWN_RADIUS,
                SPAWN_CENTER.getY() + std::sin(angle) * SPAWN_RADIUS);
            EntityHandle handle = createFn(spawnPos);
            aiMgr.assignBehavior(handle, "Wander");
        }
    };

    spawnRing([&](const Vector2D& pos) { return createEntity(pos); });
    for (const auto& handle : m_entityHandles) {
        const size_t idx = edm.getIndex(handle);
        BOOST_REQUIRE_NE(idx, SIZE_MAX);
        BOOST_CHECK_NE(edm.getHotDataByIndex(idx).collisionLayers,
            VoidLight::CollisionLayer::Layer_Enemy);
    }

    tickCollision(3);
    BOOST_CHECK_EQUAL(collision.getPerfStats().lastPairs, 0u);
    BOOST_CHECK_GE(edm.getActiveIndicesWithCollision().size(),
        static_cast<size_t>(NUM_ENTITIES));

    for (auto& handle : m_entityHandles) {
        if (handle.isValid()) {
            aiMgr.unregisterEntity(handle);
            edm.destroyEntity(handle);
        }
    }
    edm.processDestructionQueue();
    m_entityHandles.clear();

    spawnRing([&](const Vector2D& pos) { return createWarrior(pos); });
    for (const auto& handle : m_entityHandles) {
        const size_t idx = edm.getIndex(handle);
        BOOST_REQUIRE_NE(idx, SIZE_MAX);
        BOOST_CHECK_EQUAL(edm.getCharacterDataByIndex(idx).faction, 1);
        BOOST_CHECK_NE(edm.getHotDataByIndex(idx).collisionLayers,
            VoidLight::CollisionLayer::Layer_Enemy);
    }

    // Registered player far from the ring; its standing with faction 1 drives
    // the Warriors' collision grouping (the stance table does not).
    const EntityHandle player = edm.registerPlayer(900001, Vector2D(5000.0f, 5000.0f));
    BOOST_REQUIRE(player.isValid());
    m_entityHandles.push_back(player);
    aiMgr.setPlayerHandle(player);
    aiMgr.adjustPlayerStanding(player, 1, AIManager::PLAYER_STANDING_MIN);
    BOOST_REQUIRE(aiMgr.getPlayerRelation(1) == FactionStance::Hostile);
    for (const auto& handle : m_entityHandles) {
        if (handle == player) {
            continue;
        }
        const size_t idx = edm.getIndex(handle);
        BOOST_REQUIRE_NE(idx, SIZE_MAX);
        BOOST_CHECK_EQUAL(edm.getHotDataByIndex(idx).collisionLayers,
            VoidLight::CollisionLayer::Layer_Enemy);
    }

    tickCollision(3);
#ifdef DEBUG
    BOOST_CHECK_GT(collision.getPerfStats().lastPairs, 0u);
#else
    BOOST_CHECK_GE(edm.getActiveIndicesWithCollision().size(),
        static_cast<size_t>(NUM_ENTITIES));
#endif

    std::cout << "=== TEST 2: PASSED ===" << std::endl;
}

/**
 * TEST 3: TestAIBoundaryAvoidance
 *
 * Verifies AI entities stay within world boundaries.
 * Tests collision-based boundary enforcement.
 */
BOOST_AUTO_TEST_CASE(TestAIBoundaryAvoidance) {
    std::cout << "\n=== TEST 3: AI Boundary Avoidance ===" << std::endl;

    // Wander clamps to PathfinderManager cached world extents, not CollisionManager
    // wall bodies. With no world loaded, AI falls back to 32000px and walks out
    // of any hand-placed 2000px box.
    loadBareWorld(40, 40, 4242);

    float worldWidth = 0.0f;
    float worldHeight = 0.0f;
    BOOST_REQUIRE(PathfinderManager::Instance().getCachedWorldBounds(worldWidth, worldHeight));
    BOOST_REQUIRE_GT(worldWidth, 200.0f);
    BOOST_REQUIRE_GT(worldHeight, 200.0f);

    const int NUM_ENTITIES = 15;
    std::uniform_real_distribution<float> posDist(100.0f, worldWidth - 100.0f);

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        Vector2D startPos(posDist(m_rng), posDist(m_rng));
        auto entity = createEntity(startPos);
        AIManager::Instance().assignBehavior(entity, "Wander");
    }

    std::cout << "Created " << NUM_ENTITIES << " wanderers in "
              << worldWidth << "x" << worldHeight << " world" << std::endl;

    updateSimulation(120, 0.016f);

    const float TOLERANCE = 50.0f;
    int entitiesOutOfBounds = 0;
    for (const auto& handle : m_entityHandles) {
        Vector2D pos = TestEntityHelper::getPosition(handle);
        if (pos.getX() < -TOLERANCE || pos.getX() > worldWidth + TOLERANCE ||
            pos.getY() < -TOLERANCE || pos.getY() > worldHeight + TOLERANCE) {
            entitiesOutOfBounds++;
            std::cout << "FAILURE: Entity " << handle.getId() << " out of bounds at ("
                      << pos.getX() << ", " << pos.getY() << ")" << std::endl;
        }
    }

    std::cout << "Entities out of bounds: " << entitiesOutOfBounds << " / " << NUM_ENTITIES << std::endl;
    BOOST_CHECK_EQUAL(entitiesOutOfBounds, 0);

    std::cout << "=== TEST 3: PASSED ===" << std::endl;
}

/**
 * TEST 4: TestAICollisionPerformanceUnderLoad
 *
 * Verifies performance stays within frame budget with 1000+ AI entities.
 * Tests that collision queries scale efficiently.
 */
BOOST_AUTO_TEST_CASE(TestAICollisionPerformanceUnderLoad) {
    std::cout << "\n=== TEST 4: AI Collision Performance Under Load ===" << std::endl;

    // Create a large number of entities to stress test the system
    const int NUM_ENTITIES = 1000;
    // Keep the cluster inside BackgroundSimulationManager's default Active
    // radius (~1650). Scattered 5000px spawns retire most NPCs from collision.
    const Vector2D CLUSTER_CENTER(500.0f, 500.0f);
    const float CLUSTER_RADIUS = 400.0f;
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> radiusDist(0.0f, CLUSTER_RADIUS);

    std::cout << "Creating " << NUM_ENTITIES << " entities..." << std::endl;

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        const float angle = angleDist(m_rng);
        const float radius = radiusDist(m_rng);
        Vector2D startPos(
            CLUSTER_CENTER.getX() + std::cos(angle) * radius,
            CLUSTER_CENTER.getY() + std::sin(angle) * radius);
        auto entity = createEntity(startPos);
        AIManager::Instance().assignBehavior(entity, "Wander");
    }


    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Rebuild static spatial hash to ensure consistent state after prior tests
    CollisionManager::Instance().rebuildStaticFromWorld();

    std::cout << "Entities created. Starting performance test..." << std::endl;

    // Run simulation for 60 frames (1 second at 60 FPS)
    const int TEST_FRAMES = 60;
    std::vector<double> frameTimes;
    frameTimes.reserve(TEST_FRAMES);

    for (int frame = 0; frame < TEST_FRAMES; ++frame) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        // Update simulation tiers first (required for collision to find Active entities)
        BackgroundSimulationManager::Instance().update(CLUSTER_CENTER, 0.016f);

        // Update AI
        AIManager::Instance().update(0.016f);

        // Update collision
        CollisionManager::Instance().update(0.016f);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        double frameMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        frameTimes.push_back(frameMs);

        // Small sleep to allow async processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Calculate statistics
    double totalTime = 0.0;
    double maxTime = 0.0;
    for (double time : frameTimes) {
        totalTime += time;
        if (time > maxTime) {
            maxTime = time;
        }
    }
    double avgTime = totalTime / frameTimes.size();

    // Get collision statistics
    auto collisionStats = CollisionManager::Instance().getPerfStats();

    std::cout << "\n=== Performance Results ===" << std::endl;
    std::cout << "Entities: " << NUM_ENTITIES << std::endl;
    std::cout << "Average frame time: " << avgTime << " ms" << std::endl;
    std::cout << "Max frame time: " << maxTime << " ms" << std::endl;
    std::cout << "Collision pairs per frame: " << collisionStats.lastPairs << std::endl;
    std::cout << "Collision bodies: " << collisionStats.bodyCount << std::endl;

    // VERIFICATION: Frame time should stay within 60 FPS budget (16.67ms)
    // Allow generous tolerance for CI environments (50ms)
    const double MAX_FRAME_TIME_MS = 50.0;

    std::cout << "\nPerformance check: avgTime (" << avgTime << " ms) < " << MAX_FRAME_TIME_MS << " ms" << std::endl;

    // CRITICAL: Performance must be acceptable
    BOOST_CHECK_LT(avgTime, MAX_FRAME_TIME_MS);

    // Nearby Neutral wanderers stay Active collision participants. They do not
    // NPC-NPC pair, so lastPairs may be 0 with no Environment overlap.
    BOOST_CHECK_GE(EntityDataManager::Instance().getActiveIndicesWithCollision().size(),
        static_cast<size_t>(NUM_ENTITIES));
    BOOST_CHECK_GT(collisionStats.bodyCount, 0u);

    // Verify behaviors are registered
    size_t behaviorCount = AIManager::Instance().getBehaviorCount();
    std::cout << "AI behaviors registered: " << behaviorCount << std::endl;
    BOOST_CHECK_GT(behaviorCount, 0);

    std::cout << "=== TEST 4: PASSED ===" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
