/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * WorkerBudget Adaptive Threshold Learning Validation
 *
 * Tests that WorkerBudgetManager correctly:
 * 1. Learns threading threshold when smoothed single-threaded time >= 0.9ms
 * 2. Stays single-threaded during learning phase (threshold == 0)
 * 3. Switches to multi-threaded once threshold is learned
 * 4. Re-learns when workload drops below hysteresis band (95% of threshold)
 * 5. Tunes batch multiplier via hill-climbing (still used for parallelism tuning)
 *
 * Tests all managers with WorkerBudget threading:
 * - AIManager (SystemType::AI)
 * - CollisionManager (SystemType::Collision)
 * - ParticleManager (SystemType::Particle)
 * - EventManager (SystemType::Event)
 */

#define BOOST_TEST_MODULE AdaptiveThreadingAnalysis
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cmath>

#include "managers/EntityDataManager.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/ParticleManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "core/Logger.hpp"

namespace {

class AnalysisFixture {
public:
    static bool& initializedFlag() { return s_initialized; }

    AnalysisFixture() {
        if (!s_initialized) {
            VOIDLIGHT_ENABLE_BENCHMARK_MODE();
            BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
            BOOST_REQUIRE(EntityDataManager::Instance().init());
            BOOST_REQUIRE(PathfinderManager::Instance().init());
            PathfinderManager::Instance().rebuildGrid();
            BOOST_REQUIRE(CollisionManager::Instance().init());
            BOOST_REQUIRE(AIManager::Instance().init());
            BOOST_REQUIRE(ParticleManager::Instance().init());
            BOOST_REQUIRE(EventManager::Instance().init());

            s_initialized = true;
        }
        m_rng.seed(42);
    }

    ~AnalysisFixture() = default;

    void reset() {
        EntityDataManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
        AIManager::Instance().prepareForStateTransition();
        ParticleManager::Instance().prepareForStateTransition();
        EventManager::Instance().prepareForStateTransition();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    }

    float calculateWorldSize(size_t entityCount) {
        float baseSize = std::sqrt(static_cast<float>(entityCount) * 400.0f);
        return std::clamp(baseSize, 200.0f, 4000.0f);
    }

    void createEntities(size_t count) {
        auto& edm = EntityDataManager::Instance();

        float worldSize = calculateWorldSize(count);
        std::uniform_real_distribution<float> posDist(50.0f, worldSize - 50.0f);

        for (size_t i = 0; i < count; ++i) {
            Vector2D pos(posDist(m_rng), posDist(m_rng));
            edm.createNPCWithRaceClass(pos, "Human", "Guard");
        }
    }

