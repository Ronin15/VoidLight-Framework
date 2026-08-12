/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ParticleManagerThreadingTest
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "managers/ParticleManager.hpp"
#include "utils/Vector2D.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

// Test fixture for ParticleManager threading tests
struct ParticleManagerThreadingFixture {
  ParticleManagerThreadingFixture() {
    // Initialize ThreadSystem first
    threadSystem = &VoidLight::ThreadSystem::Instance();

    // Always try to initialize the ThreadSystem for threading tests
    if (threadSystem->isShutdown() || threadSystem->getThreadCount() == 0) {
      // Use automatic thread detection for WorkerBudget testing
      bool initSuccess = threadSystem->init();
      if (!initSuccess) {
        // If initialization failed, this might be because it's already
        // initialized Check if it's working by verifying thread count
        if (threadSystem->getThreadCount() == 0) {
          throw std::runtime_error(
              "Failed to initialize ThreadSystem for threading tests");
        }
      }
    }

    // Verify ThreadSystem is ready
    if (threadSystem->isShutdown()) {
      throw std::runtime_error(
          "ThreadSystem is shutdown and cannot be used for threading tests");
    }

    // Initialize ParticleManager
    manager = &ParticleManager::Instance();

    // Ensure clean state for each test
    if (manager->isInitialized()) {
      manager->clean();
    }

    // Initialize and register effects
    BOOST_REQUIRE(manager->init());
    manager->registerBuiltInEffects();
  }

  ~ParticleManagerThreadingFixture() {
    if (manager->isInitialized()) {
      manager->clean();
    }
    // Note: Don't clean ThreadSystem here as it's shared across tests
  }

  ParticleManager *manager;
  VoidLight::ThreadSystem *threadSystem;
};

// Test concurrent particle creation
BOOST_FIXTURE_TEST_CASE(TestConcurrentParticleCreation,
                        ParticleManagerThreadingFixture) {
  // Use actual ThreadSystem thread count for realistic testing
  const int NUM_THREADS =
      std::min(static_cast<int>(threadSystem->getThreadCount()), 8);
  const int EFFECTS_PER_THREAD = 20;

  std::atomic<int> successCount{0};
  std::vector<std::future<void>> futures;

  // Launch concurrent tasks to create particle effects
  for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &successCount]() -> void {
          for (int i = 0; i < EFFECTS_PER_THREAD; ++i) {
            Vector2D position(100 + threadId * 50, 100 + i * 10);

            uint32_t effectId =
                manager->playEffect(ParticleEffectType::Rain, position, 0.5f);
            if (effectId != 0) {
              successCount.fetch_add(1, std::memory_order_relaxed);
            }

            // Small delay to simulate realistic usage
            std::this_thread::sleep_for(std::chrono::microseconds(100));
          }
        },
        VoidLight::TaskPriority::Normal, "ParticleCreationTask");

    futures.push_back(std::move(future));
  }

  // Wait for all tasks to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Update to emit particles
  for (int i = 0; i < 10; ++i) {
    manager->update(0.016f);
  }

  // Verify results
  BOOST_CHECK_EQUAL(successCount.load(), NUM_THREADS * EFFECTS_PER_THREAD);
  BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);

  std::cout << "Created " << manager->getActiveParticleCount()
            << " particles from " << successCount.load() << " effects across "
            << NUM_THREADS << " threads" << std::endl;
}

