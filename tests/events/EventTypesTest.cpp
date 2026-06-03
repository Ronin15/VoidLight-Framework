/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE EventTypesTest
#include <boost/test/unit_test.hpp>

#include "events/EventFactory.hpp"
#include "events/MerchantSpawnEvent.hpp"
#include "events/NPCSpawnEvent.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "events/SceneChangeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "entities/Player.hpp"
#include "entities/EntityHandle.hpp"
#include "managers/EntityDataManager.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct EventTypesFixture {
  EventTypesFixture() {
    // Initialize EntityDataManager (required for Player entity creation in DOD)
    BOOST_REQUIRE(EntityDataManager::Instance().init());

    // Make sure we start fresh with each test
    EventFactory::Instance().clean();
    BOOST_CHECK(EventFactory::Instance().init());

    // Always register standard event creators explicitly for each test
    // Make sure to register Weather creator first as it's used in most tests
    registerWeatherCreator();
    registerSceneChangeCreator();
    registerNPCSpawnCreator();
    registerMerchantSpawnCreator();
  }

  ~EventTypesFixture() {
    // Clean up EventFactory after each test
    EventFactory::Instance().clean();
    // Clean up EntityDataManager
    EntityDataManager::Instance().clean();
  }

  // Register each creator separately for better control
  void registerWeatherCreator() {
    EventFactory::Instance().registerCustomEventCreator(
        "Weather", [](const EventDefinition &def) {
          std::string weatherType = def.params.count("weatherType")
                                        ? def.params.at("weatherType")
                                        : "Clear";
          float intensity = def.numParams.count("intensity")
                                ? def.numParams.at("intensity")
                                : 0.5f;
          float transitionTime = def.numParams.count("transitionTime")
                                     ? def.numParams.at("transitionTime")
                                     : 5.0f;

          return EventFactory::Instance().createWeatherEvent(
              def.name, weatherType, intensity, transitionTime);
        });
  }

  void registerSceneChangeCreator() {
    EventFactory::Instance().registerCustomEventCreator(
        "SceneChange", [](const EventDefinition &def) {
          std::string targetScene = def.params.count("targetScene")
                                        ? def.params.at("targetScene")
                                        : "";
          std::string transitionType = def.params.count("transitionType")
                                           ? def.params.at("transitionType")
                                           : "fade";
          float duration = def.numParams.count("duration")
                               ? def.numParams.at("duration")
                               : 1.0f;

          return EventFactory::Instance().createSceneChangeEvent(
              def.name, targetScene, transitionType, duration);
        });
  }

  void registerNPCSpawnCreator() {
    EventFactory::Instance().registerCustomEventCreator(
        "NPCSpawn", [](const EventDefinition &def) {
          std::string npcType = def.params.count("npcType")
                                    ? def.params.at("npcType")
                                    : "";
          int count = static_cast<int>(def.numParams.count("count")
                                           ? def.numParams.at("count")
                                           : 1.0f);
          float spawnRadius = def.numParams.count("spawnRadius")
                                  ? def.numParams.at("spawnRadius")
                                  : 0.0f;

          return EventFactory::Instance().createNPCSpawnEvent(
              def.name, npcType, count, spawnRadius);
        });
  }

  void registerMerchantSpawnCreator() {
    EventFactory::Instance().registerCustomEventCreator(
        "MerchantSpawn", [](const EventDefinition &def) {
          std::string merchantClass = def.params.count("merchantClass")
                                          ? def.params.at("merchantClass")
                                          : "GeneralMerchant";
          std::string merchantRace = def.params.count("merchantRace")
                                         ? def.params.at("merchantRace")
                                         : "Human";
          int count = static_cast<int>(def.numParams.count("count")
                                           ? def.numParams.at("count")
                                           : 1.0f);
          float spawnRadius = def.numParams.count("spawnRadius")
                                  ? def.numParams.at("spawnRadius")
                                  : 0.0f;

          return EventFactory::Instance().createMerchantSpawnEvent(
              def.name, merchantClass, merchantRace, count, spawnRadius);
        });
  }
};

BOOST_AUTO_TEST_CASE(EventFactoryCleanInitRestoresBuiltInCreators) {
  auto& factory = EventFactory::Instance();
  factory.clean();
  BOOST_REQUIRE(factory.init());

  EventDefinition def;
  def.type = "MerchantSpawn";
  def.name = "MerchantAfterClean";

  auto event = factory.createEvent(def);
  BOOST_REQUIRE(event != nullptr);
  BOOST_CHECK_EQUAL(event->getType(), "MerchantSpawn");

  factory.clean();
}

// Test WeatherEvent creation and functionality
BOOST_FIXTURE_TEST_CASE(WeatherEventBasics, EventTypesFixture) {
  // Create a weather event
  auto rainEvent = std::make_shared<WeatherEvent>("Rain", WeatherType::Rainy);

  // Check basic properties
  BOOST_CHECK_EQUAL(rainEvent->getName(), "Rain");
  BOOST_CHECK_EQUAL(rainEvent->getType(), "Weather");
  BOOST_CHECK_EQUAL(rainEvent->getWeatherType(), WeatherType::Rainy);
  BOOST_CHECK_EQUAL(rainEvent->getWeatherTypeString(), "Rainy");
  BOOST_CHECK(rainEvent->isActive());

  // Test weather parameters
  WeatherParams params;
  params.intensity = 0.8f;
  params.visibility = 0.5f;
  params.transitionTime = 3.0f;
  params.particleEffect = "heavy_rain";
  params.soundEffect = "rain_sound";

  rainEvent->setWeatherParams(params);
  BOOST_CHECK_EQUAL(rainEvent->getWeatherParams().intensity, 0.8f);
  BOOST_CHECK_EQUAL(rainEvent->getWeatherParams().visibility, 0.5f);
  BOOST_CHECK_EQUAL(rainEvent->getWeatherParams().transitionTime, 3.0f);
  BOOST_CHECK_EQUAL(rainEvent->getWeatherParams().particleEffect, "heavy_rain");
  BOOST_CHECK_EQUAL(rainEvent->getWeatherParams().soundEffect, "rain_sound");

  // Test custom weather type
  auto customWeather = std::make_shared<WeatherEvent>("Custom", "AcidRain");
  BOOST_CHECK_EQUAL(customWeather->getWeatherType(), WeatherType::Custom);
  BOOST_CHECK_EQUAL(customWeather->getWeatherTypeString(), "AcidRain");

  // Test conditions without any conditions set
  auto baseEvent =
      std::make_shared<WeatherEvent>("BaseTest", WeatherType::Clear);
  BOOST_CHECK(
      !baseEvent->checkConditions()); // No conditions set, should return false

  // Create a new event instance for each condition test to avoid interference
  // Test with a simple false condition in its own scope
  {
    auto falseEvent =
        std::make_shared<WeatherEvent>("FalseTest", WeatherType::Clear);
    // Make sure there are no existing conditions
    falseEvent->clean();
    // Add a condition that always returns false
    falseEvent->addTimeCondition(
        []() { return false; }); // Use direct lambda to avoid capture issues
    // This should fail since the condition returns false
    BOOST_CHECK(!falseEvent->checkConditions());
  }

  // Test with a simple true condition in its own scope
  {
    auto trueEvent =
        std::make_shared<WeatherEvent>("TrueTest", WeatherType::Clear);
    // Make sure there are no existing conditions
    trueEvent->clean();
    // Add a condition that always returns true - no capture to avoid lifetime
    // issues
    trueEvent->addTimeCondition([]() { return true; });
    // This should pass since the condition returns true
    BOOST_CHECK(trueEvent->checkConditions());
  }
}

