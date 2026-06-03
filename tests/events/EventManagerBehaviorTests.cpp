/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE EventManagerBehaviorTests
#include <boost/test/unit_test.hpp>
#include <atomic>
#include <vector>

#include "EventManagerTestAccess.hpp"
#include "core/ThreadSystem.hpp"
#include "events/CameraEvent.hpp"
#include "events/Event.hpp"
#include "events/WeatherEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"

struct ThreadSystemFixture {
  ThreadSystemFixture() {
    if (!VoidLight::ThreadSystem::Instance().init()) {
      throw std::runtime_error("Failed to initialize ThreadSystem for EventManagerBehaviorTests");
    }
  }
  ~ThreadSystemFixture() {
    VoidLight::ThreadSystem::Instance().clean();
  }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

namespace {

class TestEvent : public Event {
public:
  explicit TestEvent(const std::string &name) : m_name(name) {}
  void update() override {}
  void execute() override { ++m_executeCount; }
  void reset() override { m_executeCount = 0; }
  void clean() override {}
  std::string getName() const override { return m_name; }
  std::string getType() const override { return "Custom"; }
  std::string getTypeName() const override { return "TestEvent"; }
  EventTypeId getTypeId() const override { return EventTypeId::Custom; }
  bool checkConditions() override { return true; }

  int getExecuteCount() const { return m_executeCount; }

private:
  std::string m_name;
  int m_executeCount{0};
};

struct EventFixture {
  EventFixture() {
    EventManagerTestAccess::reset();
    BOOST_REQUIRE(EntityDataManager::Instance().init());
  }
  ~EventFixture() {
    // Clean after each test
    EventManager::Instance().clean();
    EntityDataManager::Instance().clean();
  }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(EventBehaviorSuite, EventFixture)

// Test dispatch-only architecture: events dispatched to handlers
BOOST_AUTO_TEST_CASE(DispatchEvent_WithHandlers_CallsHandlers) {
  auto e = std::make_shared<TestEvent>("TestA");

  std::atomic<int> handlerCallCount{0};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&handlerCallCount](const EventData &data) {
        if (data.isActive()) ++handlerCallCount;
      });

  // Dispatch directly (dispatch-only architecture)
  EventManager::Instance().dispatchEvent(e);
  EventManager::Instance().update(); // Process deferred events

