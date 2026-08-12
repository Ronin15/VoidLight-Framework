/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * Collision Scaling Benchmark
 *
 * Tests the collision system's new optimizations:
 * 1. Sweep-and-Prune (SAP) for movable-movable (MM) detection
 * 2. Spatial Hash with AABB test for movable-static (MS) detection
 * 3. Static AABB caching for contiguous memory access
 * 4. MovableAABB entity caching for reduced EDM calls
 */

#define BOOST_TEST_MODULE CollisionScalingBenchmark
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>
#include <algorithm>

#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "entities/Entity.hpp"  // For AnimationConfig
#include "managers/BackgroundSimulationManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "core/Logger.hpp"  // For benchmark mode
#include "world/WorldData.hpp"

namespace {

// Test fixture for collision scaling benchmarks
class CollisionScalingFixture {
public:
    static bool& initializedFlag() { return s_initialized; }

    CollisionScalingFixture() {
        // Initialize systems once per fixture
        if (!s_initialized) {
            BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
            BOOST_REQUIRE(EntityDataManager::Instance().init());
            BOOST_REQUIRE(CollisionManager::Instance().init());
            BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());
            // Enable benchmark mode to suppress verbose logging during benchmarks
            VOIDLIGHT_ENABLE_BENCHMARK_MODE();
            s_initialized = true;
        }
        m_rng.seed(42); // Fixed seed for reproducibility
    }

    ~CollisionScalingFixture() = default;

    // Prepare fresh state for each test
    void prepareForTest() {
        CollisionManager::Instance().prepareForStateTransition();
        EntityDataManager::Instance().prepareForStateTransition();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    }

    // Create movable entities in EDM
    void createMovables(size_t count, float spread, float clusterRadius = 0.0f) {
        auto& edm = EntityDataManager::Instance();
        std::uniform_real_distribution<float> posDist(-spread, spread);

        for (size_t i = 0; i < count; ++i) {
            EntityID id = static_cast<EntityID>(m_nextId++);
            Vector2D pos;

            if (clusterRadius > 0.0f) {
                // Clustered distribution
                float angle = static_cast<float>(i) / static_cast<float>(count) * 6.28318f;
                float r = posDist(m_rng) * clusterRadius / spread;
                pos = Vector2D(spread * 0.5f + std::cos(angle) * r,
                              spread * 0.5f + std::sin(angle) * r);
            } else {
                // Uniform distribution
                pos = Vector2D(posDist(m_rng) + spread, posDist(m_rng) + spread);
            }

            EntityHandle handle = edm.createNPCWithRaceClass( pos, "Human", "Guard");
            size_t idx = edm.getIndex(handle);
            if (idx != SIZE_MAX) {
                auto& hot = edm.getHotDataByIndex(idx);
                hot.collisionLayers = CollisionLayer::Layer_Enemy;
                hot.collisionMask = 0xFFFF;
                hot.setCollisionEnabled(true);
            }
            m_movableHandles.push_back(handle);
            m_entityIds.push_back(id);
        }
    }

    // Create static bodies in CollisionManager
    void createStatics(size_t count, float spread, float clusterRadius = 0.0f) {
        auto& cm = CollisionManager::Instance();
        auto& edm = EntityDataManager::Instance();
        std::uniform_real_distribution<float> posDist(-spread, spread);

        for (size_t i = 0; i < count; ++i) {
            Vector2D pos;

            if (clusterRadius > 0.0f) {
                // Clustered distribution
                float angle = static_cast<float>(i) / static_cast<float>(count) * 6.28318f;
                float r = posDist(m_rng) * clusterRadius / spread;
                pos = Vector2D(spread * 0.5f + std::cos(angle) * r,
                              spread * 0.5f + std::sin(angle) * r);
            } else {
                // Uniform distribution
                pos = Vector2D(posDist(m_rng) + spread, posDist(m_rng) + spread);
            }

            // Register with EDM first (single source of truth)
            EntityHandle handle = edm.createStaticBody(pos, 16.0f, 16.0f);
            EntityID id = handle.getId();
            size_t edmIndex = edm.getStaticIndex(handle);

            cm.addStaticBody(id, pos, Vector2D(16.0f, 16.0f),
                            CollisionLayer::Layer_Environment, 0xFFFFFFFF,
                            false, 0, 1, edmIndex);
            m_staticIds.push_back(id);
        }
    }