// Test SceneChangeEvent creation and functionality
BOOST_FIXTURE_TEST_CASE(SceneChangeEventBasics, EventTypesFixture) {
  // Create a scene change event
  auto sceneEvent =
      std::make_shared<SceneChangeEvent>("ToMainMenu", "MainMenu");

  // Check basic properties
  BOOST_CHECK_EQUAL(sceneEvent->getName(), "ToMainMenu");
  BOOST_CHECK_EQUAL(sceneEvent->getType(), "SceneChange");
  BOOST_CHECK_EQUAL(sceneEvent->getTargetSceneID(), "MainMenu");
  BOOST_CHECK(sceneEvent->isActive());

  // Test transition type
  sceneEvent->setTransitionType(TransitionType::Dissolve);
  BOOST_CHECK_EQUAL(sceneEvent->getTransitionType(), TransitionType::Dissolve);

  // Test transition parameters
  TransitionParams params;
  params.duration = 2.5f;
  params.transitionEffect = "dissolve";
  params.playSound = true;
  params.soundEffect = "transition_sound";
  params.soundVolume = 0.7f;

  sceneEvent->setTransitionParams(params);
  BOOST_CHECK_EQUAL(sceneEvent->getTransitionParams().duration, 2.5f);
  BOOST_CHECK_EQUAL(sceneEvent->getTransitionParams().transitionEffect,
                    "dissolve");
  BOOST_CHECK(sceneEvent->getTransitionParams().playSound);
  BOOST_CHECK_EQUAL(sceneEvent->getTransitionParams().soundEffect,
                    "transition_sound");
  BOOST_CHECK_EQUAL(sceneEvent->getTransitionParams().soundVolume, 0.7f);

  // Test trigger zones
  sceneEvent->setTriggerZone(100.0f, 200.0f, 50.0f); // Circle zone

  // Test player input trigger
  sceneEvent->setRequirePlayerInput(true);
  sceneEvent->setInputKey("E");

  // Test timer trigger
  sceneEvent->setTimerTrigger(5.0f);
  sceneEvent->startTimer();
  BOOST_CHECK(!sceneEvent->isTimerComplete()); // Timer just started

  // Test custom conditions
  bool conditionFlag = false;
  sceneEvent->addCondition([&conditionFlag]() { return conditionFlag; });

  BOOST_CHECK(
      !sceneEvent->checkConditions()); // Should be false until condition is met

  conditionFlag = true;
  // In a real test, this would return true if all other conditions were also
  // met Here we expect false because isPlayerInTriggerZone() and
  // isPlayerInputTriggered() will return false
}

// Test NPCSpawnEvent creation and functionality
BOOST_FIXTURE_TEST_CASE(NPCSpawnEventBasics, EventTypesFixture) {
  // Create an NPC spawn event
  auto spawnEvent = std::make_shared<NPCSpawnEvent>("SpawnGuards", "Guard");

  // Check basic properties
  BOOST_CHECK_EQUAL(spawnEvent->getName(), "SpawnGuards");
  BOOST_CHECK_EQUAL(spawnEvent->getType(), "NPCSpawn");
  BOOST_CHECK(spawnEvent->isActive());

  // Test spawn parameters
  SpawnParameters params;
  params.npcType = "EliteGuard";
  params.count = 3;
  params.spawnRadius = 10.0f;
  params.facingPlayer = true;
  params.fadeIn = true;
  params.fadeTime = 1.5f;
  params.playSpawnEffect = true;
  params.spawnEffectID = "smoke";
  params.spawnSoundID = "spawn_sound";

  spawnEvent->setSpawnParameters(params);
  BOOST_CHECK_EQUAL(spawnEvent->getSpawnParameters().npcType, "EliteGuard");
  BOOST_CHECK_EQUAL(spawnEvent->getSpawnParameters().count, 3);
  BOOST_CHECK_EQUAL(spawnEvent->getSpawnParameters().spawnRadius, 10.0f);
  BOOST_CHECK(spawnEvent->getSpawnParameters().facingPlayer);
  BOOST_CHECK(spawnEvent->getSpawnParameters().fadeIn);
  BOOST_CHECK_EQUAL(spawnEvent->getSpawnParameters().fadeTime, 1.5f);
  BOOST_CHECK(spawnEvent->getSpawnParameters().playSpawnEffect);
  BOOST_CHECK_EQUAL(spawnEvent->getSpawnParameters().spawnEffectID, "smoke");
  BOOST_CHECK_EQUAL(spawnEvent->getSpawnParameters().spawnSoundID,
                    "spawn_sound");

  // Test spawn locations
  spawnEvent->clearSpawnPoints();
  spawnEvent->addSpawnPoint(100.0f, 200.0f);
  spawnEvent->addSpawnPoint(150.0f, 250.0f);

  // Test spawn area
  spawnEvent->setSpawnArea(0.0f, 0.0f, 50.0f); // Circular area

  // Test proximity trigger
  spawnEvent->setProximityTrigger(100.0f);

  // Test time of day trigger
  spawnEvent->setTimeOfDayTrigger(19.0f, 6.0f); // Night time only

  // Test respawn
  spawnEvent->setRespawnTime(30.0f);
  BOOST_CHECK(spawnEvent->areAllEntitiesDead()); // No entities spawned yet
  BOOST_CHECK(!spawnEvent->canRespawn()); // Respawn timer not elapsed yet

  // Test max spawn count
  spawnEvent->setMaxSpawnCount(5);
  BOOST_CHECK_EQUAL(spawnEvent->getMaxSpawnCount(), 5);
  BOOST_CHECK_EQUAL(spawnEvent->getCurrentSpawnCount(), 0);

  // Test custom conditions
  bool conditionFlag = false;
  spawnEvent->addCondition([&conditionFlag]() { return conditionFlag; });

  BOOST_CHECK(
      !spawnEvent->checkConditions()); // Should be false until condition is met

  conditionFlag = true;
  // In a real test, this would still return false because the proximity and
  // time conditions aren't met
}

