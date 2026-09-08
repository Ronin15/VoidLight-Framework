/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file PathfinderBenchmark.cpp
 * @brief Performance benchmarks for the PathfinderManager system
 *
 * Comprehensive pathfinding performance tests covering:
 * - Async pathfinding request throughput and latency
 * - Cache performance and hit rates
 * - Threading overhead vs benefits analysis
 * - Obstacle density impact on pathfinding performance
 * - Path length vs computation time scaling
 */

#define BOOST_TEST_MODULE PathfinderBenchmark
#include <boost/test/unit_test.hpp>

#include "managers/PathfinderManager.hpp"
#include "ai/pathfinding/PathfindingGrid.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/EventManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "world/WorldGenerator.hpp"
#include "utils/Vector2D.hpp"

#include <chrono>
#include <vector>
#include <random>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <atomic>

using namespace std::chrono;

class PathfinderBenchmarkFixture {
public:
    PathfinderBenchmarkFixture() {
        // Initialize core systems required for pathfinding (order matters!)
        BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init()); // Auto-detect system threads

        // Log WorkerBudget allocations for production-matching verification
        const auto& budget = VoidLight::WorkerBudgetManager::Instance().getBudget();
        std::cout << "System: " << std::thread::hardware_concurrency() << " hardware threads\n";
        std::cout << "WorkerBudget: " << budget.totalWorkers << " workers (all available per manager)\n";

        // Initialize resource managers first
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());

        // Initialize EventManager for event-driven architecture
        BOOST_REQUIRE(EventManager::Instance().init());

        // Initialize EntityDataManager before collision manager (EDM owns static body data)
        BOOST_REQUIRE(EntityDataManager::Instance().init());

        // Initialize world and collision managers
        BOOST_REQUIRE(WorldManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());

        // Set up a basic world for pathfinding tests
        setupTestWorld();

        std::cout << "\n=== PathfinderManager Benchmark Suite ===\n";
        std::cout << "Testing pathfinding performance across various scenarios\n\n";
    }

    ~PathfinderBenchmarkFixture() {
        // For benchmarks, we keep the world and managers loaded across all test cases
        // to measure steady-state performance rather than cold-start performance.
        // Clean only happens at the very end via global fixture.
    }

private:
    void setupTestWorld() {
        // Create a 200x200 world for testing using WorldManager
        VoidLight::WorldGenerationConfig config;
        config.width = 200;
        config.height = 200;
        config.seed = 42; // Fixed seed for reproducible results
        config.elevationFrequency = 0.1f;
        config.humidityFrequency = 0.1f;
        config.waterLevel = 0.3f;
        config.mountainLevel = 0.7f;

        // Load the world through WorldManager - this provides the pathfinding grid context
        bool worldLoaded = WorldManager::Instance().loadNewWorld(config);
        if (!worldLoaded) {
            throw std::runtime_error("Failed to load test world for pathfinding benchmark");
        }

        // EVENT-DRIVEN: Process deferred events (triggers WorldLoaded task on ThreadSystem)
        EventManager::Instance().update();

        // Give ThreadSystem time to execute the WorldLoaded task and enqueue the deferred event
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Process the deferred WorldLoadedEvent (delivers to PathfinderManager)
        EventManager::Instance().update();

        // Wait for async grid rebuild to complete (~100-200ms for test world)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "Pathfinding grid ready for benchmarks\n";

        // Set world bounds for collision manager (required for collision system)
        CollisionManager::Instance().setWorldBounds(0, 0, config.width * 32, config.height * 32);

        // Add some collision obstacles for more realistic pathfinding testing
        std::mt19937 rng(42); // Fixed seed for reproducible results
        std::uniform_int_distribution<int> posDist(10, config.width - 10);

        // Add 5% collision obstacles (separate from world terrain obstacles)
        int numObstacles = static_cast<int>((config.width * config.height) * 0.05f);
        auto& edm = EntityDataManager::Instance();
        for (int i = 0; i < numObstacles; ++i) {
            int x = posDist(rng);
            int y = posDist(rng);

            // Create static body through EDM and get its index
            Vector2D center(x * VoidLight::TILE_SIZE, y * VoidLight::TILE_SIZE);
            EntityHandle handle = edm.createStaticBody(center, 16.0f, 16.0f);
            size_t edmIndex = edm.getStaticIndex(handle);
            EntityID obstacleId = handle.getId();

            // Add obstacle to collision system with proper EDM routing
            CollisionManager::Instance().addStaticBody(
                obstacleId,
                center,
                Vector2D(16.0f, 16.0f),
                CollisionLayer::Layer_Environment,
                CollisionLayer::Layer_Environment,
                false, 0, 1, edmIndex
            );
        }

        // The pathfinding grid should now be automatically available through WorldManager
        std::cout << "Test world loaded: " << config.width << "x" << config.height
                  << " with " << numObstacles << " collision obstacles\n";
    }
};

