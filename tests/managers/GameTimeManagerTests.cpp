/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE GameTimeManagerTests
#include <boost/test/unit_test.hpp>

#include "managers/GameTimeManager.hpp"
#include "managers/EventManager.hpp"
#include "events/TimeEvent.hpp"
#include <atomic>
#include <cmath>
#include <string>

// Test tolerance for floating-point comparisons
constexpr float EPSILON = 0.001f;

// Helper to check if two floats are approximately equal
bool approxEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// Test Fixture
// ============================================================================

class GameTimeManagerTestFixture {
public:
    GameTimeManagerTestFixture() {
        // Get the singleton instance and initialize with default values
        gameTime = &GameTimeManager::Instance();
        BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));  // Start at noon, normal time scale
    }

    ~GameTimeManagerTestFixture() {
        // Reset to known state for next test
        gameTime->setGlobalPause(false);  // Ensure not paused
        BOOST_CHECK(gameTime->init(12.0f, 1.0f));  // Reset to defaults
    }

protected:
    GameTimeManager* gameTime;
};

// ============================================================================
// SINGLETON PATTERN TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SingletonTests)

BOOST_AUTO_TEST_CASE(TestSingletonPattern) {
    GameTimeManager* instance1 = &GameTimeManager::Instance();
    GameTimeManager* instance2 = &GameTimeManager::Instance();

    BOOST_CHECK(instance1 == instance2);
    BOOST_CHECK(instance1 != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(InitializationTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestInitializationWithDefaults) {
    // Re-init with defaults
    bool result = gameTime->init(12.0f, 1.0f);

    BOOST_CHECK(result);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), 12.0f));
    BOOST_CHECK_EQUAL(gameTime->getGameDay(), 1);
    BOOST_CHECK(approxEqual(gameTime->getTimeScale(), 1.0f));
}

BOOST_AUTO_TEST_CASE(TestInitializationWithCustomValues) {
    // Init with custom start hour and time scale
    bool result = gameTime->init(6.0f, 2.0f);

    BOOST_CHECK(result);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), 6.0f));
    BOOST_CHECK(approxEqual(gameTime->getTimeScale(), 2.0f));
}

BOOST_AUTO_TEST_CASE(TestInitializationWithInvalidHour) {
    // Negative hour should fail
    bool result1 = gameTime->init(-1.0f, 1.0f);
    BOOST_CHECK(!result1);

    // Hour >= 24 should fail
    bool result2 = gameTime->init(24.0f, 1.0f);
    BOOST_CHECK(!result2);

    bool result3 = gameTime->init(25.0f, 1.0f);
    BOOST_CHECK(!result3);
}

BOOST_AUTO_TEST_CASE(TestInitializationWithInvalidTimeScale) {
    // Zero time scale should fail
    bool result1 = gameTime->init(12.0f, 0.0f);
    BOOST_CHECK(!result1);

    // Negative time scale should fail
    bool result2 = gameTime->init(12.0f, -1.0f);
    BOOST_CHECK(!result2);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TIME PROGRESSION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TimeProgressionTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestTimeProgression) {
    // Init at midnight
    BOOST_REQUIRE(gameTime->init(0.0f, 1.0f));
    float initialHour = gameTime->getGameHour();

    // Update with 1 hour of game time (3600 seconds at 1.0 time scale)
    gameTime->update(3600.0f);

    float newHour = gameTime->getGameHour();
    BOOST_CHECK(approxEqual(newHour, initialHour + 1.0f));
}

BOOST_AUTO_TEST_CASE(TestTimeProgressionWithScale) {
    // Init at midnight with 2x time scale
    BOOST_REQUIRE(gameTime->init(0.0f, 2.0f));
    float initialHour = gameTime->getGameHour();

    // Update with 1 real hour (3600 real seconds = 2 game hours at 2x scale)
    gameTime->update(3600.0f);

    float newHour = gameTime->getGameHour();
    BOOST_CHECK(approxEqual(newHour, initialHour + 2.0f));
}