BOOST_FIXTURE_TEST_CASE(MerchantSpawnEventBasics, EventTypesFixture) {
  MerchantSpawnParameters params;
  params.merchantClass = "GeneralMerchant";
  params.merchantRace = "Human";
  params.count = 1;

  auto merchantEvent = std::make_shared<MerchantSpawnEvent>("SpawnMerchant", params);

  BOOST_CHECK_EQUAL(merchantEvent->getName(), "SpawnMerchant");
  BOOST_CHECK_EQUAL(merchantEvent->getType(), "MerchantSpawn");
  BOOST_CHECK_EQUAL(merchantEvent->getTypeName(), "MerchantSpawnEvent");
  BOOST_CHECK(merchantEvent->getTypeId() == EventTypeId::MerchantSpawn);
  BOOST_CHECK_EQUAL(merchantEvent->getMerchantSpawnParameters().merchantClass,
                    "GeneralMerchant");
}

// Test EventFactory creation methods
BOOST_FIXTURE_TEST_CASE(EventFactoryCreation, EventTypesFixture) {
  // Make sure EventFactory is properly initialized and Weather creator is
  // registered
  EventFactory::Instance().clean();
  EventFactory::Instance().init();
  registerWeatherCreator();

  // Test weather event creation
  auto rainEvent =
      EventFactory::Instance().createWeatherEvent("Rain", "Rainy", 0.7f);
  BOOST_REQUIRE(rainEvent != nullptr);
  BOOST_CHECK_EQUAL(rainEvent->getName(), "Rain");
  BOOST_CHECK_EQUAL(rainEvent->getType(), "Weather");
  BOOST_CHECK_EQUAL(
      static_cast<WeatherEvent *>(rainEvent.get())->getWeatherTypeString(),
      "Rainy");

  // Test scene change event creation
  auto sceneEvent = EventFactory::Instance().createSceneChangeEvent(
      "ToMainMenu", "MainMenu", "fade", 1.5f);
  BOOST_REQUIRE(sceneEvent != nullptr);
  BOOST_CHECK_EQUAL(sceneEvent->getName(), "ToMainMenu");
  BOOST_CHECK_EQUAL(sceneEvent->getType(), "SceneChange");
  BOOST_CHECK_EQUAL(
      static_cast<SceneChangeEvent *>(sceneEvent.get())->getTargetSceneID(),
      "MainMenu");

  auto spawnEvent = EventFactory::Instance().createNPCSpawnEvent("SpawnGuards",
                                                                 "Guard", 3, 10.0f);
  BOOST_REQUIRE(spawnEvent != nullptr);
  BOOST_CHECK_EQUAL(spawnEvent->getName(), "SpawnGuards");
  BOOST_CHECK_EQUAL(spawnEvent->getType(), "NPCSpawn");
  BOOST_CHECK_EQUAL(
      static_cast<NPCSpawnEvent *>(spawnEvent.get())->getSpawnParameters().npcType,
      "Guard");
  BOOST_CHECK_EQUAL(
      static_cast<NPCSpawnEvent *>(spawnEvent.get())->getSpawnParameters().count,
      3);
  BOOST_CHECK_EQUAL(
      static_cast<NPCSpawnEvent *>(spawnEvent.get())->getSpawnParameters().spawnRadius,
      10.0f);

  // Test event creation from definition
  EventDefinition def;
  def.type = "Weather";
  def.name = "Storm";
  def.params["weatherType"] = "Stormy";
  def.numParams["intensity"] = 0.9f;
  def.numParams["transitionTime"] = 4.0f;
  def.boolParams["oneTime"] = true;

  auto stormEvent = EventFactory::Instance().createEvent(def);
  BOOST_REQUIRE(stormEvent != nullptr);
  BOOST_CHECK_EQUAL(stormEvent->getName(), "Storm");
  BOOST_CHECK_EQUAL(stormEvent->getType(), "Weather");
  BOOST_CHECK_EQUAL(
      static_cast<WeatherEvent *>(stormEvent.get())->getWeatherTypeString(),
      "Stormy");
  BOOST_CHECK(stormEvent->isOneTime());
}

// Test event sequence creation
BOOST_FIXTURE_TEST_CASE(EventSequenceCreation, EventTypesFixture) {
  // Create a weather sequence: Rain -> Lightning -> Clear
  std::vector<EventDefinition> weatherSequence = {{"Weather",
                                                   "StartRain",
                                                   {{"weatherType", "Rainy"}},
                                                   {{"intensity", 0.5f}},
                                                   {}},
                                                  {"Weather",
                                                   "Thunderstorm",
                                                   {{"weatherType", "Stormy"}},
                                                   {{"intensity", 0.9f}},
                                                   {}},
                                                  {"Weather",
                                                   "ClearSkies",
                                                   {{"weatherType", "Clear"}},
                                                   {{"transitionTime", 8.0f}},
                                                   {}}};

  auto sequence = EventFactory::Instance().createEventSequence(
      "WeatherSequence", weatherSequence, true);
  BOOST_CHECK_EQUAL(sequence.size(), 3);

  // Verify the sequence preserves the requested creation order
  BOOST_CHECK_EQUAL(sequence[0]->getName(), "StartRain");
  BOOST_CHECK_EQUAL(sequence[1]->getName(), "Thunderstorm");
  BOOST_CHECK_EQUAL(sequence[2]->getName(), "ClearSkies");
}

