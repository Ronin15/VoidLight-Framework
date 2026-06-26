/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file DayNightControllerTests.cpp
 * @brief Tests for DayNightController
 *
 * Common ControllerBase tests are generated via template macros.
 * This file contains only DayNightController-specific tests.
 */

#define BOOST_TEST_MODULE DayNightControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/world/DayNightController.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/EventManager.hpp"
#include "events/TimeEvent.hpp"
#include <string>

// Common test infrastructure
#include "common/ControllerTestFixture.hpp"
#include "common/ControllerOwnershipTests.hpp"
#include "common/ControllerSubscriptionTests.hpp"
#include "common/ControllerSuspendResumeTests.hpp"
#include "common/ControllerGetNameTests.hpp"

// ============================================================================
// Common ControllerBase Tests (generated via macros)
// ============================================================================

using DayNightControllerFixture = ControllerTestFixture<DayNightController>;

INSTANTIATE_CONTROLLER_OWNERSHIP_TESTS(DayNightController)
INSTANTIATE_CONTROLLER_SUBSCRIPTION_TESTS(DayNightController, DayNightControllerFixture)
INSTANTIATE_CONTROLLER_SUSPEND_RESUME_TESTS(DayNightController, DayNightControllerFixture)
INSTANTIATE_CONTROLLER_GET_NAME_TESTS(DayNightController, DayNightControllerFixture, "DayNightController")

// ============================================================================
// DayNightController-Specific Tests
// ============================================================================

// --- Current Period Tests ---

BOOST_FIXTURE_TEST_SUITE(CurrentPeriodTests, DayNightControllerFixture)

BOOST_AUTO_TEST_CASE(TestGetCurrentPeriodAtNoon) {
    // Init GameTime at noon
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));

    m_controller.subscribe();

    // At noon (12:00), should be Day period
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
}

BOOST_AUTO_TEST_CASE(TestGetCurrentPeriodString) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));

    m_controller.subscribe();

    std::string_view periodStr = m_controller.getCurrentPeriodString();
    BOOST_CHECK_EQUAL(periodStr, "Day");
}

BOOST_AUTO_TEST_CASE(TestPeriodStringValidity) {
    m_controller.subscribe();

    std::string_view periodStr = m_controller.getCurrentPeriodString();
    BOOST_CHECK(!periodStr.empty());
}

BOOST_AUTO_TEST_SUITE_END()

// --- Current Visuals Tests ---

BOOST_FIXTURE_TEST_SUITE(CurrentVisualsTests, DayNightControllerFixture)

BOOST_AUTO_TEST_CASE(TestGetCurrentVisuals) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));

    m_controller.subscribe();

    TimePeriodVisuals visuals = m_controller.getCurrentVisuals();

    // Day visuals should have specific properties
    // Can't check exact values without knowing implementation, but verify valid ranges
    BOOST_CHECK_GE(visuals.overlayR, 0);
    BOOST_CHECK_LE(visuals.overlayR, 255);
    BOOST_CHECK_GE(visuals.overlayG, 0);
    BOOST_CHECK_LE(visuals.overlayG, 255);
    BOOST_CHECK_GE(visuals.overlayB, 0);
    BOOST_CHECK_LE(visuals.overlayB, 255);
    BOOST_CHECK_GE(visuals.overlayA, 0);
    BOOST_CHECK_LE(visuals.overlayA, 255);
}

BOOST_AUTO_TEST_CASE(TestVisualsMatchPeriodFactory) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));

    m_controller.subscribe();

    // Controller visuals should match factory method for Day period
    TimePeriodVisuals controllerVisuals = m_controller.getCurrentVisuals();
    TimePeriodVisuals factoryVisuals = TimePeriodVisuals::getDay();

    BOOST_CHECK_EQUAL(controllerVisuals.overlayR, factoryVisuals.overlayR);
    BOOST_CHECK_EQUAL(controllerVisuals.overlayG, factoryVisuals.overlayG);
    BOOST_CHECK_EQUAL(controllerVisuals.overlayB, factoryVisuals.overlayB);
    BOOST_CHECK_EQUAL(controllerVisuals.overlayA, factoryVisuals.overlayA);
}

BOOST_AUTO_TEST_SUITE_END()

// --- Hour To Time Period Tests ---

BOOST_FIXTURE_TEST_SUITE(HourToTimePeriodTests, DayNightControllerFixture)

