/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE WorldManagerEventIntegrationTests
#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include "managers/WorldManager.hpp"
#include "managers/EventManager.hpp"
#include "events/WorldEvent.hpp"
#include "events/HarvestResourceEvent.hpp"
#include "events/TimeEvent.hpp"
#include "world/WorldData.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"

using namespace VoidLight;

// Global fixture: Initialize ThreadSystem once for the entire test module
// This ensures worker threads are available for all tests that need async task execution
struct GlobalThreadSystemFixture {
    GlobalThreadSystemFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) { // Auto-detect system threads
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~GlobalThreadSystemFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(GlobalThreadSystemFixture);

BOOST_AUTO_TEST_SUITE(WorldManagerEventIntegrationTests)

namespace {

void drainWorldLoadEvents()
{
    for (int i = 0; i < 100; ++i) {
        EventManager::Instance().update();
        if (!VoidLight::ThreadSystem::Instance().isBusy()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EventManager::Instance().drainAllDeferredEvents();
}

} // namespace

// Verify WorldLoadedEvent payload matches WorldManager dimensions
BOOST_AUTO_TEST_CASE(TestWorldLoadedEventPayload) {
    // ThreadSystem is initialized by global fixture

    // Init managers
    BOOST_REQUIRE(WorldManager::Instance().init());
    BOOST_REQUIRE(EventManager::Instance().init());

    std::atomic<bool> gotLoaded{false};
    std::string capturedWorldId;
    int capturedW = -1, capturedH = -1;

    EventManager::Instance().registerHandler(EventTypeId::World, [&](const EventData& data){
        if (!data.event) return;
        auto loaded = std::dynamic_pointer_cast<WorldLoadedEvent>(data.event);
        if (loaded) {
            gotLoaded.store(true);
            capturedWorldId = loaded->getWorldId();
            capturedW = loaded->getWidth();
            capturedH = loaded->getHeight();
        }
    });

    WorldGenerationConfig cfg{};
    cfg.width = 5; cfg.height = 5; cfg.seed = 4242; cfg.elevationFrequency = 0.1f; cfg.humidityFrequency = 0.1f; cfg.waterLevel = 0.3f; cfg.mountainLevel = 0.7f;
    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(cfg));

    // Wait for ThreadSystem task execution - give workers time to process the task
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Process events - more aggressive pumping to ensure deferred events are processed
    for (int i = 0; i < 50 && !gotLoaded.load(); ++i) {
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Validate
    int w=0, h=0;
    WorldManager::Instance().getWorldDimensions(w,h);
    BOOST_CHECK(gotLoaded.load());
    BOOST_CHECK_EQUAL(capturedW, w);
    BOOST_CHECK_EQUAL(capturedH, h);
    BOOST_CHECK_EQUAL(capturedWorldId, WorldManager::Instance().getCurrentWorldId());

    WorldManager::Instance().clean();
    EventManager::Instance().clean();
    // DON'T clean ThreadSystem - let it persist for subsequent tests like EventManager tests do
}

// End-to-end harvest integration via EventManager trigger -> WorldManager handler
BOOST_AUTO_TEST_CASE(TestHarvestResourceIntegration) {
    BOOST_REQUIRE(WorldManager::Instance().init());
    BOOST_REQUIRE(EventManager::Instance().init());

    WorldGenerationConfig cfg{};
    cfg.width = 20; cfg.height = 20; cfg.seed = 7777; cfg.elevationFrequency = 0.1f; cfg.humidityFrequency = 0.1f; cfg.waterLevel = 0.2f; cfg.mountainLevel = 0.8f;
    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(cfg));

    int targetX=-1, targetY=-1;
    for (int y=0; y<cfg.height && targetX==-1; ++y) {
        for (int x=0; x<cfg.width; ++x) {
            const auto tile = WorldManager::Instance().getTileCopyAt(x, y);
            if (tile.has_value() && tile->obstacleType != ObstacleType::NONE) {
                targetX = x;
                targetY = y;
                break;
            }
        }
    }
    BOOST_REQUIRE_MESSAGE(targetX!=-1 && targetY!=-1, "No obstacle tile found to harvest in generated world");

    std::atomic<int> tileChangedCount{0};
    EventManager::Instance().registerHandler(EventTypeId::World, [&](const EventData& data){
        if (!data.event) return;
        auto changed = std::dynamic_pointer_cast<TileChangedEvent>(data.event);
        if (changed) { tileChangedCount.fetch_add(1); }
    });

    auto harvestToken = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::Harvest, [](const EventData& data) {
            if (!data.isActive() || !data.event) {
                return;
            }

            auto harvestEvent =
                std::dynamic_pointer_cast<HarvestResourceEvent>(data.event);
            if (!harvestEvent) {
                return;
            }

            WorldManager::Instance().handleHarvestResource(
                harvestEvent->getEntityId(), harvestEvent->getTargetX(),
                harvestEvent->getTargetY());
        });

    // Dispatch harvest event directly (dispatch-only architecture)
    auto harvestEvent = std::make_shared<HarvestResourceEvent>(1, targetX, targetY, "test_harvest");
    EventManager::Instance().dispatchEvent(harvestEvent);

    // Allow processing with event pumping
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() - start < timeout && tileChangedCount.load() == 0) {
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto after = WorldManager::Instance().getTileCopyAt(targetX, targetY);
    BOOST_REQUIRE(after.has_value());
    BOOST_CHECK_EQUAL(after->obstacleType, ObstacleType::NONE);
    BOOST_CHECK_GE(tileChangedCount.load(), 1);

    EventManager::Instance().removeHandler(harvestToken);

    WorldManager::Instance().clean();
    EventManager::Instance().clean();
}

/**
 * @brief Test basic WorldManager and EventManager integration
 * Simple test that verifies both managers can be initialized and work together
 */
BOOST_AUTO_TEST_CASE(TestBasicWorldManagerEventIntegration) {
    WORLD_MANAGER_INFO("Starting basic WorldManager event integration test");

    // Initialize managers
    bool worldInit = WorldManager::Instance().init();
    bool eventInit = EventManager::Instance().init();

    BOOST_REQUIRE(worldInit);
    BOOST_REQUIRE(eventInit);

    // Verify both managers are working
    BOOST_CHECK(WorldManager::Instance().isInitialized());
    BOOST_CHECK(EventManager::Instance().isInitialized());

    // Test trigger method works (dispatch-only architecture)
    std::atomic<bool> handlerCalled{false};
    EventManager::Instance().registerHandler(EventTypeId::World, [&](const EventData& data){
        if (data.event) handlerCalled.store(true);
    });

    // Trigger a world loaded event via the dispatch hub
    EventManager::Instance().triggerWorldLoaded("test_world", 10, 10);

    // Process events
    EventManager::Instance().update();

    BOOST_CHECK(handlerCalled.load());

    // Clean up
    WorldManager::Instance().clean();
    EventManager::Instance().clean();

    WORLD_MANAGER_INFO("Basic WorldManager event integration test completed successfully");
}

BOOST_AUTO_TEST_CASE(TestSeasonChangeSubscriptionThroughWorldLoad) {
    BOOST_REQUIRE(EventManager::Instance().init());
    BOOST_REQUIRE(WorldManager::Instance().init());

    WorldGenerationConfig config{};
    config.width = 5;
    config.height = 5;
    config.seed = 13579;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(config));
    drainWorldLoadEvents();

    BOOST_CHECK(WorldManager::Instance().getCurrentSeason() == Season::Spring);

    auto seasonEvent = std::make_shared<SeasonChangedEvent>(
        Season::Winter, Season::Spring, "Winter");
    EventManager::Instance().dispatchEvent(
        seasonEvent, EventManager::DispatchMode::Deferred);
    EventManager::Instance().update();

    BOOST_CHECK(WorldManager::Instance().getCurrentSeason() == Season::Winter);

    WorldManager::Instance().clean();
    EventManager::Instance().clean();
}

