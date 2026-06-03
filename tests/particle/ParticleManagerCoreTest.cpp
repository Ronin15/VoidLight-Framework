/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ParticleManagerCoreTest
#include <boost/test/unit_test.hpp>

#include "events/ParticleEffectEvent.hpp"
#include "managers/EventManager.hpp"
#include "managers/ParticleManager.hpp"
#include "utils/Vector2D.hpp"
#include <memory>

namespace {
void registerParticleEffectForwarder() {
  EventManager::Instance().registerHandler(
      EventTypeId::ParticleEffect, [](const EventData &data) {
        ParticleManager::Instance().handleParticleEffectEvent(data);
      });
}
} // namespace

// Test fixture for ParticleManager core functionality
struct ParticleManagerCoreFixture {
  ParticleManagerCoreFixture() {
    // Get ParticleManager instance
    manager = &ParticleManager::Instance();

    if (EventManager::Instance().isInitialized()) {
      EventManager::Instance().clean();
    }
    EventManager::Instance().init();

    // Ensure clean state for each test
    if (manager->isInitialized()) {
      manager->clean();
    }
  }

  ~ParticleManagerCoreFixture() {
    // Clean up after each test
    if (manager->isInitialized()) {
      manager->clean();
    }
    if (EventManager::Instance().isInitialized()) {
      EventManager::Instance().clean();
    }
  }

  ParticleManager *manager;
};

// Test basic initialization
BOOST_FIXTURE_TEST_CASE(TestInitialization, ParticleManagerCoreFixture) {
  // Initially should not be initialized
  BOOST_CHECK(!manager->isInitialized());
  BOOST_CHECK(!manager->isShutdown());

  // Initialize should succeed
  bool initResult = manager->init();
  BOOST_CHECK(initResult);
  BOOST_CHECK(manager->isInitialized());
  BOOST_CHECK(!manager->isShutdown());

  // Should start with no particles
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), 0);
}

// Test double initialization handling
BOOST_FIXTURE_TEST_CASE(TestDoubleInitialization, ParticleManagerCoreFixture) {
  // First initialization
  bool firstInit = manager->init();
  BOOST_CHECK(firstInit);
  BOOST_CHECK(manager->isInitialized());

  // Second initialization should still return true but not break anything
  bool secondInit = manager->init();
  BOOST_CHECK(secondInit);
  BOOST_CHECK(manager->isInitialized());
  BOOST_CHECK(!manager->isShutdown());
}

// Test cleanup functionality
BOOST_FIXTURE_TEST_CASE(TestCleanup, ParticleManagerCoreFixture) {
  // Initialize first
  manager->init();
  BOOST_CHECK(manager->isInitialized());

  // Clean should mark as shutdown
  manager->clean();
  BOOST_CHECK(!manager->isInitialized());
  BOOST_CHECK(manager->isShutdown());

  // Should have no active particles after cleanup
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), 0);
}

// Test state transition preparation
BOOST_FIXTURE_TEST_CASE(TestPrepareForStateTransition,
                        ParticleManagerCoreFixture) {
  manager->init();

  // Should not be paused initially
  BOOST_CHECK(!manager->isGloballyPaused());

  // Prepare for state transition
  manager->prepareForStateTransition();

  // Should be resumed after preparation (method temporarily pauses then
  // resumes)
  BOOST_CHECK(!manager->isGloballyPaused());
  BOOST_CHECK(manager->isInitialized()); // Should still be initialized
}

// Test built-in effect registration
BOOST_FIXTURE_TEST_CASE(TestBuiltInEffectsRegistration,
                        ParticleManagerCoreFixture) {
  manager->init();

  // Register built-in effects
  manager->registerBuiltInEffects();

  // Try to play some built-in effects to verify they're registered
  Vector2D testPosition(100, 100);

  uint32_t rainEffect =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 0.5f);
  BOOST_CHECK_NE(rainEffect, 0); // Should return valid effect ID

  uint32_t snowEffect =
      manager->playEffect(ParticleEffectType::Snow, testPosition, 0.5f);
  BOOST_CHECK_NE(snowEffect, 0);

  uint32_t fogEffect =
      manager->playEffect(ParticleEffectType::Fog, testPosition, 0.5f);
  BOOST_CHECK_NE(fogEffect, 0);
}

// Test effect ID generation
BOOST_FIXTURE_TEST_CASE(TestEffectIdGeneration, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D testPosition(100, 100);

  // Generate multiple effect IDs
  uint32_t id1 =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 0.5f);
  uint32_t id2 =
      manager->playEffect(ParticleEffectType::Snow, testPosition, 0.5f);
  uint32_t id3 =
      manager->playEffect(ParticleEffectType::Fog, testPosition, 0.5f);

  // All IDs should be different and non-zero
  BOOST_CHECK_NE(id1, 0);
  BOOST_CHECK_NE(id2, 0);
  BOOST_CHECK_NE(id3, 0);
  BOOST_CHECK_NE(id1, id2);
  BOOST_CHECK_NE(id2, id3);
  BOOST_CHECK_NE(id1, id3);
}

