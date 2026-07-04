/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE EventManagerTest
// BOOST_TEST_NO_SIGNAL_HANDLING is already defined on the command line
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "entities/EntityHandle.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "events/CameraEvent.hpp"
#include "events/CollisionObstacleChangedEvent.hpp"
#include "events/EntityEvents.hpp"
#include "events/Event.hpp"
#include "events/MerchantSpawnEvent.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "events/WorldEvent.hpp"
#include "events/WorldTriggerEvent.hpp"
#include "collisions/TriggerTag.hpp"
#include "managers/EventManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "utils/ResourceHandle.hpp"
#include "EventManagerTestAccess.hpp"

// Test handle for ResourceChangeEvent tests - no real entity needed
static const EntityHandle TEST_PLAYER_HANDLE{1, EntityKind::Player, 1};

// Mock Event class for testing
class MockEvent : public Event {
public:
  MockEvent(const std::string &name)
      : m_name(name), m_executed(false), m_conditionsMet(false) {}

  void update() override { m_updated = true; }

  void execute() override { m_executed = true; }

  void reset() override {
    m_updated = false;
    m_executed = false;
    m_conditionsMet = false;
  }

  void clean() override {}

  std::string getName() const override { return m_name; }
  std::string getType() const override { return "Mock"; }
  std::string getTypeName() const override { return "MockEvent"; }
  EventTypeId getTypeId() const override { return EventTypeId::Custom; }

  bool checkConditions() override { return m_conditionsMet; }

  void setConditionsMet(bool met) { m_conditionsMet = met; }
  bool wasExecuted() const { return m_executed; }
  bool wasUpdated() const { return m_updated; }

private:
  std::string m_name;
  bool m_executed{false};
  bool m_updated{false};
  bool m_conditionsMet{false};
};

// Global fixture to initialize ThreadSystem and EntityDataManager once for all tests
struct GlobalEventTestFixture {
  GlobalEventTestFixture() {
    // Initialize ThreadSystem once for all tests
    if (!VoidLight::ThreadSystem::Exists()) {
      if (!VoidLight::ThreadSystem::Instance().init()) {
        throw std::runtime_error("ThreadSystem::init() failed");
      }
    }
    // Initialize EntityDataManager (required for Player entity creation in DOD)
    if (!EntityDataManager::Instance().init()) {
      throw std::runtime_error("EntityDataManager::init() failed");
    }
    // Ensure benchmark mode is disabled for regular tests
    VOIDLIGHT_DISABLE_BENCHMARK_MODE();
  }

  ~GlobalEventTestFixture() {
    // Clean up EntityDataManager
    EntityDataManager::Instance().clean();
    // Clean up ThreadSystem at the very end
    if (VoidLight::ThreadSystem::Exists()) {
      VoidLight::ThreadSystem::Instance().clean();
    }
  }
};

BOOST_GLOBAL_FIXTURE(GlobalEventTestFixture);

struct EventManagerFixture {
  EventManagerFixture() {
    // Don't reinitialize ThreadSystem - use the global one
    EventManagerTestAccess::reset();
  }

  ~EventManagerFixture() {
    // Disable threading before cleanup
    VOIDLIGHT_DEBUG_ONLY(EventManager::Instance().enableThreading(false);)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Clean up the EventManager
    EventManager::Instance().clean();
  }
};

// ==================== Basic Initialization Tests ====================

BOOST_FIXTURE_TEST_CASE(InitAndClean, EventManagerFixture) {
  BOOST_REQUIRE(EventManager::Instance().init());
  BOOST_CHECK(EventManager::Instance().isInitialized());
  EventManager::Instance().clean();
}

// ==================== Dispatch Architecture Tests ====================

BOOST_FIXTURE_TEST_CASE(DispatchEvent_WithHandler_CallsHandler, EventManagerFixture) {
  auto mockEvent = std::make_shared<MockEvent>("TestEvent");

  std::atomic<bool> handlerCalled{false};
  auto token = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&handlerCalled](const EventData &data) {
        if (data.isActive()) handlerCalled.store(true);
      });

  // Dispatch the event
  BOOST_CHECK(EventManager::Instance().dispatchEvent(mockEvent));
  EventManager::Instance().update(); // Process deferred events

  BOOST_CHECK(handlerCalled.load());
  EventManager::Instance().removeHandler(token);
}

BOOST_FIXTURE_TEST_CASE(DispatchEvent_ImmediateMode_CallsHandlerSynchronously, EventManagerFixture) {
  auto mockEvent = std::make_shared<MockEvent>("TestEvent");

  std::atomic<bool> handlerCalled{false};
  auto token = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&handlerCalled](const EventData &data) {
        if (data.isActive()) handlerCalled.store(true);
      });

  // Dispatch with Immediate mode - should call handler before returning
  EventManager::Instance().dispatchEvent(mockEvent, EventManager::DispatchMode::Immediate);

  // Handler should already be called (no update() needed)
  BOOST_CHECK(handlerCalled.load());
  EventManager::Instance().removeHandler(token);
}