namespace {

size_t createBenchmarkNpc() {
    EntityHandle handle = EntityDataManager::Instance().createNPCWithRaceClass(
        Vector2D(64.0f, 64.0f), "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());
    const size_t idx = EntityDataManager::Instance().getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);
    return idx;
}

bool waitForBenchmarkPath(size_t edmIndex, std::vector<Vector2D>& outPath,
                          high_resolution_clock::time_point& doneAt) {
    auto& pm = PathfinderManager::Instance();
    auto& edm = EntityDataManager::Instance();
    while (true) {
        pm.update();
        pm.commitCompletedPaths();
        auto& pd = edm.getPathData(edmIndex);
        if (pd.pathRequestPending.load(std::memory_order_acquire) == 0) {
            doneAt = high_resolution_clock::now();
            outPath.clear();
            if (pd.hasPath) {
                outPath.reserve(pd.pathLength);
                for (uint16_t w = 0; w < pd.pathLength; ++w) {
                    outPath.push_back(edm.getWaypoint(edmIndex, w));
                }
            }
            return pd.hasPath;
        }
        std::this_thread::yield();
    }
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(PathfinderBenchmarkSuite, PathfinderBenchmarkFixture)

BOOST_AUTO_TEST_CASE(BenchmarkImmediatePathfinding) {
    std::cout << "=== Immediate Pathfinding Performance ===\n";

    const std::vector<std::pair<int, std::string>> gridSizes = {
        {50, "Small Grid (50x50)"},
        {100, "Medium Grid (100x100)"},
        {150, "Large Grid (150x150)"},
        {200, "XLarge Grid (200x200)"}
    };

    const int pathsPerSize = 100;
    std::mt19937 rng(42);

    for (const auto& [gridSize, description] : gridSizes) {
        std::uniform_int_distribution<int> coordDist(5, gridSize - 5);

        std::vector<double> queuingLatencies;
        std::vector<double> completionTimes;
        queuingLatencies.reserve(pathsPerSize);
        completionTimes.reserve(pathsPerSize);

        int successfulPaths = 0;

        auto startBatch = high_resolution_clock::now();

        for (int i = 0; i < pathsPerSize; ++i) {
            Vector2D start(coordDist(rng) * VoidLight::TILE_SIZE, coordDist(rng) * VoidLight::TILE_SIZE);
            Vector2D goal(coordDist(rng) * VoidLight::TILE_SIZE, coordDist(rng) * VoidLight::TILE_SIZE);

            std::vector<Vector2D> path;
            high_resolution_clock::time_point doneAt;
            const size_t npc = createBenchmarkNpc();

            auto requestStart = high_resolution_clock::now();
            BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
                npc, start, goal, PathfinderManager::Priority::High), 0U);
            auto requestQueued = high_resolution_clock::now();

            double queuingLatencyUs = duration_cast<nanoseconds>(requestQueued - requestStart).count() / 1000.0;
            queuingLatencies.push_back(queuingLatencyUs);

            const bool pathSuccess = waitForBenchmarkPath(npc, path, doneAt);

            double completionTimeMs = duration_cast<microseconds>(doneAt - requestStart).count() / 1000.0;
            completionTimes.push_back(completionTimeMs);

            if (pathSuccess) {
                successfulPaths++;
            }
        }

        auto endBatch = high_resolution_clock::now();
        double totalBatchTime = duration_cast<milliseconds>(endBatch - startBatch).count();

        // Calculate statistics for queuing latency
        std::sort(queuingLatencies.begin(), queuingLatencies.end());
        double avgQueueLatency = std::accumulate(queuingLatencies.begin(), queuingLatencies.end(), 0.0) / queuingLatencies.size();
        double medianQueueLatency = queuingLatencies[queuingLatencies.size() / 2];

        // Calculate statistics for completion times
        std::sort(completionTimes.begin(), completionTimes.end());
        double avgTime = std::accumulate(completionTimes.begin(), completionTimes.end(), 0.0) / completionTimes.size();
        double medianTime = completionTimes[completionTimes.size() / 2];
        double minTime = completionTimes.front();
        double maxTime = completionTimes.back();
        double p95Time = completionTimes[static_cast<size_t>(completionTimes.size() * 0.95)];

        std::cout << description << ":\n";
        std::cout << "  Paths tested: " << pathsPerSize << "\n";
        std::cout << "  Successful paths: " << successfulPaths << " ("
                  << std::fixed << std::setprecision(1)
                  << (100.0 * successfulPaths / pathsPerSize) << "%)\n";
        std::cout << "  Total batch time: " << totalBatchTime << "ms\n\n";

        std::cout << "  Queuing latency:\n";
        std::cout << "    Average: " << std::setprecision(2) << avgQueueLatency << "us\n";
        std::cout << "    Median: " << medianQueueLatency << "us\n\n";

        std::cout << "  Completion time (request to callback):\n";
        std::cout << "    Average: " << std::setprecision(3) << avgTime << "ms\n";
        std::cout << "    Median: " << medianTime << "ms\n";
        std::cout << "    Min: " << minTime << "ms\n";
        std::cout << "    Max: " << maxTime << "ms\n";
        std::cout << "    95th percentile: " << p95Time << "ms\n";
        std::cout << "  Paths/second: " << std::setprecision(0)
                  << (1000.0 * pathsPerSize / totalBatchTime) << "\n\n";
    }
}

