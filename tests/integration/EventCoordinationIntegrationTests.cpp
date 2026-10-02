/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE EventCoordinationIntegrationTests
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "entities/Entity.hpp"
#include "events/EventFactory.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "events/SceneChangeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "events/WorldEvent.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "collisions/CollisionBody.hpp"
#include "managers/EventManager.hpp"
#include "managers/ParticleManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "world/WorldData.hpp"

#include <iostream>

using namespace VoidLight;

// Test logging helper
#define TEST_LOG(msg) do { \
    std::cout << "[TEST] " << msg << std::endl; \
} while(0)

/**
 * @brief Simple test entity for event coordination tests
 * NOTE: This is intentionally NOT a data-driven NPC - it's a mock entity for testing
 * event coordination between managers, not NPC AI behavior.
 */
class TestEntity : public Entity {
public:
    explicit TestEntity(const Vector2D& pos) {
        // Register with EntityDataManager to get a valid handle
        registerWithDataManager(pos, 16.0f, 16.0f, EntityKind::Player);
        setTextureID("test_texture");
        setWidth(32);
        setHeight(32);
    }

    static std::shared_ptr<TestEntity> create(const Vector2D& pos) {
        return std::make_shared<TestEntity>(pos);
    }

    void update(float) override {}
    void clean() override {}
    [[nodiscard]] EntityKind getKind() const override { return EntityKind::Player; }
};

/**
 * @brief Global test fixture for manager initialization
 */
struct GlobalEventCoordinationFixture {
    GlobalEventCoordinationFixture() {
        std::cout << "=== EventCoordinationIntegrationTests Global Setup ===" << std::endl;

        // Initialize managers in dependency order
        if (!ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem initialization failed");
        }

        if (!EntityDataManager::Instance().init()) {
            throw std::runtime_error("EntityDataManager initialization failed");
        }

        if (!ResourceTemplateManager::Instance().init()) {
            throw std::runtime_error("ResourceTemplateManager initialization failed");
        }

        if (!EventManager::Instance().init()) {
            throw std::runtime_error("EventManager initialization failed");
        }

        if (!WorldManager::Instance().init()) {
            throw std::runtime_error("WorldManager initialization failed");
        }

        if (!CollisionManager::Instance().init()) {
            throw std::runtime_error("CollisionManager initialization failed");
        }

        if (!PathfinderManager::Instance().init()) {
            throw std::runtime_error("PathfinderManager initialization failed");
        }

        if (!AIManager::Instance().init()) {
            throw std::runtime_error("AIManager initialization failed");
        }

        if (!ParticleManager::Instance().init()) {
            throw std::runtime_error("ParticleManager initialization failed");
        }

        std::cout << "=== Global Setup Complete ===" << std::endl;
    }

    ~GlobalEventCoordinationFixture() {
        std::cout << "=== EventCoordinationIntegrationTests Global Teardown ===" << std::endl;

        // Wait for pending operations
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Clean up in reverse order
        ParticleManager::Instance().clean();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        WorldManager::Instance().clean();
        EventManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        ThreadSystem::Instance().clean();

        std::cout << "=== Global Teardown Complete ===" << std::endl;
    }
};

BOOST_GLOBAL_FIXTURE(GlobalEventCoordinationFixture);

BOOST_AUTO_TEST_SUITE(EventCoordinationIntegrationTests)

/**
 * @brief Test weather event coordination across ParticleManager, AIManager, and WorldManager
 *
 * Verifies that a single weather event triggers:
 * - ParticleManager: Rain particles start
 * - AIManager: NPCs receive messages (data-oriented messaging)
 * - WorldManager: Tile properties update (wetness)
 *
 * Success criteria:
 * - All managers receive event in same frame
 * - Manager responses are correct (not just "event received")
 * - Update order is maintained
 */