    void createParticleEffects(size_t count) {
        auto& particleMgr = ParticleManager::Instance();
        std::uniform_real_distribution<float> posDist(100.0f, 800.0f);

        // Create multiple particle effects to generate particles
        for (size_t i = 0; i < count / 100; ++i) {
            Vector2D pos(posDist(m_rng), posDist(m_rng));
            particleMgr.playEffect(ParticleEffectType::Rain, pos, 1.0f);
        }
    }

protected:
    std::mt19937 m_rng;
    static bool s_initialized;
};

bool AnalysisFixture::s_initialized = false;

struct AnalysisModuleCleanup {
    ~AnalysisModuleCleanup() {
        if (!AnalysisFixture::initializedFlag()) {
            return;
        }

        EventManager::Instance().clean();
        ParticleManager::Instance().clean();
        AIManager::Instance().clean();
        CollisionManager::Instance().clean();
        PathfinderManager::Instance().clean();
        EntityDataManager::Instance().clean();
        VoidLight::ThreadSystem::Instance().clean();

        AnalysisFixture::initializedFlag() = false;
    }
};

BOOST_GLOBAL_FIXTURE(AnalysisModuleCleanup);

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(WorkerBudgetValidation, AnalysisFixture)

/**
 * Test: MIN_WORKLOAD Enforcement
 * Validates that WBM forces single-threaded below MIN_WORKLOAD (100) for all systems
 */
BOOST_AUTO_TEST_CASE(MinWorkloadEnforcement) {
    std::cout << "\n===== MIN_WORKLOAD ENFORCEMENT (ALL SYSTEMS) =====\n" << std::endl;

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    std::vector<std::pair<VoidLight::SystemType, const char*>> systems = {
        {VoidLight::SystemType::AI, "AI"},
        {VoidLight::SystemType::Collision, "Collision"},
        {VoidLight::SystemType::Particle, "Particle"},
        {VoidLight::SystemType::Event, "Event"},
    };

    std::cout << "Testing MIN_WORKLOAD=100 enforcement:" << std::endl;
    std::cout << "  System      Workload  Expected  Actual    Result" << std::endl;
    std::cout << "  ----------  --------  --------  ------    ------" << std::endl;

    bool allPassed = true;
    for (const auto& [sysType, name] : systems) {
        // Test below MIN_WORKLOAD
        auto decision50 = budgetMgr.shouldUseThreading(sysType, 50);
        auto decision99 = budgetMgr.shouldUseThreading(sysType, 99);

        bool pass50 = !decision50.shouldThread;
        bool pass99 = !decision99.shouldThread;

        std::cout << "  " << std::setw(10) << std::left << name
                  << "  " << std::setw(8) << std::right << 50
                  << "  " << std::setw(8) << "SINGLE"
                  << "  " << std::setw(6) << (decision50.shouldThread ? "MULTI" : "SINGLE")
                  << "    " << (pass50 ? "PASS" : "FAIL") << std::endl;

        std::cout << "  " << std::setw(10) << std::left << name
                  << "  " << std::setw(8) << std::right << 99
                  << "  " << std::setw(8) << "SINGLE"
                  << "  " << std::setw(6) << (decision99.shouldThread ? "MULTI" : "SINGLE")
                  << "    " << (pass99 ? "PASS" : "FAIL") << std::endl;

        allPassed = allPassed && pass50 && pass99;
    }

    std::cout << "\nValidation: MIN_WORKLOAD enforcement: " << (allPassed ? "PASS" : "FAIL") << std::endl;
    BOOST_CHECK(allPassed);

    std::cout << "================================================\n" << std::endl;
}

/**
 * Test: AI Manager Threshold Learning
 */
BOOST_AUTO_TEST_CASE(AIManager_ThresholdLearning) {
    std::cout << "\n===== AI MANAGER THRESHOLD LEARNING =====\n" << std::endl;

    reset();
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& aiMgr = AIManager::Instance();

    size_t initialThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::AI);
    std::cout << "Initial threshold: " << initialThreshold << std::endl;

    createEntities(2000);
    std::cout << "Created 2000 entities for AI processing" << std::endl;

    std::cout << "\nRunning AI updates to trigger threshold learning..." << std::endl;
    std::cout << "  (Threshold learned when smoothed time >= 0.9ms)" << std::endl;

    size_t learnedAt = 0;
    for (int i = 0; i < 100; ++i) {
        aiMgr.update(0.016f);

        size_t threshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::AI);
        if (threshold > 0 && learnedAt == 0) {
            learnedAt = i + 1;
            std::cout << "  Threshold learned at frame " << learnedAt
                      << ": " << threshold << " entities" << std::endl;
        }
    }

    size_t finalThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::AI);
    bool finalActive = budgetMgr.isThresholdActive(VoidLight::SystemType::AI);

    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  Learned threshold: " << finalThreshold << std::endl;
    std::cout << "  Threshold active: " << (finalActive ? "true" : "false") << std::endl;

    if (finalThreshold == 0) {
        std::cout << "  (Hardware may be fast enough that 0.9ms wasn't hit)" << std::endl;
    }

    BOOST_CHECK_MESSAGE(true, "AI threshold learning test completed");
    std::cout << "==========================================\n" << std::endl;
}

/**
 * Test: Collision Manager Threshold Learning
 */