// Test event cooldown functionality
BOOST_FIXTURE_TEST_CASE(EventCooldownFunctionality, EventTypesFixture) {
  auto event = std::make_shared<WeatherEvent>("TestEvent", WeatherType::Rainy);

  // Set cooldown time
  event->setCooldown(2.0f);
  BOOST_CHECK_EQUAL(event->getCooldown(), 2.0f);
  BOOST_CHECK(!event->isOnCooldown());

  // Start cooldown
  event->startCooldown();
  BOOST_CHECK(event->isOnCooldown());

  // Reset cooldown
  event->resetCooldown();
  BOOST_CHECK(!event->isOnCooldown());
}

// Test ParticleEffectEvent creation and basic functionality
BOOST_FIXTURE_TEST_CASE(ParticleEffectEventBasics, EventTypesFixture) {
  // Test constructor with Vector2D
  Vector2D position(100.0f, 200.0f);
  ParticleEffectEvent effectEvent1("TestEffect1", ParticleEffectType::Fire,
                                   position, 1.5f, 5.0f, "group1",
                                   "fire_sound");

  // Check basic properties
  BOOST_CHECK_EQUAL(effectEvent1.getName(), "TestEffect1");
  BOOST_CHECK_EQUAL(effectEvent1.getType(), "ParticleEffect");
  BOOST_CHECK_EQUAL(effectEvent1.getEffectName(), "Fire");
  BOOST_CHECK_EQUAL(effectEvent1.getPosition().getX(), position.getX());
  BOOST_CHECK_EQUAL(effectEvent1.getPosition().getY(), position.getY());
  BOOST_CHECK_EQUAL(effectEvent1.getIntensity(), 1.5f);
  BOOST_CHECK_EQUAL(effectEvent1.getDuration(), 5.0f);
  BOOST_CHECK_EQUAL(effectEvent1.getGroupTag(), "group1");

  // Test constructor with x,y coordinates
  ParticleEffectEvent effectEvent2("TestEffect2", ParticleEffectType::Smoke,
                                   300.0f, 400.0f, 0.8f, -1.0f, "group2");
  BOOST_CHECK_EQUAL(effectEvent2.getName(), "TestEffect2");
  BOOST_CHECK_EQUAL(effectEvent2.getEffectName(), "Smoke");
  BOOST_CHECK_EQUAL(effectEvent2.getPosition().getX(), 300.0f);
  BOOST_CHECK_EQUAL(effectEvent2.getPosition().getY(), 400.0f);
  BOOST_CHECK_EQUAL(effectEvent2.getIntensity(), 0.8f);
  BOOST_CHECK_EQUAL(effectEvent2.getDuration(), -1.0f); // Infinite duration
  BOOST_CHECK_EQUAL(effectEvent2.getGroupTag(), "group2");
}

// Test ParticleEffectEvent property setters and getters
BOOST_FIXTURE_TEST_CASE(ParticleEffectEventProperties, EventTypesFixture) {
  ParticleEffectEvent effectEvent("PropTest", ParticleEffectType::Sparks, 50.0f,
                                  60.0f);
  // Test position setters
  effectEvent.setPosition(150.0f, 250.0f);
  BOOST_CHECK_EQUAL(effectEvent.getPosition().getX(), 150.0f);
  BOOST_CHECK_EQUAL(effectEvent.getPosition().getY(), 250.0f);

  Vector2D newPos(200.0f, 300.0f);
  effectEvent.setPosition(newPos);
  BOOST_CHECK_EQUAL(effectEvent.getPosition().getX(), newPos.getX());
  BOOST_CHECK_EQUAL(effectEvent.getPosition().getY(), newPos.getY());

  // Test intensity adjustment
  effectEvent.setIntensity(2.5f);
  BOOST_CHECK_EQUAL(effectEvent.getIntensity(), 2.5f);

  // Test duration setting
  effectEvent.setDuration(15.0f);
  BOOST_CHECK_EQUAL(effectEvent.getDuration(), 15.0f);

  // Test group tagging
  effectEvent.setGroupTag("newGroup");
  BOOST_CHECK_EQUAL(effectEvent.getGroupTag(), "newGroup");

  // Test default values
  ParticleEffectEvent defaultEvent("Default", ParticleEffectType::Rain, 0.0f,
                                   0.0f);
  BOOST_CHECK_EQUAL(defaultEvent.getIntensity(), 1.0f);
  BOOST_CHECK_EQUAL(defaultEvent.getDuration(), -1.0f);
  BOOST_CHECK_EQUAL(defaultEvent.getGroupTag(), "");
}

// Test ParticleEffectEvent conditions and state
BOOST_FIXTURE_TEST_CASE(ParticleEffectEventConditions, EventTypesFixture) {
  ParticleEffectEvent effectEvent("ConditionTest", ParticleEffectType::Snow,
                                  0.0f, 0.0f);
  // Should be active by default
  BOOST_CHECK(effectEvent.isActive());

  // Check conditions - should pass basic checks (active state, non-empty effect
  // name) Note: ParticleManager availability check will fail in test
  // environment
  BOOST_CHECK(
      !effectEvent
           .checkConditions()); // Fails due to ParticleManager not initialized

  // Test with empty effect name
  ParticleEffectEvent emptyEvent("Empty", ParticleEffectType::Rain, 0.0f, 0.0f);
  BOOST_CHECK(
      !emptyEvent.checkConditions()); // Should fail due to empty effect name

  // Test inactive event
  effectEvent.setActive(false);
  BOOST_CHECK(
      !effectEvent.checkConditions()); // Should fail due to inactive state
}

