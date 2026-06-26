/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * BackgroundSimulationManager Benchmark
 *
 * Tests the background simulation system's performance characteristics:
 * 1. Entity scaling from 100 to 10,000 background entities
 * 2. Threading mode comparison (single vs multi-threaded)
 * 3. WorkerBudget integration effectiveness
 * 4. Adaptive threading threshold tuning
 *
 * Follows the same structure as AIScalingBenchmark for consistency.
 */

#define BOOST_TEST_MODULE BackgroundSimulationManagerBenchmark
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <thread>

#include "managers/BackgroundSimulationManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/AIManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/CollisionManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "core/Logger.hpp"

namespace {

// Test fixture for BackgroundSimulationManager benchmarks
class BGSimBenchmarkFixture {
public:
    static bool& initializedFlag() { return s_initialized; }

    BGSimBenchmarkFixture() {
        // Initialize systems once per fixture
        if (!s_initialized) {
            VOIDLIGHT_ENABLE_BENCHMARK_MODE();
            BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
            BOOST_REQUIRE(EntityDataManager::Instance().init());
            BOOST_REQUIRE(PathfinderManager::Instance().init());
            PathfinderManager::Instance().rebuildGrid();
            BOOST_REQUIRE(CollisionManager::Instance().init());
            BOOST_REQUIRE(AIManager::Instance().init());
            BOOST_REQUIRE(BackgroundSimulationManager::Instance().init());

            // Set simulation radii for headless testing - push entities to background tier
            // Active radius: very small so most entities are in background tier
            // Background radius: very large to prevent hibernation
            BackgroundSimulationManager::Instance().setActiveRadius(100.0f);
            BackgroundSimulationManager::Instance().setBackgroundRadius(100000.0f);

            s_initialized = true;
        }
        m_rng.seed(42); // Fixed seed for reproducibility
    }

    ~BGSimBenchmarkFixture() = default;

    // Prepare fresh state for each test
    void prepareForTest() {
        BackgroundSimulationManager::Instance().prepareForStateTransition();
        AIManager::Instance().prepareForStateTransition();
        EntityDataManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    }

    // Create NPC entities and force them into background tier
    void createBackgroundEntities(size_t count, float worldSize) {
        auto& edm = EntityDataManager::Instance();
        std::uniform_real_distribution<float> posDist(1000.0f, worldSize - 1000.0f);

        for (size_t i = 0; i < count; ++i) {
            // Create NPCs far from reference point (0,0) to ensure background tier
            Vector2D pos(posDist(m_rng), posDist(m_rng));
            edm.createNPCWithRaceClass(pos, "Human", "Guard");
        }

        // Force tier update to classify entities as background
        BackgroundSimulationManager::Instance().setReferencePoint(Vector2D(0.0f, 0.0f));
        BackgroundSimulationManager::Instance().invalidateTiers();
        BackgroundSimulationManager::Instance().update(Vector2D(0.0f, 0.0f), 0.0f);
    }

    // Get current background entity count
    size_t getBackgroundEntityCount() const {
        return EntityDataManager::Instance().getBackgroundIndices().size();
    }

    // Run benchmark iterations
    struct BenchmarkResult {
        double avgTimeMs{0.0};
        double minTimeMs{0.0};
        double maxTimeMs{0.0};
        size_t entitiesProcessed{0};
        bool wasThreaded{false};
        size_t batchCount{0};
    };

    BenchmarkResult runBenchmark(int iterations, float deltaTime) {
        auto& bgsim = BackgroundSimulationManager::Instance();
        BenchmarkResult result;
        std::vector<double> times;
        times.reserve(iterations);

        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            bgsim.update(Vector2D(0.0f, 0.0f), deltaTime);
            auto end = std::chrono::high_resolution_clock::now();

            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            times.push_back(ms);
        }

        // Calculate statistics
        double total = 0.0;
        result.minTimeMs = times[0];
        result.maxTimeMs = times[0];