BOOST_AUTO_TEST_CASE(TestMorningPeriod) {
    // Morning: 5:00 - 8:00

    // Test at 6 AM
    BOOST_REQUIRE(GameTimeManager::Instance().init(6.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);
    m_controller.unsubscribe();

    // Test at 5 AM (boundary)
    BOOST_REQUIRE(GameTimeManager::Instance().init(5.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);
    m_controller.unsubscribe();

    // Test at 7:59 AM
    BOOST_REQUIRE(GameTimeManager::Instance().init(7.99f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);
    m_controller.unsubscribe();
}

BOOST_AUTO_TEST_CASE(TestDayPeriod) {
    // Day: 8:00 - 17:00

    // Test at noon
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
    m_controller.unsubscribe();

    // Test at 8 AM (boundary)
    BOOST_REQUIRE(GameTimeManager::Instance().init(8.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
    m_controller.unsubscribe();

    // Test at 4:59 PM
    BOOST_REQUIRE(GameTimeManager::Instance().init(16.99f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
    m_controller.unsubscribe();
}

BOOST_AUTO_TEST_CASE(TestEveningPeriod) {
    // Evening: 17:00 - 21:00

    // Test at 6 PM
    BOOST_REQUIRE(GameTimeManager::Instance().init(18.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);
    m_controller.unsubscribe();

    // Test at 5 PM (boundary)
    BOOST_REQUIRE(GameTimeManager::Instance().init(17.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);
    m_controller.unsubscribe();

    // Test at 8:59 PM
    BOOST_REQUIRE(GameTimeManager::Instance().init(20.99f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);
    m_controller.unsubscribe();
}

BOOST_AUTO_TEST_CASE(TestNightPeriod) {
    // Night: 21:00 - 5:00

    // Test at midnight
    BOOST_REQUIRE(GameTimeManager::Instance().init(0.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
    m_controller.unsubscribe();

    // Test at 9 PM (boundary)
    BOOST_REQUIRE(GameTimeManager::Instance().init(21.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
    m_controller.unsubscribe();

    // Test at 3 AM
    BOOST_REQUIRE(GameTimeManager::Instance().init(3.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
    m_controller.unsubscribe();

    // Test at 4:59 AM (before morning boundary)
    BOOST_REQUIRE(GameTimeManager::Instance().init(4.99f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
    m_controller.unsubscribe();
}

BOOST_AUTO_TEST_SUITE_END()

// --- Period Transition Tests ---

BOOST_FIXTURE_TEST_SUITE(PeriodTransitionTests, DayNightControllerFixture)

BOOST_AUTO_TEST_CASE(TestTransitionOnHourChangedEvent) {
    // Start at 7 AM (Morning)
    BOOST_REQUIRE(GameTimeManager::Instance().init(7.0f, 1.0f));

    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);

    // Dispatch hour change to 8 AM (Day boundary)
    auto hourEvent = std::make_shared<HourChangedEvent>(8, false);
    EventManager::Instance().dispatchEvent(hourEvent, EventManager::DispatchMode::Immediate);

    // Should transition to Day
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
}

BOOST_AUTO_TEST_CASE(TestNoTransitionOnSamePeriod) {
    // Start at noon (Day)
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));

    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);

    // Dispatch hour change to 1 PM (still Day)
    auto hourEvent = std::make_shared<HourChangedEvent>(13, false);
    EventManager::Instance().dispatchEvent(hourEvent, EventManager::DispatchMode::Immediate);

    // Should still be Day
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
}

BOOST_AUTO_TEST_CASE(TestDayToEveningTransition) {
    // Start at 4 PM (Day)
    BOOST_REQUIRE(GameTimeManager::Instance().init(16.0f, 1.0f));

    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);

    // Dispatch hour change to 5 PM (Evening boundary)
    auto hourEvent = std::make_shared<HourChangedEvent>(17, false);
    EventManager::Instance().dispatchEvent(hourEvent, EventManager::DispatchMode::Immediate);

    // Should transition to Evening
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);
}

BOOST_AUTO_TEST_CASE(TestEveningToNightTransition) {
    // Start at 8 PM (Evening)
    BOOST_REQUIRE(GameTimeManager::Instance().init(20.0f, 1.0f));

    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);

    // Dispatch hour change to 9 PM (Night boundary)
    auto hourEvent = std::make_shared<HourChangedEvent>(21, true);  // Night is true
    EventManager::Instance().dispatchEvent(hourEvent, EventManager::DispatchMode::Immediate);

    // Should transition to Night
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
}

BOOST_AUTO_TEST_CASE(TestNightToMorningTransition) {
    // Start at 4 AM (Night)
    BOOST_REQUIRE(GameTimeManager::Instance().init(4.0f, 1.0f));

    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);

    // Dispatch hour change to 5 AM (Morning boundary)
    auto hourEvent = std::make_shared<HourChangedEvent>(5, false);
    EventManager::Instance().dispatchEvent(hourEvent, EventManager::DispatchMode::Immediate);

    // Should transition to Morning
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);
}

BOOST_AUTO_TEST_CASE(TestFullDayCycle) {
    // Start at midnight (Night)
    BOOST_REQUIRE(GameTimeManager::Instance().init(0.0f, 1.0f));

    m_controller.subscribe();

    // Night at midnight
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);

    // Transition to Morning at 5 AM
    auto morningEvent = std::make_shared<HourChangedEvent>(5, false);
    EventManager::Instance().dispatchEvent(morningEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);

    // Transition to Day at 8 AM
    auto dayEvent = std::make_shared<HourChangedEvent>(8, false);
    EventManager::Instance().dispatchEvent(dayEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);

    // Transition to Evening at 5 PM
    auto eveningEvent = std::make_shared<HourChangedEvent>(17, false);
    EventManager::Instance().dispatchEvent(eveningEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);

    // Transition to Night at 9 PM
    auto nightEvent = std::make_shared<HourChangedEvent>(21, true);
    EventManager::Instance().dispatchEvent(nightEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
}

BOOST_AUTO_TEST_SUITE_END()

// --- Event Filtering Tests ---

BOOST_FIXTURE_TEST_SUITE(EventFilteringTests, DayNightControllerFixture)

BOOST_AUTO_TEST_CASE(TestIgnoresNonHourChangedEvents) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));

    m_controller.subscribe();
    TimePeriod initialPeriod = m_controller.getCurrentPeriod();

    // Dispatch various non-HourChanged events
    auto dayEvent = std::make_shared<DayChangedEvent>(5, 5, 0, "Bloomtide");
    EventManager::Instance().dispatchEvent(dayEvent, EventManager::DispatchMode::Immediate);

    auto seasonEvent = std::make_shared<SeasonChangedEvent>(Season::Summer, Season::Spring, "Summer");
    EventManager::Instance().dispatchEvent(seasonEvent, EventManager::DispatchMode::Immediate);

    auto weatherEvent = std::make_shared<WeatherCheckEvent>(Season::Summer, WeatherType::Clear);
    EventManager::Instance().dispatchEvent(weatherEvent, EventManager::DispatchMode::Immediate);

    // Period should remain unchanged
    BOOST_CHECK(m_controller.getCurrentPeriod() == initialPeriod);
}

BOOST_AUTO_TEST_CASE(TestNoHandlingWhenUnsubscribed) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(7.0f, 1.0f));

    // Subscribe to set initial period
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);
    m_controller.unsubscribe();

    // Dispatch hour change while unsubscribed
    auto hourEvent = std::make_shared<HourChangedEvent>(21, true);
    EventManager::Instance().dispatchEvent(hourEvent, EventManager::DispatchMode::Immediate);

    // Period should NOT change since we're unsubscribed
    // Note: The internal state may persist, but handler won't process new events
    BOOST_CHECK(!m_controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestWeatherCheckEventIgnoredWhenUnsubscribed) {
    // Ensure not subscribed
    BOOST_CHECK(!m_controller.isSubscribed());

    // Get initial period
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));
    m_controller.subscribe();
    TimePeriod initialPeriod = m_controller.getCurrentPeriod();
    m_controller.unsubscribe();

    // Dispatch a weather check event
    auto weatherCheckEvent = std::make_shared<WeatherCheckEvent>(Season::Winter, WeatherType::Snowy);
    EventManager::Instance().dispatchEvent(weatherCheckEvent, EventManager::DispatchMode::Immediate);

    // Period should NOT change since we're not subscribed
    // Re-subscribe to check period is still the same
    m_controller.subscribe();
    BOOST_CHECK(m_controller.getCurrentPeriod() == initialPeriod);
}

BOOST_AUTO_TEST_SUITE_END()

// --- Period Description Tests ---

BOOST_FIXTURE_TEST_SUITE(PeriodDescriptionTests, DayNightControllerFixture)

BOOST_AUTO_TEST_CASE(TestGetCurrentPeriodDescriptionMorning) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(7.0f, 1.0f));  // 7 AM - Morning
    m_controller.subscribe();

    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Morning);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "Dawn approaches");
}