BOOST_FIXTURE_TEST_CASE(DispatchEvent_DeferredMode_RequiresUpdate, EventManagerFixture) {
  auto mockEvent = std::make_shared<MockEvent>("TestEvent");

  std::atomic<bool> handlerCalled{false};
  auto token = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&handlerCalled](const EventData &data) {
        if (data.isActive()) handlerCalled.store(true);
      });

  // Dispatch with Deferred mode (default)
  EventManager::Instance().dispatchEvent(mockEvent, EventManager::DispatchMode::Deferred);

  // Handler should NOT be called yet
  BOOST_CHECK(!handlerCalled.load());

  // Now process deferred events
  EventManager::Instance().update();

  // Handler should now be called
  BOOST_CHECK(handlerCalled.load());
  EventManager::Instance().removeHandler(token);
}

// ==================== Handler Registration Tests ====================

BOOST_FIXTURE_TEST_CASE(RegisterHandlerWithToken_CanBeRemoved, EventManagerFixture) {
  std::atomic<int> callCount{0};

  auto token = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom,
      [&callCount](const EventData &data) {
        if (data.isActive()) ++callCount;
      });

  // Dispatch once - handler should be called
  auto e1 = std::make_shared<MockEvent>("Test1");
  EventManager::Instance().dispatchEvent(e1, EventManager::DispatchMode::Immediate);
  BOOST_CHECK_EQUAL(callCount.load(), 1);

  // Remove handler
  EventManager::Instance().removeHandler(token);

  // Dispatch again - handler should NOT be called
  auto e2 = std::make_shared<MockEvent>("Test2");
  EventManager::Instance().dispatchEvent(e2, EventManager::DispatchMode::Immediate);
  BOOST_CHECK_EQUAL(callCount.load(), 1); // Still 1, not incremented
}

// ==================== Trigger Method Tests ====================