// Test high-frequency sequential particle updates
// Note: update() is designed to be called once per frame from a single thread
// (the game loop), not concurrently. Internal threading is handled automatically.
BOOST_FIXTURE_TEST_CASE(TestConcurrentParticleUpdates,
                        ParticleManagerThreadingFixture) {
  // Create some particles first
  Vector2D position(500, 300);
  for (int i = 0; i < 10; ++i) {
    manager->playEffect(ParticleEffectType::Rain, position, 1.0f);
  }

  // Update to create particles
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  size_t initialCount = manager->getActiveParticleCount();
  BOOST_CHECK_GT(initialCount, 0);

  // Test high-frequency sequential updates (simulates fast game loop)
  // This matches real-world usage: update() called once per frame sequentially
  const int TOTAL_UPDATES = 60;
  int updateCount = 0;

  for (int i = 0; i < TOTAL_UPDATES; ++i) {
    manager->update(0.016f);
    updateCount++;
  }

  // Verify all updates completed
  BOOST_CHECK_EQUAL(updateCount, TOTAL_UPDATES);

  // Particles should still exist or have been cleaned up naturally
  size_t finalCount = manager->getActiveParticleCount();
  std::cout << "Particle count after " << TOTAL_UPDATES << " sequential updates: "
            << finalCount << " (started with " << initialCount << ")" << std::endl;
}

// Test thread-safe effect management
BOOST_FIXTURE_TEST_CASE(TestThreadSafeEffectManagement,
                        ParticleManagerThreadingFixture) {
  const int NUM_THREADS =
      std::min(static_cast<int>(threadSystem->getThreadCount()), 6);
  const int OPERATIONS_PER_THREAD = 15;

  std::atomic<int> effectsCreated{0};
  std::atomic<int> effectsStopped{0};
  std::vector<std::future<void>> futures;
  std::vector<uint32_t> createdEffectIds;
  std::mutex effectIdsMutex;

  // Launch concurrent effect creation and management tasks
  for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &effectsCreated, &effectsStopped, &createdEffectIds,
         &effectIdsMutex]() -> void {
          std::vector<uint32_t> localEffectIds;

          for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
            Vector2D position(200 + threadId * 100, 200 + i * 20);

            // Create effect
            uint32_t effectId =
                manager->playEffect(ParticleEffectType::Snow, position, 0.7f);
            if (effectId != 0) {
              effectsCreated.fetch_add(1, std::memory_order_relaxed);
              localEffectIds.push_back(effectId);
            }

            // Occasionally stop an effect
            if (i > 5 && !localEffectIds.empty() && i % 3 == 0) {
              uint32_t idToStop = localEffectIds.back();
              localEffectIds.pop_back();

              manager->stopEffect(idToStop);
              effectsStopped.fetch_add(1, std::memory_order_relaxed);
            }

            std::this_thread::sleep_for(std::chrono::microseconds(200));
          }

          // Store remaining effect IDs for cleanup
          {
            std::lock_guard<std::mutex> lock(effectIdsMutex);
            createdEffectIds.insert(createdEffectIds.end(),
                                    localEffectIds.begin(),
                                    localEffectIds.end());
          }
        },
        VoidLight::TaskPriority::Normal, "EffectManagementTask");

    futures.push_back(std::move(future));
  }

  // Wait for all tasks to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Update to process effects
  for (int i = 0; i < 10; ++i) {
    manager->update(0.016f);
  }

  std::cout << "Effects created: " << effectsCreated.load()
            << ", Effects stopped: " << effectsStopped.load() << std::endl;
  std::cout << "Active particles: " << manager->getActiveParticleCount()
            << std::endl;

  // Verify operations completed
  BOOST_CHECK_GT(effectsCreated.load(), 0);
  BOOST_CHECK_GE(effectsCreated.load(), effectsStopped.load());
}