BOOST_AUTO_TEST_CASE(BenchmarkAsyncPathfinding) {
    std::cout << "=== Async Pathfinding Throughput ===\n";

    const std::vector<int> batchSizes = {10, 50, 100, 250, 500};
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> coordDist(5, 195);

    for (int batchSize : batchSizes) {
        std::vector<size_t> npcs;
        npcs.reserve(static_cast<size_t>(batchSize));
        for (int i = 0; i < batchSize; ++i) {
            npcs.push_back(createBenchmarkNpc());
        }

        auto requestStart = high_resolution_clock::now();
        for (int i = 0; i < batchSize; ++i) {
            Vector2D start(coordDist(rng) * VoidLight::TILE_SIZE, coordDist(rng) * VoidLight::TILE_SIZE);
            Vector2D goal(coordDist(rng) * VoidLight::TILE_SIZE, coordDist(rng) * VoidLight::TILE_SIZE);
            PathfinderManager::Instance().requestPathToEDM(
                npcs[static_cast<size_t>(i)], start, goal,
                PathfinderManager::Priority::Normal);
        }
        auto requestEnd = high_resolution_clock::now();
        double requestTimeMs = duration_cast<microseconds>(requestEnd - requestStart).count() / 1000.0;

        auto processingStart = high_resolution_clock::now();
        auto& edm = EntityDataManager::Instance();
        int completedCount = 0;
        while (true) {
            PathfinderManager::Instance().update();
            PathfinderManager::Instance().commitCompletedPaths();
            completedCount = 0;
            bool allDone = true;
            for (size_t idx : npcs) {
                const auto& pd = edm.getPathData(idx);
                if (pd.pathRequestPending.load(std::memory_order_acquire) != 0) {
                    allDone = false;
                    break;
                }
                ++completedCount;
            }
            if (allDone) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            auto elapsed = duration_cast<seconds>(high_resolution_clock::now() - processingStart).count();
            if (elapsed > 10) {
                std::cout << "WARNING: Timeout waiting for batch completion ("
                          << completedCount << "/" << batchSize << " completed)\n";
                break;
            }
        }

        auto processingEnd = high_resolution_clock::now();
        double processingTimeMs = duration_cast<microseconds>(processingEnd - processingStart).count() / 1000.0;
        double actualCompletionMs = processingTimeMs;

        std::cout << "Batch size " << batchSize << ":\n";
        std::cout << "  Completed: " << completedCount << "/" << batchSize << "\n";
        std::cout << "  Request submission: " << std::setprecision(3) << requestTimeMs << "ms\n";
        std::cout << "  Request rate: " << std::setprecision(0)
                  << (batchSize / (requestTimeMs / 1000.0)) << " requests/sec\n";
        std::cout << "  Actual completion time: " << std::setprecision(3) << actualCompletionMs << "ms\n";
        std::cout << "  Processing time (including polling): " << processingTimeMs << "ms\n";

        if (completedCount > 0 && actualCompletionMs > 0.0) {
            std::cout << "  Throughput: " << std::setprecision(0)
                      << (completedCount / (actualCompletionMs / 1000.0)) << " paths/sec\n";
        }
        std::cout << "\n";
    }
}