BOOST_FIXTURE_TEST_CASE(ChangeWeather_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> weatherHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Weather, [&weatherHandlerCalled](const EventData &data) {
        if (data.event) weatherHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().changeWeather("Rainy", 1.0f,
                                                    EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(weatherHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(SpawnNPC_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> npcHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::NPCSpawn, [&npcHandlerCalled](const EventData &data) {
        if (data.event) npcHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().spawnNPC("Guard", 10.0f, 20.0f, 1, 0.0f, "",
                                               {}, false,
                                               EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(npcHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(SpawnMerchant_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> merchantHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::MerchantSpawn, [&merchantHandlerCalled](const EventData &data) {
        if (std::dynamic_pointer_cast<MerchantSpawnEvent>(data.event)) {
          merchantHandlerCalled.store(true);
        }
      });

  bool ok = EventManager::Instance().spawnMerchant(
      "GeneralMerchant", 10.0f, 20.0f, "Human", 1, 0.0f, false,
      EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(merchantHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerParticleEffect_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> particleHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::ParticleEffect, [&particleHandlerCalled](const EventData &data) {
        if (data.event) particleHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerParticleEffect("Fire", 100.0f, 200.0f,
                                                            1.0f, -1.0f, "",
                                                            EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(particleHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerResourceChange_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> resourceHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::ResourceChange, [&resourceHandlerCalled](const EventData &data) {
        if (data.event) resourceHandlerCalled.store(true);
      });

  VoidLight::ResourceHandle testResource(1, 1);
  bool ok = EventManager::Instance().triggerResourceChange(
      TEST_PLAYER_HANDLE, testResource, 5, 10, "test",
      EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(resourceHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerWorldLoaded_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> worldHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::World, [&worldHandlerCalled](const EventData &data) {
        if (data.event) worldHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerWorldLoaded("test_world", 100, 100,
                                                         EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(worldHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerCameraMoved_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> cameraHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Camera, [&cameraHandlerCalled](const EventData &data) {
        if (data.event) cameraHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCameraMoved(
      Vector2D(100, 100), Vector2D(0, 0),
      EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(cameraHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

// ==================== Deferred Dispatch Tests ====================

BOOST_FIXTURE_TEST_CASE(DeferredDispatch_MultipleTriggers_ProcessedInUpdate, EventManagerFixture) {
  std::atomic<int> handlerCallCount{0};

  EventManager::Instance().registerHandler(
      EventTypeId::Weather,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });
  EventManager::Instance().registerHandler(
      EventTypeId::NPCSpawn,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });

  // Trigger multiple events with deferred dispatch (default)
  BOOST_CHECK(EventManager::Instance().changeWeather("Storm", 3.0f));
  BOOST_CHECK(EventManager::Instance().spawnNPC("Boss", 500.0f, 300.0f));

  // Events should be queued, handlers not called yet
  BOOST_CHECK_EQUAL(handlerCallCount.load(), 0);

  // Process deferred events
  EventManager::Instance().update();

  // Allow processing time
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Handlers should now be called
  BOOST_CHECK_GE(handlerCallCount.load(), 2);
}


BOOST_FIXTURE_TEST_CASE(GetPendingEventCount_TracksQueuedEvents, EventManagerFixture) {
  // Initially should have no pending events
  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 0);

  // Queue some deferred events
  EventManager::Instance().changeWeather("Rainy", 1.0f);
  EventManager::Instance().spawnNPC("Guard", 0, 0);

  // Should have pending events
  BOOST_CHECK_GT(EventManager::Instance().getPendingEventCount(), 0);

  // Process events
  EventManager::Instance().update();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Should have no pending events after processing
  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 0);
}

// ==================== Event Pool Recycling Tests ====================

BOOST_FIXTURE_TEST_CASE(EventPoolRecycling_WeatherEvents, EventManagerFixture) {
  EventPtr firstWeather = nullptr;
  EventPtr secondWeather = nullptr;
  EventManager::Instance().registerHandler(
      EventTypeId::Weather, [&](const EventData &data) {
        if (!firstWeather) firstWeather = data.event;
        else secondWeather = data.event;
      });

  EventManager::Instance().changeWeather("Storm", 1.0f, EventManager::DispatchMode::Immediate);
  EventManager::Instance().changeWeather("Clear", 1.0f, EventManager::DispatchMode::Immediate);

  BOOST_CHECK(firstWeather != nullptr);
  BOOST_CHECK(secondWeather != nullptr);
  BOOST_CHECK_EQUAL(firstWeather.get(), secondWeather.get()); // Same pooled event reused
}

BOOST_FIXTURE_TEST_CASE(EventPoolRecycling_NPCSpawnEvents, EventManagerFixture) {
  EventPtr firstNPC = nullptr;
  EventPtr secondNPC = nullptr;
  EventManager::Instance().registerHandler(
      EventTypeId::NPCSpawn, [&](const EventData &data) {
        if (!firstNPC) firstNPC = data.event;
        else secondNPC = data.event;
      });

  EventManager::Instance().spawnNPC("A", 0, 0, 1, 0, "", {}, false, EventManager::DispatchMode::Immediate);
  EventManager::Instance().spawnNPC("B", 0, 0, 1, 0, "", {}, false, EventManager::DispatchMode::Immediate);

  BOOST_CHECK_EQUAL(firstNPC.get(), secondNPC.get()); // Same pooled event reused
}

BOOST_FIXTURE_TEST_CASE(EventPoolRecycling_ParticleEffectEvents, EventManagerFixture) {
  EventPtr firstParticle = nullptr;
  EventPtr secondParticle = nullptr;
  EventManager::Instance().registerHandler(
      EventTypeId::ParticleEffect, [&](const EventData &data) {
        if (!firstParticle) firstParticle = data.event;
        else secondParticle = data.event;
      });

  EventManager::Instance().triggerParticleEffect("Fire", 0, 0, 1.0f, 1.0f, "",
                                                  EventManager::DispatchMode::Immediate);
  EventManager::Instance().triggerParticleEffect("Fire", 0, 0, 1.0f, 1.0f, "",
                                                  EventManager::DispatchMode::Immediate);

  BOOST_CHECK_EQUAL(firstParticle.get(), secondParticle.get()); // Same pooled event reused
}

// ==================== Thread Safety Tests ====================

BOOST_FIXTURE_TEST_CASE(ThreadSafety_ConcurrentTriggers, EventManagerFixture) {
  EventManager::Instance().clean();
  BOOST_REQUIRE(EventManager::Instance().init());

  std::atomic<int> handlerCallCount{0};

  EventManager::Instance().registerHandler(
      EventTypeId::ResourceChange,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });

  VoidLight::ResourceHandle testResource(1, 1);

  // Trigger multiple resource change events concurrently with immediate dispatch
  std::vector<std::thread> threads;
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([i, &testResource]() {
      EventManager::Instance().triggerResourceChange(
          TEST_PLAYER_HANDLE, testResource, i * 5, (i + 1) * 5, "concurrent_test",
          EventManager::DispatchMode::Immediate);
    });
  }

  // Wait for all threads
  for (auto &thread : threads) {
    thread.join();
  }

  // Allow a bit of time for any async operations to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify all events were processed
  BOOST_CHECK_GE(handlerCallCount.load(), 5);
}

// ==================== State Transition Tests ====================

BOOST_FIXTURE_TEST_CASE(StateTransitionPreparation_CleansUpProperly, EventManagerFixture) {
  EventManager::Instance().clean();
  BOOST_REQUIRE(EventManager::Instance().init());

  // Register a handler
  bool handlerCalled = false;
  EventManager::Instance().registerHandler(
      EventTypeId::Custom,
      [&handlerCalled](const EventData &) { handlerCalled = true; });

  // Test state transition preparation
  EventManager::Instance().prepareForStateTransition();

  // Verify manager is still functional after preparation
  BOOST_CHECK(EventManager::Instance().isInitialized());

  // Handlers should be cleared
  auto mockEvent = std::make_shared<MockEvent>("Test");
  EventManager::Instance().dispatchEvent(mockEvent, EventManager::DispatchMode::Immediate);
  BOOST_CHECK(!handlerCalled); // Handler was removed during transition prep
}

// ==================== Threading Control Tests (Debug Only) ====================

VOIDLIGHT_DEBUG_ONLY(
BOOST_FIXTURE_TEST_CASE(DynamicThreadingControl, EventManagerFixture) {
  EventManager::Instance().clean();
  BOOST_REQUIRE(EventManager::Instance().init());

  std::atomic<int> handlerCallCount{0};

  EventManager::Instance().registerHandler(
      EventTypeId::Weather,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });

  // Debug toggle should not affect correctness of serial deferred delivery.
  EventManager::Instance().enableThreading(false);

  // Trigger event with deferred dispatch
  BOOST_CHECK(EventManager::Instance().changeWeather("Clear", 1.0f));
  EventManager::Instance().update();

  int callsWithoutThreading = handlerCallCount.load();
  BOOST_CHECK_GE(callsWithoutThreading, 1);

  // Reset counter and enable threading
  handlerCallCount.store(0);
  EventManager::Instance().enableThreading(true);

  // Trigger another event
  BOOST_CHECK(EventManager::Instance().changeWeather("Rainy", 1.0f));
  EventManager::Instance().update();

  int callsWithThreading = handlerCallCount.load();
  BOOST_CHECK_GE(callsWithThreading, 1);

  EventManager::Instance().enableThreading(false);
}
)

// ==================== Additional Trigger Method Tests ====================

BOOST_FIXTURE_TEST_CASE(TriggerWorldUnloaded_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> worldHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::World, [&worldHandlerCalled](const EventData &data) {
        if (data.event) worldHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerWorldUnloaded("test_world",
                                                           EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(worldHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerTileChanged_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> worldHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::World, [&worldHandlerCalled](const EventData &data) {
        if (data.event) worldHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerTileChanged(10, 20, "biome_change",
                                                         EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(worldHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerWorldGenerated_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> worldHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::World, [&worldHandlerCalled](const EventData &data) {
        if (data.event) worldHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerWorldGenerated("new_world", 200, 200, 1.5f,
                                                            EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(worldHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerStaticCollidersReady_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> worldHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::World, [&worldHandlerCalled](const EventData &data) {
        if (data.event) worldHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerStaticCollidersReady(100, 25,
                                                                  EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(worldHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerCameraModeChanged_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> cameraHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Camera, [&cameraHandlerCalled](const EventData &data) {
        if (data.event) cameraHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCameraModeChanged(1, 0,
                                                               EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(cameraHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerCameraShakeStarted_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> cameraHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Camera, [&cameraHandlerCalled](const EventData &data) {
        if (data.event) cameraHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCameraShakeStarted(0.5f, 1.0f,
                                                                EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(cameraHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerCameraShakeEnded_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> cameraHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Camera, [&cameraHandlerCalled](const EventData &data) {
        if (data.event) cameraHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCameraShakeEnded(EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(cameraHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerCameraZoomChanged_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> cameraHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Camera, [&cameraHandlerCalled](const EventData &data) {
        if (data.event) cameraHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCameraZoomChanged(2.0f, 1.0f,
                                                               EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(cameraHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerCollisionObstacleChanged_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> obstacleHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::CollisionObstacleChanged, [&obstacleHandlerCalled](const EventData &data) {
        if (data.event) obstacleHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCollisionObstacleChanged(
      Vector2D(100.0f, 200.0f), 64.0f, "tree_removed",
      EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(obstacleHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(TriggerDamage_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> combatHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Combat, [&combatHandlerCalled](const EventData &data) {
        if (data.event) combatHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerDamage(EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(combatHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(ImmediateDamageCommit_RunsBeforeCustomCombatHandlers,
                        EventManagerFixture) {
  auto& edm = EntityDataManager::Instance();
  EntityHandle playerHandle = edm.registerPlayer(9101, Vector2D(100.0f, 100.0f));
  BOOST_REQUIRE(playerHandle.isValid());

  auto& playerData = edm.getCharacterData(playerHandle);
  playerData.maxHealth = 100.0f;
  playerData.health = 100.0f;
  playerData.mass = 1.0f;

  auto damageEvent = std::make_shared<DamageEvent>(
      EntityEventType::DamageIntent, EntityHandle{}, playerHandle, 25.0f,
      Vector2D(5.0f, 0.0f));

  float observedCommittedHealth = -1.0f;
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Combat, [&edm, playerHandle, &observedCommittedHealth](
                              const EventData& data) {
        BOOST_REQUIRE(data.event);
        observedCommittedHealth = edm.getCharacterData(playerHandle).health;
      });

  BOOST_CHECK(EventManager::Instance().dispatchEvent(
      damageEvent, EventManager::DispatchMode::Immediate));

  BOOST_CHECK_CLOSE(edm.getCharacterData(playerHandle).health, 75.0f, 0.01f);
  BOOST_CHECK_CLOSE(observedCommittedHealth, 75.0f, 0.01f);

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(ImmediateDamageCommit_AppliesArmorDefense,
                        EventManagerFixture) {
  auto& edm = EntityDataManager::Instance();
  EntityHandle playerHandle = edm.registerPlayer(9111, Vector2D(100.0f, 100.0f));
  BOOST_REQUIRE(playerHandle.isValid());

  auto& playerData = edm.getCharacterData(playerHandle);
  playerData.maxHealth = 100.0f;
  playerData.health = 100.0f;
  playerData.mass = 1.0f;
  playerData.armorDefense = 100.0f;

  auto damageEvent = std::make_shared<DamageEvent>(
      EntityEventType::DamageIntent, EntityHandle{}, playerHandle, 40.0f,
      Vector2D(0.0f, 0.0f));

  BOOST_CHECK(EventManager::Instance().dispatchEvent(
      damageEvent, EventManager::DispatchMode::Immediate));

  BOOST_CHECK_CLOSE(edm.getCharacterData(playerHandle).health, 80.0f, 0.01f);
}

BOOST_FIXTURE_TEST_CASE(DeferredDamageCommit_RunsBeforeCustomCombatHandlers,
                        EventManagerFixture) {
  auto& edm = EntityDataManager::Instance();
  EntityHandle playerHandle = edm.registerPlayer(9102, Vector2D(100.0f, 100.0f));
  BOOST_REQUIRE(playerHandle.isValid());

  auto& playerData = edm.getCharacterData(playerHandle);
  playerData.maxHealth = 100.0f;
  playerData.health = 100.0f;
  playerData.mass = 1.0f;

  auto damageEvent = std::make_shared<DamageEvent>(
      EntityEventType::DamageIntent, EntityHandle{}, playerHandle, 120.0f,
      Vector2D(2.0f, 0.0f));

  float observedCommittedHealth = -1.0f;
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Combat, [&edm, playerHandle, &observedCommittedHealth](
                              const EventData& data) {
        BOOST_REQUIRE(data.event);
        observedCommittedHealth = edm.getCharacterData(playerHandle).health;
      });

  BOOST_CHECK(
      EventManager::Instance().dispatchEvent(damageEvent, EventManager::DispatchMode::Deferred));
  EventManager::Instance().update();

  BOOST_CHECK_CLOSE(edm.getCharacterData(playerHandle).health, 0.0f, 0.01f);
  BOOST_CHECK_CLOSE(observedCommittedHealth, 0.0f, 0.01f);

  EventManager::Instance().removeHandler(tok);
}

BOOST_FIXTURE_TEST_CASE(PrepareForStateTransition_DropsDeferredCombatEvents,
                        EventManagerFixture) {
  auto& edm = EntityDataManager::Instance();
  EntityHandle playerHandle = edm.registerPlayer(9103, Vector2D(120.0f, 120.0f));
  BOOST_REQUIRE(playerHandle.isValid());

  auto& playerData = edm.getCharacterData(playerHandle);
  playerData.maxHealth = 100.0f;
  playerData.health = 100.0f;
  playerData.mass = 1.0f;

  auto damageEvent = std::make_shared<DamageEvent>(
      EntityEventType::DamageIntent, EntityHandle{}, playerHandle, 30.0f,
      Vector2D(4.0f, 0.0f));

  BOOST_CHECK(EventManager::Instance().dispatchEvent(
      damageEvent, EventManager::DispatchMode::Deferred));
  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 1);

  EventManager::Instance().prepareForStateTransition();

  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 0);

  EventManager::Instance().update();

  BOOST_CHECK_CLOSE(edm.getCharacterData(playerHandle).health, 100.0f, 0.01f);

  edm.destroyEntity(playerHandle);
}

BOOST_FIXTURE_TEST_CASE(CleanAndInit_DropsDeferredCombatEvents,
                        EventManagerFixture) {
  auto& edm = EntityDataManager::Instance();
  EntityHandle playerHandle = edm.registerPlayer(9104, Vector2D(140.0f, 140.0f));
  BOOST_REQUIRE(playerHandle.isValid());

  auto& playerData = edm.getCharacterData(playerHandle);
  playerData.maxHealth = 100.0f;
  playerData.health = 100.0f;
  playerData.mass = 1.0f;

  auto damageEvent = std::make_shared<DamageEvent>(
      EntityEventType::DamageIntent, EntityHandle{}, playerHandle, 45.0f,
      Vector2D(6.0f, 0.0f));

  BOOST_CHECK(EventManager::Instance().dispatchEvent(
      damageEvent, EventManager::DispatchMode::Deferred));
  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 1);

  EventManager::Instance().clean();
  BOOST_REQUIRE(EventManager::Instance().init());

  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 0);

  EventManager::Instance().update();

  BOOST_CHECK_CLOSE(edm.getCharacterData(playerHandle).health, 100.0f, 0.01f);

  edm.destroyEntity(playerHandle);
}

BOOST_FIXTURE_TEST_CASE(TriggerWorldTrigger_DispatchesToHandlers, EventManagerFixture) {
  std::atomic<bool> triggerHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::WorldTrigger, [&triggerHandlerCalled](const EventData &data) {
        if (data.event) triggerHandlerCalled.store(true);
      });

  WorldTriggerEvent event(1, 2, VoidLight::TriggerTag::Portal, Vector2D(100.0f, 200.0f), TriggerPhase::Enter);
  bool ok = EventManager::Instance().triggerWorldTrigger(event,
                                                          EventManager::DispatchMode::Immediate);
  BOOST_CHECK(ok);
  BOOST_CHECK(triggerHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

// ==================== Additional Event Pool Recycling Tests ====================

BOOST_FIXTURE_TEST_CASE(EventPoolRecycling_ResourceChangeEvents, EventManagerFixture) {
  EventPtr firstResource = nullptr;
  EventPtr secondResource = nullptr;
  EventManager::Instance().registerHandler(
      EventTypeId::ResourceChange, [&](const EventData &data) {
        if (!firstResource) firstResource = data.event;
        else secondResource = data.event;
      });

  VoidLight::ResourceHandle testResource(1, 1);
  EventManager::Instance().triggerResourceChange(TEST_PLAYER_HANDLE, testResource, 0, 10, "test",
                                                  EventManager::DispatchMode::Immediate);
  EventManager::Instance().triggerResourceChange(TEST_PLAYER_HANDLE, testResource, 10, 20, "test",
                                                  EventManager::DispatchMode::Immediate);

  BOOST_CHECK(firstResource != nullptr);
  BOOST_CHECK(secondResource != nullptr);
  BOOST_CHECK_EQUAL(firstResource.get(), secondResource.get()); // Same pooled event reused
}

BOOST_FIXTURE_TEST_CASE(EventPoolRecycling_CollisionObstacleChangedEvents, EventManagerFixture) {
  EventPtr firstObstacle = nullptr;
  EventPtr secondObstacle = nullptr;
  EventManager::Instance().registerHandler(
      EventTypeId::CollisionObstacleChanged, [&](const EventData &data) {
        if (!firstObstacle) firstObstacle = data.event;
        else secondObstacle = data.event;
      });

  EventManager::Instance().triggerCollisionObstacleChanged(Vector2D(0, 0), 64.0f, "added",
                                                            EventManager::DispatchMode::Immediate);
  EventManager::Instance().triggerCollisionObstacleChanged(Vector2D(100, 100), 32.0f, "removed",
                                                            EventManager::DispatchMode::Immediate);

  BOOST_CHECK(firstObstacle != nullptr);
  BOOST_CHECK(secondObstacle != nullptr);
  BOOST_CHECK_EQUAL(firstObstacle.get(), secondObstacle.get()); // Same pooled event reused
}

BOOST_FIXTURE_TEST_CASE(EventPoolRecycling_DamageEvents, EventManagerFixture) {
  EventPtr firstDamage = nullptr;
  EventPtr secondDamage = nullptr;
  EventManager::Instance().registerHandler(
      EventTypeId::Combat, [&](const EventData &data) {
        if (!firstDamage) firstDamage = data.event;
        else secondDamage = data.event;
      });

  EventManager::Instance().triggerDamage(EventManager::DispatchMode::Immediate);
  EventManager::Instance().triggerDamage(EventManager::DispatchMode::Immediate);

  BOOST_CHECK(firstDamage != nullptr);
  BOOST_CHECK(secondDamage != nullptr);
  BOOST_CHECK_EQUAL(firstDamage.get(), secondDamage.get()); // Same pooled event reused
}

// ==================== Additional API Coverage Tests ====================

BOOST_FIXTURE_TEST_CASE(DrainAllDeferredEvents_ProcessesAllEvents, EventManagerFixture) {
  std::atomic<int> handlerCallCount{0};

  EventManager::Instance().registerHandler(
      EventTypeId::Weather,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });

  // Queue multiple deferred events
  for (int i = 0; i < 5; ++i) {
    EventManager::Instance().changeWeather("Test", 1.0f);
  }

  // Should have pending events
  BOOST_CHECK_GT(EventManager::Instance().getPendingEventCount(), 0);

  // Drain all events (used in testing for deterministic processing)
  EventManager::Instance().drainAllDeferredEvents();

  // All events should be processed
  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 0);
  BOOST_CHECK_GE(handlerCallCount.load(), 5);
}

BOOST_FIXTURE_TEST_CASE(EnqueueBatch_ProcessesAllEventsInBatch, EventManagerFixture) {
  std::atomic<int> handlerCallCount{0};

  EventManager::Instance().registerHandler(
      EventTypeId::Custom,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });

  // Create a batch of deferred events
  std::vector<EventManager::DeferredEvent> batch;
  for (int i = 0; i < 10; ++i) {
    EventData data;
    data.event = std::make_shared<MockEvent>("BatchEvent");
    data.typeId = EventTypeId::Custom;
    data.setActive(true);
    batch.push_back(EventManager::DeferredEvent{EventTypeId::Custom, std::move(data)});
  }

  // Enqueue the batch
  EventManager::Instance().enqueueBatch(std::move(batch));

  // Process all events
  EventManager::Instance().drainAllDeferredEvents();

  // All batch events should be processed
  BOOST_CHECK_GE(handlerCallCount.load(), 10);
}

BOOST_FIXTURE_TEST_CASE(DeferredQueueOverflow_IsCappedAtMaxDispatchQueue, EventManagerFixture) {
  EventManager::Instance().registerHandler(EventTypeId::Combat, [](const EventData&) {});

  constexpr size_t queueCap = 8192;
  for (size_t i = 0; i < queueCap + 25; ++i) {
    EventManager::Instance().triggerDamage(EventManager::DispatchMode::Deferred);
  }

  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), queueCap);

  EventManager::Instance().drainAllDeferredEvents();
  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), 0);
}

BOOST_FIXTURE_TEST_CASE(EnqueueBatchOverflow_TrimsOversizedIncomingBatchToQueueCap, EventManagerFixture) {
  std::atomic<int> handlerCallCount{0};
  EventManager::Instance().registerHandler(
      EventTypeId::Combat,
      [&handlerCallCount](const EventData&) { handlerCallCount.fetch_add(1); });

  constexpr size_t queueCap = 8192;
  std::vector<EventManager::DeferredEvent> batch;
  batch.reserve(queueCap + 17);

  for (size_t i = 0; i < queueCap + 17; ++i) {
    EventData data;
    data.typeId = EventTypeId::Combat;
    data.setActive(true);
    data.event = std::make_shared<DamageEvent>();
    batch.push_back(EventManager::DeferredEvent{EventTypeId::Combat, std::move(data)});
  }

  EventManager::Instance().enqueueBatch(std::move(batch));

  BOOST_CHECK_EQUAL(EventManager::Instance().getPendingEventCount(), queueCap);

  EventManager::Instance().drainAllDeferredEvents();
  BOOST_CHECK_EQUAL(handlerCallCount.load(), static_cast<int>(queueCap));
}

BOOST_FIXTURE_TEST_CASE(GetHandlerCount_ReturnsCorrectCount, EventManagerFixture) {
  // Initially no handlers
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 0);

  // Add handlers
  auto tok1 = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Weather, [](const EventData &) {});
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 1);

  auto tok2 = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Weather, [](const EventData &) {});
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 2);

  // Remove one handler
  EventManager::Instance().removeHandler(tok1);
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 1);

  // Remove remaining handler
  EventManager::Instance().removeHandler(tok2);
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 0);
}

BOOST_FIXTURE_TEST_CASE(RemoveHandlers_ClearsAllForType, EventManagerFixture) {
  // Add multiple handlers for Weather
  EventManager::Instance().registerHandler(EventTypeId::Weather, [](const EventData &) {});
  EventManager::Instance().registerHandler(EventTypeId::Weather, [](const EventData &) {});
  EventManager::Instance().registerHandler(EventTypeId::Weather, [](const EventData &) {});
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 3);

  // Also add a handler for a different type (Camera - not internally registered)
  EventManager::Instance().registerHandler(EventTypeId::Camera, [](const EventData &) {});
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Camera), 1);

  // Remove all Weather handlers
  EventManager::Instance().removeHandlers(EventTypeId::Weather);
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Weather), 0);

  // Camera handler should still exist
  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Camera), 1);
}

BOOST_FIXTURE_TEST_CASE(GlobalPause_BlocksUpdateProcessing, EventManagerFixture) {
  std::atomic<int> handlerCallCount{0};

  EventManager::Instance().registerHandler(
      EventTypeId::Weather,
      [&handlerCallCount](const EventData &) { handlerCallCount.fetch_add(1); });

  // Queue a deferred event
  EventManager::Instance().changeWeather("Test", 1.0f);
  BOOST_CHECK_GT(EventManager::Instance().getPendingEventCount(), 0);

  // Enable global pause
  EventManager::Instance().setGlobalPause(true);
  BOOST_CHECK(EventManager::Instance().isGloballyPaused());

  // Update should not process events while paused
  EventManager::Instance().update();
  BOOST_CHECK_EQUAL(handlerCallCount.load(), 0);
  BOOST_CHECK_GT(EventManager::Instance().getPendingEventCount(), 0); // Still pending

  // Disable global pause
  EventManager::Instance().setGlobalPause(false);
  BOOST_CHECK(!EventManager::Instance().isGloballyPaused());

  // Now update should process events
  EventManager::Instance().update();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  BOOST_CHECK_GE(handlerCallCount.load(), 1);
}

BOOST_FIXTURE_TEST_CASE(MultipleHandlers_AllExecutedInOrder, EventManagerFixture) {
  std::vector<int> callOrder;
  std::mutex orderMutex;

  EventManager::Instance().registerHandler(EventTypeId::Weather, [&](const EventData &) {
    std::lock_guard<std::mutex> lock(orderMutex);
    callOrder.push_back(1);
  });
  EventManager::Instance().registerHandler(EventTypeId::Weather, [&](const EventData &) {
    std::lock_guard<std::mutex> lock(orderMutex);
    callOrder.push_back(2);
  });
  EventManager::Instance().registerHandler(EventTypeId::Weather, [&](const EventData &) {
    std::lock_guard<std::mutex> lock(orderMutex);
    callOrder.push_back(3);
  });

  // Trigger event with immediate dispatch for deterministic order
  EventManager::Instance().changeWeather("Test", 1.0f, EventManager::DispatchMode::Immediate);

  // All three handlers should be called
  BOOST_CHECK_EQUAL(callOrder.size(), 3);
  // They should be called in registration order
  BOOST_CHECK_EQUAL(callOrder[0], 1);
  BOOST_CHECK_EQUAL(callOrder[1], 2);
  BOOST_CHECK_EQUAL(callOrder[2], 3);
}

BOOST_FIXTURE_TEST_CASE(HandlerException_DoesNotStopOtherHandlers, EventManagerFixture) {
  std::atomic<int> handlerCallCount{0};

  // First handler - throws exception
  EventManager::Instance().registerHandler(EventTypeId::Weather, [](const EventData &) {
    throw std::runtime_error("Test exception");
  });

  // Second handler - should still be called
  EventManager::Instance().registerHandler(EventTypeId::Weather, [&](const EventData &) {
    handlerCallCount.fetch_add(1);
  });

  // Third handler - should still be called
  EventManager::Instance().registerHandler(EventTypeId::Weather, [&](const EventData &) {
    handlerCallCount.fetch_add(1);
  });

  // Trigger event - exception in first handler should not stop others
  EventManager::Instance().changeWeather("Test", 1.0f, EventManager::DispatchMode::Immediate);

  // Second and third handlers should still have been called
  BOOST_CHECK_GE(handlerCallCount.load(), 2);
}

// ==================== Edge Case Tests ====================

BOOST_FIXTURE_TEST_CASE(NullEventDispatch_ReturnsFalse, EventManagerFixture) {
  // Dispatching a null event should return false
  bool result = EventManager::Instance().dispatchEvent(nullptr, EventManager::DispatchMode::Immediate);
  BOOST_CHECK(!result);

  result = EventManager::Instance().dispatchEvent(nullptr, EventManager::DispatchMode::Deferred);
  BOOST_CHECK(!result);
}

BOOST_FIXTURE_TEST_CASE(Reinitialize_WorksAfterClean, EventManagerFixture) {
  // Clean the manager
  EventManager::Instance().clean();
  BOOST_CHECK(!EventManager::Instance().isInitialized());

  // Re-initialize should work
  BOOST_REQUIRE(EventManager::Instance().init());
  BOOST_CHECK(EventManager::Instance().isInitialized());

  // Should be fully functional
  std::atomic<bool> handlerCalled{false};
  EventManager::Instance().registerHandler(EventTypeId::Weather, [&](const EventData &) {
    handlerCalled.store(true);
  });

  EventManager::Instance().changeWeather("Test", 1.0f, EventManager::DispatchMode::Immediate);
  BOOST_CHECK(handlerCalled.load());
}

BOOST_FIXTURE_TEST_CASE(DoubleInit_WarnsAndSucceeds, EventManagerFixture) {
  // First init (from fixture) should succeed
  BOOST_CHECK(EventManager::Instance().isInitialized());

  // Second init should also return true (with warning logged)
  BOOST_REQUIRE(EventManager::Instance().init());
  BOOST_CHECK(EventManager::Instance().isInitialized());
}

BOOST_FIXTURE_TEST_CASE(IdempotentClean_SafeMultipleCalls, EventManagerFixture) {
  // Clean multiple times - should be safe
  EventManager::Instance().clean();
  BOOST_CHECK(!EventManager::Instance().isInitialized());

  // Second clean should also be safe (no crash)
  EventManager::Instance().clean();
  BOOST_CHECK(!EventManager::Instance().isInitialized());

  // Third clean should also be safe
  EventManager::Instance().clean();
  BOOST_CHECK(!EventManager::Instance().isInitialized());

  // Re-init should still work
  BOOST_REQUIRE(EventManager::Instance().init());
  BOOST_CHECK(EventManager::Instance().isInitialized());
}