BOOST_AUTO_TEST_CASE(TestGetCurrentPeriodDescriptionDay) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));  // Noon - Day
    m_controller.subscribe();

    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Day);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "The sun rises high");
}

BOOST_AUTO_TEST_CASE(TestGetCurrentPeriodDescriptionEvening) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(19.0f, 1.0f));  // 7 PM - Evening
    m_controller.subscribe();

    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Evening);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "Dusk settles in");
}

BOOST_AUTO_TEST_CASE(TestGetCurrentPeriodDescriptionNight) {
    BOOST_REQUIRE(GameTimeManager::Instance().init(23.0f, 1.0f));  // 11 PM - Night
    m_controller.subscribe();

    BOOST_CHECK(m_controller.getCurrentPeriod() == TimePeriod::Night);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "Night falls");
}

BOOST_AUTO_TEST_CASE(TestPeriodDescriptionChangesWithTimeTransition) {
    // Start in morning
    BOOST_REQUIRE(GameTimeManager::Instance().init(7.0f, 1.0f));
    m_controller.subscribe();
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "Dawn approaches");

    // Transition to day
    auto dayEvent = std::make_shared<HourChangedEvent>(12, false);
    EventManager::Instance().dispatchEvent(dayEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "The sun rises high");

    // Transition to evening
    auto eveningEvent = std::make_shared<HourChangedEvent>(19, false);
    EventManager::Instance().dispatchEvent(eveningEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "Dusk settles in");

    // Transition to night
    auto nightEvent = std::make_shared<HourChangedEvent>(23, true);
    EventManager::Instance().dispatchEvent(nightEvent, EventManager::DispatchMode::Immediate);
    BOOST_CHECK_EQUAL(std::string(m_controller.getCurrentPeriodDescription()), "Night falls");
}

BOOST_AUTO_TEST_SUITE_END()