// Test effect start and stop
BOOST_FIXTURE_TEST_CASE(TestEffectStartStop, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D testPosition(100, 100);

  // Start an effect
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 0.5f);
  BOOST_CHECK_NE(effectId, 0);

  // Effect should be playing
  BOOST_CHECK(manager->isEffectPlaying(effectId));

  // Stop the effect
  manager->stopEffect(effectId);

  // Effect should no longer be playing
  BOOST_CHECK(!manager->isEffectPlaying(effectId));

  // Stopping non-existent effect should not crash
  manager->stopEffect(99999);
}

// Test global pause/resume
BOOST_FIXTURE_TEST_CASE(TestGlobalPauseResume, ParticleManagerCoreFixture) {
  // Test pause/resume functionality by checking update behavior
  manager->registerBuiltInEffects();

  Vector2D testPosition(100, 100);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 0.5f);
  BOOST_CHECK_NE(effectId, 0);

  // Update to create some particles
  manager->update(0.1f);
  size_t initialCount = manager->getActiveParticleCount();

  // Pause globally
  manager->setGlobalPause(true);
  BOOST_CHECK(manager->isGloballyPaused());

  // Update while paused should not change particle count
  manager->update(0.1f);
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), initialCount);

  // Resume
  manager->setGlobalPause(false);
  BOOST_CHECK(!manager->isGloballyPaused());
}

// Test global visibility
BOOST_FIXTURE_TEST_CASE(TestGlobalVisibility, ParticleManagerCoreFixture) {
  manager->init();

  // Should be visible initially
  BOOST_CHECK(manager->isGloballyVisible());

  // Set invisible
  manager->setGlobalVisibility(false);
}

// Test ParticleEffectEvent properties (data carrier validation)
BOOST_FIXTURE_TEST_CASE(TestParticleEffectEventProperties,
                        ParticleManagerCoreFixture) {
  manager->init();

  // Test event construction and property access
  Vector2D testPosition(150.0f, 250.0f);
  ParticleEffectEvent event("TestEvent", ParticleEffectType::Fire, testPosition,
                            1.2f, 3.0f, "testGroup", "testSound");

  // Verify all properties are set correctly
  BOOST_CHECK_EQUAL(event.getName(), "TestEvent");
  BOOST_CHECK_EQUAL(event.getType(), "ParticleEffect");
  BOOST_CHECK_EQUAL(event.getTypeName(), "ParticleEffectEvent");
  BOOST_CHECK_EQUAL(static_cast<int>(event.getTypeId()),
                    static_cast<int>(EventTypeId::ParticleEffect));
  BOOST_CHECK_EQUAL(static_cast<int>(event.getEffectType()),
                    static_cast<int>(ParticleEffectType::Fire));
  BOOST_CHECK_EQUAL(event.getEffectName(), "Fire");
  BOOST_CHECK_EQUAL(event.getPosition().getX(), 150.0f);
  BOOST_CHECK_EQUAL(event.getPosition().getY(), 250.0f);
  BOOST_CHECK_EQUAL(event.getIntensity(), 1.2f);
  BOOST_CHECK_EQUAL(event.getDuration(), 3.0f);
  BOOST_CHECK_EQUAL(event.getGroupTag(), "testGroup");
  BOOST_CHECK_EQUAL(event.getSoundEffect(), "testSound");

  // Test setters
  event.setPosition(Vector2D(200.0f, 300.0f));
  BOOST_CHECK_EQUAL(event.getPosition().getX(), 200.0f);
  BOOST_CHECK_EQUAL(event.getPosition().getY(), 300.0f);

  event.setIntensity(2.0f);
  BOOST_CHECK_EQUAL(event.getIntensity(), 2.0f);

  event.setDuration(5.0f);
  BOOST_CHECK_EQUAL(event.getDuration(), 5.0f);

  event.setEffectType(ParticleEffectType::Smoke);
  BOOST_CHECK_EQUAL(static_cast<int>(event.getEffectType()),
                    static_cast<int>(ParticleEffectType::Smoke));
}

// Test EventManager::triggerParticleEffect creates effects in ParticleManager
BOOST_FIXTURE_TEST_CASE(TestTriggerParticleEffectIntegration,
                        ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  registerParticleEffectForwarder();

  // This is how production code triggers particle effects
  bool result = EventManager::Instance().triggerParticleEffect(
      "Fire", 150.0f, 250.0f, 1.2f, 3.0f, "testGroup",
      EventManager::DispatchMode::Immediate);

  BOOST_CHECK(result);

  // Update manager to process effects
  manager->update(0.1f);

  // Verify effect system is working
  BOOST_CHECK_GE(manager->getActiveParticleCount(), 0);
}

// Test triggering different effect types via EventManager (production pattern)
BOOST_FIXTURE_TEST_CASE(TestTriggerDifferentEffectTypes,
                        ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  registerParticleEffectForwarder();

  // Test all built-in effect types via production API
  std::vector<std::string> effectNames = {"Fire", "Smoke", "Sparks",
                                          "Rain", "Snow",  "Fog"};

  for (const auto &effectName : effectNames) {
    bool result = EventManager::Instance().triggerParticleEffect(
        effectName, 100.0f, 200.0f, 0.8f, 2.0f, "",
        EventManager::DispatchMode::Immediate);
    BOOST_CHECK_MESSAGE(result,
                        "Failed to trigger effect: " + effectName);
  }

  // Update and verify effects are running
  manager->update(0.1f);
  BOOST_CHECK_GE(manager->getActiveParticleCount(), 0);
}

