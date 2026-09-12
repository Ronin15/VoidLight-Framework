/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE AICollisionIntegrationTests
#include <boost/test/unit_test.hpp>

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
 * Production collision grouping is stance-owned: Neutral NPCs are
 * Layer_Default and do not pair with each other. Hostile-toward-player
 * NPCs are Layer_Default until setStance, then Layer_Enemy and pair with
 * other Enemy bodies. Default NPCs still collide with Layer_Environment.
 *
 * Wander crowd steering uses AIInternal nearby queries, not CollisionManager
 * pair generation. Do not force collisionMask = 0xFFFF to inflate lastPairs.
 *
 * These tests verify:
 * 1. AI wanderers vs Environment obstacles (production Default mask)
 * 2. Stance remap: Neutral Guards do not NPC-NPC pair; Hostile Warriors do
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
    AICollisionTestFixture() {
        std::cout << "\n--- Test Setup ---" << std::endl;

        // Clear any previous state
        AIManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
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

        // Prepare for next test
        AIManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();

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
    void createObstacle([[maybe_unused]] EntityID id, const Vector2D& pos, float halfW, float halfH) {
        auto& edm = EntityDataManager::Instance();
        EntityHandle handle = edm.createStaticBody(pos, halfW, halfH);
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
            edmIndex
        );
        m_obstacleIds.push_back(edmId);
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
        BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(worldConfig));
        auto& worldMgr = WorldManager::Instance();
        worldMgr.clearPopulatedNpcs(worldMgr.getCurrentWorldId());
        EntityDataManager::Instance().processDestructionQueue();
        EventManager::Instance().update();
        PathfinderManager::Instance().rebuildGrid();
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

    // Helper: Check if entity is overlapping any obstacle
    bool isEntityOverlappingObstacles(EntityID entityId) {
        for (EntityID obstacleId : m_obstacleIds) {
            if (CollisionManager::Instance().overlaps(entityId, obstacleId)) {
                return true;
            }
        }
        return false;
    }

    std::mt19937 m_rng;
    std::vector<EntityHandle> m_entityHandles;
    std::vector<EntityID> m_obstacleIds;
};

BOOST_FIXTURE_TEST_SUITE(AICollisionIntegrationTestSuite, AICollisionTestFixture)

/**
 * TEST 1: TestAINavigatesObstacleField
 *
 * Verifies AI entities navigate around obstacles during pathfinding.
 * CRITICAL: This test ensures AI actually uses CollisionManager for obstacle avoidance.
 */
BOOST_AUTO_TEST_CASE(TestAINavigatesObstacleField) {
    std::cout << "\n=== TEST 1: AI Navigates Obstacle Field ===" << std::endl;

    // Create a grid of static obstacles (5x5 grid with gaps)
    const float OBSTACLE_SIZE = 64.0f;
    const float GRID_SPACING = 200.0f;
    const Vector2D GRID_ORIGIN(500.0f, 500.0f);

    EntityID obstacleIdCounter = 10000;
    int obstaclesCreated = 0;

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            // Create gaps for pathfinding (skip some positions)
            if ((row == 2 && col == 2) || (row == 0 && col == 4) || (row == 4 && col == 0)) {
                continue; // Leave gaps
            }

            Vector2D obstaclePos(
                GRID_ORIGIN.getX() + col * GRID_SPACING,
                GRID_ORIGIN.getY() + row * GRID_SPACING
            );

            createObstacle(
                obstacleIdCounter++,
                obstaclePos,
                OBSTACLE_SIZE / 2.0f,
                OBSTACLE_SIZE / 2.0f
            );
            obstaclesCreated++;
        }
    }

    std::cout << "Created " << obstaclesCreated << " obstacles in grid pattern" << std::endl;

    // Process collision commands

    // Rebuild static spatial hash for pathfinding
    CollisionManager::Instance().rebuildStaticFromWorld();

    // Set up a minimal world for pathfinding grid
    VoidLight::WorldGenerationConfig worldConfig{};
    worldConfig.width = 50;
    worldConfig.height = 50;
    worldConfig.seed = 12345;
    worldConfig.elevationFrequency = 0.05f;
    worldConfig.humidityFrequency = 0.05f;
    worldConfig.waterLevel = 0.3f;
    worldConfig.mountainLevel = 0.7f;

    std::cout << "Setting up world for pathfinding grid..." << std::endl;
    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(worldConfig));

    {
        auto& worldMgr = WorldManager::Instance();
        worldMgr.clearPopulatedNpcs(worldMgr.getCurrentWorldId());
        EntityDataManager::Instance().processDestructionQueue();
    }

    // Wait for world generation to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::cout << "Rebuilding pathfinding grid with active world..." << std::endl;
    PathfinderManager::Instance().rebuildGrid();

    // Wait for grid rebuild to complete (async operation)
    // We can use a simple sleep here as rebuild is async on ThreadSystem
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Pathfinding grid rebuild complete" << std::endl;

    // Create AI entities with wander behavior (will navigate around obstacles)
    const int NUM_ENTITIES = 10;

    // Spawn entities in the center gap (row=2, col=2) to avoid spawning on obstacles
    const Vector2D SPAWN_CENTER(
        GRID_ORIGIN.getX() + 2 * GRID_SPACING,
        GRID_ORIGIN.getY() + 2 * GRID_SPACING
    );

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        // Spawn in small cluster around center gap
        Vector2D startPos(
            SPAWN_CENTER.getX() + (i % 3 - 1) * 30.0f,
            SPAWN_CENTER.getY() + (i / 3 - 1) * 30.0f
        );

        auto entity = createEntity(startPos);
        AIManager::Instance().assignBehavior(entity, "Wander");
    }

    // Process collision commands for entities

    std::cout << "Created " << NUM_ENTITIES << " AI entities with wander behavior" << std::endl;

    // Capture initial behavior execution count (DOD: AIManager tracks executions)
    size_t initialBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();

    // Run simulation for 200 frames (3.3 seconds at 60 FPS)
    std::cout << "Running simulation for 200 frames..." << std::endl;
    updateSimulation(200, 0.016f);

    // VERIFICATION: Check that entities are NOT overlapping obstacles
    int entitiesOverlappingObstacles = 0;
    for (const auto& handle : m_entityHandles) {
        EntityID entityId = handle.getId();
        if (isEntityOverlappingObstacles(entityId)) {
            entitiesOverlappingObstacles++;
            std::cout << "FAILURE: Entity " << entityId << " is overlapping an obstacle!" << std::endl;
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
 * Hostile-toward-player Warriors remap to Layer_Enemy and do pair.
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

    aiMgr.setStance(1, 0, FactionStance::Hostile);
    for (const auto& handle : m_entityHandles) {
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