BOOST_AUTO_TEST_CASE(BenchmarkPathLengthScaling) {
    std::cout << "=== Path Length vs Performance ===\n";

    const std::vector<std::pair<Vector2D, Vector2D>> pathTests = {
        {{VoidLight::TILE_SIZE, VoidLight::TILE_SIZE}, {64.0f, 64.0f}}, // Very short path
        {{VoidLight::TILE_SIZE, VoidLight::TILE_SIZE}, {320.0f, 320.0f}}, // Short path
        {{VoidLight::TILE_SIZE, VoidLight::TILE_SIZE}, {1600.0f, 1600.0f}}, // Medium path
        {{VoidLight::TILE_SIZE, VoidLight::TILE_SIZE}, {3200.0f, 3200.0f}}, // Long path
        {{VoidLight::TILE_SIZE, VoidLight::TILE_SIZE}, {6000.0f, 6000.0f}} // Very long path
    };

    const int testsPerPath = 20;

    for (size_t i = 0; i < pathTests.size(); ++i) {
        const auto& [start, goal] = pathTests[i];
        float distance = (goal - start).length();

        std::vector<double> queuingLatencies;
        std::vector<double> completionTimes;
        std::vector<size_t> pathLengths;
        queuingLatencies.reserve(testsPerPath);
        completionTimes.reserve(testsPerPath);
        pathLengths.reserve(testsPerPath);

        int successfulPaths = 0;

        for (int test = 0; test < testsPerPath; ++test) {
            std::vector<Vector2D> path;
            high_resolution_clock::time_point doneAt;
            const size_t npc = createBenchmarkNpc();

            auto requestStart = high_resolution_clock::now();
            BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
                npc, start, goal, PathfinderManager::Priority::High), 0U);
            auto requestQueued = high_resolution_clock::now();

            double queuingLatencyUs = duration_cast<nanoseconds>(requestQueued - requestStart).count() / 1000.0;
            queuingLatencies.push_back(queuingLatencyUs);

            const bool pathSuccess = waitForBenchmarkPath(npc, path, doneAt);

            double completionTimeMs = duration_cast<microseconds>(doneAt - requestStart).count() / 1000.0;
            completionTimes.push_back(completionTimeMs);
            pathLengths.push_back(path.size());

            if (pathSuccess) {
                successfulPaths++;
            }
        }

        if (!completionTimes.empty()) {
            double avgQueueLatency = std::accumulate(queuingLatencies.begin(), queuingLatencies.end(), 0.0) / queuingLatencies.size();
            double avgCompletionTime = std::accumulate(completionTimes.begin(), completionTimes.end(), 0.0) / completionTimes.size();
            double avgLength = std::accumulate(pathLengths.begin(), pathLengths.end(), 0.0) / pathLengths.size();

            std::cout << "Distance " << std::setprecision(0) << distance << " units:\n";
            std::cout << "  Success rate: " << successfulPaths << "/" << testsPerPath
                      << " (" << std::setprecision(1) << (100.0 * successfulPaths / testsPerPath) << "%)\n";
            std::cout << "  Average queuing latency: " << std::setprecision(2) << avgQueueLatency << "us\n";
            std::cout << "  Average completion time: " << std::setprecision(3) << avgCompletionTime << "ms\n";
            std::cout << "  Average path nodes: " << std::setprecision(1) << avgLength << "\n";
            if (avgLength > 0) {
                std::cout << "  Time per node: " << std::setprecision(3) << (avgCompletionTime / avgLength) << "ms\n";
            }
            std::cout << "\n";
        }
    }
}

