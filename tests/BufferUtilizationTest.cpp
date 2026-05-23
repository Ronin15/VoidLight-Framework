/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE BufferUtilizationTest
#include <boost/test/unit_test.hpp>
#include "core/WorkerBudget.hpp"
#include "core/ThreadSystem.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdlib>

// Global fixture: ThreadSystem initialized once for entire test suite
struct GlobalThreadSystemFixture {
    GlobalThreadSystemFixture() {
        // Initialize ThreadSystem with default worker count (hardware_concurrency - 1)
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~GlobalThreadSystemFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

// Apply global fixture to entire test module
BOOST_GLOBAL_FIXTURE(GlobalThreadSystemFixture);

BOOST_AUTO_TEST_SUITE(WorkerBudgetManagerTests)

BOOST_AUTO_TEST_CASE(TestBudgetAllocation) {
    std::cout << "\n=== Testing WorkerBudgetManager Budget Allocation ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = budgetMgr.getBudget();

    std::cout << "System: " << budget.totalWorkers << " workers total\n";
    std::cout << "Sequential execution model: each manager gets all "
              << budget.totalWorkers << " workers during its window\n";

    // Verify we have workers
    BOOST_CHECK_GT(budget.totalWorkers, 0);
}

BOOST_AUTO_TEST_CASE(TestOptimalWorkersAllWorkloads) {
    std::cout << "\n=== Testing Optimal Workers - Sequential Execution Model ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = budgetMgr.getBudget();

    // Any workload should get all workers (sequential execution model)
    size_t lowWorkload = 500;
    size_t optimalLow = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::AI, lowWorkload);

    size_t highWorkload = 5000;
    size_t optimalHigh = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::AI, highWorkload);

    std::cout << "Low workload (" << lowWorkload << " entities): "
              << optimalLow << " workers\n";
    std::cout << "High workload (" << highWorkload << " entities): "
              << optimalHigh << " workers\n";
    std::cout << "Total workers: " << budget.totalWorkers << "\n";

    // Both should get all workers (sequential execution = no contention)
    BOOST_CHECK_EQUAL(optimalLow, budget.totalWorkers);
    BOOST_CHECK_EQUAL(optimalHigh, budget.totalWorkers);
}

BOOST_AUTO_TEST_CASE(TestZeroWorkloadReturnsZero) {
    std::cout << "\n=== Testing Zero Workload Returns Zero Workers ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    size_t optimalWorkers = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::AI, 0);

    std::cout << "Zero workload: " << optimalWorkers << " workers\n";

    // Zero work = zero workers needed
    BOOST_CHECK_EQUAL(optimalWorkers, 0);
}

BOOST_AUTO_TEST_CASE(TestBatchStrategy) {
    std::cout << "\n=== Testing Batch Strategy ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    size_t workload = 1000;
    size_t optimalWorkers = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::AI, workload);

    auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::AI, workload, optimalWorkers);

    std::cout << "Workload: " << workload << ", Workers: " << optimalWorkers << "\n";
    std::cout << "Batch strategy: " << batchCount << " batches of size " << batchSize << "\n";

    // Batches should cover all work
    BOOST_CHECK_GE(batchCount * batchSize, workload);

    // Should have reasonable batch count
    BOOST_CHECK_GE(batchCount, 1);
}

BOOST_AUTO_TEST_CASE(TestExecutionReporting) {
    std::cout << "\n=== Testing Execution Reporting ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    // Report some executions (workload, wasThreaded, batchCount, timeMs)
    size_t workload = 1000;
    budgetMgr.reportExecution(VoidLight::SystemType::AI, workload, true, 4, 0.5);  // 0.5ms total
    budgetMgr.reportExecution(VoidLight::SystemType::AI, workload, true, 4, 0.5);

    // Get batch strategy after reporting
    size_t workers = 4;
    auto [batchCount1, batchSize1] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::AI, workload, workers);

    std::cout << "After fast execution (0.5ms total): " << batchCount1 << " batches\n";

    // Report slow execution
    budgetMgr.reportExecution(VoidLight::SystemType::AI, workload, true, 2, 10.0);  // 10ms total
    budgetMgr.reportExecution(VoidLight::SystemType::AI, workload, true, 2, 10.0);

    auto [batchCount2, batchSize2] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::AI, workload, workers);

    std::cout << "After slow execution (10ms total): " << batchCount2 << " batches\n";

    // Both should produce valid batch strategies
    BOOST_CHECK_GE(batchCount1, 1);
    BOOST_CHECK_GE(batchCount2, 1);
}