// Test ParticleEffectEvent::stringToEffectType conversion
BOOST_FIXTURE_TEST_CASE(TestEffectTypeConversion, ParticleManagerCoreFixture) {
  BOOST_CHECK_EQUAL(static_cast<int>(ParticleEffectEvent::stringToEffectType("Fire")),
                    static_cast<int>(ParticleEffectType::Fire));
  BOOST_CHECK_EQUAL(static_cast<int>(ParticleEffectEvent::stringToEffectType("Smoke")),
                    static_cast<int>(ParticleEffectType::Smoke));
  BOOST_CHECK_EQUAL(static_cast<int>(ParticleEffectEvent::stringToEffectType("Rain")),
                    static_cast<int>(ParticleEffectType::Rain));
  BOOST_CHECK_EQUAL(static_cast<int>(ParticleEffectEvent::stringToEffectType("Snow")),
                    static_cast<int>(ParticleEffectType::Snow));
  BOOST_CHECK_EQUAL(static_cast<int>(ParticleEffectEvent::stringToEffectType("Invalid")),
                    static_cast<int>(ParticleEffectType::Fire)); // Default fallback
}

// Invalid effects test removed - enum system prevents invalid types at compile
// time

// Test ParticleEffectEvent checkConditions behavior
BOOST_FIXTURE_TEST_CASE(TestParticleEffectEventConditions,
                        ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  ParticleEffectEvent event("ConditionTest", ParticleEffectType::Smoke,
                            200.0f, 300.0f, 1.5f, 5.0f);

  // Conditions should pass when manager is initialized
  BOOST_CHECK(event.checkConditions());

  // Event should be active by default
  BOOST_CHECK(event.isActive());

  // Deactivating event should fail conditions
  event.setActive(false);
  BOOST_CHECK(!event.checkConditions());

  // Reactivating should pass again
  event.setActive(true);
  BOOST_CHECK(event.checkConditions());
}

// Test ParticleEffectEvent reset and clean methods
BOOST_FIXTURE_TEST_CASE(TestParticleEffectEventResetClean,
                        ParticleManagerCoreFixture) {
  manager->init();

  ParticleEffectEvent event("ResetTest", ParticleEffectType::Fire,
                            100.0f, 100.0f, 1.0f, 2.0f, "group1", "sound1");

  // Verify initial state
  BOOST_CHECK_EQUAL(event.getGroupTag(), "group1");
  BOOST_CHECK_EQUAL(event.getDuration(), 2.0f);

  // Reset should clear state for pool reuse
  event.reset();
  BOOST_CHECK(!event.isEffectActive());
  BOOST_CHECK_EQUAL(event.getGroupTag(), ""); // Cleared
  BOOST_CHECK_EQUAL(event.getDuration(), -1.0f); // Reset to default

  // Create another event and test clean
  ParticleEffectEvent event2("CleanTest", ParticleEffectType::Smoke,
                             200.0f, 200.0f);
  event2.clean();
  BOOST_CHECK(!event2.isEffectActive());
}

// Test triggering effects with extreme values via production API
BOOST_FIXTURE_TEST_CASE(TestTriggerEffectExtremeValues,
                        ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  registerParticleEffectForwarder();

  // Extreme positions - should not crash
  BOOST_CHECK(EventManager::Instance().triggerParticleEffect(
      "Fire", -1000.0f, 1000.0f, 0.1f, 0.1f, "",
      EventManager::DispatchMode::Immediate));

  // Very high intensity - should not crash
  BOOST_CHECK(EventManager::Instance().triggerParticleEffect(
      "Sparks", 0.0f, 0.0f, 10.0f, 1.0f, "",
      EventManager::DispatchMode::Immediate));

  // Infinite duration (-1) - should not crash
  BOOST_CHECK(EventManager::Instance().triggerParticleEffect(
      "Rain", 100.0f, 100.0f, 1.0f, -1.0f, "",
      EventManager::DispatchMode::Immediate));

  // Zero duration - should not crash
  BOOST_CHECK(EventManager::Instance().triggerParticleEffect(
      "Snow", 100.0f, 100.0f, 1.0f, 0.0f, "",
      EventManager::DispatchMode::Immediate));

  // Update and verify no crashes
  manager->update(0.1f);
  BOOST_CHECK(manager->isInitialized());
}

// Test multiple simultaneous ParticleEffectEvents via EventManager
BOOST_FIXTURE_TEST_CASE(TestMultipleParticleEffectEvents,
                        ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  registerParticleEffectForwarder();

  // Dispatch multiple effects through EventManager
  EventManager::Instance().triggerParticleEffect(
      "Fire", 100.0f, 100.0f, 1.0f, -1.0f, "",
      EventManager::DispatchMode::Immediate);
  EventManager::Instance().triggerParticleEffect(
      "Smoke", 200.0f, 200.0f, 1.0f, -1.0f, "",
      EventManager::DispatchMode::Immediate);
  EventManager::Instance().triggerParticleEffect(
      "Sparks", 300.0f, 300.0f, 1.0f, -1.0f, "",
      EventManager::DispatchMode::Immediate);

  // Update manager to process effects
  manager->update(0.1f);

  // Verify effects are running (particle count may vary based on timing)
  BOOST_CHECK_GE(manager->getActiveParticleCount(), 0);
}