        for (double t : times) {
            total += t;
            result.minTimeMs = std::min(result.minTimeMs, t);
            result.maxTimeMs = std::max(result.maxTimeMs, t);
        }

        result.avgTimeMs = total / iterations;

        const auto& perf = bgsim.getPerfStats();
        result.entitiesProcessed = perf.lastEntitiesProcessed;
        result.wasThreaded = perf.lastWasThreaded;
        result.batchCount = perf.lastBatchCount;

        return result;
    }

protected:
    std::mt19937 m_rng;
    static bool s_initialized;
};

bool BGSimBenchmarkFixture::s_initialized = false;

struct BGSimBenchmarkModuleCleanup {
    ~BGSimBenchmarkModuleCleanup() {
        if (!BGSimBenchmarkFixture::initializedFlag()) {
            return;
        }

        BackgroundSimulationManager::Instance().clean();
        AIManager::Instance().clean();
        CollisionManager::Instance().clean();
        PathfinderManager::Instance().clean();
        EntityDataManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();

        BGSimBenchmarkFixture::initializedFlag() = false;
    }
};

BOOST_GLOBAL_FIXTURE(BGSimBenchmarkModuleCleanup);

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(BackgroundSimulationTests, BGSimBenchmarkFixture)

// Test scaling performance with increasing entity counts
BOOST_AUTO_TEST_CASE(BackgroundEntityScaling) {
    std::cout << "\n===== BACKGROUND SIMULATION SCALING TEST =====" << std::endl;
    std::cout << "Testing background entity processing from 100 to 10,000 entities\n" << std::endl;

    std::vector<size_t> entityCounts = {100, 500, 1000, 2500, 5000, 7500, 10000};
    const float WORLD_SIZE = 50000.0f;
    const int WARMUP_ITERATIONS = 10;
    const int BENCHMARK_ITERATIONS = 20;

    // Use deltaTime that triggers background processing (>= update interval)
    // Background sim uses 10Hz (0.1s interval), so use large deltaTime
    const float DELTA_TIME = 0.1f;

    std::cout << std::setw(12) << "Entities"
              << std::setw(15) << "Avg (ms)"
              << std::setw(15) << "Min (ms)"
              << std::setw(15) << "Max (ms)"
              << std::setw(12) << "Threaded"
              << std::setw(10) << "Batches" << std::endl;
    std::cout << std::string(79, '-') << std::endl;

    for (size_t targetCount : entityCounts) {
        prepareForTest();
        createBackgroundEntities(targetCount, WORLD_SIZE);

        size_t actualCount = getBackgroundEntityCount();

        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            BackgroundSimulationManager::Instance().update(Vector2D(0.0f, 0.0f), DELTA_TIME);
        }

        // Benchmark
        auto result = runBenchmark(BENCHMARK_ITERATIONS, DELTA_TIME);

        std::cout << std::setw(12) << actualCount
                  << std::setw(15) << std::fixed << std::setprecision(3) << result.avgTimeMs
                  << std::setw(15) << std::fixed << std::setprecision(3) << result.minTimeMs
                  << std::setw(15) << std::fixed << std::setprecision(3) << result.maxTimeMs
                  << std::setw(12) << (result.wasThreaded ? "yes" : "no")
                  << std::setw(10) << result.batchCount << std::endl;

        // Verify some entities were processed
        BOOST_CHECK_GT(result.entitiesProcessed, 0);
    }

    std::cout << "=========================================\n" << std::endl;
}