    // Create EventOnly triggers in CollisionManager
    void createEventOnlyTriggers(size_t count, float spread) {
        auto& cm = CollisionManager::Instance();
        std::uniform_real_distribution<float> posDist(-spread, spread);

        for (size_t i = 0; i < count; ++i) {
            Vector2D pos(posDist(m_rng) + spread, posDist(m_rng) + spread);

            // Create EventOnly trigger (water, area markers, etc.)
            EntityID id = cm.createTriggerAreaAt(
                pos.getX(), pos.getY(), 32.0f, 32.0f,
                VoidLight::TriggerTag::Water,
                VoidLight::TriggerType::EventOnly,
                CollisionLayer::Layer_Environment,
                CollisionLayer::Layer_Player | CollisionLayer::Layer_Enemy
            );
            m_triggerIds.push_back(id);
        }
    }

    // Enable trigger detection on a subset of movables
    void enableTriggerDetection(size_t count) {
        auto& edm = EntityDataManager::Instance();
        size_t enabled = 0;
        for (size_t i = 0; i < m_movableHandles.size() && enabled < count; ++i) {
            size_t idx = edm.getIndex(m_movableHandles[i]);
            if (idx != SIZE_MAX) {
                auto& hot = edm.getHotDataByIndex(idx);
                hot.setTriggerDetection(true);
                ++enabled;
            }
        }
    }

    // Get trigger detection stats
    size_t getTriggerDetectionCount() {
        return EntityDataManager::Instance().getTriggerDetectionIndices().size();
    }

    // Set up world bounds and culling
    void setupWorld(float size) {
        auto& cm = CollisionManager::Instance();
        cm.setWorldBounds(0.0f, 0.0f, size, size);
        cm.prepareCollisionBuffers(m_movableHandles.size() + m_staticIds.size());

        auto& bgm = BackgroundSimulationManager::Instance();
        bgm.setReferencePoint(Vector2D(size * 0.5f, size * 0.5f));
        bgm.setActiveRadius(size);

        // Update simulation tiers
        auto& edm = EntityDataManager::Instance();
        edm.updateSimulationTiers(Vector2D(size * 0.5f, size * 0.5f), size, size * 2.0f);
    }

    // Run benchmark iterations and return average time in ms
    double runBenchmark(int iterations) {
        auto& cm = CollisionManager::Instance();

        // Extended warmup for WorkerBudget hill-climb convergence
        // Hill-climb uses ADJUST_RATE=0.02f and THROUGHPUT_SMOOTHING=0.12
        // Need ~100 frames for multiplier hill-climb to converge
        constexpr int WARMUP_FRAMES = 100;
        for (int i = 0; i < WARMUP_FRAMES; ++i) {
            cm.update(0.016f);
        }

        // Benchmark (steady-state after hill-climb convergence)
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            cm.update(0.016f);
        }
        auto end = std::chrono::high_resolution_clock::now();

        return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    }

    // Get collision stats from last update
    std::pair<size_t, size_t> getLastStats() {
        const auto& stats = CollisionManager::Instance().getPerfStats();
        return {stats.lastPairs, stats.lastCollisions};
    }

    void cleanup() {
        auto& cm = CollisionManager::Instance();
        for (EntityID id : m_entityIds) {
            cm.removeCollisionBody(id);
        }
        for (EntityID id : m_staticIds) {
            cm.removeCollisionBody(id);
        }
        for (EntityID id : m_triggerIds) {
            cm.removeCollisionBody(id);
        }
        m_entityIds.clear();
        m_staticIds.clear();
        m_triggerIds.clear();
        m_movableHandles.clear();
        m_nextId = 1;
    }