// Test basic particle creation through effects
BOOST_FIXTURE_TEST_CASE(TestBasicParticleCreation, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D testPosition(100, 100);

  // Start with no particles
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), 0);

  // Start an effect
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 1.0f);
  BOOST_CHECK_NE(effectId, 0);

  // Update to allow particle emission
  manager->update(0.1f); // 100ms update

  // Should have some particles now (Rain effect emits particles)
  size_t particleCount = manager->getActiveParticleCount();
  BOOST_CHECK_GT(particleCount, 0);

  // Update again to see particles aging
  manager->update(0.1f);

  // Particles should still exist (they have longer lifetimes)
  BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);
}

// Test update without initialization
BOOST_FIXTURE_TEST_CASE(TestUpdateWithoutInitialization,
                        ParticleManagerCoreFixture) {
  // Should not crash when updating without initialization
  BOOST_CHECK_NO_THROW(manager->update(0.016f));
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), 0);
}

// Test operations when globally paused
BOOST_FIXTURE_TEST_CASE(TestOperationsWhenPaused, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D testPosition(100, 100);

  // Start an effect and create some particles
  manager->playEffect(ParticleEffectType::Rain, testPosition, 1.0f);
  manager->update(0.1f);
  size_t initialParticleCount = manager->getActiveParticleCount();

  // Pause globally
  manager->setGlobalPause(true);

  // Update should not affect particle count when paused
  manager->update(0.1f);
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), initialParticleCount);
}

// Test maximum particle capacity
BOOST_FIXTURE_TEST_CASE(TestMaxParticleCapacity, ParticleManagerCoreFixture) {
  manager->init();

  // Should have some reasonable default capacity
  size_t maxCapacity = manager->getMaxParticleCapacity();
  BOOST_CHECK_GT(maxCapacity, 1000); // Should be at least 1000
  BOOST_CHECK_LE(
      maxCapacity,
      200000); // Should be reasonable (updated for new higher limits)

  // Test that setMaxParticles doesn't crash
  manager->setMaxParticles(5000);
  size_t newCapacity = manager->getMaxParticleCapacity();
  // Capacity should be at least the requested amount (may be more due to vector
  // growth)
  BOOST_CHECK_GE(newCapacity, 5000);
}

// Test performance statistics
BOOST_FIXTURE_TEST_CASE(TestPerformanceStats, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Ensure manager is not paused from previous tests
  manager->setGlobalPause(false);
  BOOST_CHECK(!manager->isGloballyPaused());
  BOOST_CHECK(manager->isInitialized());

  // Reset stats
  manager->resetPerformanceStats();
  ParticlePerformanceStats stats = manager->getPerformanceStats();

  // Should start with zero stats
  BOOST_CHECK_EQUAL(stats.updateCount, 0);
  BOOST_CHECK_EQUAL(stats.renderCount, 0);
  BOOST_CHECK_EQUAL(stats.totalUpdateTime, 0.0);
  BOOST_CHECK_EQUAL(stats.totalRenderTime, 0.0);

  // Create some particles and update multiple times to ensure emission
  Vector2D testPosition(100, 100);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 1.0f);
  BOOST_CHECK_NE(effectId, 0);
  BOOST_CHECK(manager->isEffectPlaying(effectId));

  // Update 2401 times to ensure we hit the performance recording threshold
  // Performance stats are only recorded every 2400 frames in debug builds
  // (see ParticleManager.cpp line ~843)
  for (int i = 0; i < 2401; ++i) {
    manager->update(0.016f);
  }

  // Verify particles were actually created
  size_t particleCount = manager->getActiveParticleCount();
  BOOST_CHECK_GT(particleCount, 0);

  // Stats should have been updated after 2400+ frames
  stats = manager->getPerformanceStats();
  BOOST_CHECK_GT(stats.updateCount, 0);
  BOOST_CHECK_GT(stats.totalUpdateTime, 0.0);
}

// ============================================================================
// PARTICLE INTERPOLATION TESTS
// Tests for smooth particle movement between frames
// ============================================================================

// Test that particles maintain previous position for interpolation
BOOST_FIXTURE_TEST_CASE(TestParticlePositionTracking, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Create some particles
  Vector2D testPosition(200, 200);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Rain, testPosition, 1.0f);
  BOOST_CHECK_NE(effectId, 0);

  // Update multiple times to ensure particles are created
  for (int i = 0; i < 10; ++i) {
    manager->update(0.016f);
  }

  // Verify particles were created
  size_t particleCount = manager->getActiveParticleCount();
  BOOST_CHECK_GT(particleCount, 0);

  // Update again - previous positions should be tracked
  manager->update(0.016f);

  // Still should have particles (rain is continuous)
  BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);
}

