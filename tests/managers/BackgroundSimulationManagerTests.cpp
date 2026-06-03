/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE BackgroundSimulationManagerTests
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "entities/Entity.hpp"  // For AnimationConfig
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include <cmath>
#include <stdexcept>

// Test tolerance for floating-point comparisons
constexpr float EPSILON = 0.001f;

// Helper to check if two floats are approximately equal
bool approxEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// Test Fixture
// ============================================================================

struct BackgroundSimThreadFixture {
    BackgroundSimThreadFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }

    ~BackgroundSimThreadFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(BackgroundSimThreadFixture);

class BackgroundSimManagerTestFixture {
public:
    BackgroundSimManagerTestFixture() {
        BOOST_REQUIRE(VoidLight::ThreadSystem::Exists());
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();

        // Initialize EntityDataManager first (dependency)
        edm = &EntityDataManager::Instance();
        BOOST_REQUIRE(edm->init());

        // Then initialize BackgroundSimulationManager
        bgsm = &BackgroundSimulationManager::Instance();
        BOOST_REQUIRE(bgsm->init());
    }

    ~BackgroundSimManagerTestFixture() {
        bgsm->clean();
        edm->clean();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    }

protected:
    void primeBackgroundThreadingDecision(size_t workloadSize) {
        auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
        budgetMgr.prepareForStateTransition();
        for (int sample = 0; sample < 10; ++sample) {
            budgetMgr.reportExecution(
                VoidLight::SystemType::BackgroundSim, workloadSize, false, 1, 5.0);
        }
        BOOST_REQUIRE(
            budgetMgr.shouldUseThreading(
                VoidLight::SystemType::BackgroundSim, workloadSize).shouldThread);
    }

    EntityDataManager* edm;
    BackgroundSimulationManager* bgsm;
};

// ============================================================================
// SINGLETON PATTERN TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SingletonTests)

BOOST_AUTO_TEST_CASE(TestSingletonPattern) {
    BackgroundSimulationManager* instance1 = &BackgroundSimulationManager::Instance();
    BackgroundSimulationManager* instance2 = &BackgroundSimulationManager::Instance();

    BOOST_CHECK(instance1 == instance2);
    BOOST_CHECK(instance1 != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(LifecycleTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestInitialization) {
    BOOST_CHECK(bgsm->isInitialized());
    BOOST_CHECK(!bgsm->isShutdown());
}

BOOST_AUTO_TEST_CASE(TestDoubleInitialization) {
    // Second init should return true (already initialized)
    bool result = bgsm->init();
    BOOST_CHECK(result);
    BOOST_CHECK(bgsm->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestCleanAndReinit) {
    bgsm->clean();
    BOOST_CHECK(!bgsm->isInitialized());

    bool result = bgsm->init();
    BOOST_CHECK(result);
    BOOST_CHECK(bgsm->isInitialized());
}

BOOST_AUTO_TEST_CASE(TestPrepareForStateTransition) {
    // Set some state
    bgsm->setReferencePoint(Vector2D(1000.0f, 1000.0f));
    bgsm->invalidateTiers();

    // Prepare for transition
    bgsm->prepareForStateTransition();

    // Manager should still be initialized
    BOOST_CHECK(bgsm->isInitialized());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PAUSE/RESUME TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PauseResumeTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestInitiallyNotPaused) {
    BOOST_CHECK(!bgsm->isGloballyPaused());
}

BOOST_AUTO_TEST_CASE(TestSetGlobalPause) {
    bgsm->setGlobalPause(true);
    BOOST_CHECK(bgsm->isGloballyPaused());

    bgsm->setGlobalPause(false);
    BOOST_CHECK(!bgsm->isGloballyPaused());
}

BOOST_AUTO_TEST_CASE(TestNoUpdateWhenPaused) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create a background entity
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");

    // Update tiers to put it in background
    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->updateTiers();

    // Reset stats and capture baseline BEFORE pausing
    bgsm->resetPerfStats();
    uint64_t initialUpdates = bgsm->getPerfStats().totalUpdates;

    // Pause
    bgsm->setGlobalPause(true);

    // Call update multiple times - should do nothing while paused
    for (int i = 0; i < 10; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.2f);  // 200ms each, total 2000ms
    }

    // Updates should not have incremented while paused
    BOOST_CHECK_EQUAL(bgsm->getPerfStats().totalUpdates, initialUpdates);

    // Clean up
    edm->destroyEntity(handle);
    edm->processDestructionQueue();
    bgsm->setGlobalPause(false);
}

BOOST_AUTO_TEST_CASE(TestResumeAfterPause) {
    bgsm->setActiveRadius(500.0f);
    bgsm->setBackgroundRadius(1000.0f);
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");
    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);
    bgsm->resetPerfStats();

    bgsm->setGlobalPause(true);
    BOOST_CHECK(bgsm->isGloballyPaused());

    bgsm->setGlobalPause(false);
    BOOST_CHECK(!bgsm->isGloballyPaused());

    bgsm->update(Vector2D(0.0f, 0.0f), 0.2f);
    BOOST_CHECK_GT(bgsm->getPerfStats().totalUpdates, 0U);

    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// REFERENCE POINT TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ReferencePointTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestSetReferencePoint) {
    Vector2D pos(500.0f, 600.0f);
    bgsm->setReferencePoint(pos);

    Vector2D retrieved = bgsm->getReferencePoint();
    BOOST_CHECK(approxEqual(retrieved.getX(), pos.getX()));
    BOOST_CHECK(approxEqual(retrieved.getY(), pos.getY()));
}