  BOOST_CHECK_EQUAL(handlerCallCount.load(), 1);
  EventManager::Instance().removeHandler(tok);
}

// Test dispatch without handlers still succeeds
BOOST_AUTO_TEST_CASE(DispatchEvent_NoHandlers_Succeeds) {
  auto e = std::make_shared<TestEvent>("TestB");

  // No handlers registered for Custom type
  bool ok = EventManager::Instance().dispatchEvent(e);
  BOOST_CHECK(ok);

  // Update to process deferred events (should not crash)
  EventManager::Instance().update();
}

BOOST_AUTO_TEST_CASE(ChangeWeather_DispatchesToHandlers) {
  std::atomic<bool> weatherHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Weather, [&weatherHandlerCalled](const EventData &data) {
        if (data.event) weatherHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().changeWeather("Rainy", 1.0f);
  BOOST_CHECK(ok);

  EventManager::Instance().update(); // Process deferred events
  BOOST_CHECK(weatherHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_AUTO_TEST_CASE(SpawnNPC_DispatchesToHandlers) {
  std::atomic<bool> npcHandlerCalled{false};
  auto& edm = EntityDataManager::Instance();
  const size_t npcCountBefore = edm.getEntityCount(EntityKind::NPC);
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::NPCSpawn, [&npcHandlerCalled](const EventData &data) {
        if (data.event) npcHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().spawnNPC("Guard", 10.0f, 20.0f);
  BOOST_CHECK(ok);

  EventManager::Instance().update(); // Process deferred events
  BOOST_CHECK(npcHandlerCalled.load());
  BOOST_CHECK_GT(edm.getEntityCount(EntityKind::NPC), npcCountBefore);

  EventManager::Instance().removeHandler(tok);
}

BOOST_AUTO_TEST_CASE(TriggerParticleEffect_DispatchesToHandlers) {
  std::atomic<bool> particleHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::ParticleEffect, [&particleHandlerCalled](const EventData &data) {
        if (data.event) particleHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerParticleEffect("Fire", 100.0f, 200.0f);
  BOOST_CHECK(ok);

  EventManager::Instance().update(); // Process deferred events
  BOOST_CHECK(particleHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_AUTO_TEST_CASE(TriggerCameraMoved_DispatchesToHandlers) {
  std::atomic<bool> cameraHandlerCalled{false};
  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Camera, [&cameraHandlerCalled](const EventData &data) {
        if (data.event) cameraHandlerCalled.store(true);
      });

  bool ok = EventManager::Instance().triggerCameraMoved(
      Vector2D(100, 100), Vector2D(0, 0));
  BOOST_CHECK(ok);

  EventManager::Instance().update();
  BOOST_CHECK(cameraHandlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_AUTO_TEST_CASE(RegisterHandlerWithToken_CanBeRemoved) {
  std::atomic<int> callCount{0};

  auto token = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom,
      [&callCount](const EventData &data) {
        if (data.isActive()) ++callCount;
      });

  // Dispatch once - handler should be called
  auto e1 = std::make_shared<TestEvent>("Test1");
  EventManager::Instance().dispatchEvent(e1);
  EventManager::Instance().update();
  BOOST_CHECK_EQUAL(callCount.load(), 1);

  // Remove handler
  EventManager::Instance().removeHandler(token);

  // Dispatch again - handler should NOT be called
  auto e2 = std::make_shared<TestEvent>("Test2");
  EventManager::Instance().dispatchEvent(e2);
  EventManager::Instance().update();
  BOOST_CHECK_EQUAL(callCount.load(), 1); // Still 1, not incremented
}

BOOST_AUTO_TEST_CASE(ImmediateDispatch_CallsHandlersSynchronously) {
  std::atomic<bool> handlerCalled{false};

  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&handlerCalled](const EventData &data) {
        if (data.isActive()) handlerCalled.store(true);
      });

  auto e = std::make_shared<TestEvent>("ImmediateTest");

  // Dispatch with Immediate mode - should call handler before returning
  EventManager::Instance().dispatchEvent(e, EventManager::DispatchMode::Immediate);

  // Handler should already be called (no update() needed)
  BOOST_CHECK(handlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_AUTO_TEST_CASE(DeferredDispatch_RequiresUpdate) {
  std::atomic<bool> handlerCalled{false};

  auto tok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&handlerCalled](const EventData &data) {
        if (data.isActive()) handlerCalled.store(true);
      });

  auto e = std::make_shared<TestEvent>("DeferredTest");

  // Dispatch with Deferred mode (default)
  EventManager::Instance().dispatchEvent(e, EventManager::DispatchMode::Deferred);

  // Handler should NOT be called yet
  BOOST_CHECK(!handlerCalled.load());

  // Now process deferred events
  EventManager::Instance().update();

  // Handler should now be called
  BOOST_CHECK(handlerCalled.load());

  EventManager::Instance().removeHandler(tok);
}

BOOST_AUTO_TEST_CASE(DeferredDispatch_PreservesFIFOOrderAcrossTypes) {
  std::vector<int> callOrder;

  auto customTok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [&callOrder](const EventData &) { callOrder.push_back(1); });
  auto weatherTok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Weather, [&callOrder](const EventData &) { callOrder.push_back(2); });
  auto particleTok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::ParticleEffect, [&callOrder](const EventData &) { callOrder.push_back(3); });

  EventManager::Instance().dispatchEvent(
      std::make_shared<TestEvent>("First"), EventManager::DispatchMode::Deferred);
  EventManager::Instance().changeWeather(
      "Rainy", 1.0f, EventManager::DispatchMode::Deferred);
  EventManager::Instance().triggerParticleEffect(
      "Fire", 0, 0, 1.0f, 1.0f, "", EventManager::DispatchMode::Deferred);

  EventManager::Instance().update();

  BOOST_REQUIRE_EQUAL(callOrder.size(), 3);
  BOOST_CHECK_EQUAL(callOrder[0], 1);
  BOOST_CHECK_EQUAL(callOrder[1], 2);
  BOOST_CHECK_EQUAL(callOrder[2], 3);

  EventManager::Instance().removeHandler(customTok);
  EventManager::Instance().removeHandler(weatherTok);
  EventManager::Instance().removeHandler(particleTok);
}

BOOST_AUTO_TEST_CASE(PrepareForStateTransition_ClearsCustomHandlersButKeepsBuiltIns) {
  auto customTok = EventManager::Instance().registerHandlerWithToken(
      EventTypeId::Custom, [](const EventData &) {});

  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Custom), 1);
  BOOST_CHECK_GE(EventManager::Instance().getHandlerCount(EventTypeId::NPCSpawn), 1);

  EventManager::Instance().prepareForStateTransition();

  BOOST_CHECK_EQUAL(EventManager::Instance().getHandlerCount(EventTypeId::Custom), 0);
  BOOST_CHECK_GE(EventManager::Instance().getHandlerCount(EventTypeId::NPCSpawn), 1);

  // Removing the stale token should be harmless after transition cleanup.
  BOOST_CHECK(!EventManager::Instance().removeHandler(customTok));
}

BOOST_AUTO_TEST_CASE(PrepareForStateTransition_KeepsPersistentHandlersFunctional) {
  std::atomic<int> transientCalls{0};
  std::atomic<int> persistentCalls{0};

  EventManager::Instance().registerHandler(
      EventTypeId::Custom, [&transientCalls](const EventData &) {
        transientCalls.fetch_add(1, std::memory_order_release);
      });
  auto persistentToken = EventManager::Instance().registerPersistentHandlerWithToken(
      EventTypeId::Custom, [&persistentCalls](const EventData &) {
        persistentCalls.fetch_add(1, std::memory_order_release);
      });

  EventManager::Instance().prepareForStateTransition();

  EventManager::Instance().dispatchEvent(
      std::make_shared<TestEvent>("PersistentAfterTransition"),
      EventManager::DispatchMode::Deferred);
  EventManager::Instance().update();

  BOOST_CHECK_EQUAL(transientCalls.load(std::memory_order_acquire), 0);
  BOOST_CHECK_EQUAL(persistentCalls.load(std::memory_order_acquire), 1);
  BOOST_CHECK(EventManager::Instance().removeHandler(persistentToken));
}

BOOST_AUTO_TEST_SUITE_END()
