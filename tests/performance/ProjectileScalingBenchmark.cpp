/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * ProjectileManager Scaling Benchmark
 *
 * Tests the projectile system's performance characteristics:
 * 1. Entity scaling from 100 to 5,000 projectiles
 * 2. Threading mode comparison via WorkerBudget adaptive decisions
 * 3. SIMD throughput (ns/entity at various counts)
 *
 * Follows the same structure as AIScalingBenchmark for consistency.
 */

#define BOOST_TEST_MODULE ProjectileScalingBenchmark
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ProjectileManager.hpp"

namespace {

class ProjectileScalingFixture {
public:
    static bool& initializedFlag() { return s_initialized; }

    ProjectileScalingFixture() {
        if (!s_initialized) {
            VOIDLIGHT_ENABLE_BENCHMARK_MODE();
            BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
            BOOST_REQUIRE(EntityDataManager::Instance().init());
            BOOST_REQUIRE(PathfinderManager::Instance().init());
            BOOST_REQUIRE(CollisionManager::Instance().init());
            BOOST_REQUIRE(EventManager::Instance().init());
            BOOST_REQUIRE(ProjectileManager::Instance().init());
            s_initialized = true;
        }
        m_rng.seed(42);
    }

    ~ProjectileScalingFixture() = default;

    void prepareForTest() {
        ProjectileManager::Instance().prepareForStateTransition();
        EventManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
        EntityDataManager::Instance().prepareForStateTransition();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    }

    void createProjectiles(size_t count, float worldSize) {
        auto& edm = EntityDataManager::Instance();

        // Create a dummy owner if needed
        if (!m_ownerCreated) {
            m_owner = edm.registerPlayer(1, Vector2D(worldSize / 2.0f, worldSize / 2.0f));
            m_ownerCreated = true;
        }

        std::uniform_real_distribution<float> posDist(200.0f, worldSize - 200.0f);
        std::uniform_real_distribution<float> velDist(-150.0f, 150.0f);

        for (size_t i = 0; i < count; ++i) {
            Vector2D pos(posDist(m_rng), posDist(m_rng));
            Vector2D vel(velDist(m_rng), velDist(m_rng));
            EntityHandle h = edm.createProjectile(pos, vel, m_owner, 10.0f, 999.0f);
            m_handles.push_back(h);
        }
    }

    // Median-of-N benchmark (matches AIScalingBenchmark pattern)
    static constexpr int NUM_MEASUREMENT_RUNS = 5;
    static constexpr int WARMUP_FRAMES = 100;

    double runBenchmark(int iterations) {
        auto& pm = ProjectileManager::Instance();

        // Warmup for WorkerBudget hill-climb convergence
        for (int i = 0; i < WARMUP_FRAMES; ++i) {
            pm.update(0.016f);
        }

        // Multiple measurement passes — median is robust to outliers
        std::vector<double> runTimes;
        runTimes.reserve(NUM_MEASUREMENT_RUNS);

        for (int run = 0; run < NUM_MEASUREMENT_RUNS; ++run) {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                pm.update(0.016f);
            }
            auto end = std::chrono::high_resolution_clock::now();
            runTimes.push_back(
                std::chrono::duration<double, std::milli>(end - start).count() / iterations);
        }

        std::sort(runTimes.begin(), runTimes.end());
        return runTimes[NUM_MEASUREMENT_RUNS / 2];
    }

    void cleanup() {
        m_handles.clear();
    }

private:
    std::mt19937 m_rng;
    std::vector<EntityHandle> m_handles;
    EntityHandle m_owner;
    bool m_ownerCreated{false};
    static bool s_initialized;
};

bool ProjectileScalingFixture::s_initialized = false;

struct ProjectileScalingModuleCleanup {
    ~ProjectileScalingModuleCleanup() {
        if (!ProjectileScalingFixture::initializedFlag()) {
            return;
        }

        ProjectileManager::Instance().clean();
        EventManager::Instance().clean();
        CollisionManager::Instance().clean();
        PathfinderManager::Instance().clean();
        EntityDataManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();

        ProjectileScalingFixture::initializedFlag() = false;
    }
};

BOOST_GLOBAL_FIXTURE(ProjectileScalingModuleCleanup);

} // anonymous namespace


// ===========================================================================
// Benchmark Suite
// ===========================================================================

BOOST_FIXTURE_TEST_SUITE(ProjectileScalingTests, ProjectileScalingFixture)