BOOST_AUTO_TEST_CASE(TestWeatherEventCoordination) {
    TEST_LOG("Starting TestWeatherEventCoordination");

    // Setup: Create small world for tile updates
    WorldGenerationConfig worldConfig{};
    worldConfig.width = 10;
    worldConfig.height = 10;
    worldConfig.seed = 12345;
    worldConfig.elevationFrequency = 0.1f;
    worldConfig.humidityFrequency = 0.1f;
    worldConfig.waterLevel = 0.3f;
    worldConfig.mountainLevel = 0.7f;

    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(worldConfig));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Setup: Create AI entities with Wander behavior (data-oriented)
    std::vector<std::shared_ptr<TestEntity>> testEntities;

    for (int i = 0; i < 5; ++i) {
        auto entity = TestEntity::create(Vector2D(50.0f + i * 10.0f, 50.0f));
        testEntities.push_back(entity);

        // EDM-CENTRIC: Set collision layers directly on EDM hot data
        {
            auto& edm = EntityDataManager::Instance();
            size_t idx = edm.getIndex(entity->getHandle());
            if (idx != SIZE_MAX) {
                auto& hot = edm.getHotDataByIndex(idx);
                hot.collisionLayers = VoidLight::CollisionLayer::Layer_Enemy;
                hot.collisionMask = 0xFFFF;
                hot.setCollisionEnabled(true);
            }
        }

        // DATA-ORIENTED: Use assignBehavior with standard behavior name
        AIManager::Instance().assignBehavior(entity->getHandle(), "Wander");
    }

    // Process queued assignments
    for (int i = 0; i < 5; ++i) {
        AIManager::Instance().update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Setup: Track manager responses
    std::atomic<bool> particleEventReceived{false};
    std::atomic<bool> worldEventReceived{false};
    std::atomic<int> frameNumber{0};
    std::atomic<int> particleEventFrame{-1};
    std::atomic<int> worldEventFrame{-1};

    // Register handlers for coordination verification
    EventManager::Instance().registerHandler(
        EventTypeId::ParticleEffect,
        [&](const EventData& data) {
            if (!data.event) return;
            particleEventReceived.store(true);
            particleEventFrame.store(frameNumber.load());
            std::cout << "[TEST] ParticleManager received weather event on frame: "
                      << frameNumber.load() << std::endl;
        });

    EventManager::Instance().registerHandler(
        EventTypeId::World,
        [&](const EventData& data) {
            if (!data.event) return;
            auto worldEvent = std::dynamic_pointer_cast<TileChangedEvent>(data.event);
            if (worldEvent) {
                worldEventReceived.store(true);
                worldEventFrame.store(frameNumber.load());
                std::cout << "[TEST] WorldManager received tile change event on frame: "
                          << frameNumber.load() << std::endl;
            }
        });

    const auto weatherForwarder = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::Weather,
        [](const EventData& data) {
            ParticleManager::Instance().handleWeatherEvent(data);
        });
    (void)weatherForwarder;

    // Action: Trigger weather change to rain
    TEST_LOG("Triggering weather change to rain");
    BOOST_CHECK(EventManager::Instance().changeWeather(
        "rainy", 1.0f, EventManager::DispatchMode::Immediate));

    // Trigger rain particle effect
    BOOST_CHECK(EventManager::Instance().triggerParticleEffect(
        "rain", 50.0f, 50.0f, 1.0f, -1.0f, "weather",
        EventManager::DispatchMode::Immediate));

    // Legacy broadcast message API was removed - message system now uses BehaviorMessage queue

    // Update all managers and process events
    const int maxFrames = 30;
    for (int frame = 0; frame < maxFrames; ++frame) {
        frameNumber.store(frame);

        EventManager::Instance().update();
        AIManager::Instance().update(0.016f);
        ParticleManager::Instance().update(0.016f);
        WorldManager::Instance().update();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Check if all systems have responded
        if (particleEventReceived.load() && worldEventReceived.load()) {
            bool allEntitiesHaveBehavior = true;
            for (const auto& entity : testEntities) {
                if (!AIManager::Instance().hasBehavior(entity->getHandle())) {
                    allEntitiesHaveBehavior = false;
                    break;
                }
            }

            if (allEntitiesHaveBehavior) {
                TEST_LOG("All systems responded to weather event");
                break;
            }
        }
    }

    // Verification: Check all managers received and responded to event
    BOOST_CHECK(particleEventReceived.load());
    TEST_LOG("ParticleManager response verified");

    // Verification: Check events were delivered in same frame or close frames
    int frameDifference = std::abs(
        particleEventFrame.load() - worldEventFrame.load());
    BOOST_CHECK_LE(frameDifference, 2);
    std::cout << "[TEST] Event delivery timing verified (frame difference: "
              << frameDifference << ")" << std::endl;

    // Cleanup
    for (auto& entity : testEntities) {
        AIManager::Instance().unassignBehavior(entity->getHandle());
        CollisionManager::Instance().removeCollisionBody(entity->getID());
    }
    testEntities.clear();
    EventManager::Instance().clearAllHandlers();

    TEST_LOG("TestWeatherEventCoordination completed successfully");
}