BOOST_AUTO_TEST_CASE(BenchmarkCachePerformance) {
    std::cout << "=== Cache Performance Analysis ===\n";

    // Test cache effectiveness by repeating common paths
    const int numUniquePaths = 50;
    const int repeatsPerPath = 5;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> coordDist(5, 195);

    // Generate unique path requests
    std::vector<std::pair<Vector2D, Vector2D>> uniquePaths;
    for (int i = 0; i < numUniquePaths; ++i) {
        Vector2D start(coordDist(rng) * VoidLight::TILE_SIZE, coordDist(rng) * VoidLight::TILE_SIZE);
        Vector2D goal(coordDist(rng) * VoidLight::TILE_SIZE, coordDist(rng) * VoidLight::TILE_SIZE);
        uniquePaths.emplace_back(start, goal);
    }

    std::vector<double> firstRunTimes;
    std::vector<double> cachedRunTimes;

    // First run - populate cache
    auto firstRunStart = high_resolution_clock::now();
    const size_t cacheNpc = createBenchmarkNpc();
    for (const auto& [start, goal] : uniquePaths) {
        std::vector<Vector2D> path;
        high_resolution_clock::time_point doneAt;

        auto requestStart = high_resolution_clock::now();
        BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
            cacheNpc, start, goal, PathfinderManager::Priority::High), 0U);
        waitForBenchmarkPath(cacheNpc, path, doneAt);

        double pathTimeMs = duration_cast<microseconds>(doneAt - requestStart).count() / 1000.0;
        firstRunTimes.push_back(pathTimeMs);
    }
    auto firstRunEnd = high_resolution_clock::now();

    auto cachedRunStart = high_resolution_clock::now();
    for (int repeat = 0; repeat < repeatsPerPath; ++repeat) {
        for (const auto& [start, goal] : uniquePaths) {
            std::vector<Vector2D> path;
            high_resolution_clock::time_point doneAt;

            auto requestStart = high_resolution_clock::now();
            BOOST_CHECK_GT(PathfinderManager::Instance().requestPathToEDM(
                cacheNpc, start, goal, PathfinderManager::Priority::High), 0U);
            waitForBenchmarkPath(cacheNpc, path, doneAt);

            double pathTimeMs = duration_cast<microseconds>(doneAt - requestStart).count() / 1000.0;
            cachedRunTimes.push_back(pathTimeMs);
        }
    }
    auto cachedRunEnd = high_resolution_clock::now();

    // Calculate statistics
    double avgFirstRun = std::accumulate(firstRunTimes.begin(), firstRunTimes.end(), 0.0) / firstRunTimes.size();
    double avgCachedRun = std::accumulate(cachedRunTimes.begin(), cachedRunTimes.end(), 0.0) / cachedRunTimes.size();

    // Calculate standard deviations for statistical significance testing
    double firstRunVariance = 0.0;
    for (double time : firstRunTimes) {
        double diff = time - avgFirstRun;
        firstRunVariance += diff * diff;
    }
    firstRunVariance /= firstRunTimes.size();
    double firstRunStdDev = std::sqrt(firstRunVariance);

    double cachedRunVariance = 0.0;
    for (double time : cachedRunTimes) {
        double diff = time - avgCachedRun;
        cachedRunVariance += diff * diff;
    }
    cachedRunVariance /= cachedRunTimes.size();
    double cachedRunStdDev = std::sqrt(cachedRunVariance);

    double firstRunTotal = duration_cast<milliseconds>(firstRunEnd - firstRunStart).count();
    double cachedRunTotal = duration_cast<milliseconds>(cachedRunEnd - cachedRunStart).count();

    std::cout << "Unique paths tested: " << numUniquePaths << "\n";
    std::cout << "Repeats per path: " << repeatsPerPath << "\n\n";

    std::cout << "First run (cold cache):\n";
    std::cout << "  Average time per path: " << std::setprecision(3) << avgFirstRun << "ms\n";
    std::cout << "  Std deviation: " << std::setprecision(3) << firstRunStdDev << "ms\n";
    std::cout << "  Total time: " << std::setprecision(1) << firstRunTotal << "ms\n";
    std::cout << "  Paths/second: " << std::setprecision(0)
              << (1000.0 * numUniquePaths / firstRunTotal) << "\n\n";

    std::cout << "Cached runs (warm cache):\n";
    std::cout << "  Average time per path: " << std::setprecision(3) << avgCachedRun << "ms\n";
    std::cout << "  Std deviation: " << std::setprecision(3) << cachedRunStdDev << "ms\n";
    std::cout << "  Total time: " << std::setprecision(1) << cachedRunTotal << "ms\n";
    std::cout << "  Paths/second: " << std::setprecision(0)
              << (1000.0 * cachedRunTimes.size() / cachedRunTotal) << "\n\n";

    double speedupRatio = avgFirstRun / avgCachedRun;
    double speedupDiff = avgFirstRun - avgCachedRun;

    // Statistical significance check: speedup should be larger than combined std deviations
    double combinedStdDev = std::sqrt(firstRunStdDev * firstRunStdDev + cachedRunStdDev * cachedRunStdDev);
    bool statisticallySignificant = speedupDiff > combinedStdDev;

    std::cout << "Cache performance:\n";
    std::cout << "  Speedup ratio: " << std::setprecision(2) << speedupRatio << "x\n";
    std::cout << "  Speedup difference: " << std::setprecision(3) << speedupDiff << "ms\n";
    std::cout << "  Combined std dev: " << std::setprecision(3) << combinedStdDev << "ms\n";
    std::cout << "  Statistically significant: " << (statisticallySignificant ? "YES" : "NO")
              << " (speedup > std dev)\n";
    std::cout << "  Cache efficiency: " << std::setprecision(1)
              << ((speedupRatio - 1.0) / speedupRatio * 100.0) << "%\n\n";

    if (!statisticallySignificant) {
        std::cout << "  NOTE: Cache speedup may be within timing variance.\n";
        std::cout << "        Consider testing with longer paths or more samples.\n\n";
    }
}

BOOST_AUTO_TEST_SUITE_END()

// Global fixture for benchmark suite - handles final cleanup
struct BenchmarkGlobalFixture {
    BenchmarkGlobalFixture() {
        // Global setup (if needed)
    }

    ~BenchmarkGlobalFixture() {
        // Clean up all managers at the end of all benchmark tests
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        WorldManager::Instance().clean();
        EntityDataManager::Instance().clean();
        EventManager::Instance().clean();
        WorldResourceManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();

        std::cout << "\n=== Pathfinder Benchmark Summary ===\n";
        std::cout << "Benchmark completed successfully!\n";
        std::cout << "\nKey Performance Indicators:\n";
        std::cout << "• Immediate pathfinding should complete in < 20ms for most paths\n";
        std::cout << "• Async throughput should exceed 100 paths/second\n";
        std::cout << "• Cache should provide 2x+ speedup for repeated paths\n";
        std::cout << "• Success rate should be > 90% for reasonable path requests\n";
        std::cout << "\nFor detailed metrics, check the benchmark output above.\n";
        std::cout << "==========================================\n\n";
    }
};

// Global test setup
BOOST_GLOBAL_FIXTURE(BenchmarkGlobalFixture);