BOOST_AUTO_TEST_CASE(PrintHeader)
{
    const auto& budget = VoidLight::WorkerBudgetManager::Instance().getBudget();

    std::cout << "\n=== Projectile Scaling Benchmark ===\n";
    std::cout << "Date: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "System: " << std::thread::hardware_concurrency() << " hardware threads\n";
    std::cout << "WorkerBudget: " << budget.totalWorkers << " workers\n";
    std::cout << "SIMD: 4-wide position integration\n";
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Projectile Entity Scaling (Primary benchmark)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ProjectileScaling)
{
    std::cout << "--- Projectile Entity Scaling ---\n";
    std::cout << std::setw(12) << "Projectiles"
              << std::setw(12) << "Time (ms)"
              << std::setw(14) << "Entities/ms"
              << std::setw(12) << "ns/entity"
              << std::setw(12) << "Threading"
              << std::setw(10) << "Status\n";

    std::vector<size_t> entityCounts = {100, 500, 1000, 2000, 5000};
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    size_t bestCount = 0;
    double bestThroughput = 0.0;

    for (size_t count : entityCounts) {
        prepareForTest();

        // World size scales with entity count to avoid boundary kills
        float worldSize = std::max(8000.0f, std::sqrt(static_cast<float>(count)) * 200.0f);
        createProjectiles(count, worldSize);

        // Verify projectile count
        size_t activeCount = EntityDataManager::Instance()
            .getIndicesByKind(EntityKind::Projectile).size();

        int iterations = std::max(50, 300000 / static_cast<int>(count));
        double medianMs = runBenchmark(iterations);

        double throughput = (medianMs > 0.0)
            ? static_cast<double>(count) / medianMs
            : 0.0;
        double nsPerEntity = (medianMs > 0.0 && count > 0)
            ? (medianMs * 1000000.0) / static_cast<double>(count)
            : 0.0;

        auto decision = budgetMgr.shouldUseThreading(
            VoidLight::SystemType::ProjectileSim, count);
        const char* threading = decision.shouldThread ? "multi" : "single";
        const char* status = (activeCount > 0 && medianMs > 0.0) ? "OK" : "FAIL";

        if (throughput > bestThroughput) {
            bestThroughput = throughput;
            bestCount = count;
        }

        std::cout << std::setw(12) << count
                  << std::setw(12) << std::fixed << std::setprecision(3) << medianMs
                  << std::setw(14) << std::fixed << std::setprecision(0) << throughput
                  << std::setw(12) << std::fixed << std::setprecision(1) << nsPerEntity
                  << std::setw(12) << threading
                  << std::setw(10) << status << "\n";

        cleanup();
    }

    std::cout << "\nSCALABILITY SUMMARY:\n";
    std::cout << "Measurement: median of " << NUM_MEASUREMENT_RUNS << " runs per count\n";
    std::cout << "Best throughput: " << std::fixed << std::setprecision(0)
              << bestThroughput << " entities/ms (at " << bestCount << " projectiles)\n";
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Threading Mode Comparison
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ThreadingModeComparison)
{
    std::cout << "--- Threading Mode Comparison ---\n";
    std::cout << "(Small counts likely single-threaded, large counts multi-threaded)\n";
    std::cout << std::setw(12) << "Projectiles"
              << std::setw(12) << "Time (ms)"
              << std::setw(14) << "Entities/ms"
              << std::setw(12) << "Threading\n";

    // Test at counts that straddle the typical threading threshold
    std::vector<size_t> entityCounts = {200, 500, 1000, 2000, 5000};

    for (size_t count : entityCounts) {
        prepareForTest();

        float worldSize = std::max(8000.0f, std::sqrt(static_cast<float>(count)) * 200.0f);
        createProjectiles(count, worldSize);

        int iterations = std::max(50, 300000 / static_cast<int>(count));
        double medianMs = runBenchmark(iterations);

        double throughput = (medianMs > 0.0)
            ? static_cast<double>(count) / medianMs
            : 0.0;

        const auto& stats = ProjectileManager::Instance().getPerfStats();
        const char* threading = stats.lastWasThreaded ? "multi" : "single";

        std::cout << std::setw(12) << count
                  << std::setw(12) << std::fixed << std::setprecision(3) << medianMs
                  << std::setw(14) << std::fixed << std::setprecision(0) << throughput
                  << std::setw(12) << threading << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// SIMD Throughput Analysis
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(SIMDThroughput)
{
    std::cout << "--- SIMD 4-Wide Throughput ---\n";
    std::cout << "Measuring position integration throughput (SIMD 4-wide batching)\n";
    std::cout << std::setw(12) << "Projectiles"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "ns/entity"
              << std::setw(16) << "Throughput/ms\n";

    // Focus on counts where SIMD efficiency matters
    std::vector<size_t> entityCounts = {4, 16, 64, 256, 1000, 4000};

    for (size_t count : entityCounts) {
        prepareForTest();

        float worldSize = 16000.0f;
        createProjectiles(count, worldSize);

        // More iterations for small counts to get stable timing
        int iterations = std::max(100, 500000 / static_cast<int>(count));
        double medianMs = runBenchmark(iterations);

        double nsPerEntity = (medianMs > 0.0 && count > 0)
            ? (medianMs * 1000000.0) / static_cast<double>(count)
            : 0.0;
        double throughput = (medianMs > 0.0)
            ? static_cast<double>(count) / medianMs
            : 0.0;

        std::cout << std::setw(12) << count
                  << std::setw(12) << std::fixed << std::setprecision(4) << medianMs
                  << std::setw(12) << std::fixed << std::setprecision(1) << nsPerEntity
                  << std::setw(16) << std::fixed << std::setprecision(0) << throughput << "\n";

        cleanup();
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
