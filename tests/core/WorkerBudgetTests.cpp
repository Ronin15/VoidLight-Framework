/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

/**
 * @file WorkerBudgetTests.cpp
 * @brief Targeted unit tests for WorkerBudgetManager — coverage complementary
 *        to the broader BufferUtilizationTest.cpp (buffer_utilization_tests).
 *
 * Focus areas not covered by the existing suite:
 * - prepareForStateTransition() resets ALL system types, not just one
 * - getBatchMultiplier() initial value after reset
 * - getExpectedThroughput() before and after multi-threaded reporting
 * - markFrameStart() per-frame cache invalidation hook
 * - Coverage of Collision, BackgroundSim, and ProjectileSim SystemType variants
 * - Batch strategy correctness (batchCount * batchSize >= workload) for all types
 * - Independence of per-system tuning state (one system's threshold does not
 *   bleed into another's)
 *
 * Each test calls prepareForStateTransition() at entry to obtain a clean
 * singleton state and at exit to avoid polluting subsequent tests.
 */

#define BOOST_TEST_MODULE WorkerBudgetTests
#include <boost/test/unit_test.hpp>

#include "core/WorkerBudget.hpp"
#include "core/ThreadSystem.hpp"

#include <array>

// ============================================================================
// Global fixture — ThreadSystem must be alive for WorkerBudgetManager
// ============================================================================

struct GlobalThreadSystemFixture
{
    GlobalThreadSystemFixture()
    {
        if (!VoidLight::ThreadSystem::Instance().init())
        {
            throw std::runtime_error("ThreadSystem::init() failed in WorkerBudgetTests");
        }
    }

    ~GlobalThreadSystemFixture()
    {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(GlobalThreadSystemFixture);

// ============================================================================
// State-Transition / Reset Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(WorkerBudgetStateTransitionTests)

BOOST_AUTO_TEST_CASE(PrepareForStateTransitionResetsLearnedThreshold)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    // Teach a threshold: 10+ single-threaded samples > LEARNING_TIME_THRESHOLD_MS (0.9ms)
    const auto system = VoidLight::SystemType::Collision;
    for (int i = 0; i < 11; ++i)
    {
        mgr.reportExecution(system, 2000, false, 1, 2.0);
    }
    BOOST_REQUIRE_GT(mgr.getLearnedThreshold(system), 0u);
    BOOST_REQUIRE(mgr.isThresholdActive(system));

    mgr.prepareForStateTransition();

    BOOST_CHECK_EQUAL(mgr.getLearnedThreshold(system), 0u);
    BOOST_CHECK(!mgr.isThresholdActive(system));
}

BOOST_AUTO_TEST_CASE(PrepareForStateTransitionResetsAllSevenSystemTypes)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    constexpr std::array<VoidLight::SystemType, 7> allSystems{
        VoidLight::SystemType::AI,
        VoidLight::SystemType::Particle,
        VoidLight::SystemType::Pathfinding,
        VoidLight::SystemType::Event,
        VoidLight::SystemType::Collision,
        VoidLight::SystemType::BackgroundSim,
        VoidLight::SystemType::ProjectileSim,
    };

    // Teach a threshold to every system
    for (auto sys : allSystems)
    {
        for (int i = 0; i < 11; ++i)
        {
            mgr.reportExecution(sys, 1500, false, 1, 2.0);
        }
        BOOST_REQUIRE_GT(mgr.getLearnedThreshold(sys), 0u);
    }

    mgr.prepareForStateTransition();

    for (auto sys : allSystems)
    {
        BOOST_CHECK_EQUAL(mgr.getLearnedThreshold(sys), 0u);
        BOOST_CHECK(!mgr.isThresholdActive(sys));
    }

    mgr.prepareForStateTransition();  // clean exit
}

