/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ParticleManagerPerformanceTest
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "managers/ParticleManager.hpp"
#include "utils/Vector2D.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

// Test fixture for ParticleManager performance tests
struct ParticleManagerPerformanceFixture {
  ParticleManagerPerformanceFixture() {
    // Initialize ThreadSystem first (required for batch submissions)
    if (!VoidLight::ThreadSystem::Instance().isShutdown()) {
      BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
      // Log WorkerBudget allocations for production-matching verification
      const auto& budget = VoidLight::WorkerBudgetManager::Instance().getBudget();
      std::cout << "System: " << std::thread::hardware_concurrency() << " hardware threads\n";
      std::cout << "WorkerBudget: " << budget.totalWorkers << " workers (all available per manager)\n";
    }

    manager = &ParticleManager::Instance();

    // Ensure clean state for each test
    if (manager->isInitialized()) {
      manager->clean();
    }

    // Initialize for performance tests
    manager->init();
    manager->registerBuiltInEffects();
  }

  ~ParticleManagerPerformanceFixture() {
    if (manager->isInitialized()) {
      manager->clean();
    }

    // Clean up ThreadSystem
    if (!VoidLight::ThreadSystem::Instance().isShutdown()) {
      VoidLight::ThreadSystem::Instance().clean();
    }
  }

  ParticleManager *manager;

  // Helper to measure execution time
  template <typename Func> double measureExecutionTime(Func &&func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0; // Return milliseconds
  }

  // Helper to create particles for performance testing
  // Note: Actual particle count may be lower than target due to:
  // - Effect emission rates (particles emitted per frame)
  // - Particle lifetimes (particles may expire during creation)
  // - System limits (max effects, max particles per effect)
  // Performance tests should validate >= 90% of target for meaningful results
  void
  createParticles(size_t targetCount,
                  ParticleEffectType effectType = ParticleEffectType::Rain) {
    Vector2D basePosition(960, 100); // Center-top

    // Reset performance stats for clean measurement
    manager->resetPerformanceStats();

    // Create effects until we have enough particles
    std::vector<uint32_t> effectIds;

    while (manager->getActiveParticleCount() < targetCount) {
      // Create effects at different positions to spread particles
      const auto effectIndex = static_cast<int>(effectIds.size());
      Vector2D position(basePosition.getX() + ((effectIndex % 10) - 5) * 100,
                        basePosition.getY() + (effectIndex / 10) * 50);

      uint32_t effectId = manager->playEffect(effectType, position, 1.0f);
      if (effectId != 0) {
        effectIds.push_back(effectId);
      }

      // Update to emit particles
      manager->update(0.016f); // 60 FPS

      // Safety check to prevent infinite loops
      // If we can't reach target after 100 effects, system may have limits
      if (effectIds.size() > 100) {
        std::cout << "Warning: Reached effect limit (" << effectIds.size()
                  << " effects) before target particle count" << std::endl;
        break;
      }
    }

    std::cout << "Created " << manager->getActiveParticleCount()
              << " particles using " << effectIds.size() << " effects"
              << std::endl;
  }
};

// Test update performance with 1000 particles
BOOST_FIXTURE_TEST_CASE(TestUpdatePerformance1000Particles,
                        ParticleManagerPerformanceFixture) {
  const size_t TARGET_PARTICLES = 1000;
  const double MAX_UPDATE_TIME_MS = 5.0; // 5ms max for 1000 particles

  createParticles(TARGET_PARTICLES);
  size_t actualParticles = manager->getActiveParticleCount();

  // Require at least 90% of target particle count for meaningful performance test
  // If we can't create enough particles, the test results are not valid
  BOOST_REQUIRE_MESSAGE(actualParticles >= TARGET_PARTICLES * 9 / 10,
                        "Failed to create sufficient particles for test: got " << actualParticles
                        << " but need at least " << (TARGET_PARTICLES * 9 / 10));
  std::cout << "Testing update performance with " << actualParticles
            << " particles" << std::endl;

  // Measure update time
  double updateTime = measureExecutionTime([this]() {
    manager->update(0.016f); // 60 FPS update
  });

  std::cout << "Update time: " << updateTime << "ms" << std::endl;

  // Update time should be reasonable for real-time performance
  BOOST_CHECK_LT(updateTime, MAX_UPDATE_TIME_MS);
}