BOOST_AUTO_TEST_CASE(TestDayProgression) {
    // Init at 23:00
    BOOST_REQUIRE(gameTime->init(23.0f, 1.0f));
    int initialDay = gameTime->getGameDay();

    // Update with 2 hours (past midnight)
    gameTime->update(7200.0f);

    int newDay = gameTime->getGameDay();
    float newHour = gameTime->getGameHour();

    BOOST_CHECK_EQUAL(newDay, initialDay + 1);
    BOOST_CHECK(approxEqual(newHour, 1.0f));
}

BOOST_AUTO_TEST_CASE(TestTimeScaleChange) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    BOOST_CHECK(approxEqual(gameTime->getTimeScale(), 1.0f));

    gameTime->setTimeScale(5.0f);
    BOOST_CHECK(approxEqual(gameTime->getTimeScale(), 5.0f));

    gameTime->setTimeScale(0.5f);
    BOOST_CHECK(approxEqual(gameTime->getTimeScale(), 0.5f));
}

BOOST_AUTO_TEST_CASE(TestTotalGameTimeSeconds) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    float initialSeconds = gameTime->getTotalGameTimeSeconds();

    // 12 hours = 43200 seconds
    BOOST_CHECK(approxEqual(initialSeconds, 43200.0f));

    // Update with 1 hour
    gameTime->update(3600.0f);
    float newSeconds = gameTime->getTotalGameTimeSeconds();

    BOOST_CHECK(approxEqual(newSeconds, initialSeconds + 3600.0f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PAUSE/RESUME TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PauseResumeTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestPauseResume) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Initially not paused
    BOOST_CHECK(!gameTime->isGloballyPaused());

    // Pause
    gameTime->setGlobalPause(true);
    BOOST_CHECK(gameTime->isGloballyPaused());

    // Resume
    gameTime->setGlobalPause(false);
    BOOST_CHECK(!gameTime->isGloballyPaused());
}

BOOST_AUTO_TEST_CASE(TestUpdateWhilePaused) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    float initialHour = gameTime->getGameHour();

    // Pause and update
    gameTime->setGlobalPause(true);
    gameTime->update(3600.0f);

    // Time should not have advanced
    float newHour = gameTime->getGameHour();
    BOOST_CHECK(approxEqual(newHour, initialHour));
}