BOOST_AUTO_TEST_CASE(TestAllSystemTypes) {
    std::cout << "\n=== Testing All System Types ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = budgetMgr.getBudget();

    size_t workload = 2000;

    // Test each system type - all should get same allocation (sequential execution)
    std::vector<std::pair<VoidLight::SystemType, const char*>> systems = {
        {VoidLight::SystemType::AI, "AI"},
        {VoidLight::SystemType::Particle, "Particle"},
        {VoidLight::SystemType::Pathfinding, "Pathfinding"},
        {VoidLight::SystemType::Event, "Event"}
    };

    for (const auto& [type, name] : systems) {
        size_t optimalWorkers = budgetMgr.getOptimalWorkers(type, workload);
        auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(type, workload, optimalWorkers);

        std::cout << name << ": optimal=" << optimalWorkers
                  << ", batches=" << batchCount << "x" << batchSize << "\n";

        // All systems get all workers (sequential execution model)
        BOOST_CHECK_EQUAL(optimalWorkers, budget.totalWorkers);

        // Batch strategy should be valid
        BOOST_CHECK_GE(batchCount, 1);
        BOOST_CHECK_GE(batchSize, 1);
    }
}

BOOST_AUTO_TEST_CASE(TestCacheInvalidation) {
    std::cout << "\n=== Testing Cache Invalidation ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    // Get initial budget
    const auto& budget1 = budgetMgr.getBudget();
    std::cout << "Initial budget - totalWorkers: " << budget1.totalWorkers << "\n";

    // Invalidate cache
    budgetMgr.invalidateCache();

    // Get budget again (should recalculate)
    const auto& budget2 = budgetMgr.getBudget();
    std::cout << "After invalidation - totalWorkers: " << budget2.totalWorkers << "\n";

    // Should have same values (ThreadSystem hasn't changed)
    BOOST_CHECK_EQUAL(budget1.totalWorkers, budget2.totalWorkers);
}

BOOST_AUTO_TEST_CASE(TestThreadingThresholds) {
    std::cout << "\n=== Testing Threading Thresholds ===\n";
    std::cout << "Validating WorkerBudget threshold learning state machine...\n\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    const auto systemType = VoidLight::SystemType::BackgroundSim;
    const size_t learningWorkload = 1000;

    budgetMgr.prepareForStateTransition();

    auto initialDecision = budgetMgr.shouldUseThreading(systemType, learningWorkload);
    BOOST_CHECK(!initialDecision.shouldThread);
    BOOST_CHECK_EQUAL(budgetMgr.getLearnedThreshold(systemType), 0);
    BOOST_CHECK(!budgetMgr.isThresholdActive(systemType));

    for (int sample = 0; sample < 9; ++sample) {
        budgetMgr.reportExecution(systemType, learningWorkload, false, 1, 2.0);

        auto warmupDecision = budgetMgr.shouldUseThreading(systemType, learningWorkload);
        BOOST_CHECK(!warmupDecision.shouldThread);
        BOOST_CHECK_EQUAL(budgetMgr.getLearnedThreshold(systemType), 0);
        BOOST_CHECK(!budgetMgr.isThresholdActive(systemType));
    }

    budgetMgr.reportExecution(systemType, learningWorkload, false, 1, 2.0);

    BOOST_CHECK_EQUAL(budgetMgr.getLearnedThreshold(systemType), learningWorkload);
    BOOST_CHECK(budgetMgr.isThresholdActive(systemType));

    auto threadedDecision = budgetMgr.shouldUseThreading(systemType, learningWorkload);
    BOOST_CHECK(threadedDecision.shouldThread);

    const size_t hysteresisDropWorkload = 949;
    auto relearnDecision = budgetMgr.shouldUseThreading(systemType, hysteresisDropWorkload);
    BOOST_CHECK(!relearnDecision.shouldThread);
    BOOST_CHECK_EQUAL(budgetMgr.getLearnedThreshold(systemType), 0);
    BOOST_CHECK(!budgetMgr.isThresholdActive(systemType));

    std::cout << "Threshold learning, activation, and hysteresis re-learning verified\n";

    budgetMgr.prepareForStateTransition();
}