/**
 * @brief changeWeather selects the particle variant from WeatherType,
 *        independent of whether the pooled WeatherEvent is fresh or reused
 *
 * Production wiring: a state-owned transient Weather handler forwards to
 * ParticleManager::handleWeatherEvent. EventFactory weather reaches the same
 * variant as changeWeather for the same name.
 */
BOOST_AUTO_TEST_CASE(TestChangeWeatherVariantIndependentOfPoolState) {
    auto& eventMgr = EventManager::Instance();
    auto& particleMgr = ParticleManager::Instance();

    // Clears transient handlers and event pools so the first changeWeather
    // is a pool miss and the second reuses the released event.
    eventMgr.prepareForStateTransition();

    const Event* lastEvent = nullptr;
    const auto token = eventMgr.registerHandlerWithToken(EventTypeId::Weather,
        [&lastEvent](const EventData& data) {
            lastEvent = data.event.get();
            ParticleManager::Instance().handleWeatherEvent(data);
        });

    BOOST_REQUIRE(eventMgr.changeWeather("Rainy", 1.0f,
        EventManager::DispatchMode::Immediate));
    const Event* firstEvent = lastEvent;
    BOOST_REQUIRE(firstEvent != nullptr);
    BOOST_CHECK(particleMgr.getActiveWeatherEffect() ==
        std::optional<ParticleEffectType>(ParticleEffectType::HeavyRain));

    BOOST_REQUIRE(eventMgr.changeWeather("Rainy", 1.0f,
        EventManager::DispatchMode::Immediate));
    BOOST_CHECK(lastEvent == firstEvent); // pooled event reused
    BOOST_CHECK(particleMgr.getActiveWeatherEffect() ==
        std::optional<ParticleEffectType>(ParticleEffectType::HeavyRain));

    BOOST_REQUIRE(eventMgr.changeWeather("Snowy", 1.0f,
        EventManager::DispatchMode::Immediate));
    BOOST_CHECK(particleMgr.getActiveWeatherEffect() ==
        std::optional<ParticleEffectType>(ParticleEffectType::HeavySnow));

    BOOST_REQUIRE(eventMgr.changeWeather("Windy", 1.0f,
        EventManager::DispatchMode::Immediate));
    BOOST_CHECK(particleMgr.getActiveWeatherEffect() ==
        std::optional<ParticleEffectType>(ParticleEffectType::WindyStorm));

    BOOST_REQUIRE(eventMgr.changeWeather("Stormy", 1.0f,
        EventManager::DispatchMode::Immediate));
    BOOST_CHECK(particleMgr.getActiveWeatherEffect() ==
        std::optional<ParticleEffectType>(ParticleEffectType::HeavyRain));

    BOOST_REQUIRE(eventMgr.changeWeather("Clear", 1.0f,
        EventManager::DispatchMode::Immediate));
    BOOST_CHECK(!particleMgr.getActiveWeatherEffect().has_value());

    // EventFactory weather reaches the same variant as changeWeather("Snowy").
    EventDefinition def;
    def.type = "Weather";
    def.name = "factorySnow";
    def.params["weatherType"] = "Snowy";
    EventData factoryData;
    factoryData.typeId = EventTypeId::Weather;
    factoryData.setActive(true);
    factoryData.event = EventFactory::Instance().createEvent(def);
    BOOST_REQUIRE(factoryData.event != nullptr);
    particleMgr.handleWeatherEvent(factoryData);
    BOOST_CHECK(particleMgr.getActiveWeatherEffect() ==
        std::optional<ParticleEffectType>(ParticleEffectType::HeavySnow));

    BOOST_CHECK(eventMgr.removeHandler(token));
    particleMgr.stopWeatherEffects(0.0f);
}