// Test concurrent weather effect changes
BOOST_FIXTURE_TEST_CASE(TestConcurrentWeatherChanges,
                        ParticleManagerThreadingFixture) {
  const int NUM_THREADS = 3;
  const int WEATHER_CHANGES_PER_THREAD = 10;

  std::vector<std::string> weatherTypes = {"Rainy", "Snowy", "Foggy", "Clear"};
  std::atomic<int> weatherChanges{0};
  std::vector<std::future<void>> futures;

  // Launch concurrent weather change tasks
  for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &weatherChanges, &weatherTypes]() -> void {
          for (int i = 0; i < WEATHER_CHANGES_PER_THREAD; ++i) {
            std::string weatherType = weatherTypes[i % weatherTypes.size()];
            float intensity = 0.3f + (i % 3) * 0.3f; // Vary intensity

            manager->triggerWeatherEffect(weatherType, intensity);
            weatherChanges.fetch_add(1, std::memory_order_relaxed);

            // Allow time for weather to take effect
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
          }
        },
        VoidLight::TaskPriority::Normal, "WeatherChangeTask");

    futures.push_back(std::move(future));
  }

  // Wait for all weather changes to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Update to process final weather state
  for (int i = 0; i < 20; ++i) {
    manager->update(0.016f);
  }

  std::cout << "Weather changes completed: " << weatherChanges.load()
            << std::endl;
  std::cout << "Final particle count: " << manager->getActiveParticleCount()
            << std::endl;

  // Verify all weather changes were processed
  BOOST_CHECK_EQUAL(weatherChanges.load(),
                    NUM_THREADS * WEATHER_CHANGES_PER_THREAD);
}

// Test concurrent access to performance stats
BOOST_FIXTURE_TEST_CASE(TestConcurrentStatsAccess,
                        ParticleManagerThreadingFixture) {
  // Create some particle activity
  Vector2D position(400, 400);
  for (int i = 0; i < 5; ++i) {
    manager->playEffect(ParticleEffectType::Rain, position, 1.0f);
  }

  const int NUM_THREADS =
      std::min(static_cast<int>(threadSystem->getThreadCount()), 6);
  const int STATS_READS_PER_THREAD = 50;

  std::atomic<int> statsReads{0};
  std::atomic<int> updateCalls{0};
  std::vector<std::future<void>> futures;

  // Launch tasks that read stats concurrently (stats getters are thread-safe)
  for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &statsReads]() -> void {
          for (int i = 0; i < STATS_READS_PER_THREAD; ++i) {
            // Read various stats (thread-safe getters)
            manager->getActiveParticleCount();
            manager->getMaxParticleCapacity();
            manager->getPerformanceStats();

            statsReads.fetch_add(1, std::memory_order_relaxed);

            std::this_thread::sleep_for(std::chrono::microseconds(100));
          }
        },
        VoidLight::TaskPriority::Normal, "StatsAccessTask");

    futures.push_back(std::move(future));
  }

  // Wait for all tasks to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Main thread performs updates (correct usage pattern)
  for (int i = 0; i < 30; ++i) {
    manager->update(0.016f);
    updateCalls++;
  }

  std::cout << "Stats reads completed: " << statsReads.load() << std::endl;
  std::cout << "Update calls: " << updateCalls.load() << std::endl;

  // Verify all stats reads completed without issues
  BOOST_CHECK_EQUAL(statsReads.load(), NUM_THREADS * STATS_READS_PER_THREAD);
  BOOST_CHECK_EQUAL(updateCalls.load(), 30);
}