// Test ParticleEffectEvent lifecycle
BOOST_FIXTURE_TEST_CASE(ParticleEffectEventLifecycle, EventTypesFixture) {
  ParticleEffectEvent effectEvent("LifecycleTest", ParticleEffectType::Fire,
                                  100.0f, 100.0f, 1.0f, 3.0f);

  // Initially should not be active
  BOOST_CHECK(!effectEvent.isEffectActive());

  // Test update method (should not crash)
  effectEvent.update();

  // Test reset method
  effectEvent.reset();
  BOOST_CHECK(!effectEvent.isEffectActive());

  // Test clean method
  effectEvent.clean();
  BOOST_CHECK(!effectEvent.isEffectActive());

  // Test stopEffect method (should not crash even if no effect is running)
  effectEvent.stopEffect();
  BOOST_CHECK(!effectEvent.isEffectActive());
}

// Test ParticleEffectEvent edge cases
BOOST_FIXTURE_TEST_CASE(ParticleEffectEventEdgeCases, EventTypesFixture) {
  // Test with extreme values
  ParticleEffectEvent extremeEvent("Extreme", ParticleEffectType::Custom,
                                   -1000.0f, 1000.0f, 0.0f, 0.0f);
  BOOST_CHECK_EQUAL(extremeEvent.getPosition().getX(), -1000.0f);
  BOOST_CHECK_EQUAL(extremeEvent.getPosition().getY(), 1000.0f);
  BOOST_CHECK_EQUAL(extremeEvent.getIntensity(), 0.0f);
  BOOST_CHECK_EQUAL(extremeEvent.getDuration(), 0.0f);

  // Test with very high intensity
  extremeEvent.setIntensity(10.0f);
  BOOST_CHECK_EQUAL(extremeEvent.getIntensity(), 10.0f);

  // Test with very long duration
  extremeEvent.setDuration(9999.0f);
  BOOST_CHECK_EQUAL(extremeEvent.getDuration(), 9999.0f);

  // Test execution without ParticleManager (should handle gracefully)
  extremeEvent.execute();                      // Should not crash
  BOOST_CHECK(!extremeEvent.isEffectActive()); // Effect won't be active due to
                                               // no ParticleManager
}

// ============================================================================
// TIME EVENT TESTS
// ============================================================================

#include "events/TimeEvent.hpp"
#include "managers/GameTimeManager.hpp"

// Test HourChangedEvent creation and properties
BOOST_FIXTURE_TEST_CASE(HourChangedEventBasics, EventTypesFixture) {
  HourChangedEvent event(14, false);

  BOOST_CHECK_EQUAL(event.getHour(), 14);
  BOOST_CHECK(!event.isNight());
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::HourChanged);
  BOOST_CHECK_EQUAL(event.getTypeName(), "HourChangedEvent");
  BOOST_CHECK_EQUAL(event.getName(), "HourChangedEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Test night flag
  HourChangedEvent nightEvent(2, true);
  BOOST_CHECK_EQUAL(nightEvent.getHour(), 2);
  BOOST_CHECK(nightEvent.isNight());

  // Test reset
  HourChangedEvent resetEvent(10, true);
  resetEvent.reset();
  BOOST_CHECK_EQUAL(resetEvent.getHour(), 0);
  BOOST_CHECK(!resetEvent.isNight());
}

// Test DayChangedEvent creation and properties
BOOST_FIXTURE_TEST_CASE(DayChangedEventBasics, EventTypesFixture) {
  DayChangedEvent event(15, 15, 0, "Bloomtide");

  BOOST_CHECK_EQUAL(event.getDay(), 15);
  BOOST_CHECK_EQUAL(event.getDayOfMonth(), 15);
  BOOST_CHECK_EQUAL(event.getMonth(), 0);
  BOOST_CHECK_EQUAL(event.getMonthName(), "Bloomtide");
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::DayChanged);
  BOOST_CHECK_EQUAL(event.getTypeName(), "DayChangedEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Test reset
  DayChangedEvent resetEvent(5, 5, 1, "Sunpeak");
  resetEvent.reset();
  BOOST_CHECK_EQUAL(resetEvent.getDay(), 0);
  BOOST_CHECK_EQUAL(resetEvent.getDayOfMonth(), 0);
  BOOST_CHECK_EQUAL(resetEvent.getMonth(), 0);
  BOOST_CHECK(resetEvent.getMonthName().empty());
}

// Test MonthChangedEvent creation and properties
BOOST_FIXTURE_TEST_CASE(MonthChangedEventBasics, EventTypesFixture) {
  MonthChangedEvent event(1, "Sunpeak", Season::Summer);

  BOOST_CHECK_EQUAL(event.getMonth(), 1);
  BOOST_CHECK_EQUAL(event.getMonthName(), "Sunpeak");
  BOOST_CHECK(event.getSeason() == Season::Summer);
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::MonthChanged);
  BOOST_CHECK_EQUAL(event.getTypeName(), "MonthChangedEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Test reset
  MonthChangedEvent resetEvent(2, "Harvestmoon", Season::Fall);
  resetEvent.reset();
  BOOST_CHECK_EQUAL(resetEvent.getMonth(), 0);
  BOOST_CHECK(resetEvent.getMonthName().empty());
  BOOST_CHECK(resetEvent.getSeason() == Season::Spring);
}

// Test SeasonChangedEvent creation and properties
BOOST_FIXTURE_TEST_CASE(SeasonChangedEventBasics, EventTypesFixture) {
  SeasonChangedEvent event(Season::Winter, Season::Fall, "Winter");

  BOOST_CHECK(event.getSeason() == Season::Winter);
  BOOST_CHECK(event.getPreviousSeason() == Season::Fall);
  BOOST_CHECK_EQUAL(event.getSeasonName(), "Winter");
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::SeasonChanged);
  BOOST_CHECK_EQUAL(event.getTypeName(), "SeasonChangedEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Test reset
  SeasonChangedEvent resetEvent(Season::Summer, Season::Spring, "Summer");
  resetEvent.reset();
  BOOST_CHECK(resetEvent.getSeason() == Season::Spring);
  BOOST_CHECK(resetEvent.getPreviousSeason() == Season::Spring);
  BOOST_CHECK(resetEvent.getSeasonName().empty());
}