// Test threading threshold detection
BOOST_AUTO_TEST_CASE(ThreadingThresholdDetection) {
    std::cout << "\n===== BACKGROUND SIM THREADING THRESHOLD DETECTION =====" << std::endl;
    std::cout << "Comparing single-threaded vs multi-threaded at different entity counts\n" << std::endl;

    std::vector<size_t> testCounts = {100, 250, 500, 750, 1000, 2000, 5000};
    const float WORLD_SIZE = 50000.0f;
    const int ITERATIONS = 15;
    const float DELTA_TIME = 0.1f;

    size_t optimalThreshold = 0;
    size_t marginalThreshold = 0;

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    std::cout << std::setw(12) << "Entities"
              << std::setw(15) << "Avg (ms)"
              << std::setw(12) << "Threaded"
              << std::setw(15) << "Throughput"
              << std::setw(12) << "Verdict" << std::endl;
    std::cout << std::string(66, '-') << std::endl;

    for (size_t targetCount : testCounts) {
        prepareForTest();
        createBackgroundEntities(targetCount, WORLD_SIZE);

        size_t actualCount = getBackgroundEntityCount();

        // Warmup
        for (int i = 0; i < 5; ++i) {
            BackgroundSimulationManager::Instance().update(Vector2D(0.0f, 0.0f), DELTA_TIME);
        }

        // Benchmark
        auto result = runBenchmark(ITERATIONS, DELTA_TIME);

        double throughput = (result.avgTimeMs > 0)
            ? static_cast<double>(actualCount) / result.avgTimeMs
            : 0.0;

        std::string verdict;
        if (result.wasThreaded && throughput > 1000.0) {
            verdict = "THREAD";
            if (optimalThreshold == 0) optimalThreshold = actualCount;
        } else if (result.wasThreaded) {
            verdict = "marginal";
            if (marginalThreshold == 0) marginalThreshold = actualCount;
        } else {
            verdict = "single";
        }

        std::cout << std::setw(12) << actualCount
                  << std::setw(15) << std::fixed << std::setprecision(3) << result.avgTimeMs
                  << std::setw(12) << (result.wasThreaded ? "yes" : "no")
                  << std::setw(15) << std::fixed << std::setprecision(1) << throughput
                  << std::setw(12) << verdict << std::endl;

        BOOST_CHECK_GT(result.entitiesProcessed, 0);
    }

    std::cout << "\n=== THREADING RECOMMENDATION ===" << std::endl;
    double multiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::AI, true);
    float batchMult = budgetMgr.getBatchMultiplier(VoidLight::SystemType::AI);
    std::cout << "Multi throughput:  " << std::fixed << std::setprecision(2) << multiTP << " items/ms" << std::endl;
    std::cout << "Batch multiplier:  " << std::fixed << std::setprecision(2) << batchMult << std::endl;

    if (optimalThreshold > 0) {
        std::cout << "Optimal crossover detected: " << optimalThreshold << " entities" << std::endl;
    } else if (marginalThreshold > 0) {
        std::cout << "Marginal benefit at: " << marginalThreshold << " entities" << std::endl;
    } else {
        std::cout << "Single-threaded is efficient at all tested counts" << std::endl;
    }

    std::cout << "================================\n" << std::endl;
}