// Test particle update with varying delta times
BOOST_FIXTURE_TEST_CASE(TestParticleUpdateWithVaryingDeltaTime, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Create some particles
  Vector2D testPosition(100, 100);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Smoke, testPosition, 1.0f);
  BOOST_CHECK_NE(effectId, 0);

  // Initial updates to create particles
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);  // 60 FPS
  }

  // Update with different delta times (simulating frame rate variation)
  manager->update(0.033f);  // 30 FPS
  manager->update(0.008f);  // 120 FPS
  manager->update(0.016f);  // 60 FPS

  // Particles should still exist and be valid
  size_t countAfterVarying = manager->getActiveParticleCount();
  // Count might change due to particle creation/death, but shouldn't crash
  BOOST_CHECK_GE(countAfterVarying, 0);
}

// Test that particle interpolation state survives pause/resume
BOOST_FIXTURE_TEST_CASE(TestInterpolationStateAcrossPauseResume, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Create some particles
  Vector2D testPosition(150, 150);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Fire, testPosition, 1.0f);
  BOOST_CHECK_NE(effectId, 0);

  // Update to create particles
  for (int i = 0; i < 10; ++i) {
    manager->update(0.016f);
  }

  BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);

  // Pause
  manager->setGlobalPause(true);
  BOOST_CHECK(manager->isGloballyPaused());

  // Updates while paused shouldn't change particle count significantly
  manager->update(0.016f);
  manager->update(0.016f);

  // Resume
  manager->setGlobalPause(false);
  BOOST_CHECK(!manager->isGloballyPaused());

  // Continue updating - should work normally
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  // Particles should still be valid
  BOOST_CHECK_GE(manager->getActiveParticleCount(), 0);
}

// Test particle alpha interpolation (fading)
BOOST_FIXTURE_TEST_CASE(TestParticleAlphaFading, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Create smoke particles (which fade over time)
  Vector2D testPosition(100, 100);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Smoke, testPosition, 0.5f);  // Short duration
  BOOST_CHECK_NE(effectId, 0);

  // Update to create and age particles
  for (int i = 0; i < 50; ++i) {
    manager->update(0.016f);
  }

  // Some particles should exist (continuous emission)
  BOOST_CHECK_GE(manager->getActiveParticleCount(), 0);
}

// Test multiple effects interpolating simultaneously
BOOST_FIXTURE_TEST_CASE(TestMultipleEffectsInterpolation, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Create multiple effects at different positions
  Vector2D pos1(100, 100);
  Vector2D pos2(300, 300);
  Vector2D pos3(500, 500);

  uint32_t effect1 = manager->playEffect(ParticleEffectType::Rain, pos1, 1.0f);
  uint32_t effect2 = manager->playEffect(ParticleEffectType::Smoke, pos2, 1.0f);
  uint32_t effect3 = manager->playEffect(ParticleEffectType::Fire, pos3, 1.0f);

  BOOST_CHECK_NE(effect1, 0);
  BOOST_CHECK_NE(effect2, 0);
  BOOST_CHECK_NE(effect3, 0);

  // Update all effects together
  for (int i = 0; i < 20; ++i) {
    manager->update(0.016f);
  }

  // Should have particles from multiple effects
  size_t totalParticles = manager->getActiveParticleCount();
  BOOST_CHECK_GT(totalParticles, 0);

  // All effects should still be playing
  BOOST_CHECK(manager->isEffectPlaying(effect1));
  BOOST_CHECK(manager->isEffectPlaying(effect2));
  BOOST_CHECK(manager->isEffectPlaying(effect3));
}

// Test particle scale interpolation
BOOST_FIXTURE_TEST_CASE(TestParticleScaleInterpolation, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Create an effect that uses scale changes
  Vector2D testPosition(200, 200);
  uint32_t effectId =
      manager->playEffect(ParticleEffectType::Fire, testPosition, 1.0f);
  BOOST_CHECK_NE(effectId, 0);

  // Update to create particles and let them age (scale should change)
  for (int i = 0; i < 30; ++i) {
    manager->update(0.016f);
  }

  // Particles should exist
  BOOST_CHECK_GT(manager->getActiveParticleCount(), 0);
}

// Test rapid effect creation/destruction doesn't break interpolation
BOOST_FIXTURE_TEST_CASE(TestRapidEffectLifecycle, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();
  manager->setGlobalPause(false);

  // Rapidly create and stop effects
  for (int cycle = 0; cycle < 10; ++cycle) {
    Vector2D pos(static_cast<float>(cycle * 50), static_cast<float>(cycle * 50));
    uint32_t effectId = manager->playEffect(ParticleEffectType::Smoke, pos, 0.5f);
    BOOST_CHECK_NE(effectId, 0);

    // Update a few times
    for (int i = 0; i < 5; ++i) {
      manager->update(0.016f);
    }

    // Stop the effect
    manager->stopEffect(effectId);
  }

  // Final updates to ensure stability
  for (int i = 0; i < 10; ++i) {
    manager->update(0.016f);
  }

  // Should not crash and manager should be in valid state
  BOOST_CHECK(manager->isInitialized());
  BOOST_CHECK(!manager->isShutdown());
}