// Test YearChangedEvent creation and properties
BOOST_FIXTURE_TEST_CASE(YearChangedEventBasics, EventTypesFixture) {
  YearChangedEvent event(5);

  BOOST_CHECK_EQUAL(event.getYear(), 5);
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::YearChanged);
  BOOST_CHECK_EQUAL(event.getTypeName(), "YearChangedEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Test reset
  YearChangedEvent resetEvent(10);
  resetEvent.reset();
  BOOST_CHECK_EQUAL(resetEvent.getYear(), 0);
}

// Test WeatherCheckEvent creation and properties
BOOST_FIXTURE_TEST_CASE(WeatherCheckEventBasics, EventTypesFixture) {
  WeatherCheckEvent event(Season::Winter, WeatherType::Snowy);

  BOOST_CHECK(event.getSeason() == Season::Winter);
  BOOST_CHECK(event.getRecommendedWeather() == WeatherType::Snowy);
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::WeatherCheck);
  BOOST_CHECK_EQUAL(event.getTypeName(), "WeatherCheckEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Test reset
  WeatherCheckEvent resetEvent(Season::Summer, WeatherType::Clear);
  resetEvent.reset();
  BOOST_CHECK(resetEvent.getSeason() == Season::Spring);
}

// Test TimePeriodChangedEvent creation and properties
BOOST_FIXTURE_TEST_CASE(TimePeriodChangedEventBasics, EventTypesFixture) {
  TimePeriodVisuals visuals = TimePeriodVisuals::getNight();
  TimePeriodChangedEvent event(TimePeriod::Night, TimePeriod::Evening, visuals);

  BOOST_CHECK(event.getPeriod() == TimePeriod::Night);
  BOOST_CHECK(event.getPreviousPeriod() == TimePeriod::Evening);
  BOOST_CHECK_EQUAL(std::string(event.getPeriodName()), "Night");
  BOOST_CHECK(event.getTimeEventType() == TimeEventType::TimePeriodChanged);
  BOOST_CHECK_EQUAL(event.getTypeName(), "TimePeriodChangedEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Time);

  // Check visuals
  const auto& v = event.getVisuals();
  BOOST_CHECK_EQUAL(v.overlayR, 20);
  BOOST_CHECK_EQUAL(v.overlayG, 20);
  BOOST_CHECK_EQUAL(v.overlayB, 60);
  BOOST_CHECK_EQUAL(v.overlayA, 90);

  // Test reset
  TimePeriodChangedEvent resetEvent(TimePeriod::Morning, TimePeriod::Night,
                                    TimePeriodVisuals::getMorning());
  resetEvent.reset();
  BOOST_CHECK(resetEvent.getPeriod() == TimePeriod::Day);
  BOOST_CHECK(resetEvent.getPreviousPeriod() == TimePeriod::Day);
}

// Test TimePeriodVisuals factory methods
BOOST_FIXTURE_TEST_CASE(TimePeriodVisualsFactoryMethods, EventTypesFixture) {
  // Morning - red-orange dawn
  auto morning = TimePeriodVisuals::getMorning();
  BOOST_CHECK_EQUAL(morning.overlayR, 255);
  BOOST_CHECK_EQUAL(morning.overlayG, 140);
  BOOST_CHECK_EQUAL(morning.overlayB, 80);
  BOOST_CHECK_EQUAL(morning.overlayA, 30);

  // Day - slight yellow
  auto day = TimePeriodVisuals::getDay();
  BOOST_CHECK_EQUAL(day.overlayR, 255);
  BOOST_CHECK_EQUAL(day.overlayG, 255);
  BOOST_CHECK_EQUAL(day.overlayB, 200);
  BOOST_CHECK_EQUAL(day.overlayA, 8);

  // Evening - orange-red sunset
  auto evening = TimePeriodVisuals::getEvening();
  BOOST_CHECK_EQUAL(evening.overlayR, 255);
  BOOST_CHECK_EQUAL(evening.overlayG, 80);
  BOOST_CHECK_EQUAL(evening.overlayB, 40);
  BOOST_CHECK_EQUAL(evening.overlayA, 40);

  // Night - darker blue/purple
  auto night = TimePeriodVisuals::getNight();
  BOOST_CHECK_EQUAL(night.overlayR, 20);
  BOOST_CHECK_EQUAL(night.overlayG, 20);
  BOOST_CHECK_EQUAL(night.overlayB, 60);
  BOOST_CHECK_EQUAL(night.overlayA, 90);

  // Test getForPeriod
  auto forMorning = TimePeriodVisuals::getForPeriod(TimePeriod::Morning);
  BOOST_CHECK_EQUAL(forMorning.overlayA, morning.overlayA);

  auto forDay = TimePeriodVisuals::getForPeriod(TimePeriod::Day);
  BOOST_CHECK_EQUAL(forDay.overlayA, day.overlayA);

  auto forEvening = TimePeriodVisuals::getForPeriod(TimePeriod::Evening);
  BOOST_CHECK_EQUAL(forEvening.overlayA, evening.overlayA);

  auto forNight = TimePeriodVisuals::getForPeriod(TimePeriod::Night);
  BOOST_CHECK_EQUAL(forNight.overlayA, night.overlayA);
}

// Test TimeEvent base class
BOOST_FIXTURE_TEST_CASE(TimeEventBaseClass, EventTypesFixture) {
  HourChangedEvent event(12, false);

  // Test Event interface methods
  BOOST_CHECK(event.checkConditions());  // Always true for TimeEvent
  BOOST_CHECK_EQUAL(event.getType(), "HourChangedEvent");
  BOOST_CHECK_EQUAL(event.getName(), "HourChangedEvent");

  // Test update/execute/clean don't crash
  event.update();
  event.execute();
  event.clean();
}

// ============================================================================
// EVENTTYPEID COVERAGE TESTS
// Tests for all EventTypeId enum values to ensure complete coverage
// ============================================================================

#include "events/CameraEvent.hpp"
#include "events/CollisionObstacleChangedEvent.hpp"
#include "events/HarvestResourceEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "events/WorldEvent.hpp"
#include "events/WorldTriggerEvent.hpp"

// Test EventTypeId enum values
BOOST_AUTO_TEST_CASE(TestEventTypeIdEnumValues) {
  // Verify EventTypeId enum has expected values
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Weather), 0);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::SceneChange), 1);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::NPCSpawn), 2);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::ParticleEffect), 3);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::ResourceChange), 4);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::World), 5);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Camera), 6);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Harvest), 7);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Collision), 8);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::WorldTrigger), 9);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::CollisionObstacleChanged), 10);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Custom), 11);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Time), 12);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Combat), 13);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::Entity), 14);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::BehaviorMessage), 15);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::MerchantSpawn), 16);
  BOOST_CHECK_EQUAL(static_cast<uint8_t>(EventTypeId::COUNT), 17);
}