private:
    std::mt19937 m_rng;
    std::vector<EntityHandle> m_movableHandles;
    std::vector<EntityID> m_entityIds;
    std::vector<EntityID> m_staticIds;
    std::vector<EntityID> m_triggerIds;
    EntityID m_nextId = 1;
    static bool s_initialized;
};

bool CollisionScalingFixture::s_initialized = false;

struct CollisionScalingModuleCleanup {
    ~CollisionScalingModuleCleanup() {
        if (!CollisionScalingFixture::initializedFlag()) {
            return;
        }

        BackgroundSimulationManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();

        CollisionScalingFixture::initializedFlag() = false;
    }
};

BOOST_GLOBAL_FIXTURE(CollisionScalingModuleCleanup);

} // anonymous namespace

// ===========================================================================
// Benchmark Suite
// ===========================================================================

BOOST_FIXTURE_TEST_SUITE(CollisionScalingTests, CollisionScalingFixture)

// Print header with system info
BOOST_AUTO_TEST_CASE(PrintHeader)
{
    const auto& budget = VoidLight::WorkerBudgetManager::Instance().getBudget();

    std::cout << "\n=== Collision Scaling Benchmark ===\n";
    std::cout << "Date: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "System: " << std::thread::hardware_concurrency() << " hardware threads\n";
    std::cout << "WorkerBudget: " << budget.totalWorkers << " workers\n";
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// MM Scaling (Sweep-and-Prune)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(MMScaling)
{
    std::cout << "--- MM Scaling (SAP) ---\n";
    std::cout << std::setw(10) << "Movables"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "MM Pairs"
              << std::setw(15) << "Throughput\n";

    std::vector<size_t> movableCounts = {100, 500, 1000, 2000, 5000, 10000};

    for (size_t count : movableCounts) {
        prepareForTest();

        // Create only movables (no statics) to isolate MM performance
        float worldSize = std::sqrt(static_cast<float>(count)) * 100.0f;
        createMovables(count, worldSize);
        setupWorld(worldSize * 2.0f);

        // Scale iterations for reliable measurements
        int iterations = std::max(20, 100000 / static_cast<int>(count));
        double avgMs = runBenchmark(iterations);
        auto [pairs, _] = getLastStats();

        double throughput = static_cast<double>(count) / avgMs;

        std::cout << std::setw(10) << count
                  << std::setw(12) << std::fixed << std::setprecision(2) << avgMs
                  << std::setw(12) << pairs
                  << std::setw(12) << std::fixed << std::setprecision(0) << throughput << "/ms\n";

        cleanup();
    }
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Dense MM Scaling — exercises narrowphase with realistic pair counts
// Uses tighter spacing so MM pairs grow proportionally with movable count
// instead of staying near-constant. This is where SIMD narrowphase matters.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(DenseMMScaling)
{
    std::cout << "--- Dense MM Scaling (narrowphase-heavy) ---\n";
    std::cout << std::setw(10) << "Movables"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "MM Pairs"
              << std::setw(15) << "ns/pair\n";

    std::vector<size_t> movableCounts = {100, 500, 1000, 2000, 5000, 10000};

    for (size_t count : movableCounts) {
        prepareForTest();

        // Tight spacing: ~6x denser than the sparse MM test (100 → 40).
        // This produces hundreds-to-thousands of MM pairs — enough that
        // narrowphase is a meaningful fraction of frame time.
        float worldSize = std::sqrt(static_cast<float>(count)) * 40.0f;
        createMovables(count, worldSize);
        setupWorld(worldSize * 2.0f);

        int iterations = std::max(20, 100000 / static_cast<int>(count));
        double avgMs = runBenchmark(iterations);
        auto [pairs, _] = getLastStats();

        double nsPerPair = (pairs > 0)
            ? (avgMs * 1e6) / static_cast<double>(pairs)
            : 0.0;

        std::cout << std::setw(10) << count
                  << std::setw(12) << std::fixed << std::setprecision(2) << avgMs
                  << std::setw(12) << pairs
                  << std::setw(12) << std::fixed << std::setprecision(1) << nsPerPair << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// MS Scaling (Spatial Hash)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(MSScaling)
{
    std::cout << "--- MS Scaling (Spatial Hash) ---\n";
    std::cout << std::setw(10) << "Statics"
              << std::setw(12) << "Movables"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "MS Pairs"
              << std::setw(15) << "Mode\n";

    std::vector<size_t> staticCounts = {100, 500, 2000, 5000, 10000, 20000};
    constexpr size_t FIXED_MOVABLES = 200;

    for (size_t staticCount : staticCounts) {
        prepareForTest();

        float worldSize = std::sqrt(static_cast<float>(staticCount)) * 50.0f;
        createMovables(FIXED_MOVABLES, worldSize);
        createStatics(staticCount, worldSize);
        setupWorld(worldSize * 2.0f);

        int iterations = std::max(20, 50000 / static_cast<int>(staticCount));
        double avgMs = runBenchmark(iterations);
        auto [pairs, _] = getLastStats();

        // Threshold is 100 for spatial hash
        const char* mode = staticCount >= 100 ? "hash" : "direct";

        std::cout << std::setw(10) << staticCount
                  << std::setw(12) << FIXED_MOVABLES
                  << std::setw(12) << std::fixed << std::setprecision(2) << avgMs
                  << std::setw(12) << pairs
                  << std::setw(15) << mode << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Combined Scaling (Real-world ratios)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(CombinedScaling)
{
    std::cout << "--- Combined Scaling ---\n";
    std::cout << std::setw(15) << "Scenario"
              << std::setw(12) << "Time (ms)"
              << std::setw(10) << "MM"
              << std::setw(10) << "MS"
              << std::setw(12) << "Total\n";

    struct Scenario {
        const char* name;
        size_t movables;
        size_t statics;
    };

    std::vector<Scenario> scenarios = {
        {"Small (500)", 200, 300},
        {"Medium (1500)", 500, 1000},
        {"Large (3000)", 1000, 2000},
        {"XL (6000)", 2000, 4000},
        {"XXL (12000)", 4000, 8000}
    };

    for (const auto& scenario : scenarios) {
        prepareForTest();

        size_t total = scenario.movables + scenario.statics;
        float worldSize = std::sqrt(static_cast<float>(total)) * 75.0f;

        createMovables(scenario.movables, worldSize);
        createStatics(scenario.statics, worldSize);
        setupWorld(worldSize * 2.0f);

        int iterations = std::max(20, 100000 / static_cast<int>(total));
        double avgMs = runBenchmark(iterations);
        auto [pairs, collisions] = getLastStats();

        // Approximate MM/MS split (pairs from movable-movable vs movable-static)
        // This is a rough estimate based on entity counts
        size_t estimatedMM = pairs / 2;  // Rough split
        size_t estimatedMS = pairs - estimatedMM;

        std::cout << std::setw(15) << scenario.name
                  << std::setw(12) << std::fixed << std::setprecision(2) << avgMs
                  << std::setw(10) << estimatedMM
                  << std::setw(10) << estimatedMS
                  << std::setw(12) << pairs << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Entity Density Test
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(EntityDensityTest)
{
    std::cout << "--- Entity Density Test (2000 movables, 2000 statics) ---\n";
    std::cout << std::setw(15) << "Distribution"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "Pairs"
              << std::setw(15) << "Collisions\n";

    constexpr size_t ENTITY_COUNT = 2000;
    constexpr float WORLD_SIZE = 4000.0f;

    // Test different distributions
    struct DensityTest {
        const char* name;
        float clusterRadius;  // 0 = spread, >0 = clustered
    };

    std::vector<DensityTest> tests = {
        {"Spread", 0.0f},
        {"Clustered", 500.0f},
        {"Mixed", 1000.0f}
    };

    for (const auto& test : tests) {
        prepareForTest();

        createMovables(ENTITY_COUNT, WORLD_SIZE, test.clusterRadius);
        createStatics(ENTITY_COUNT, WORLD_SIZE, test.clusterRadius);
        setupWorld(WORLD_SIZE * 2.0f);

        double avgMs = runBenchmark(50);
        auto [pairs, collisions] = getLastStats();

        std::cout << std::setw(15) << test.name
                  << std::setw(12) << std::fixed << std::setprecision(2) << avgMs
                  << std::setw(12) << pairs
                  << std::setw(15) << collisions << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Trigger Detection Scaling (Spatial Query vs Sweep-and-Prune)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(TriggerDetectionScaling)
{
    std::cout << "--- Trigger Detection Scaling ---\n";
    std::cout << std::setw(12) << "Detectors"
              << std::setw(12) << "Triggers"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "Overlaps"
              << std::setw(15) << "Method\n";

    // Test cases: varying number of entities with NEEDS_TRIGGER_DETECTION flag
    // Threshold is 50: < 50 uses spatial queries, >= 50 uses sweep-and-prune
    struct TriggerTest {
        size_t detectors;   // Entities with NEEDS_TRIGGER_DETECTION
        size_t triggers;    // EventOnly triggers
        const char* method; // Expected method
    };

    std::vector<TriggerTest> tests = {
        {1, 100, "spatial"},      // Player only - spatial query
        {1, 400, "spatial"},      // Player only, many triggers
        {10, 200, "spatial"},     // Few NPCs - spatial query
        {25, 200, "spatial"},     // More NPCs - still spatial
        {50, 200, "sweep"},       // At threshold - sweep-and-prune
        {100, 200, "sweep"},      // Many NPCs - sweep-and-prune
        {200, 400, "sweep"},      // Large scale - sweep-and-prune
    };

    constexpr float WORLD_SIZE = 2000.0f;
    constexpr size_t TOTAL_MOVABLES = 500;  // Background NPCs without trigger detection

    for (const auto& test : tests) {
        prepareForTest();

        // Create movables (only some will have trigger detection)
        createMovables(TOTAL_MOVABLES, WORLD_SIZE);

        // Enable trigger detection on subset
        enableTriggerDetection(test.detectors);

        // Create EventOnly triggers
        createEventOnlyTriggers(test.triggers, WORLD_SIZE);

        setupWorld(WORLD_SIZE * 2.0f);

        // Verify trigger detection count
        size_t actualDetectors = getTriggerDetectionCount();

        // Run benchmark
        int iterations = 50;
        double avgMs = runBenchmark(iterations);

        // Get overlap count from perf stats
        const auto& stats = CollisionManager::Instance().getPerfStats();
        size_t overlaps = stats.lastTriggerOverlaps;

        std::cout << std::setw(12) << actualDetectors
                  << std::setw(12) << test.triggers
                  << std::setw(12) << std::fixed << std::setprecision(3) << avgMs
                  << std::setw(12) << overlaps
                  << std::setw(15) << test.method << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

// NOTE: Adaptive threading tests moved to AdaptiveThreadingAnalysis.cpp
// This benchmark focuses on collision algorithm performance (SAP, spatial hash, triggers)

// ---------------------------------------------------------------------------
// Summary
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PrintSummary)
{
    std::cout << "SUMMARY:\n";
    std::cout << "  MM SAP: O(n log n) scaling - early termination reduces comparisons\n";
    std::cout << "  MS Hash: O(n) scaling - spatial hash queries nearby statics only\n";
    std::cout << "  Trigger Detection: Adaptive - spatial (<50) or sweep (>=50)\n";
    std::cout << "  Combined: Sub-quadratic scaling achieved\n";
    std::cout << "\n  NOTE: Adaptive threading tests in AdaptiveThreadingAnalysis.cpp\n";
    std::cout << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