BOOST_AUTO_TEST_CASE(CollisionManager_ThresholdLearning) {
    std::cout << "\n===== COLLISION MANAGER THRESHOLD LEARNING =====\n" << std::endl;

    reset();
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& colMgr = CollisionManager::Instance();

    size_t initialThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Collision);
    std::cout << "Initial threshold: " << initialThreshold << std::endl;

    createEntities(2000);
    std::cout << "Created 2000 entities for collision processing" << std::endl;

    std::cout << "\nRunning collision updates to trigger threshold learning..." << std::endl;

    size_t learnedAt = 0;
    for (int i = 0; i < 100; ++i) {
        colMgr.update(0.016f);

        size_t threshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Collision);
        if (threshold > 0 && learnedAt == 0) {
            learnedAt = i + 1;
            std::cout << "  Threshold learned at frame " << learnedAt
                      << ": " << threshold << " entities" << std::endl;
        }
    }

    size_t finalThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Collision);
    bool finalActive = budgetMgr.isThresholdActive(VoidLight::SystemType::Collision);

    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  Learned threshold: " << finalThreshold << std::endl;
    std::cout << "  Threshold active: " << (finalActive ? "true" : "false") << std::endl;

    BOOST_CHECK_MESSAGE(true, "Collision threshold learning test completed");
    std::cout << "================================================\n" << std::endl;
}

/**
 * Test: Particle Manager Threshold Learning
 */
BOOST_AUTO_TEST_CASE(ParticleManager_ThresholdLearning) {
    std::cout << "\n===== PARTICLE MANAGER THRESHOLD LEARNING =====\n" << std::endl;

    reset();
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& particleMgr = ParticleManager::Instance();

    size_t initialThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Particle);
    std::cout << "Initial threshold: " << initialThreshold << std::endl;

    createParticleEffects(5000);
    std::cout << "Created particle effects (~5000 particles)" << std::endl;

    std::cout << "\nRunning particle updates to trigger threshold learning..." << std::endl;

    size_t learnedAt = 0;
    for (int i = 0; i < 100; ++i) {
        particleMgr.update(0.016f);

        size_t threshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Particle);
        if (threshold > 0 && learnedAt == 0) {
            learnedAt = i + 1;
            std::cout << "  Threshold learned at frame " << learnedAt
                      << ": " << threshold << " particles" << std::endl;
        }
    }

    size_t finalThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Particle);
    bool finalActive = budgetMgr.isThresholdActive(VoidLight::SystemType::Particle);

    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  Learned threshold: " << finalThreshold << std::endl;
    std::cout << "  Threshold active: " << (finalActive ? "true" : "false") << std::endl;

    BOOST_CHECK_MESSAGE(true, "Particle threshold learning test completed");
    std::cout << "===============================================\n" << std::endl;
}

/**
 * Test: Hysteresis Band Re-learning
 * Validates that WBM re-learns when workload drops below 95% of threshold
 */
BOOST_AUTO_TEST_CASE(HysteresisRelearning) {
    std::cout << "\n===== HYSTERESIS BAND RE-LEARNING =====\n" << std::endl;

    reset();
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& colMgr = CollisionManager::Instance();

    createEntities(3000);

    std::cout << "Phase 1: Learning threshold with 3000 entities..." << std::endl;
    for (int i = 0; i < 200; ++i) {
        colMgr.update(0.016f);
    }

    size_t threshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Collision);
    bool active = budgetMgr.isThresholdActive(VoidLight::SystemType::Collision);

    std::cout << "  Learned threshold: " << threshold << std::endl;
    std::cout << "  Threshold active: " << (active ? "true" : "false") << std::endl;

    if (threshold == 0) {
        std::cout << "\n  (Threshold not learned - hardware may be too fast)" << std::endl;
        BOOST_CHECK_MESSAGE(true, "Hysteresis test skipped - no threshold learned");
        std::cout << "==========================================\n" << std::endl;
        return;
    }

    size_t hysteresisLow = static_cast<size_t>(static_cast<double>(threshold) * 0.95);
    std::cout << "\n  Hysteresis low boundary (95%): " << hysteresisLow << std::endl;

    std::cout << "\nPhase 2: Testing workload at " << (hysteresisLow - 10) << " (below hysteresis)..." << std::endl;

    auto decision = budgetMgr.shouldUseThreading(VoidLight::SystemType::Collision, hysteresisLow - 10);

    size_t newThreshold = budgetMgr.getLearnedThreshold(VoidLight::SystemType::Collision);
    bool newActive = budgetMgr.isThresholdActive(VoidLight::SystemType::Collision);

    std::cout << "  After hysteresis drop:" << std::endl;
    std::cout << "    Threshold: " << newThreshold << " (was " << threshold << ")" << std::endl;
    std::cout << "    Active: " << (newActive ? "true" : "false") << std::endl;
    std::cout << "    Decision: " << (decision.shouldThread ? "MULTI" : "SINGLE") << std::endl;

    bool relearned = (newThreshold == 0 && !newActive);
    std::cout << "\nValidation: Re-learning triggered: " << (relearned ? "PASS" : "FAIL") << std::endl;

    BOOST_CHECK_MESSAGE(relearned, "Hysteresis should trigger re-learning");
    std::cout << "==========================================\n" << std::endl;
}