// Test ResourceChangeEvent
BOOST_FIXTURE_TEST_CASE(ResourceChangeEventBasics, EventTypesFixture) {
    // Create a mock player entity
    auto player = std::make_shared<Player>();
    player->initializeInventory(); // Important for the test

    // Create a resource handle
    VoidLight::ResourceHandle woodHandle(1, 1);

    // Get player's EntityHandle
    EntityHandle playerHandle = player->getHandle();

    // Create the event
    ResourceChangeEvent event(playerHandle, woodHandle, 100, 150, "crafted");

    // Check event properties
    BOOST_CHECK(event.getOwnerHandle() == playerHandle);
    BOOST_CHECK(event.getResourceHandle() == woodHandle);
    BOOST_CHECK_EQUAL(event.getOldQuantity(), 100);
    BOOST_CHECK_EQUAL(event.getNewQuantity(), 150);
    BOOST_CHECK_EQUAL(event.getQuantityChange(), 50);
    BOOST_CHECK_EQUAL(event.getChangeReason(), "crafted");

    // Check convenience methods
    BOOST_CHECK(event.isIncrease());
    BOOST_CHECK(!event.isDecrease());

    // Test reset - clears all data for event pool recycling
    event.reset();
    BOOST_CHECK(event.getOwnerHandle() == EntityHandle{});
    BOOST_CHECK_EQUAL(event.getNewQuantity(), 0);
    BOOST_CHECK_EQUAL(event.getOldQuantity(), 0);
    BOOST_CHECK_EQUAL(event.getQuantityChange(), 0);
    BOOST_CHECK(event.getChangeReason().empty());
}

// Test WorldEvent types
BOOST_FIXTURE_TEST_CASE(WorldEventBasics, EventTypesFixture) {
  // Test WorldLoadedEvent
  WorldLoadedEvent loadedEvent("test_world", 100, 100);
  BOOST_CHECK_EQUAL(loadedEvent.getName(), "WorldLoadedEvent");
  BOOST_CHECK_EQUAL(loadedEvent.getType(), "WorldLoadedEvent");
  BOOST_CHECK(loadedEvent.getTypeId() == EventTypeId::World);
  BOOST_CHECK_EQUAL(loadedEvent.getWorldId(), "test_world");
  BOOST_CHECK_EQUAL(loadedEvent.getWidth(), 100);
  BOOST_CHECK_EQUAL(loadedEvent.getHeight(), 100);

  // Test TileChangedEvent
  TileChangedEvent tileEvent(10, 20, "biome_change");
  BOOST_CHECK_EQUAL(tileEvent.getName(), "TileChangedEvent");
  BOOST_CHECK(tileEvent.getTypeId() == EventTypeId::World);
  BOOST_CHECK_EQUAL(tileEvent.getX(), 10);
  BOOST_CHECK_EQUAL(tileEvent.getY(), 20);
  BOOST_CHECK_EQUAL(tileEvent.getChangeType(), "biome_change");

  // Test WorldGeneratedEvent
  WorldGeneratedEvent genEvent("world_001", 200, 200, 2.5f);
  BOOST_CHECK_EQUAL(genEvent.getName(), "WorldGeneratedEvent");
  BOOST_CHECK(genEvent.getTypeId() == EventTypeId::World);
  BOOST_CHECK_EQUAL(genEvent.getWorldId(), "world_001");
  BOOST_CHECK_EQUAL(genEvent.getWidth(), 200);
  BOOST_CHECK_EQUAL(genEvent.getHeight(), 200);
  BOOST_CHECK_EQUAL(genEvent.getGenerationTime(), 2.5f);
}

// Test CameraEvent
BOOST_FIXTURE_TEST_CASE(CameraEventBasics, EventTypesFixture) {
    // Test CameraMovedEvent
    CameraMovedEvent movedEvent(Vector2D(100.0f, 200.0f), Vector2D(50.0f, 150.0f));
    BOOST_CHECK(movedEvent.getEventType() == CameraEventType::CameraMoved);
    BOOST_CHECK_EQUAL(movedEvent.getNewX(), 100.0f);
    BOOST_CHECK_EQUAL(movedEvent.getNewY(), 200.0f);
    BOOST_CHECK_EQUAL(movedEvent.getOldX(), 50.0f);
    BOOST_CHECK_EQUAL(movedEvent.getOldY(), 150.0f);
    BOOST_CHECK(movedEvent.getTypeId() == EventTypeId::Camera);
    BOOST_CHECK_EQUAL(movedEvent.getTypeName(), "CameraMovedEvent");


    // Test CameraZoomChangedEvent
    CameraZoomChangedEvent zoomEvent(2.0f, 1.0f);
    BOOST_CHECK(zoomEvent.getEventType() == CameraEventType::CameraZoomChanged);
    BOOST_CHECK_EQUAL(zoomEvent.getNewZoom(), 2.0f);
    BOOST_CHECK_EQUAL(zoomEvent.getOldZoom(), 1.0f);
    BOOST_CHECK(zoomEvent.getTypeId() == EventTypeId::Camera);

    // Test CameraShakeStartedEvent
    CameraShakeStartedEvent shakeEvent(1.0f, 5.0f);
    BOOST_CHECK(shakeEvent.getEventType() == CameraEventType::CameraShakeStarted);
    BOOST_CHECK_EQUAL(shakeEvent.getDuration(), 1.0f);
    BOOST_CHECK_EQUAL(shakeEvent.getIntensity(), 5.0f);
    BOOST_CHECK(shakeEvent.getTypeId() == EventTypeId::Camera);
}