BOOST_AUTO_TEST_CASE(PrepareForStateTransitionResetsBatchMultiplier)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    // After reset the multiplier must be at its initial value (1.0)
    BOOST_CHECK_CLOSE(mgr.getBatchMultiplier(VoidLight::SystemType::AI), 1.0f, 0.01f);
    BOOST_CHECK_CLOSE(mgr.getBatchMultiplier(VoidLight::SystemType::Collision), 1.0f, 0.01f);
    BOOST_CHECK_CLOSE(mgr.getBatchMultiplier(VoidLight::SystemType::ProjectileSim), 1.0f, 0.01f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Accessor Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(WorkerBudgetAccessorTests)

BOOST_AUTO_TEST_CASE(GetBatchMultiplierInitialValueIsOne)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    for (auto sys : {
             VoidLight::SystemType::AI,
             VoidLight::SystemType::Particle,
             VoidLight::SystemType::Pathfinding,
             VoidLight::SystemType::Event,
             VoidLight::SystemType::Collision,
             VoidLight::SystemType::BackgroundSim,
             VoidLight::SystemType::ProjectileSim,
         })
    {
        BOOST_CHECK_CLOSE(mgr.getBatchMultiplier(sys), 1.0f, 0.01f);
    }
}

BOOST_AUTO_TEST_CASE(GetExpectedThroughputZeroBeforeMultithreadedReport)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    // No multi-threaded reports yet → multi-threaded expected throughput must be 0
    BOOST_CHECK_CLOSE(mgr.getExpectedThroughput(VoidLight::SystemType::BackgroundSim, true),
                      0.0, 0.001);
    BOOST_CHECK_CLOSE(mgr.getExpectedThroughput(VoidLight::SystemType::ProjectileSim, true),
                      0.0, 0.001);
}

BOOST_AUTO_TEST_CASE(GetExpectedThroughputUpdatesAfterMultithreadedReport)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    // Report two multi-threaded runs: 1000 items / 2.0 ms → ~500 items/ms
    mgr.reportExecution(VoidLight::SystemType::Pathfinding, 1000, true, 4, 2.0);
    mgr.reportExecution(VoidLight::SystemType::Pathfinding, 1000, true, 4, 2.0);

    const double throughput = mgr.getExpectedThroughput(VoidLight::SystemType::Pathfinding, true);
    BOOST_CHECK_GT(throughput, 0.0);

    mgr.prepareForStateTransition();
}

BOOST_AUTO_TEST_CASE(GetLearnedThresholdZeroAfterReset)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    BOOST_CHECK_EQUAL(mgr.getLearnedThreshold(VoidLight::SystemType::AI), 0u);
    BOOST_CHECK_EQUAL(mgr.getLearnedThreshold(VoidLight::SystemType::Collision), 0u);
    BOOST_CHECK_EQUAL(mgr.getLearnedThreshold(VoidLight::SystemType::ProjectileSim), 0u);
}

BOOST_AUTO_TEST_CASE(IsThresholdActiveFalseAfterReset)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    for (auto sys : {
             VoidLight::SystemType::AI,
             VoidLight::SystemType::Event,
             VoidLight::SystemType::BackgroundSim,
             VoidLight::SystemType::Collision,
         })
    {
        BOOST_CHECK(!mgr.isThresholdActive(sys));
    }
}

BOOST_AUTO_TEST_CASE(MarkFrameStartDoesNotCrash)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();

    // markFrameStart() increments the internal frame counter used to gate
    // per-frame caching of queue pressure reads.
    BOOST_CHECK_NO_THROW(mgr.markFrameStart());
    BOOST_CHECK_NO_THROW(mgr.markFrameStart());
    BOOST_CHECK_NO_THROW(mgr.markFrameStart());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SystemType Coverage Tests — all seven SystemType variants exercised
// ============================================================================

BOOST_AUTO_TEST_SUITE(WorkerBudgetSystemCoverageTests)

BOOST_AUTO_TEST_CASE(CollisionSystemTypeGetsBudget)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = mgr.getBudget();

    const size_t workers = mgr.getOptimalWorkers(VoidLight::SystemType::Collision, 500);
    BOOST_CHECK_EQUAL(workers, budget.totalWorkers);
}

BOOST_AUTO_TEST_CASE(BackgroundSimSystemTypeGetsBudget)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = mgr.getBudget();

    const size_t workers = mgr.getOptimalWorkers(VoidLight::SystemType::BackgroundSim, 1000);
    BOOST_CHECK_EQUAL(workers, budget.totalWorkers);
}