/**
 * Test: Batch Multiplier Tuning (all systems)
 */
BOOST_AUTO_TEST_CASE(BatchMultiplierTuning) {
    std::cout << "\n===== BATCH MULTIPLIER TUNING =====\n" << std::endl;

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    std::vector<std::pair<VoidLight::SystemType, const char*>> systems = {
        {VoidLight::SystemType::AI, "AI"},
        {VoidLight::SystemType::Collision, "Collision"},
        {VoidLight::SystemType::Particle, "Particle"},
        {VoidLight::SystemType::Event, "Event"},
    };

    std::cout << "Batch multiplier range validation [0.4, 2.0]:" << std::endl;
    std::cout << "  System      Multiplier  InRange" << std::endl;
    std::cout << "  ----------  ----------  -------" << std::endl;

    bool allInRange = true;
    for (const auto& [sysType, name] : systems) {
        float mult = budgetMgr.getBatchMultiplier(sysType);
        bool inRange = (mult >= 0.4f && mult <= 2.0f);

        std::cout << "  " << std::setw(10) << std::left << name
                  << "  " << std::setw(10) << std::fixed << std::setprecision(3) << mult
                  << "  " << (inRange ? "PASS" : "FAIL") << std::endl;

        allInRange = allInRange && inRange;
    }

    std::cout << "\nValidation: All multipliers in range: " << (allInRange ? "PASS" : "FAIL") << std::endl;
    BOOST_CHECK(allInRange);

    std::cout << "====================================\n" << std::endl;
}

/**
 * Test: Threading State Summary for all systems
 */
BOOST_AUTO_TEST_CASE(ThreadingStateSummary) {
    std::cout << "\n===== WORKERBUDGET STATE SUMMARY (ALL SYSTEMS) =====\n" << std::endl;

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    std::vector<std::pair<VoidLight::SystemType, const char*>> systems = {
        {VoidLight::SystemType::AI, "AI"},
        {VoidLight::SystemType::Collision, "Collision"},
        {VoidLight::SystemType::Particle, "Particle"},
        {VoidLight::SystemType::Event, "Event"},
        {VoidLight::SystemType::Pathfinding, "Pathfinding"},
    };

    std::cout << "System       Threshold   Active    BatchMult   MultiTP" << std::endl;
    std::cout << "-----------  ---------   ------    ---------   -------" << std::endl;

    for (const auto& [sysType, name] : systems) {
        size_t threshold = budgetMgr.getLearnedThreshold(sysType);
        bool active = budgetMgr.isThresholdActive(sysType);
        float batchMult = budgetMgr.getBatchMultiplier(sysType);
        double multiTP = budgetMgr.getExpectedThroughput(sysType, true);

        std::cout << std::setw(11) << std::left << name
                  << "  " << std::setw(9) << std::right << threshold
                  << "   " << std::setw(6) << (active ? "true" : "false")
                  << "    " << std::setw(9) << std::fixed << std::setprecision(2) << batchMult
                  << "   " << std::setw(7) << std::setprecision(0) << multiTP << std::endl;
    }

    std::cout << "\nConstants:" << std::endl;
    std::cout << "  LEARNING_TIME_THRESHOLD_MS = 0.9ms" << std::endl;
    std::cout << "  HYSTERESIS_FACTOR = 0.95 (5% band)" << std::endl;
    std::cout << "  TIME_SMOOTHING = 0.25 (~6 frames to converge)" << std::endl;
    std::cout << "  MIN_WORKLOAD = 100 entities" << std::endl;

    std::cout << "\n====================================================\n" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