// Test HarvestResourceEvent
BOOST_FIXTURE_TEST_CASE(HarvestResourceEventBasics, EventTypesFixture) {
  HarvestResourceEvent event(42, 10, 20, "wood");

  BOOST_CHECK_EQUAL(event.getName(), "HarvestResource");
  BOOST_CHECK_EQUAL(event.getType(), "HarvestResourceEvent");
  BOOST_CHECK(event.getTypeId() == EventTypeId::Harvest);
  BOOST_CHECK_EQUAL(event.getEntityId(), 42);
  BOOST_CHECK_EQUAL(event.getTargetX(), 10);
  BOOST_CHECK_EQUAL(event.getTargetY(), 20);
  BOOST_CHECK_EQUAL(event.getResourceType(), "wood");

  // Test checkConditions (should be valid with proper coords)
  BOOST_CHECK(event.checkConditions());

}

// Test WorldTriggerEvent
BOOST_FIXTURE_TEST_CASE(WorldTriggerEventBasics, EventTypesFixture) {
    WorldTriggerEvent event(42, 1, VoidLight::TriggerTag::Water, Vector2D(5, 10), TriggerPhase::Enter);

    BOOST_CHECK_EQUAL(event.getName(), "WorldTriggerEvent");
    BOOST_CHECK_EQUAL(event.getType(), "WorldTriggerEvent");
    BOOST_CHECK(event.getTypeId() == EventTypeId::WorldTrigger);
    BOOST_CHECK_EQUAL(event.getPlayerId(), 42);
    BOOST_CHECK_EQUAL(event.getTriggerId(), 1);
    BOOST_CHECK(event.getTag() == VoidLight::TriggerTag::Water);
    BOOST_CHECK_EQUAL(event.getPosition().getX(), 5);
    BOOST_CHECK_EQUAL(event.getPosition().getY(), 10);
    BOOST_CHECK(event.getPhase() == TriggerPhase::Enter);
}

// Test CollisionObstacleChangedEvent
BOOST_FIXTURE_TEST_CASE(CollisionObstacleChangedEventBasics, EventTypesFixture) {
    CollisionObstacleChangedEvent event(CollisionObstacleChangedEvent::ChangeType::REMOVED, Vector2D(100, 200), 128.0f, "obstacle_removed");

    BOOST_CHECK_EQUAL(event.getName(), "collision_obstacle_changed");
    BOOST_CHECK_EQUAL(event.getType(), "CollisionObstacleChanged");
    BOOST_CHECK(event.getTypeId() == EventTypeId::CollisionObstacleChanged);
    BOOST_CHECK(event.getChangeType() == CollisionObstacleChangedEvent::ChangeType::REMOVED);
    BOOST_CHECK_EQUAL(event.getPosition().getX(), 100);
    BOOST_CHECK_EQUAL(event.getPosition().getY(), 200);
    BOOST_CHECK_EQUAL(event.getRadius(), 128.0f);
    BOOST_CHECK_EQUAL(event.getDescription(), "obstacle_removed");
}

// Test all event types return correct TypeId
BOOST_FIXTURE_TEST_CASE(AllEventTypesReturnCorrectTypeId, EventTypesFixture) {
  // Weather
  WeatherEvent weatherEvent("test", WeatherType::Clear);
  BOOST_CHECK(weatherEvent.getTypeId() == EventTypeId::Weather);

  // SceneChange
  SceneChangeEvent sceneEvent("test", "target");
  BOOST_CHECK(sceneEvent.getTypeId() == EventTypeId::SceneChange);

  // NPCSpawn
  NPCSpawnEvent npcEvent("test", "Guard");
  BOOST_CHECK(npcEvent.getTypeId() == EventTypeId::NPCSpawn);

  MerchantSpawnEvent merchantEvent("merchant_test", "GeneralMerchant");
  BOOST_CHECK(merchantEvent.getTypeId() == EventTypeId::MerchantSpawn);

  // ParticleEffect
  ParticleEffectEvent particleEvent("test", ParticleEffectType::Fire, 0.0f, 0.0f);
  BOOST_CHECK(particleEvent.getTypeId() == EventTypeId::ParticleEffect);

  // ResourceChange
  auto player = std::make_shared<Player>();
  VoidLight::ResourceHandle goldHandle(2, 1);
  ResourceChangeEvent resourceEvent(player->getHandle(), goldHandle, 0, 10, "looted");
  BOOST_CHECK(resourceEvent.getTypeId() == EventTypeId::ResourceChange);

  // World
  WorldLoadedEvent worldEvent("world", 10, 10);
  BOOST_CHECK(worldEvent.getTypeId() == EventTypeId::World);

  // Camera
  CameraMovedEvent cameraEvent(Vector2D(0.0f, 0.0f), Vector2D(0.0f, 0.0f));
  BOOST_CHECK(cameraEvent.getTypeId() == EventTypeId::Camera);

  // Harvest
  HarvestResourceEvent harvestEvent(1, 0, 0, "wood");
  BOOST_CHECK(harvestEvent.getTypeId() == EventTypeId::Harvest);

  // WorldTrigger
  WorldTriggerEvent triggerEvent(1, 2, VoidLight::TriggerTag::None, Vector2D(0,0), TriggerPhase::Enter);
  BOOST_CHECK(triggerEvent.getTypeId() == EventTypeId::WorldTrigger);

  // CollisionObstacleChanged
  CollisionObstacleChangedEvent obstacleEvent(CollisionObstacleChangedEvent::ChangeType::REMOVED, Vector2D(0, 0), 64.0f, "removed");
  BOOST_CHECK(obstacleEvent.getTypeId() == EventTypeId::CollisionObstacleChanged);

  // Time
  HourChangedEvent timeEvent(12, false);
  BOOST_CHECK(timeEvent.getTypeId() == EventTypeId::Time);
}