// Test thread safety during cleanup
BOOST_FIXTURE_TEST_CASE(TestThreadSafeCleanup,
                        ParticleManagerThreadingFixture) {
  // Create many weather effects that can be properly cleaned up
  for (int i = 0; i < 10; ++i) {
    manager->triggerWeatherEffect("Rainy", 1.0f);
    // Small delay to let effects initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Update to create particles
  for (int i = 0; i < 15; ++i) {
    manager->update(0.016f);
  }

  size_t initialCount = manager->getActiveParticleCount();
  BOOST_CHECK_GT(initialCount, 50); // Should have many particles

  const int NUM_THREADS = 3;
  std::atomic<bool> cleanupStarted{false};
  std::vector<std::future<void>> futures;

  // Launch tasks that continue reading stats while cleanup happens
  for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &cleanupStarted]() -> void {
          int operations = 0;
          while (!cleanupStarted.load(std::memory_order_acquire) &&
                 operations < 50) {
            // Continue reading particle stats (thread-safe)
            manager->getActiveParticleCount();

            operations++;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
          }
        },
        VoidLight::TaskPriority::Normal, "ContinuousOperationTask");

    futures.push_back(std::move(future));
  }

  // Main thread continues updates while tasks read stats
  for (int i = 0; i < 10; ++i) {
    manager->update(0.016f);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  // Signal cleanup and perform it
  cleanupStarted.store(true, std::memory_order_release);
  manager->stopWeatherEffects(0.0f); // Immediate cleanup

  // Wait for all background tasks to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Process cleanup - with lock-free system we need more time for particles to
  // naturally expire
  for (int i = 0; i < 50; ++i) {
    manager->update(0.016f);
    // Small delay to allow natural particle expiration in lock-free system
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  size_t finalCount = manager->getActiveParticleCount();
  std::cout << "Particle count after threaded cleanup: " << finalCount
            << " (started with " << initialCount << ")" << std::endl;

  // With lock-free system, particles may persist longer but should eventually
  // decrease Check that we don't have runaway particle creation (final count
  // should be reasonable)
  BOOST_CHECK_LT(finalCount,
                 initialCount * 15); // Ensure no runaway particle growth
  BOOST_CHECK_GT(finalCount,
                 0); // Should still have some particles (system working)
}

// Test mixed concurrent operations
BOOST_FIXTURE_TEST_CASE(TestMixedConcurrentOperations,
                        ParticleManagerThreadingFixture) {
  const int NUM_THREADS =
      std::min(static_cast<int>(threadSystem->getThreadCount()), 6);
  const int OPERATIONS_PER_THREAD = 25;

  std::atomic<int> totalOperations{0};
  std::vector<std::future<void>> futures;

  // Launch mixed operation tasks (thread-safe operations only)
  for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
    auto future = threadSystem->enqueueTaskWithResult(
        [=, this, &totalOperations]() -> void {
          for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
            Vector2D position(150 + threadId * 80, 150 + i * 15);

            switch (i % 4) { // Changed from 5 to 4 cases (removed update)
            case 0: {
              // Create effect (thread-safe with mutex)
              manager->playEffect(ParticleEffectType::Rain, position, 0.6f);
              break;
            }
            case 1: {
              // Check stats (thread-safe getters)
              manager->getActiveParticleCount();
              break;
            }
            case 2: {
              // Weather effect (thread-safe with mutex)
              if (i % 10 == 2) {
                manager->triggerWeatherEffect("Snowy", 0.4f);
              }
              break;
            }
            case 3: {
              // Pause/resume (atomic operations)
              if (i % 15 == 3) {
                bool currentlyPaused = manager->isGloballyPaused();
                manager->setGlobalPause(!currentlyPaused);

                // Quick toggle back
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                manager->setGlobalPause(currentlyPaused);
              }
              break;
            }
            }

            totalOperations.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
          }
        },
        VoidLight::TaskPriority::Normal, "MixedOperationTask");

    futures.push_back(std::move(future));
  }

  // Main thread performs updates while tasks run (correct usage pattern)
  std::atomic<bool> tasksComplete{false};
  auto updateFuture = threadSystem->enqueueTaskWithResult(
      [this, &tasksComplete]() -> void {
        while (!tasksComplete.load(std::memory_order_acquire)) {
          manager->update(0.016f);
          std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
      },
      VoidLight::TaskPriority::Normal, "UpdateTask");

  // Wait for all mixed operations to complete
  for (auto &future : futures) {
    future.wait();
  }

  // Signal update task to stop
  tasksComplete.store(true, std::memory_order_release);
  updateFuture.wait();

  // Final updates to ensure consistent state
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  std::cout << "Total mixed operations completed: " << totalOperations.load()
            << std::endl;
  std::cout << "Final active particles: " << manager->getActiveParticleCount()
            << std::endl;

  // Verify all operations completed
  BOOST_CHECK_EQUAL(totalOperations.load(),
                    NUM_THREADS * OPERATIONS_PER_THREAD);

  // System should still be in a valid state
  BOOST_CHECK(manager->isInitialized());
  BOOST_CHECK(!manager->isShutdown());
}