BOOST_AUTO_TEST_CASE(TestResumeAfterPause) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Pause, then resume
    gameTime->setGlobalPause(true);
    gameTime->setGlobalPause(false);

    float initialHour = gameTime->getGameHour();

    // Update should work after resume
    gameTime->update(3600.0f);

    float newHour = gameTime->getGameHour();
    BOOST_CHECK(approxEqual(newHour, initialHour + 1.0f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// DAYTIME/NIGHTTIME TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(DaytimeNighttimeTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestDaytimeDetection) {
    // Set to noon - should be daytime
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    BOOST_CHECK(gameTime->isDaytime());
    BOOST_CHECK(!gameTime->isNighttime());

    // Set to 8 AM - should be daytime
    BOOST_REQUIRE(gameTime->init(8.0f, 1.0f));
    BOOST_CHECK(gameTime->isDaytime());
    BOOST_CHECK(!gameTime->isNighttime());
}

BOOST_AUTO_TEST_CASE(TestNighttimeDetection) {
    // Set to midnight - should be nighttime
    BOOST_REQUIRE(gameTime->init(0.0f, 1.0f));
    BOOST_CHECK(!gameTime->isDaytime());
    BOOST_CHECK(gameTime->isNighttime());

    // Set to 3 AM - should be nighttime
    BOOST_REQUIRE(gameTime->init(3.0f, 1.0f));
    BOOST_CHECK(!gameTime->isDaytime());
    BOOST_CHECK(gameTime->isNighttime());

    // Set to 22:00 (10 PM) - should be nighttime
    BOOST_REQUIRE(gameTime->init(22.0f, 1.0f));
    BOOST_CHECK(!gameTime->isDaytime());
    BOOST_CHECK(gameTime->isNighttime());
}

BOOST_AUTO_TEST_CASE(TestCustomDaylightHours) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Set custom daylight hours: 8 AM to 6 PM
    gameTime->setDaylightHours(8.0f, 18.0f);

    // 7 AM should be nighttime with these settings
    gameTime->setGameHour(7.0f);
    BOOST_CHECK(gameTime->isNighttime());

    // 9 AM should be daytime
    gameTime->setGameHour(9.0f);
    BOOST_CHECK(gameTime->isDaytime());

    // 7 PM (19:00) should be nighttime
    gameTime->setGameHour(19.0f);
    BOOST_CHECK(gameTime->isNighttime());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TIME OF DAY NAME TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TimeOfDayNameTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestTimeOfDayName) {
    // Morning: 5:00 - 8:00
    BOOST_REQUIRE(gameTime->init(6.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Morning");

    // Day: 8:00 - 17:00
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Day");

    // Evening: 17:00 - 21:00
    BOOST_REQUIRE(gameTime->init(19.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Evening");

    // Night: 21:00 - 5:00
    BOOST_REQUIRE(gameTime->init(23.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Night");

    BOOST_REQUIRE(gameTime->init(2.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Night");
}

BOOST_AUTO_TEST_CASE(TestTimeOfDayBoundaries) {
    // At 5:00 exactly - should be Morning
    BOOST_REQUIRE(gameTime->init(5.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Morning");

    // At 8:00 exactly - should be Day
    BOOST_REQUIRE(gameTime->init(8.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Day");

    // At 17:00 exactly - should be Evening
    BOOST_REQUIRE(gameTime->init(17.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Evening");

    // At 21:00 exactly - should be Night
    BOOST_REQUIRE(gameTime->init(21.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::string(gameTime->getTimeOfDayName()), "Night");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SET GAME HOUR/DAY TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SetGameHourDayTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestSetGameHour) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Set to valid hour
    gameTime->setGameHour(18.0f);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), 18.0f));

    // Set to 0 (midnight)
    gameTime->setGameHour(0.0f);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), 0.0f));

    // Set to 23.5 (11:30 PM)
    gameTime->setGameHour(23.5f);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), 23.5f));
}

BOOST_AUTO_TEST_CASE(TestSetGameHourInvalidValues) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    float initialHour = gameTime->getGameHour();

    // Negative hour should be ignored
    gameTime->setGameHour(-1.0f);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), initialHour));

    // Hour >= 24 should be ignored
    gameTime->setGameHour(24.0f);
    BOOST_CHECK(approxEqual(gameTime->getGameHour(), initialHour));
}

BOOST_AUTO_TEST_CASE(TestSetGameDay) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Set valid day
    gameTime->setGameDay(5);
    BOOST_CHECK_EQUAL(gameTime->getGameDay(), 5);

    gameTime->setGameDay(100);
    BOOST_CHECK_EQUAL(gameTime->getGameDay(), 100);
}

BOOST_AUTO_TEST_CASE(TestSetGameDayMinimum) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Day 0 should be converted to 1
    gameTime->setGameDay(0);
    BOOST_CHECK_EQUAL(gameTime->getGameDay(), 1);

    // Negative day should be converted to 1
    gameTime->setGameDay(-5);
    BOOST_CHECK_EQUAL(gameTime->getGameDay(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FORMAT TIME TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(FormatTimeTests, GameTimeManagerTestFixture)

BOOST_AUTO_TEST_CASE(TestFormatCurrentTime24Hour) {
    BOOST_REQUIRE(gameTime->init(14.5f, 1.0f));  // 2:30 PM

    std::string formatted(gameTime->formatCurrentTime(true));
    BOOST_CHECK_EQUAL(formatted, "14:30");
}

BOOST_AUTO_TEST_CASE(TestFormatCurrentTime12Hour) {
    // Test PM time
    BOOST_REQUIRE(gameTime->init(14.5f, 1.0f));  // 2:30 PM
    std::string formatted1(gameTime->formatCurrentTime(false));
    BOOST_CHECK_EQUAL(formatted1, "2:30 PM");

    // Test AM time
    BOOST_REQUIRE(gameTime->init(9.25f, 1.0f));  // 9:15 AM
    std::string formatted2(gameTime->formatCurrentTime(false));
    BOOST_CHECK_EQUAL(formatted2, "9:15 AM");
}

BOOST_AUTO_TEST_CASE(TestFormatCurrentTimeMidnight) {
    BOOST_REQUIRE(gameTime->init(0.0f, 1.0f));  // Midnight

    std::string formatted24(gameTime->formatCurrentTime(true));
    BOOST_CHECK_EQUAL(formatted24, "00:00");

    std::string formatted12(gameTime->formatCurrentTime(false));
    BOOST_CHECK_EQUAL(formatted12, "12:00 AM");
}

BOOST_AUTO_TEST_CASE(TestFormatCurrentTimeNoon) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));  // Noon

    std::string formatted24(gameTime->formatCurrentTime(true));
    BOOST_CHECK_EQUAL(formatted24, "12:00");

    std::string formatted12(gameTime->formatCurrentTime(false));
    BOOST_CHECK_EQUAL(formatted12, "12:00 PM");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// EVENT EMISSION TESTS