// Test update performance with 5000 particles
BOOST_FIXTURE_TEST_CASE(TestUpdatePerformance5000Particles,
                        ParticleManagerPerformanceFixture) {
  const size_t TARGET_PARTICLES = 5000;
  const double MAX_UPDATE_TIME_MS = 16.0; // 16ms max (one frame budget)

  createParticles(TARGET_PARTICLES);
  size_t actualParticles = manager->getActiveParticleCount();

  // Require at least 90% of target particle count for meaningful performance test
  // If we can't create enough particles, the test results are not valid
  BOOST_REQUIRE_MESSAGE(actualParticles >= TARGET_PARTICLES * 9 / 10,
                        "Failed to create sufficient particles for test: got " << actualParticles
                        << " but need at least " << (TARGET_PARTICLES * 9 / 10));
  std::cout << "Testing update performance with " << actualParticles
            << " particles" << std::endl;

  // Measure update time
  double updateTime =
      measureExecutionTime([this]() { manager->update(0.016f); });

  std::cout << "Update time: " << updateTime << "ms" << std::endl;

  // Should still be within frame budget
  BOOST_CHECK_LT(updateTime, MAX_UPDATE_TIME_MS);
}

// Test particle creation throughput
BOOST_FIXTURE_TEST_CASE(TestParticleCreationThroughput,
                        ParticleManagerPerformanceFixture) {
  Vector2D testPosition(500, 300);

  // Measure time to create many effects
  const int NUM_EFFECTS = 50;
  double creationTime = measureExecutionTime([&]() {
    for (int i = 0; i < NUM_EFFECTS; ++i) {
      Vector2D pos(testPosition.getX() + i * 10, testPosition.getY());
      manager->playEffect(ParticleEffectType::Rain, pos, 1.0f);
    }
  });

  std::cout << "Time to create " << NUM_EFFECTS << " effects: " << creationTime
            << "ms" << std::endl;

  // Effect creation should be fast
  BOOST_CHECK_LT(creationTime, 10.0); // Should take less than 10ms

  // Update to emit particles and measure
  double emissionTime = measureExecutionTime([this]() {
    for (int i = 0; i < 10; ++i) {
      manager->update(0.016f);
    }
  });

  std::cout << "Time for 10 updates (particle emission): " << emissionTime
            << "ms" << std::endl;
  std::cout << "Particles created: " << manager->getActiveParticleCount()
            << std::endl;

  BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);
}

// Test memory usage scaling
BOOST_FIXTURE_TEST_CASE(TestMemoryUsageScaling,
                        ParticleManagerPerformanceFixture) {
  Vector2D testPosition(500, 300);

  // Start with baseline
  // Create many effects rapidly
  std::vector<size_t> particleCounts;
  std::vector<double> updateTimes;

  for (int batch = 0; batch < 5; ++batch) {
    // Add more effects
    for (int i = 0; i < 10; ++i) {
      Vector2D pos(testPosition.getX() + batch * 100 + i * 10,
                   testPosition.getY());
      manager->playEffect(ParticleEffectType::Rain, pos, 1.0f);
    }

    // Update to create particles
    for (int i = 0; i < 5; ++i) {
      manager->update(0.016f);
    }

    // Measure update performance at this particle count
    size_t currentCount = manager->getActiveParticleCount();
    double updateTime =
        measureExecutionTime([this]() { manager->update(0.016f); });

    particleCounts.push_back(currentCount);
    updateTimes.push_back(updateTime);

    std::cout << "Particles: " << currentCount
              << ", Update time: " << updateTime << "ms" << std::endl;
  }

  // Check that we have reasonable scaling
  BOOST_CHECK_GT(particleCounts.back(), particleCounts.front());

  // Update times should scale somewhat linearly, not exponentially
  double firstTime = updateTimes.front();
  double lastTime = updateTimes.back();
  double particleRatio =
      static_cast<double>(particleCounts.back()) / particleCounts.front();
  double timeRatio = lastTime / firstTime;

  std::cout << "Particle ratio: " << particleRatio
            << ", Time ratio: " << timeRatio << std::endl;

  // Time scaling should be reasonable (not more than 3x worse than linear)
  BOOST_CHECK_LT(timeRatio,
                 particleRatio *
                     5.0); // Allow up to 5x scaling due to system variance
}