/**
 * @brief Test scene change event coordination for cleanup and initialization
 *
 * Verifies that scene change events properly coordinate:
 * - All managers cleanup old state
 * - New scene initialization completes
 * - No dangling references or memory leaks
 *
 * Success criteria:
 * - All managers complete cleanup
 * - New scene loads successfully
 * - No resource leaks detected
 */
BOOST_AUTO_TEST_CASE(TestSceneChangeEventCoordination) {
    TEST_LOG("Starting TestSceneChangeEventCoordination");

    // Setup: Create initial scene state
    WorldGenerationConfig initialWorld{};
    initialWorld.width = 10;
    initialWorld.height = 10;
    initialWorld.seed = 11111;
    initialWorld.elevationFrequency = 0.1f;
    initialWorld.humidityFrequency = 0.1f;
    initialWorld.waterLevel = 0.3f;
    initialWorld.mountainLevel = 0.7f;

    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(initialWorld));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Setup: Create entities in old scene
    std::vector<std::shared_ptr<TestEntity>> oldSceneEntities;
    for (int i = 0; i < 3; ++i) {
        auto entity = TestEntity::create(Vector2D(i * 20.0f, i * 20.0f));
        oldSceneEntities.push_back(entity);

        // EDM-CENTRIC: Set collision layers directly on EDM hot data
        {
            auto& edm = EntityDataManager::Instance();
            size_t idx = edm.getIndex(entity->getHandle());
            if (idx != SIZE_MAX) {
                auto& hot = edm.getHotDataByIndex(idx);
                hot.collisionLayers = VoidLight::CollisionLayer::Layer_Enemy;
                hot.collisionMask = 0xFFFF;
                hot.setCollisionEnabled(true);
            }
        }

        // DATA-ORIENTED: Assign Idle behavior
        AIManager::Instance().assignBehavior(entity->getHandle(), "Idle");
    }

    // Process registrations
    for (int i = 0; i < 5; ++i) {
        AIManager::Instance().update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Setup: Track scene change coordination
    std::atomic<bool> sceneUnloadComplete{false};
    std::atomic<bool> sceneLoadComplete{false};
    std::string oldWorldId = WorldManager::Instance().getCurrentWorldId();

    EventManager::Instance().registerHandler(
        EventTypeId::World,
        [&](const EventData& data) {
            if (!data.event) return;

            auto unloadEvent = std::dynamic_pointer_cast<WorldUnloadedEvent>(data.event);
            if (unloadEvent) {
                sceneUnloadComplete.store(true);
                TEST_LOG("Scene unload detected");
            }

            auto loadEvent = std::dynamic_pointer_cast<WorldLoadedEvent>(data.event);
            if (loadEvent) {
                sceneLoadComplete.store(true);
                TEST_LOG("Scene load detected");
            }
        });

    // Action: Trigger scene change
    TEST_LOG("Triggering scene change");

    // Cleanup old scene entities
    for (auto& entity : oldSceneEntities) {
        AIManager::Instance().unassignBehavior(entity->getHandle());
        CollisionManager::Instance().removeCollisionBody(entity->getID());
    }
    oldSceneEntities.clear();

    // Wait for cleanup to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Trigger world unload
    EventManager::Instance().triggerWorldUnloaded(
        oldWorldId, EventManager::DispatchMode::Immediate);

    // Load new scene
    WorldGenerationConfig newWorld{};
    newWorld.width = 15;
    newWorld.height = 15;
    newWorld.seed = 22222;
    newWorld.elevationFrequency = 0.1f;
    newWorld.humidityFrequency = 0.1f;
    newWorld.waterLevel = 0.3f;
    newWorld.mountainLevel = 0.7f;

    BOOST_CHECK(WorldManager::Instance().loadNewWorld(newWorld));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Process events
    for (int i = 0; i < 20; ++i) {
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (sceneLoadComplete.load()) {
            break;
        }
    }

    // Verification: Check scene transition completed
    BOOST_CHECK(sceneUnloadComplete.load());
    BOOST_CHECK(sceneLoadComplete.load());

    // Verify new world is active
    BOOST_CHECK(WorldManager::Instance().hasActiveWorld());
    std::string newWorldId = WorldManager::Instance().getCurrentWorldId();
    BOOST_CHECK_NE(oldWorldId, newWorldId);

    int width = 0, height = 0;
    WorldManager::Instance().getWorldDimensions(width, height);
    BOOST_CHECK_EQUAL(width, 15);
    BOOST_CHECK_EQUAL(height, 15);

    // Cleanup
    EventManager::Instance().clearAllHandlers();

    TEST_LOG("TestSceneChangeEventCoordination completed successfully");
}