// Tests for GameTimeManager's event dispatching via EventManager
// ============================================================================

// Fixture that includes EventManager for event tests
class GameTimeEventTestFixture {
public:
    GameTimeEventTestFixture() {
        gameTime = &GameTimeManager::Instance();
        eventManager = &EventManager::Instance();

        // Initialize EventManager for event handling
        BOOST_REQUIRE(eventManager->init());

        // Initialize GameTime to known state
        BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));  // Start at noon, normal time scale
    }

    ~GameTimeEventTestFixture() {
        // Clean up event handlers
        eventManager->clean();

        // Reset time state
        gameTime->setGlobalPause(false);
        BOOST_CHECK(gameTime->init(12.0f, 1.0f));
    }

protected:
    GameTimeManager* gameTime;
    EventManager* eventManager;
};

BOOST_FIXTURE_TEST_SUITE(EventEmissionTests, GameTimeEventTestFixture)

BOOST_AUTO_TEST_CASE(TestHourChangedEventEmission) {
    // Initialize to just before hour change
    BOOST_REQUIRE(gameTime->init(11.95f, 1.0f));  // 11:57 AM

    std::atomic<bool> eventReceived{false};
    int receivedHour = -1;
    bool receivedIsNight = true;

    // Register handler for Time events (which includes HourChangedEvent)
    eventManager->registerHandler(EventTypeId::Time,
        [&](const EventData& data) {
            if (!data.event) return;
            auto hourEvent = std::dynamic_pointer_cast<HourChangedEvent>(data.event);
            if (hourEvent) {
                eventReceived.store(true);
                receivedHour = hourEvent->getHour();
                receivedIsNight = hourEvent->isNight();
            }
        });

    // Advance time enough to trigger hour change (11:57 -> 12:00+)
    // With timeScale=1.0, 1 real second = 1 game minute
    // Need ~4 minutes of game time = 4 * 60 = 240 real seconds at scale 1
    // But update() takes deltaTime in real seconds
    gameTime->update(300.0f);  // 5 minutes real time = 5 game minutes

    // Process deferred events
    eventManager->update();

    BOOST_CHECK(eventReceived.load());
    BOOST_CHECK_EQUAL(receivedHour, 12);  // Noon
    BOOST_CHECK(!receivedIsNight);  // Noon is daytime
}

BOOST_AUTO_TEST_CASE(TestDayChangedEventEmission) {
    // Initialize to near end of day
    BOOST_REQUIRE(gameTime->init(23.95f, 1.0f));  // 11:57 PM

    std::atomic<bool> eventReceived{false};
    int receivedDay = -1;
    int receivedDayOfMonth = -1;

    eventManager->registerHandler(EventTypeId::Time,
        [&](const EventData& data) {
            if (!data.event) return;
            auto dayEvent = std::dynamic_pointer_cast<DayChangedEvent>(data.event);
            if (dayEvent) {
                eventReceived.store(true);
                receivedDay = dayEvent->getDay();
                receivedDayOfMonth = dayEvent->getDayOfMonth();
            }
        });

    // Advance time enough to trigger day change (wrap past midnight)
    gameTime->update(600.0f);  // 10 real minutes = 10 game minutes

    // Process deferred events
    eventManager->update();

    BOOST_CHECK(eventReceived.load());
    BOOST_CHECK_EQUAL(receivedDay, 2);  // Day 2
    BOOST_CHECK_GE(receivedDayOfMonth, 1);
}