BOOST_AUTO_TEST_CASE(ProjectileSimSystemTypeGetsBudget)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = mgr.getBudget();

    const size_t workers = mgr.getOptimalWorkers(VoidLight::SystemType::ProjectileSim, 200);
    BOOST_CHECK_EQUAL(workers, budget.totalWorkers);
}

BOOST_AUTO_TEST_CASE(BatchStrategyCoversEntireWorkloadForAllSystemTypes)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();

    constexpr std::array<VoidLight::SystemType, 7> allSystems{
        VoidLight::SystemType::AI,
        VoidLight::SystemType::Particle,
        VoidLight::SystemType::Pathfinding,
        VoidLight::SystemType::Event,
        VoidLight::SystemType::Collision,
        VoidLight::SystemType::BackgroundSim,
        VoidLight::SystemType::ProjectileSim,
    };

    constexpr size_t workload = 1200;

    for (auto sys : allSystems)
    {
        const size_t workers = mgr.getOptimalWorkers(sys, workload);
        const auto [batchCount, batchSize] = mgr.getBatchStrategy(sys, workload, workers);

        // The batch plan must cover all items without leaving any unprocessed.
        BOOST_CHECK_GE(batchCount * batchSize, workload);
        BOOST_CHECK_GE(batchCount, 1u);
        BOOST_CHECK_GE(batchSize, 1u);
    }
}

BOOST_AUTO_TEST_CASE(SmallWorkloadBelowMinimumGetsBudget)
{
    // A workload of 1 is well below MIN_WORKLOAD (100).
    // The sequential execution model must still return all workers.
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    const auto& budget = mgr.getBudget();

    const size_t workers = mgr.getOptimalWorkers(VoidLight::SystemType::AI, 1);
    BOOST_CHECK_EQUAL(workers, budget.totalWorkers);
}

BOOST_AUTO_TEST_CASE(MultipleSystemsHaveIndependentThresholds)
{
    // Teaching a threshold to one system must not affect sibling systems.
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    const auto targetSystem = VoidLight::SystemType::Collision;
    for (int i = 0; i < 11; ++i)
    {
        mgr.reportExecution(targetSystem, 1500, false, 1, 2.0);
    }

    BOOST_REQUIRE_GT(mgr.getLearnedThreshold(targetSystem), 0u);

    // All other systems must remain at zero threshold
    for (auto sys : {
             VoidLight::SystemType::AI,
             VoidLight::SystemType::Particle,
             VoidLight::SystemType::Pathfinding,
             VoidLight::SystemType::Event,
             VoidLight::SystemType::BackgroundSim,
             VoidLight::SystemType::ProjectileSim,
         })
    {
        BOOST_CHECK_EQUAL(mgr.getLearnedThreshold(sys), 0u);
    }

    mgr.prepareForStateTransition();
}

BOOST_AUTO_TEST_CASE(ShouldUseThreadingReturnsFalseBeforeLearning)
{
    // Without sufficient single-threaded samples, the decision must be
    // single-threaded to avoid premature threading overhead.
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    const auto decision = mgr.shouldUseThreading(VoidLight::SystemType::ProjectileSim, 1000);
    BOOST_CHECK(!decision.shouldThread);

    mgr.prepareForStateTransition();
}

BOOST_AUTO_TEST_CASE(ShouldUseThreadingReturnsTrueAfterThresholdLearned)
{
    auto& mgr = VoidLight::WorkerBudgetManager::Instance();
    mgr.prepareForStateTransition();

    const auto system = VoidLight::SystemType::BackgroundSim;
    constexpr size_t workload = 1000;

    // Teach the threshold (10 samples of 2ms single-threaded → crosses 0.9ms)
    for (int i = 0; i < 10; ++i)
    {
        mgr.reportExecution(system, workload, false, 1, 2.0);
    }

    BOOST_REQUIRE_GT(mgr.getLearnedThreshold(system), 0u);

    const auto decision = mgr.shouldUseThreading(system, workload);
    BOOST_CHECK(decision.shouldThread);

    mgr.prepareForStateTransition();
}

BOOST_AUTO_TEST_SUITE_END()