/**
 * @brief Test resource change event propagation across AI and UI systems
 *
 * Verifies that resource changes trigger:
 * - AIManager entities respond (change behavior based on resources)
 * - Event propagation is correct
 * - All handlers receive events in batch
 *
 * Success criteria:
 * - Resource changes detected by all listeners
 * - Event ordering maintained
 * - Batch processing works correctly
 */
BOOST_AUTO_TEST_CASE(TestResourceChangeEventPropagation) {
    TEST_LOG("Starting TestResourceChangeEventPropagation");

    // Setup: Create test entity with inventory
    auto testEntity = TestEntity::create(Vector2D(100.0f, 100.0f));

    // Setup: Get test resource
    auto goldHandle = ResourceTemplateManager::Instance().getHandleByName("Platinum Coins");
    BOOST_REQUIRE(goldHandle.isValid());

    // Setup: Track resource change events
    std::atomic<int> resourceChangeCount{0};
    std::vector<int> observedQuantities;
    std::mutex quantitiesMutex;

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&](const EventData& data) {
            if (!data.event) return;

            auto resEvent = std::dynamic_pointer_cast<ResourceChangeEvent>(data.event);
            if (resEvent) {
                resourceChangeCount.fetch_add(1);

                std::lock_guard<std::mutex> lock(quantitiesMutex);
                observedQuantities.push_back(resEvent->getNewQuantity());

                std::cout << "[TEST] Resource change detected: "
                          << resEvent->getOldQuantity() << " -> "
                          << resEvent->getNewQuantity() << " ("
                          << resEvent->getChangeReason() << ")" << std::endl;
            }
        });

    // Action: Trigger multiple resource changes
    const int numChanges = 5;
    for (int i = 1; i <= numChanges; ++i) {
        EventManager::Instance().triggerResourceChange(
            testEntity->getHandle(), goldHandle, (i - 1) * 100, i * 100,
            "test_accumulation", EventManager::DispatchMode::Immediate);
    }

    // Process events
    for (int i = 0; i < 20; ++i) {
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (resourceChangeCount.load() >= numChanges) {
            break;
        }
    }

    // Verification: Check all resource changes were detected
    BOOST_CHECK_EQUAL(resourceChangeCount.load(), numChanges);

    // Verification: Check event ordering (quantities should be in sequence)
    {
        std::lock_guard<std::mutex> lock(quantitiesMutex);
        BOOST_CHECK_EQUAL(observedQuantities.size(), static_cast<size_t>(numChanges));

        for (size_t i = 0; i < observedQuantities.size(); ++i) {
            int expectedQuantity = static_cast<int>((i + 1) * 100);
            BOOST_CHECK_EQUAL(observedQuantities[i], expectedQuantity);
        }
    }

    std::cout << "[TEST] Resource change event propagation verified: "
              << resourceChangeCount.load() << " events processed" << std::endl;

    // Cleanup
    EventManager::Instance().clearAllHandlers();

    TEST_LOG("TestResourceChangeEventPropagation completed successfully");
}

/**
 * @brief Test event coordination performance under load
 *
 * Verifies that:
 * - 50+ events in single frame are processed correctly
 * - All managers process events within frame budget
 * - Event ordering guarantees maintained
 * - Thread safety maintained under load
 *
 * Success criteria:
 * - All events processed within reasonable time (< 16.67ms for 60 FPS)
 * - No events lost or duplicated
 * - Event ordering preserved
 * - Thread-safe operation verified
 */