BOOST_AUTO_TEST_CASE(TestReferencePointViaUpdate) {
    Vector2D pos(1000.0f, 2000.0f);

    // Force tier update by invalidating
    bgsm->invalidateTiers();
    bgsm->update(pos, 0.016f);

    Vector2D retrieved = bgsm->getReferencePoint();
    BOOST_CHECK(approxEqual(retrieved.getX(), pos.getX()));
    BOOST_CHECK(approxEqual(retrieved.getY(), pos.getY()));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TIER MANAGEMENT TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TierManagementTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestUpdateTiers) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create entities at different distances
    EntityHandle near = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");   // Active (<500)
    EntityHandle far = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");      // Background (500-1000)

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->updateTiers();

    // Check tiers were assigned
    const auto& nearHot = edm->getHotData(near);
    const auto& farHot = edm->getHotData(far);

    BOOST_CHECK_EQUAL(static_cast<int>(nearHot.tier), static_cast<int>(SimulationTier::Active));
    BOOST_CHECK_EQUAL(static_cast<int>(farHot.tier), static_cast<int>(SimulationTier::Background));

    // Clean up
    edm->destroyEntity(near);
    edm->destroyEntity(far);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestInvalidateTiers) {
    bgsm->invalidateTiers();
    BOOST_CHECK(bgsm->hasWork());
}

BOOST_AUTO_TEST_CASE(TestHasWorkWithNoEntities) {
    // With no entities and fresh state, should still have work (tier check)
    bgsm->invalidateTiers();
    BOOST_CHECK(bgsm->hasWork());

    // After update with no entities, there should be no remaining background work.
    bgsm->update(Vector2D(0.0f, 0.0f), 0.016f);
    BOOST_CHECK(!bgsm->hasWork());
}

BOOST_AUTO_TEST_CASE(TestHasWorkWithBackgroundEntities) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create entity in background range (500-1000)
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();  // Force tier recalc
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);

    BOOST_CHECK(edm->isValidHandle(handle));
    BOOST_CHECK(bgsm->hasWork());

    // Clean up
    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ConfigurationTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestSetActiveRadius) {
    bgsm->setActiveRadius(2000.0f);
    BOOST_CHECK(approxEqual(bgsm->getActiveRadius(), 2000.0f));
}

BOOST_AUTO_TEST_CASE(TestSetBackgroundRadius) {
    bgsm->setBackgroundRadius(8000.0f);
    BOOST_CHECK(approxEqual(bgsm->getBackgroundRadius(), 8000.0f));
}