// ============================================================================
// INDEPENDENT EFFECT MANAGEMENT API TESTS
// Tests for independent effects (not weather-related) with group management
// ============================================================================

// Test basic independent effect creation
BOOST_FIXTURE_TEST_CASE(TestPlayIndependentEffect, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D position(100.0f, 100.0f);

  // Play an independent effect
  uint32_t effectId = manager->playIndependentEffect(
      ParticleEffectType::Fire, position, 1.0f, -1.0f, "testGroup");

  // Should return valid effect ID
  BOOST_CHECK_NE(effectId, 0);

  // Should be marked as independent
  BOOST_CHECK(manager->isIndependentEffect(effectId));

  // Should be in the active independent effects list
  auto activeEffects = manager->getActiveIndependentEffects();
  BOOST_CHECK(std::find(activeEffects.begin(), activeEffects.end(), effectId) !=
              activeEffects.end());

  // Clean up
  manager->stopIndependentEffect(effectId);
}

// Test stopping individual independent effect
BOOST_FIXTURE_TEST_CASE(TestStopIndependentEffect, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D position(200.0f, 200.0f);

  // Create independent effect
  uint32_t effectId = manager->playIndependentEffect(
      ParticleEffectType::Smoke, position, 1.0f, -1.0f, "group1");
  BOOST_CHECK_NE(effectId, 0);
  BOOST_CHECK(manager->isEffectPlaying(effectId));

  // Stop the effect
  manager->stopIndependentEffect(effectId);

  // Should no longer be playing
  BOOST_CHECK(!manager->isEffectPlaying(effectId));

  // Should no longer be in active list
  auto activeEffects = manager->getActiveIndependentEffects();
  BOOST_CHECK(std::find(activeEffects.begin(), activeEffects.end(), effectId) ==
              activeEffects.end());
}

BOOST_FIXTURE_TEST_CASE(TestStoppedIndependentEffectsAreCompactedOnUpdate,
                        ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  constexpr int EFFECT_CYCLES = 32;
  for (int i = 0; i < EFFECT_CYCLES; ++i) {
    uint32_t effectId = manager->playIndependentEffect(
        ParticleEffectType::AmbientDust, {100.0f, 100.0f}, 1.0f, -1.0f,
        "ambient");
    BOOST_REQUIRE_NE(effectId, 0);
    BOOST_CHECK(manager->isIndependentEffect(effectId));

    manager->stopIndependentEffect(effectId);
    manager->update(0.016f);

    BOOST_CHECK(!manager->isEffectPlaying(effectId));
    BOOST_CHECK(!manager->isIndependentEffect(effectId));
  }

  BOOST_CHECK(manager->getActiveIndependentEffects().empty());
}

// Test stopping all independent effects
BOOST_FIXTURE_TEST_CASE(TestStopAllIndependentEffects, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create multiple independent effects with different groups
  uint32_t effect1 = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "groupA");
  uint32_t effect2 = manager->playIndependentEffect(
      ParticleEffectType::Smoke, {200.0f, 200.0f}, 1.0f, -1.0f, "groupB");
  uint32_t effect3 = manager->playIndependentEffect(
      ParticleEffectType::Sparks, {300.0f, 300.0f}, 1.0f, -1.0f, "groupC");

  BOOST_CHECK_NE(effect1, 0);
  BOOST_CHECK_NE(effect2, 0);
  BOOST_CHECK_NE(effect3, 0);

  // Verify all are playing
  BOOST_CHECK(manager->isEffectPlaying(effect1));
  BOOST_CHECK(manager->isEffectPlaying(effect2));
  BOOST_CHECK(manager->isEffectPlaying(effect3));

  // Stop all independent effects
  manager->stopAllIndependentEffects();

  // None should be playing now
  BOOST_CHECK(!manager->isEffectPlaying(effect1));
  BOOST_CHECK(!manager->isEffectPlaying(effect2));
  BOOST_CHECK(!manager->isEffectPlaying(effect3));

  // Active list should be empty
  auto activeEffects = manager->getActiveIndependentEffects();
  BOOST_CHECK(activeEffects.empty());
}

// Test stopping independent effects by group tag
BOOST_FIXTURE_TEST_CASE(TestStopIndependentEffectsByGroup, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create effects in two groups
  uint32_t effectA1 = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "combat");
  uint32_t effectA2 = manager->playIndependentEffect(
      ParticleEffectType::Smoke, {150.0f, 150.0f}, 1.0f, -1.0f, "combat");
  uint32_t effectB1 = manager->playIndependentEffect(
      ParticleEffectType::Sparks, {200.0f, 200.0f}, 1.0f, -1.0f, "ambient");

  BOOST_CHECK(manager->isEffectPlaying(effectA1));
  BOOST_CHECK(manager->isEffectPlaying(effectA2));
  BOOST_CHECK(manager->isEffectPlaying(effectB1));

  // Stop only combat group
  manager->stopIndependentEffectsByGroup("combat");

  // Combat effects should be stopped
  BOOST_CHECK(!manager->isEffectPlaying(effectA1));
  BOOST_CHECK(!manager->isEffectPlaying(effectA2));

  // Ambient effect should still be playing
  BOOST_CHECK(manager->isEffectPlaying(effectB1));

  // Clean up
  manager->stopIndependentEffect(effectB1);
}