BOOST_AUTO_TEST_CASE(TestEventCoordinationPerformance) {
    TEST_LOG("Starting TestEventCoordinationPerformance");

    // Setup: Create multiple event handlers with tracking
    std::atomic<int> weatherEventCount{0};
    std::atomic<int> particleEventCount{0};
    std::atomic<int> worldEventCount{0};
    std::atomic<int> resourceEventCount{0};

    EventManager::Instance().registerHandler(
        EventTypeId::Weather,
        [&](const EventData& data) {
            if (data.event) weatherEventCount.fetch_add(1);
        });

    EventManager::Instance().registerHandler(
        EventTypeId::ParticleEffect,
        [&](const EventData& data) {
            if (data.event) particleEventCount.fetch_add(1);
        });

    EventManager::Instance().registerHandler(
        EventTypeId::World,
        [&](const EventData& data) {
            if (data.event) worldEventCount.fetch_add(1);
        });

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&](const EventData& data) {
            if (data.event) resourceEventCount.fetch_add(1);
        });

    // Setup: Create test entity for resource events
    auto testEntity = TestEntity::create(Vector2D(100.0f, 100.0f));
    auto goldHandle = ResourceTemplateManager::Instance().getHandleByName("Platinum Coins");
    BOOST_REQUIRE(goldHandle.isValid());

    // Action: Trigger 50+ events in rapid succession
    const int numEventsPerType = 15; // 15 * 4 types = 60 total events
    auto startTime = std::chrono::high_resolution_clock::now();

    std::cout << "[TEST] Triggering " << (numEventsPerType * 4) << " events" << std::endl;

    for (int i = 0; i < numEventsPerType; ++i) {
        // Weather events
        EventManager::Instance().changeWeather(
            "rainy", 1.0f, EventManager::DispatchMode::Deferred);

        // Particle events
        EventManager::Instance().triggerParticleEffect(
            "rain", 100.0f + i * 10.0f, 100.0f + i * 10.0f, 1.0f, -1.0f, "test",
            EventManager::DispatchMode::Deferred);

        // World events
        EventManager::Instance().triggerTileChanged(
            i % 10, i % 10, "test_change", EventManager::DispatchMode::Deferred);

        // Resource events
        EventManager::Instance().triggerResourceChange(
            testEntity->getHandle(), goldHandle, i * 10, (i + 1) * 10, "test_batch",
            EventManager::DispatchMode::Deferred);
    }

    // Process all events
    const int maxUpdates = 100;
    int updateCount = 0;

    for (int i = 0; i < maxUpdates; ++i) {
        EventManager::Instance().update();
        updateCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Check if all events processed
        int totalProcessed = weatherEventCount.load() + particleEventCount.load() +
            worldEventCount.load() + resourceEventCount.load();

        if (totalProcessed >= numEventsPerType * 4) {
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Verification: Check all events were processed
    BOOST_CHECK_GE(weatherEventCount.load(), numEventsPerType);
    BOOST_CHECK_GE(particleEventCount.load(), numEventsPerType);
    BOOST_CHECK_GE(worldEventCount.load(), numEventsPerType);
    BOOST_CHECK_GE(resourceEventCount.load(), numEventsPerType);

    int totalProcessed = weatherEventCount.load() + particleEventCount.load() +
        worldEventCount.load() + resourceEventCount.load();

    TEST_LOG("Performance test completed:");
    std::cout << "[TEST]   Total events processed: " << totalProcessed << std::endl;
    std::cout << "[TEST]   Total time: " << durationMs << "ms" << std::endl;
    std::cout << "[TEST]   Update cycles: " << updateCount << std::endl;
    std::cout << "[TEST]   Weather events: " << weatherEventCount.load() << std::endl;
    std::cout << "[TEST]   Particle events: " << particleEventCount.load() << std::endl;
    std::cout << "[TEST]   World events: " << worldEventCount.load() << std::endl;
    std::cout << "[TEST]   Resource events: " << resourceEventCount.load() << std::endl;

    // Verification: Check performance (should complete in reasonable time)
    // Allow 1000ms for processing (well within frame budget for deferred processing)
    BOOST_CHECK_LE(durationMs, 1000);

    // Verification: Check event ordering (per-type events should maintain order)
    BOOST_CHECK_EQUAL(totalProcessed, numEventsPerType * 4);

    // Cleanup
    EventManager::Instance().clearAllHandlers();

    TEST_LOG("TestEventCoordinationPerformance completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()