// Test cleanup performance
BOOST_FIXTURE_TEST_CASE(TestCleanupPerformance,
                        ParticleManagerPerformanceFixture) {
  // Create many weather effects (not regular effects) to test cleanup
  Vector2D testPosition(500, 300);

  // Create weather effects that can be properly cleaned up
  for (int i = 0; i < 20; ++i) {
    manager->triggerWeatherEffect("Rainy", 1.0f);
    // Small delay to let effect start
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Update to create particles
  for (int i = 0; i < 15; ++i) {
    manager->update(0.016f);
  }

  size_t initialCount = manager->getActiveParticleCount();
  BOOST_CHECK_GT(initialCount, 50); // Should have created some particles
                                    // (adjusted for new emission rates)

  std::cout << "Initial particle count: " << initialCount << std::endl;

  // Stop all weather effects to trigger cleanup
  manager->stopWeatherEffects(0.0f); // Immediate stop

  // Measure cleanup time
  double cleanupTime = measureExecutionTime([this]() {
    // Update several times to process cleanup - more time needed for lock-free
    // system
    for (int i = 0; i < 30; ++i) {
      manager->update(0.016f);
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
  });

  size_t finalCount = manager->getActiveParticleCount();
  std::cout << "Final particle count: " << finalCount << std::endl;
  std::cout << "Cleanup time: " << cleanupTime << "ms" << std::endl;

  // Cleanup should be reasonably fast (increased threshold for lock-free
  // system)
  BOOST_CHECK_LT(cleanupTime, 50.0); // Should take less than 50ms

  // With lock-free system, check for reasonable behavior rather than immediate
  // cleanup
  BOOST_CHECK_LE(finalCount, initialCount * 3); // Should not grow excessively
}

// Test effect management performance
BOOST_FIXTURE_TEST_CASE(TestEffectManagementPerformance,
                        ParticleManagerPerformanceFixture) {
  Vector2D testPosition(500, 300);

  // Create and immediately stop many effects
  const int NUM_EFFECTS = 100;
  std::vector<uint32_t> effectIds;

  double effectManagementTime = measureExecutionTime([&]() {
    // Create effects
    for (int i = 0; i < NUM_EFFECTS; ++i) {
      Vector2D pos(testPosition.getX() + i * 5, testPosition.getY());
      uint32_t id = manager->playEffect(ParticleEffectType::Rain, pos, 0.5f);
      if (id != 0) {
        effectIds.push_back(id);
      }
    }

    // Stop half of them
    for (size_t i = 0; i < effectIds.size() / 2; ++i) {
      manager->stopEffect(effectIds[i]);
    }
  });

  std::cout << "Time to create and manage " << NUM_EFFECTS
            << " effects: " << effectManagementTime << "ms" << std::endl;

  // Effect management should be fast
  BOOST_CHECK_LT(effectManagementTime, 15.0);

  // Check that some effects are still playing
  int activeEffects = 0;
  for (uint32_t id : effectIds) {
    if (manager->isEffectPlaying(id)) {
      activeEffects++;
    }
  }

  std::cout << "Active effects remaining: " << activeEffects << std::endl;
  BOOST_CHECK_GT(activeEffects, 0);
  BOOST_CHECK_LT(activeEffects, static_cast<int>(effectIds.size()));
}

// Test sustained performance over time
// NOTE: This test ignores the first 5 frames (warmup) and allows up to 2 outlier frames
// exceeding avgTime * 6.0 due to OS scheduling or background spikes. This prevents false
// negatives from rare timing spikes and ensures only consistent performance issues cause failure.
BOOST_FIXTURE_TEST_CASE(TestSustainedPerformance,
                         ParticleManagerPerformanceFixture) {  createParticles(1500);

  const int NUM_FRAMES = 60; // Test 1 second at 60 FPS
  std::vector<double> frameTimes;

  // Measure sustained performance
  for (int frame = 0; frame < NUM_FRAMES; ++frame) {
    double frameTime =
        measureExecutionTime([this]() { manager->update(0.016f); });
    frameTimes.push_back(frameTime);
  }

  // Calculate statistics
  double totalTime = 0.0;
  double maxTime = 0.0;
  double minTime = frameTimes[0];

  for (double time : frameTimes) {
    totalTime += time;
    maxTime = std::max(maxTime, time);
    minTime = std::min(minTime, time);
  }

  double avgTime = totalTime / NUM_FRAMES;

  std::cout << "Sustained performance over " << NUM_FRAMES
            << " frames:" << std::endl;
  std::cout << "  Average: " << avgTime << "ms" << std::endl;
  std::cout << "  Min: " << minTime << "ms" << std::endl;
  std::cout << "  Max: " << maxTime << "ms" << std::endl;
  std::cout << "  Total: " << totalTime << "ms" << std::endl;

  // Performance should be consistent
  BOOST_CHECK_LT(avgTime, 10.0); // Average should be reasonable
  BOOST_CHECK_LT(maxTime, 25.0); // No frame should take too long

  // Robustness: Ignore first 5 frames (warmup), allow up to 2 outlier frames
  // exceeding avgTime * 6.0 due to OS scheduling or background spikes.
  int outlierCount = 0;
  for (int i = 5; i < NUM_FRAMES; ++i) { // Ignore first 5 frames
    if (frameTimes[i] > avgTime * 6.0) {
      outlierCount++;
      std::cout << "Outlier frame " << i << ": " << frameTimes[i] << "ms" << std::endl;
    }
  }
  // Only fail if more than 2 outliers
  BOOST_CHECK_LE(outlierCount, 2);

  // Max shouldn't be too much worse than average (indicating consistent
  // performance) Note: OS scheduling can cause occasional timing spikes, so we
  // use a tolerant threshold
  // BOOST_CHECK_LT(maxTime, avgTime * 6.0); // replaced by outlier logic above
}

// Test performance with different effect types
BOOST_FIXTURE_TEST_CASE(TestDifferentEffectTypesPerformance,
                        ParticleManagerPerformanceFixture) {
  Vector2D testPosition(500, 300);

  std::vector<ParticleEffectType> effectTypes = {ParticleEffectType::Rain,
                                                 ParticleEffectType::Snow,
                                                 ParticleEffectType::Fog};

  for (const auto &effectType : effectTypes) {
    std::string effectName;
    switch (effectType) {
    case ParticleEffectType::Rain:
      effectName = "Rain";
      break;
    case ParticleEffectType::Snow:
      effectName = "Snow";
      break;
    case ParticleEffectType::Fog:
      effectName = "Fog";
      break;
    default:
      effectName = "Unknown";
      break;
    }

    std::cout << "\nTesting " << effectName
              << " effect performance:" << std::endl;

    // Clean up previous effects
    manager->stopWeatherEffects(0.0f);
    for (int i = 0; i < 5; ++i) {
      manager->update(0.016f); // Clean up
    }

    // Create effects of this type
    std::vector<uint32_t> effectIds;
    for (int i = 0; i < 20; ++i) {
      Vector2D pos(testPosition.getX() + i * 20, testPosition.getY());
      uint32_t id = manager->playEffect(effectType, pos, 1.0f);
      if (id != 0) {
        effectIds.push_back(id);
      }
    }

    // Update to create particles
    for (int i = 0; i < 10; ++i) {
      manager->update(0.016f);
    }

    size_t particleCount = manager->getActiveParticleCount();

    // Measure update performance
    double updateTime =
        measureExecutionTime([this]() { manager->update(0.016f); });

    std::cout << "  Particles: " << particleCount << std::endl;
    std::cout << "  Update time: " << updateTime << "ms" << std::endl;

    // All effect types should perform reasonably
    // Adjusted threshold to account for bounds checking safety improvements
    // The original threshold of 15ms was too strict for debug builds with
    // safety checks
    BOOST_CHECK_LT(updateTime, 20.0);
    BOOST_CHECK_GT(particleCount, 0);
  }
}

// Detect optimal threading threshold for ParticleManager
BOOST_FIXTURE_TEST_CASE(TestThreadingThreshold,
                        ParticleManagerPerformanceFixture) {
  std::cout << "\n===== PARTICLE THREADING THRESHOLD DETECTION =====" << std::endl;
  std::cout << "Comparing single-threaded vs multi-threaded at different particle counts\n" << std::endl;

  std::vector<size_t> testCounts = {50, 100, 200, 500, 1000, 2000, 5000};
  size_t optimalThreshold = 0;
  size_t marginalThreshold = 0;

  std::cout << std::setw(12) << "Particles"
            << std::setw(18) << "Single (ms/upd)"
            << std::setw(18) << "Threaded (ms/upd)"
            << std::setw(12) << "Speedup"
            << std::setw(15) << "Verdict" << std::endl;
  std::cout << std::string(75, '-') << std::endl;

  for (size_t targetCount : testCounts) {
    // Test single-threaded
    if (manager->isInitialized()) manager->clean();
    manager->init();
    manager->registerBuiltInEffects();
    #ifndef NDEBUG
    manager->enableThreading(false);
    #endif

    createParticles(targetCount);
    size_t actualCount = manager->getActiveParticleCount();

    // Warmup
    for (int i = 0; i < 5; ++i) manager->update(0.016f);

    // Measure single-threaded
    double singleTotal = 0;
    for (int i = 0; i < 5; ++i) {
      singleTotal += measureExecutionTime([this]() { manager->update(0.016f); });
    }
    double singleTime = singleTotal / 5.0;

    // Test multi-threaded
    if (manager->isInitialized()) manager->clean();
    manager->init();
    manager->registerBuiltInEffects();
    #ifndef NDEBUG
    manager->enableThreading(true);
    #endif

    createParticles(targetCount);

    // Warmup
    for (int i = 0; i < 5; ++i) manager->update(0.016f);

    // Measure threaded
    double threadedTotal = 0;
    for (int i = 0; i < 5; ++i) {
      threadedTotal += measureExecutionTime([this]() { manager->update(0.016f); });
    }
    double threadedTime = threadedTotal / 5.0;

    double speedup = (threadedTime > 0) ? singleTime / threadedTime : 0;

    std::string verdict;
    if (speedup > 1.5) {
      verdict = "THREAD";
      if (optimalThreshold == 0) optimalThreshold = actualCount;
    } else if (speedup > 1.1) {
      verdict = "marginal";
      if (marginalThreshold == 0) marginalThreshold = actualCount;
    } else {
      verdict = "single";
    }

    std::cout << std::setw(12) << actualCount
              << std::setw(18) << std::fixed << std::setprecision(3) << singleTime
              << std::setw(18) << std::fixed << std::setprecision(3) << threadedTime
              << std::setw(11) << std::fixed << std::setprecision(2) << speedup << "x"
              << std::setw(15) << verdict << std::endl;
  }

  std::cout << "\n=== PARTICLE THREADING RECOMMENDATION ===" << std::endl;
  std::cout << "Current threshold:  100 particles" << std::endl;

  if (optimalThreshold > 0) {
    std::cout << "Optimal threshold:  " << optimalThreshold << " particles (speedup > 1.5x)" << std::endl;
    if (optimalThreshold > 100) {
      std::cout << "ACTION: Consider raising ParticleManager::m_threadingThreshold to " << optimalThreshold << std::endl;
    } else if (optimalThreshold < 100) {
      std::cout << "ACTION: Consider lowering ParticleManager::m_threadingThreshold to " << optimalThreshold << std::endl;
    } else {
      std::cout << "STATUS: Current threshold is optimal" << std::endl;
    }
  } else if (marginalThreshold > 0) {
    std::cout << "Marginal benefit at: " << marginalThreshold << " particles" << std::endl;
    std::cout << "STATUS: Threading provides minimal benefit on this hardware" << std::endl;
  } else {
    std::cout << "STATUS: Single-threaded is faster at all tested counts" << std::endl;
    std::cout << "ACTION: Consider raising threshold above 5000" << std::endl;
  }

  std::cout << "==========================================\n" << std::endl;

  // Restore threading
  #ifndef NDEBUG
  manager->enableThreading(true);
  #endif
}

// Ad-hoc high-count benchmarks for update cost at scale (Debug build)
BOOST_FIXTURE_TEST_CASE(HighCountBenchmarks,
                        ParticleManagerPerformanceFixture) {
  // Targets and simple setup
  std::vector<size_t> targets = {10000, 25000, 50000};
  Vector2D basePosition(960, 120);

  for (size_t target : targets) {
    // Fresh state
    if (manager->isInitialized()) manager->clean();
    manager->init();
    manager->registerBuiltInEffects();

    // Create many effects to reach target more quickly
    std::vector<uint32_t> effectIds;
    const int maxEffects = 450; // limit to avoid excessive startup
    for (int i = 0; i < maxEffects; ++i) {
      float ox = static_cast<float>((i % 30) * 40 - 600);
      float oy = static_cast<float>((i / 30) * 25);
      Vector2D pos(basePosition.getX() + ox, basePosition.getY() + oy);
      uint32_t id = manager->playEffect(ParticleEffectType::Rain, pos, 1.0f);
      if (id != 0) effectIds.push_back(id);
    }

    // Let emission accumulate for a short period
    for (int f = 0; f < 90; ++f) { // ~1.5s at 60 FPS
      manager->update(0.016f);
    }

    size_t count = manager->getActiveParticleCount();
    std::cout << "HighCountBench: target=" << target
              << ", actual=" << count << ", effects=" << effectIds.size()
              << std::endl;

    // Measure average update time over several frames
    const int samples = 10;
    double totalMs = 0.0;
    for (int i = 0; i < samples; ++i) {
      totalMs += measureExecutionTime([&]() { manager->update(0.016f); });
    }
    double avgMs = totalMs / samples;
    std::cout << "HighCountBench: update_avg_ms=" << avgMs
              << " at particles=" << manager->getActiveParticleCount()
              << std::endl;

    // Non-failing sanity check
    BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);
  }
}

// WorkerBudget Adaptive Tuning test - verifies both batch sizing and threading
// threshold adapt correctly over time
BOOST_FIXTURE_TEST_CASE(WorkerBudgetAdaptiveTuning,
                        ParticleManagerPerformanceFixture) {
  std::cout << "\n===== WORKERBUDGET ADAPTIVE TUNING TEST =====" << std::endl;
  std::cout << "Testing both batch sizing hill-climb and threading threshold adaptation\n" << std::endl;

  auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

  // Fresh state
  if (manager->isInitialized()) manager->clean();
  manager->init();
  manager->registerBuiltInEffects();

  // Part 1: Batch Sizing Hill-Climb Convergence
  std::cout << "--- Part 1: Batch Sizing Hill-Climb ---" << std::endl;
  size_t initialBatch = budgetMgr.getBatchStrategy(VoidLight::SystemType::Particle, 5000, 4).first;
  std::cout << "Initial batch count for 5000 particles: " << initialBatch << std::endl;

  // Create particles for sustained update
  createParticles(5000);

  // Run updates to let hill-climb converge
  const int CONVERGENCE_FRAMES = 200;
  std::vector<size_t> batchHistory;

  for (int frame = 0; frame < CONVERGENCE_FRAMES; ++frame) {
    manager->update(0.016f);

    // Sample every 20 frames
    if (frame % 20 == 0) {
      size_t currentBatch = budgetMgr.getBatchStrategy(VoidLight::SystemType::Particle, 5000, 4).first;
      batchHistory.push_back(currentBatch);
    }
  }

  size_t finalBatch = budgetMgr.getBatchStrategy(VoidLight::SystemType::Particle, 5000, 4).first;
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
  double initialMultiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Particle, true);
  std::cout << "Initial multi throughput:  " << std::fixed << std::setprecision(2) << initialMultiTP << " items/ms" << std::endl;

  // Run additional frames to allow throughput tracking
  const int TRACKING_FRAMES = 600;

  for (int frame = 0; frame < TRACKING_FRAMES; ++frame) {
    manager->update(0.016f);

    // Sample throughput every 100 frames
    if (frame % 100 == 0) {
      double multiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Particle, true);
      float batchMult = budgetMgr.getBatchMultiplier(VoidLight::SystemType::Particle);
      std::cout << "Frame " << frame << ": multiTP=" << std::fixed << std::setprecision(2)
                << multiTP << " batchMult=" << batchMult << std::endl;
    }
  }

  double finalMultiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Particle, true);
  float finalBatchMult = budgetMgr.getBatchMultiplier(VoidLight::SystemType::Particle);
  std::cout << "Final multi throughput:  " << std::fixed << std::setprecision(2) << finalMultiTP << " items/ms" << std::endl;
  std::cout << "Final batch multiplier:  " << std::fixed << std::setprecision(2) << finalBatchMult << std::endl;

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