BOOST_AUTO_TEST_CASE(TestBatchTuningStability) {
    std::cout << "\n=== Testing Batch Tuning Stability (Simulation) ===\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = budgetMgr.getBudget();

    // Use Particle system to avoid interference with AI state from other tests
    const auto systemType = VoidLight::SystemType::Particle;
    const size_t workload = 14000;  // Simulate 14K entities like real game
    const size_t numFrames = 200;   // Simulate 200 frames (~3.3 seconds at 60fps)

    std::cout << "Workers: " << budget.totalWorkers << ", Workload: " << workload << "\n";
    std::cout << "Simulating " << numFrames << " frames...\n\n";

    // Track batch counts
    std::vector<size_t> batchCounts;
    batchCounts.reserve(numFrames);

    // Simulate realistic timing with noise
    // Base time scales with batch count (fewer batches = less overhead but less parallelism)
    auto simulateFrameTime = [&](size_t batches) -> double {
        // Base work time per item (microseconds)
        double baseTimePerItem = 0.15;  // ~0.15µs per entity

        // Parallel speedup (not perfect - diminishing returns)
        double parallelism = std::min(static_cast<double>(batches),
                                      static_cast<double>(budget.totalWorkers));
        double speedup = 1.0 + (parallelism - 1.0) * 0.85;  // 85% parallel efficiency

        // Work time
        double workTimeMs = (workload * baseTimePerItem / 1000.0) / speedup;

        // Overhead per batch (~10-20µs per batch)
        double overheadMs = batches * 0.015;

        // Add realistic noise (±20%)
        double noise = 0.8 + (static_cast<double>(rand() % 40) / 100.0);

        return (workTimeMs + overheadMs) * noise;
    };

    // Run simulation
    for (size_t frame = 0; frame < numFrames; ++frame) {
        size_t workers = budgetMgr.getOptimalWorkers(systemType, workload);
        auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(systemType, workload, workers);

        batchCounts.push_back(batchCount);

        // Simulate frame execution
        double frameTimeMs = simulateFrameTime(batchCount);

        // Report execution (unified API)
        budgetMgr.reportExecution(systemType, workload, true, batchCount, frameTimeMs);

        // Log every 20 frames
        if (frame < 10 || frame % 20 == 0) {
            std::cout << "Frame " << frame << ": " << batchCount << " batches, "
                      << std::fixed << std::setprecision(2) << frameTimeMs << "ms\n";
        }
    }

    // Analyze stability (last 100 frames after convergence)
    size_t analysisStart = numFrames > 100 ? numFrames - 100 : 0;
    size_t minBatch = batchCounts[analysisStart];
    size_t maxBatch = batchCounts[analysisStart];
    double sumBatch = 0;

    for (size_t i = analysisStart; i < numFrames; ++i) {
        minBatch = std::min(minBatch, batchCounts[i]);
        maxBatch = std::max(maxBatch, batchCounts[i]);
        sumBatch += batchCounts[i];
    }

    double avgBatch = sumBatch / (numFrames - analysisStart);
    size_t range = maxBatch - minBatch;

    std::cout << "\n=== Stability Analysis (last 100 frames) ===\n";
    std::cout << "Batch count: min=" << minBatch << ", max=" << maxBatch
              << ", avg=" << std::fixed << std::setprecision(1) << avgBatch << "\n";
    std::cout << "Range: " << range << " batches (";

    if (range <= 2) {
        std::cout << "EXCELLENT - very stable)\n";
    } else if (range <= 4) {
        std::cout << "GOOD - stable)\n";
    } else if (range <= 6) {
        std::cout << "OK - some variance)\n";
    } else {
        std::cout << "NEEDS TUNING - too much variance)\n";
    }

    // Check convergence - should stabilize within reasonable range
    BOOST_CHECK_LE(range, 6);  // Allow up to 6 batch variance
    BOOST_CHECK_GE(avgBatch, 5);  // Should find reasonable parallelism
}

BOOST_AUTO_TEST_SUITE_END()