BOOST_AUTO_TEST_CASE(TestSetUpdateRate) {
    bgsm->setUpdateRate(30.0f);
    BOOST_CHECK(approxEqual(bgsm->getUpdateRate(), 30.0f));

    bgsm->setUpdateRate(10.0f);
    BOOST_CHECK(approxEqual(bgsm->getUpdateRate(), 10.0f));
}

BOOST_AUTO_TEST_CASE(TestConfigureForScreenSize) {
    // Test with 1920x1080
    bgsm->configureForScreenSize(1920, 1080);

    // Half-diagonal = sqrt((960)^2 + (540)^2) = sqrt(921600 + 291600) = sqrt(1213200) ≈ 1101
    // Active should be ~1.5x = ~1652
    // Background should be ~2x = ~2202
    float activeRadius = bgsm->getActiveRadius();
    float bgRadius = bgsm->getBackgroundRadius();

    BOOST_CHECK(activeRadius > 1500.0f && activeRadius < 1800.0f);
    BOOST_CHECK(bgRadius > 2000.0f && bgRadius < 2400.0f);
    BOOST_CHECK(bgRadius > activeRadius);
}

BOOST_AUTO_TEST_CASE(TestConfigureForDifferentScreenSizes) {
    // Smaller screen
    bgsm->configureForScreenSize(1280, 720);
    float smallActive = bgsm->getActiveRadius();
    float smallBg = bgsm->getBackgroundRadius();

    // Larger screen
    bgsm->configureForScreenSize(2560, 1440);
    float largeActive = bgsm->getActiveRadius();
    float largeBg = bgsm->getBackgroundRadius();

    // Larger screen should have larger radii
    BOOST_CHECK(largeActive > smallActive);
    BOOST_CHECK(largeBg > smallBg);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// UPDATE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(UpdateTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestBasicUpdate) {
    bgsm->invalidateTiers();
    bgsm->update(Vector2D(0.0f, 0.0f), 0.016f);
    BOOST_CHECK(!bgsm->hasWork());
}

BOOST_AUTO_TEST_CASE(TestUpdateWithBackgroundEntities) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create entities in background tier (500-1000)
    std::vector<EntityHandle> handles;
    for (int i = 0; i < 10; ++i) {
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(600.0f + i * 30.0f, 0.0f), "Human", "Guard"));
    }

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);
    bgsm->resetPerfStats();

    // Verify entities are in background tier
    BOOST_CHECK(!edm->getBackgroundIndices().empty());

    for (int i = 0; i < 5; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.1f);  // 100ms per update
    }

    BOOST_CHECK_GT(bgsm->getPerfStats().totalUpdates, 0U);
    for (const auto& handle : handles) {
        BOOST_CHECK(edm->isValidHandle(handle));
    }

    // Clean up
    for (const auto& handle : handles) {
        edm->destroyEntity(handle);
    }
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestTierUpdateInterval) {
    // Create entity
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");

    // Force tier to be dirty
    bgsm->invalidateTiers();

    // First update should process tier update
    bgsm->update(Vector2D(0.0f, 0.0f), 0.016f);

    // Entity is within the default active radius, so tier recalculation should
    // clear dirty state without leaving background work pending.
    BOOST_CHECK(!bgsm->hasWork());

    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestAccumulatorPattern) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000, Hibernated: >1000

    // Create entity in background tier range (between 500 and 1000)
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();  // Force tier update within update()
    bgsm->setUpdateRate(10.0f);  // 10Hz = 100ms interval

    // First update to trigger tier recalc and set hasNonActiveEntities flag
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);
    bgsm->resetPerfStats();  // Reset after tier update

    // Small updates shouldn't trigger processing
    for (int i = 0; i < 5; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.01f);  // 10ms each = 50ms total
    }

    // Larger update should trigger processing (150ms total = triggers at least once)
    bgsm->update(Vector2D(0.0f, 0.0f), 0.15f);

    const auto& stats = bgsm->getPerfStats();
    BOOST_CHECK(stats.totalUpdates > 0);

    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestWaitForAsyncCompletion) {
    const auto before = bgsm->getPerfStats().totalUpdates;
    bgsm->waitForAsyncCompletion();
    BOOST_CHECK_EQUAL(bgsm->getPerfStats().totalUpdates, before);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PERF STATS TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PerfStatsTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestGetPerfStats) {
    const auto& stats = bgsm->getPerfStats();
    // Initial stats should be zeroed
    BOOST_CHECK(stats.lastUpdateMs >= 0.0);
    BOOST_CHECK(stats.avgUpdateMs >= 0.0);
}