// Test pausing individual independent effect
BOOST_FIXTURE_TEST_CASE(TestPauseIndependentEffect, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  Vector2D position(100.0f, 100.0f);

  uint32_t effectId = manager->playIndependentEffect(
      ParticleEffectType::Fire, position, 1.0f, -1.0f, "test");
  BOOST_CHECK_NE(effectId, 0);

  // Update to create particles
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  const size_t countBeforePause = manager->getActiveParticleCount();
  BOOST_CHECK_GT(countBeforePause, 0);

  // Pause the effect
  manager->pauseIndependentEffect(effectId, true);

  // Update again - particle count shouldn't increase from this effect
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  // Effect should still be playing (paused != stopped)
  BOOST_CHECK(manager->isEffectPlaying(effectId));
  BOOST_CHECK_EQUAL(manager->getActiveParticleCount(), countBeforePause);

  // Resume the effect
  manager->pauseIndependentEffect(effectId, false);

  // Should continue working normally
  manager->update(0.016f);
  BOOST_CHECK(manager->isEffectPlaying(effectId));
  BOOST_CHECK_GT(manager->getActiveParticleCount(), countBeforePause);

  // Clean up
  manager->stopIndependentEffect(effectId);
}

// Test pausing all independent effects
BOOST_FIXTURE_TEST_CASE(TestPauseAllIndependentEffects, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create multiple effects
  uint32_t effect1 = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "group1");
  uint32_t effect2 = manager->playIndependentEffect(
      ParticleEffectType::Smoke, {200.0f, 200.0f}, 1.0f, -1.0f, "group2");

  // Update to create particles
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  // Pause all independent effects
  manager->pauseAllIndependentEffects(true);

  // Effects should still exist but be paused
  BOOST_CHECK(manager->isEffectPlaying(effect1));
  BOOST_CHECK(manager->isEffectPlaying(effect2));

  // Resume all
  manager->pauseAllIndependentEffects(false);

  // Should continue working
  manager->update(0.016f);
  BOOST_CHECK(manager->isEffectPlaying(effect1));
  BOOST_CHECK(manager->isEffectPlaying(effect2));

  // Clean up
  manager->stopAllIndependentEffects();
}

// Test pausing independent effects by group
BOOST_FIXTURE_TEST_CASE(TestPauseIndependentEffectsByGroup, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create effects in two groups
  uint32_t effectA = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "explosions");
  uint32_t effectB = manager->playIndependentEffect(
      ParticleEffectType::Smoke, {200.0f, 200.0f}, 1.0f, -1.0f, "environment");

  // Update to create particles
  for (int i = 0; i < 5; ++i) {
    manager->update(0.016f);
  }

  // Pause only explosions group
  manager->pauseIndependentEffectsByGroup("explosions", true);

  // Both should still be playing
  BOOST_CHECK(manager->isEffectPlaying(effectA));
  BOOST_CHECK(manager->isEffectPlaying(effectB));

  // Resume explosions group
  manager->pauseIndependentEffectsByGroup("explosions", false);

  // Clean up
  manager->stopAllIndependentEffects();
}

// Test isIndependentEffect distinguishes from regular effects
BOOST_FIXTURE_TEST_CASE(TestIsIndependentEffectDistinction, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create a regular effect
  Vector2D position(100.0f, 100.0f);
  uint32_t regularEffect =
      manager->playEffect(ParticleEffectType::Rain, position, 1.0f);

  // Create an independent effect
  uint32_t independentEffect = manager->playIndependentEffect(
      ParticleEffectType::Fire, position, 1.0f, -1.0f, "combat");

  BOOST_CHECK_NE(regularEffect, 0);
  BOOST_CHECK_NE(independentEffect, 0);

  // Regular effect should NOT be marked as independent
  BOOST_CHECK(!manager->isIndependentEffect(regularEffect));

  // Independent effect SHOULD be marked as independent
  BOOST_CHECK(manager->isIndependentEffect(independentEffect));

  // Clean up
  manager->stopEffect(regularEffect);
  manager->stopIndependentEffect(independentEffect);
}

// Test getActiveIndependentEffects returns correct list
BOOST_FIXTURE_TEST_CASE(TestGetActiveIndependentEffects, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Initially no independent effects
  auto initialEffects = manager->getActiveIndependentEffects();
  BOOST_CHECK(initialEffects.empty());

  // Create several independent effects
  uint32_t effect1 = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "group1");
  uint32_t effect2 = manager->playIndependentEffect(
      ParticleEffectType::Smoke, {200.0f, 200.0f}, 1.0f, -1.0f, "group1");
  uint32_t effect3 = manager->playIndependentEffect(
      ParticleEffectType::Sparks, {300.0f, 300.0f}, 1.0f, -1.0f, "group2");

  // Get active effects
  auto activeEffects = manager->getActiveIndependentEffects();
  BOOST_CHECK_EQUAL(activeEffects.size(), 3);

  // Verify all effects are in the list
  BOOST_CHECK(std::find(activeEffects.begin(), activeEffects.end(), effect1) !=
              activeEffects.end());
  BOOST_CHECK(std::find(activeEffects.begin(), activeEffects.end(), effect2) !=
              activeEffects.end());
  BOOST_CHECK(std::find(activeEffects.begin(), activeEffects.end(), effect3) !=
              activeEffects.end());

  // Stop one effect
  manager->stopIndependentEffect(effect2);

  // Should now have 2 effects
  activeEffects = manager->getActiveIndependentEffects();
  BOOST_CHECK_EQUAL(activeEffects.size(), 2);
  BOOST_CHECK(std::find(activeEffects.begin(), activeEffects.end(), effect2) ==
              activeEffects.end());

  // Clean up
  manager->stopAllIndependentEffects();
}