BOOST_AUTO_TEST_CASE(TestSeasonSubscriptionRestoredAfterStateTransitionAndWorldLoad) {
    BOOST_REQUIRE(EventManager::Instance().init());
    BOOST_REQUIRE(WorldManager::Instance().init());

    WorldGenerationConfig firstConfig{};
    firstConfig.width = 5;
    firstConfig.height = 5;
    firstConfig.seed = 13579;
    firstConfig.elevationFrequency = 0.1f;
    firstConfig.humidityFrequency = 0.1f;
    firstConfig.waterLevel = 0.3f;
    firstConfig.mountainLevel = 0.7f;

    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(firstConfig));
    drainWorldLoadEvents();
    BOOST_CHECK(WorldManager::Instance().getCurrentSeason() == Season::Spring);

    WorldManager::Instance().prepareForStateTransition();
    EventManager::Instance().prepareForStateTransition();

    WorldGenerationConfig config{};
    config.width = 5;
    config.height = 5;
    config.seed = 24680;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    BOOST_REQUIRE(WorldManager::Instance().loadNewWorld(config));
    drainWorldLoadEvents();

    auto seasonEvent = std::make_shared<SeasonChangedEvent>(
        Season::Summer, Season::Spring, "Summer");
    EventManager::Instance().dispatchEvent(
        seasonEvent, EventManager::DispatchMode::Deferred);
    EventManager::Instance().update();

    BOOST_CHECK(WorldManager::Instance().getCurrentSeason() == Season::Summer);

    WorldManager::Instance().clean();
    EventManager::Instance().clean();
}