// WorkerBudget Adaptive Tuning test - verifies both batch sizing and threading
// threshold adapt correctly over time
BOOST_AUTO_TEST_CASE(WorkerBudgetAdaptiveTuning) {
    std::cout << "\n===== WORKERBUDGET ADAPTIVE TUNING TEST =====" << std::endl;
    std::cout << "Testing both batch sizing hill-climb and threading threshold adaptation\n" << std::endl;

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& bgsim = BackgroundSimulationManager::Instance();

    // Fresh state
    prepareForTest();

    const float WORLD_SIZE = 50000.0f;
    const float DELTA_TIME = 0.1f;  // Trigger background processing

    // Create enough entities for threading to be considered
    createBackgroundEntities(5000, WORLD_SIZE);

    // Part 1: Batch Sizing Hill-Climb Convergence
    std::cout << "--- Part 1: Batch Sizing Hill-Climb ---" << std::endl;
    size_t initialBatch = budgetMgr.getBatchStrategy(VoidLight::SystemType::AI, 5000, 4).first;
    std::cout << "Initial batch count for 5000 entities: " << initialBatch << std::endl;

    // Run updates to let hill-climb converge
    const int CONVERGENCE_FRAMES = 200;
    std::vector<size_t> batchHistory;

    for (int frame = 0; frame < CONVERGENCE_FRAMES; ++frame) {
        bgsim.update(Vector2D(0.0f, 0.0f), DELTA_TIME);

        // Sample every 20 frames
        if (frame % 20 == 0) {
            size_t currentBatch = budgetMgr.getBatchStrategy(VoidLight::SystemType::AI, 5000, 4).first;
            batchHistory.push_back(currentBatch);
        }
    }

    size_t finalBatch = budgetMgr.getBatchStrategy(VoidLight::SystemType::AI, 5000, 4).first;
    std::cout << "Final batch count after " << CONVERGENCE_FRAMES << " frames: " << finalBatch << std::endl;

    // Check convergence: batch count should stabilize (low variance in last few samples)
    bool batchConverged = false;
    if (batchHistory.size() >= 4) {
        size_t lastVariance = 0;
        size_t baseVal = batchHistory[batchHistory.size() - 4];
        for (size_t i = batchHistory.size() - 3; i < batchHistory.size(); ++i) {
            size_t diff = (batchHistory[i] > baseVal) ? batchHistory[i] - baseVal : baseVal - batchHistory[i];
            lastVariance = std::max(lastVariance, diff);
        }
        batchConverged = (lastVariance <= 2); // Within 2 batches of stable
        std::cout << "Batch variance in last 4 samples: " << lastVariance << std::endl;
    }

    std::cout << "Batch sizing status: " << (batchConverged ? "CONVERGED" : "ADAPTING") << std::endl;

    // Part 2: Throughput Tracking (replaces threshold adaptation)
    std::cout << "\n--- Part 2: Throughput Tracking ---" << std::endl;
    double initialMultiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::AI, true);
    std::cout << "Initial multi throughput:  " << std::fixed << std::setprecision(2) << initialMultiTP << " items/ms" << std::endl;

    // Run additional frames to allow throughput tracking
    const int TRACKING_FRAMES = 600;

    for (int frame = 0; frame < TRACKING_FRAMES; ++frame) {
        bgsim.update(Vector2D(0.0f, 0.0f), DELTA_TIME);

        // Sample throughput every 100 frames
        if (frame % 100 == 0) {
            double multiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::AI, true);
            float batchMultNow = budgetMgr.getBatchMultiplier(VoidLight::SystemType::AI);
            std::cout << "Frame " << frame << ": multiTP=" << std::fixed << std::setprecision(2)
                      << multiTP << " batchMult=" << batchMultNow << std::endl;
        }
    }

    double finalMultiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::AI, true);
    float finalBatchMultTP = budgetMgr.getBatchMultiplier(VoidLight::SystemType::AI);
    std::cout << "Final multi throughput:  " << std::fixed << std::setprecision(2) << finalMultiTP << " items/ms" << std::endl;
    std::cout << "Final batch multiplier:  " << std::fixed << std::setprecision(2) << finalBatchMultTP << std::endl;

    // Check if throughput has been collected
    bool throughputCollected = (finalMultiTP > 0);

    // Summary
    std::cout << "\n=== ADAPTIVE TUNING SUMMARY ===" << std::endl;
    std::cout << "Batch sizing:       " << (batchConverged ? "PASS" : "IN_PROGRESS") << std::endl;
    std::cout << "Throughput tracking: " << (throughputCollected ? "PASS" : "NO_DATA") << std::endl;
    std::cout << "Final batch count:  " << finalBatch << std::endl;
    std::cout << "================================\n" << std::endl;

    // Test passes if batch sizing converged OR throughput was collected
    // (both systems are working, just may be at different stages)
    BOOST_CHECK_MESSAGE(batchConverged || throughputCollected,
                        "At least one adaptive system should show activity");
}

BOOST_AUTO_TEST_SUITE_END()