BOOST_AUTO_TEST_CASE(TestResetPerfStats) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create entity in background tier and process
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");
    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->updateTiers();

    // Accumulate updates (100ms per update to trigger processing at 10Hz)
    for (int i = 0; i < 20; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.1f);
    }

    // Reset stats
    bgsm->resetPerfStats();

    const auto& stats = bgsm->getPerfStats();
    BOOST_CHECK_EQUAL(stats.totalUpdates, 0);
    BOOST_CHECK(approxEqual(static_cast<float>(stats.lastUpdateMs), 0.0f));

    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestPerfStatsUpdateAfterProcessing) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create multiple background entities in background tier range (500-1000)
    std::vector<EntityHandle> handles;
    for (int i = 0; i < 50; ++i) {
        // Place in a ring at distance ~750 from origin (background tier)
        float angle = (i / 50.0f) * 6.28f;
        float dist = 750.0f;
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(dist * std::cos(angle), dist * std::sin(angle)), "Human", "Guard"));
    }

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();  // Force tier update within update()

    // First update to trigger tier recalc and set hasNonActiveEntities flag
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);
    bgsm->resetPerfStats();  // Reset after tier update

    // Process enough to trigger update (200ms at 10Hz = 2 updates)
    bgsm->update(Vector2D(0.0f, 0.0f), 0.2f);

    const auto& stats = bgsm->getPerfStats();
    // Stats should be updated - check any activity
    BOOST_CHECK(stats.totalUpdates > 0);

    // Clean up
    for (const auto& handle : handles) {
        edm->destroyEntity(handle);
    }
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// NPC SIMULATION TESTS (Background tier processing)
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(NPCSimulationTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestBackgroundNPCVelocityDecay) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create NPC in background tier range (distance 750)
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");

    // Set initial velocity
    auto& transform = edm->getTransform(handle);
    transform.velocity = Vector2D(100.0f, 100.0f);

    // Update tiers to put in background
    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();

    // First update to trigger tier recalc and set hasNonActiveEntities flag
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);

    // Verify in background tier
    const auto& hot = edm->getHotData(handle);
    BOOST_CHECK_EQUAL(static_cast<int>(hot.tier), static_cast<int>(SimulationTier::Background));

    // Process background simulation - need enough time to trigger updates
    // 20 updates of 100ms each = 2000ms = 20 updates at 10Hz
    for (int i = 0; i < 20; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.1f);
    }

    // Velocity should have decayed
    const auto& newTransform = edm->getTransform(handle);
    float velMag = std::sqrt(newTransform.velocity.getX() * newTransform.velocity.getX() +
                             newTransform.velocity.getY() * newTransform.velocity.getY());
    // Velocity should be less than initial (was ~141.4)
    BOOST_CHECK(velMag < 141.0f);

    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestBackgroundNPCPositionUpdate) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create NPC in background tier range (distance 750)
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");

    auto& transform = edm->getTransform(handle);
    float initialX = transform.position.getX();
    transform.velocity = Vector2D(100.0f, 0.0f);

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();

    // First update to trigger tier recalc and set hasNonActiveEntities flag
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);

    // Process - need enough time to trigger updates (100ms per update at 10Hz)
    for (int i = 0; i < 20; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.1f);
    }

    // Position should have changed
    const auto& newTransform = edm->getTransform(handle);
    BOOST_CHECK(newTransform.position.getX() != initialX);

    edm->destroyEntity(handle);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(IntegrationTests, BackgroundSimManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestFullWorkflow) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000, Hibernated: >1000
    bgsm->setUpdateRate(10.0f);

    // Create mixed entities at appropriate distances
    EntityHandle player = edm->registerPlayer(1, Vector2D(0.0f, 0.0f));
    EntityHandle nearNpc = edm->createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");       // Active (<500)
    EntityHandle farNpc = edm->createNPCWithRaceClass(Vector2D(750.0f, 0.0f), "Human", "Guard");          // Background (500-1000)
    EntityHandle veryFarNpc = edm->createNPCWithRaceClass(Vector2D(1500.0f, 0.0f), "Human", "Guard");     // Hibernated (>1000)

    // Force initial tier assignment
    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();

    // First update to trigger tier recalc
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);

    // Check tiers after initial assignment
    BOOST_CHECK_EQUAL(static_cast<int>(edm->getHotData(nearNpc).tier), static_cast<int>(SimulationTier::Active));
    BOOST_CHECK_EQUAL(static_cast<int>(edm->getHotData(farNpc).tier), static_cast<int>(SimulationTier::Background));
    BOOST_CHECK_EQUAL(static_cast<int>(edm->getHotData(veryFarNpc).tier), static_cast<int>(SimulationTier::Hibernated));

    // Simulate game loop (200 frames at 16ms = 3.2s)
    for (int frame = 0; frame < 200; ++frame) {
        bgsm->update(edm->getTransform(player).position, 0.016f);
    }

    // Verify tiers remain correct
    const auto& nearHot = edm->getHotData(nearNpc);
    const auto& farHot = edm->getHotData(farNpc);
    const auto& veryFarHot = edm->getHotData(veryFarNpc);

    BOOST_CHECK_EQUAL(static_cast<int>(nearHot.tier), static_cast<int>(SimulationTier::Active));
    BOOST_CHECK_EQUAL(static_cast<int>(farHot.tier), static_cast<int>(SimulationTier::Background));
    BOOST_CHECK_EQUAL(static_cast<int>(veryFarHot.tier), static_cast<int>(SimulationTier::Hibernated));

    // Clean up
    edm->destroyEntity(player);
    edm->destroyEntity(nearNpc);
    edm->destroyEntity(farNpc);
    edm->destroyEntity(veryFarNpc);
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_CASE(TestManyBackgroundEntities) {
    // Set explicit radii for deterministic testing
    bgsm->setActiveRadius(500.0f);      // Active: 0-500
    bgsm->setBackgroundRadius(1000.0f); // Background: 500-1000

    // Create many background entities in background tier range (500-1000)
    std::vector<EntityHandle> handles;
    for (int i = 0; i < 500; ++i) {
        // Place entities in a ring pattern in background tier range
        float angle = (i / 500.0f) * 6.28f * 10.0f;  // 10 revolutions
        float dist = 600.0f + (i % 50) * 7.0f;        // 600-949 (background tier)
        handles.push_back(edm->createNPCWithRaceClass(Vector2D(dist * std::cos(angle), dist * std::sin(angle)), "Human", "Guard"));
    }

    bgsm->setReferencePoint(Vector2D(0.0f, 0.0f));
    bgsm->invalidateTiers();  // Force tier update within update()
    bgsm->resetPerfStats();

    // First update to trigger tier recalc and set hasNonActiveEntities flag
    bgsm->update(Vector2D(0.0f, 0.0f), 0.0f);
    BOOST_REQUIRE_EQUAL(edm->getBackgroundIndices().size(), handles.size());
    primeBackgroundThreadingDecision(handles.size());
    bgsm->resetPerfStats();  // Reset again after tier update

    // Process - 50 updates of 100ms = 5000ms = should trigger 50 updates at 10Hz
    for (int i = 0; i < 50; ++i) {
        bgsm->update(Vector2D(0.0f, 0.0f), 0.1f);
    }

    const auto& stats = bgsm->getPerfStats();
    BOOST_CHECK(stats.totalUpdates > 0);
    BOOST_CHECK(stats.lastWasThreaded);
    BOOST_CHECK_GT(stats.lastBatchCount, 1u);

    // Clean up
    for (const auto& handle : handles) {
        edm->destroyEntity(handle);
    }
    edm->processDestructionQueue();
}

BOOST_AUTO_TEST_SUITE_END()