BOOST_AUTO_TEST_CASE(TestSeasonChangedEventEmission) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    gameTime->setGameDay(30);
    gameTime->setGameHour(23.95f);

    std::atomic<bool> eventReceived{false};
    Season receivedSeason = Season::Spring;
    Season receivedPreviousSeason = Season::Spring;

    eventManager->registerHandler(EventTypeId::Time,
        [&](const EventData& data) {
            if (!data.event) return;
            auto seasonEvent = std::dynamic_pointer_cast<SeasonChangedEvent>(data.event);
            if (seasonEvent) {
                eventReceived.store(true);
                receivedSeason = seasonEvent->getSeason();
                receivedPreviousSeason = seasonEvent->getPreviousSeason();
            }
        });

    gameTime->update(300.0f);
    eventManager->update();

    BOOST_CHECK(eventReceived.load());
    BOOST_CHECK(receivedPreviousSeason == Season::Spring);
    BOOST_CHECK(receivedSeason == Season::Summer);
}

BOOST_AUTO_TEST_CASE(TestMultipleTimeEventsInSequence) {
    // Test that multiple events fire correctly in sequence
    BOOST_REQUIRE(gameTime->init(23.5f, 1.0f));  // 11:30 PM

    std::atomic<int> hourEventCount{0};
    std::atomic<int> dayEventCount{0};

    eventManager->registerHandler(EventTypeId::Time,
        [&](const EventData& data) {
            if (!data.event) return;

            if (std::dynamic_pointer_cast<HourChangedEvent>(data.event)) {
                hourEventCount.fetch_add(1);
            }
            if (std::dynamic_pointer_cast<DayChangedEvent>(data.event)) {
                dayEventCount.fetch_add(1);
            }
        });

    // Advance through midnight (should trigger both hour and day change)
    gameTime->update(3600.0f);  // 1 hour of real time = 60 game minutes
    eventManager->update();

    // Should have received at least 1 hour change event
    BOOST_CHECK_GE(hourEventCount.load(), 1);

    // Day change should have happened when crossing midnight
    BOOST_CHECK_GE(dayEventCount.load(), 1);
}

BOOST_AUTO_TEST_CASE(TestNoEventWhenPaused) {
    BOOST_REQUIRE(gameTime->init(11.95f, 1.0f));

    std::atomic<bool> eventReceived{false};

    eventManager->registerHandler(EventTypeId::Time,
        [&](const EventData& data) {
            if (data.event) {
                eventReceived.store(true);
            }
        });

    // Pause the game time
    gameTime->setGlobalPause(true);
    BOOST_CHECK(gameTime->isGloballyPaused());

    // Advance time (should be ignored while paused)
    gameTime->update(600.0f);
    eventManager->update();

    // No event should have been dispatched
    BOOST_CHECK(!eventReceived.load());

    // Resume and verify events work again
    gameTime->setGlobalPause(false);
    gameTime->update(600.0f);
    eventManager->update();

    // Now event should have been received
    BOOST_CHECK(eventReceived.load());
}

BOOST_AUTO_TEST_CASE(TestYearChangedEventEmission) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    gameTime->setGameDay(120);
    gameTime->setGameHour(23.95f);

    std::atomic<bool> eventReceived{false};
    int receivedYear = -1;

    eventManager->registerHandler(EventTypeId::Time,
        [&](const EventData& data) {
            if (!data.event) return;
            auto yearEvent = std::dynamic_pointer_cast<YearChangedEvent>(data.event);
            if (yearEvent) {
                eventReceived.store(true);
                receivedYear = yearEvent->getYear();
            }
        });

    gameTime->update(300.0f);
    eventManager->update();

    BOOST_CHECK(eventReceived.load());
    BOOST_CHECK_EQUAL(receivedYear, 2);
}

BOOST_AUTO_TEST_SUITE_END()