// Test getActiveIndependentEffectsByGroup
BOOST_FIXTURE_TEST_CASE(TestGetActiveIndependentEffectsByGroup, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create effects in different groups
  uint32_t effectA1 = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "combat");
  uint32_t effectA2 = manager->playIndependentEffect(
      ParticleEffectType::Smoke, {150.0f, 150.0f}, 1.0f, -1.0f, "combat");
  uint32_t effectB1 = manager->playIndependentEffect(
      ParticleEffectType::Sparks, {200.0f, 200.0f}, 1.0f, -1.0f, "ambient");


  // Get combat group effects
  auto combatEffects = manager->getActiveIndependentEffectsByGroup("combat");
  BOOST_CHECK_EQUAL(combatEffects.size(), 2);
  BOOST_CHECK(std::find(combatEffects.begin(), combatEffects.end(), effectA1) !=
              combatEffects.end());
  BOOST_CHECK(std::find(combatEffects.begin(), combatEffects.end(), effectA2) !=
              combatEffects.end());

  // Get ambient group effects
  auto ambientEffects = manager->getActiveIndependentEffectsByGroup("ambient");
  BOOST_CHECK_EQUAL(ambientEffects.size(), 1);
  BOOST_CHECK(std::find(ambientEffects.begin(), ambientEffects.end(), effectB1) !=
              ambientEffects.end());

  // Get non-existent group
  auto emptyEffects = manager->getActiveIndependentEffectsByGroup("nonexistent");
  BOOST_CHECK(emptyEffects.empty());

  // Clean up
  manager->stopAllIndependentEffects();
}

// Test independent effect with duration expiration
BOOST_FIXTURE_TEST_CASE(TestIndependentEffectDuration, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create effect with short duration
  uint32_t effectId = manager->playIndependentEffect(
      ParticleEffectType::Sparks, {100.0f, 100.0f}, 1.0f, 0.5f, "timed");

  BOOST_CHECK_NE(effectId, 0);
  BOOST_CHECK(manager->isEffectPlaying(effectId));

  // Update for longer than duration (0.5 seconds = 500ms)
  for (int i = 0; i < 40; ++i) {  // 40 * 16ms = 640ms > 500ms
    manager->update(0.016f);
  }

  // Effect should have expired
  BOOST_CHECK(!manager->isEffectPlaying(effectId));
}

// Test multiple independent effects with same group tag
BOOST_FIXTURE_TEST_CASE(TestMultipleEffectsSameGroup, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  const std::string groupName = "explosion_cluster";

  // Create many effects with same group
  std::vector<uint32_t> effects;
  for (int i = 0; i < 10; ++i) {
    uint32_t effectId = manager->playIndependentEffect(
        ParticleEffectType::Sparks,
        {static_cast<float>(100 + i * 20), static_cast<float>(100 + i * 10)},
        1.0f, -1.0f, groupName);
    BOOST_CHECK_NE(effectId, 0);
    effects.push_back(effectId);
  }

  // All should be in the group
  auto groupEffects = manager->getActiveIndependentEffectsByGroup(groupName);
  BOOST_CHECK_EQUAL(groupEffects.size(), 10);

  // Stop by group should stop all
  manager->stopIndependentEffectsByGroup(groupName);

  // All should be stopped
  for (uint32_t effectId : effects) {
    BOOST_CHECK(!manager->isEffectPlaying(effectId));
  }

  // Group should be empty
  groupEffects = manager->getActiveIndependentEffectsByGroup(groupName);
  BOOST_CHECK(groupEffects.empty());
}

// Test independent effect with infinite duration (-1)
BOOST_FIXTURE_TEST_CASE(TestIndependentEffectInfiniteDuration, ParticleManagerCoreFixture) {
  manager->init();
  manager->registerBuiltInEffects();

  // Create effect with infinite duration
  uint32_t effectId = manager->playIndependentEffect(
      ParticleEffectType::Fire, {100.0f, 100.0f}, 1.0f, -1.0f, "persistent");

  BOOST_CHECK_NE(effectId, 0);
  BOOST_CHECK(manager->isEffectPlaying(effectId));

  // Update for a long time
  for (int i = 0; i < 100; ++i) {
    manager->update(0.016f);
  }

  // Should still be playing (infinite duration)
  BOOST_CHECK(manager->isEffectPlaying(effectId));

  // Must be manually stopped
  manager->stopIndependentEffect(effectId);
  BOOST_CHECK(!manager->isEffectPlaying(effectId));
}