/**
 * @brief Test world generation with minimal configuration
 * Uses a very small world to avoid performance issues
 */
BOOST_AUTO_TEST_CASE(TestSimpleWorldGeneration) {
    WORLD_MANAGER_INFO("Starting simple world generation test");

    // Initialize managers
    BOOST_REQUIRE(WorldManager::Instance().init());
    BOOST_REQUIRE(EventManager::Instance().init());

    // Generate a very small world to minimize processing time
    WorldGenerationConfig config{};
    config.width = 5;  // Very small to avoid hanging
    config.height = 5;
    config.seed = 12345;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    WORLD_MANAGER_INFO("Generating 5x5 world...");
    bool generateResult = WorldManager::Instance().loadNewWorld(config);

    BOOST_REQUIRE(generateResult);
    WORLD_MANAGER_INFO("World generation completed");

    // Single event processing call (no loop to avoid hanging)
    EventManager::Instance().update();
    WORLD_MANAGER_INFO("Event processing completed");

    // Verify world is active
    BOOST_CHECK(WorldManager::Instance().hasActiveWorld());

    // Clean up
    WorldManager::Instance().clean();
    EventManager::Instance().clean();

    WORLD_MANAGER_INFO("Simple world generation test completed successfully");
}

/**
 * @brief Test event dispatch and handler processing
 * Focuses purely on event system functionality using trigger methods
 */
BOOST_AUTO_TEST_CASE(TestEventDispatchAndProcessing) {
    WORLD_MANAGER_INFO("Starting event dispatch and processing test");

    // Initialize only EventManager to avoid WorldManager complexity
    BOOST_REQUIRE(EventManager::Instance().init());

    std::atomic<int> eventCount{0};

    // Register handlers for different event types
    EventManager::Instance().registerHandler(EventTypeId::World,
        [&eventCount](const EventData& eventData) {
            if (eventData.isActive()) {
                eventCount.fetch_add(1, std::memory_order_relaxed);
            }
        });

    // Dispatch world events via trigger methods (dispatch-only architecture)
    EventManager::Instance().triggerWorldLoaded("test_world_1", 10, 10);
    EventManager::Instance().triggerTileChanged(5, 5, "biome_change");
    EventManager::Instance().triggerWorldGenerated("test_world_2", 20, 20, 1.5f);

    // Process events with timeout
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Break when we've processed enough events
        if (eventCount.load() >= 3) {
            break;
        }
    }

    // Verify events were processed
    BOOST_CHECK_GE(eventCount.load(), 3);

    // Clean up
    EventManager::Instance().clean();

    WORLD_MANAGER_INFO("Event dispatch and processing test completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()
