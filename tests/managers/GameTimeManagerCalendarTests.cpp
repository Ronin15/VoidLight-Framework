/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE GameTimeManagerCalendarTests
#include <boost/test/unit_test.hpp>

#include "managers/GameTimeManager.hpp"
#include <string>

// ============================================================================
// Test Fixture
// ============================================================================

class GameTimeManagerCalendarFixture {
public:
    GameTimeManagerCalendarFixture() {
        gameTime = &GameTimeManager::Instance();
        BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    }

    ~GameTimeManagerCalendarFixture() {
        gameTime->setGlobalPause(false);
        BOOST_CHECK(gameTime->init(12.0f, 1.0f));
    }

protected:
    GameTimeManager* gameTime;
};

// ============================================================================
// CALENDAR CONFIG TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(CalendarConfigTests)

BOOST_AUTO_TEST_CASE(TestDefaultCalendarConfig) {
    CalendarConfig config = CalendarConfig::createDefault();

    // Should have 4 months
    BOOST_CHECK_EQUAL(config.months.size(), 4);

    // Verify month names and seasons
    BOOST_CHECK_EQUAL(config.months[0].name, "Bloomtide");
    BOOST_CHECK(config.months[0].season == Season::Spring);
    BOOST_CHECK_EQUAL(config.months[0].dayCount, 30);

    BOOST_CHECK_EQUAL(config.months[1].name, "Sunpeak");
    BOOST_CHECK(config.months[1].season == Season::Summer);
    BOOST_CHECK_EQUAL(config.months[1].dayCount, 30);

    BOOST_CHECK_EQUAL(config.months[2].name, "Harvestmoon");
    BOOST_CHECK(config.months[2].season == Season::Fall);
    BOOST_CHECK_EQUAL(config.months[2].dayCount, 30);

    BOOST_CHECK_EQUAL(config.months[3].name, "Frosthold");
    BOOST_CHECK(config.months[3].season == Season::Winter);
    BOOST_CHECK_EQUAL(config.months[3].dayCount, 30);
}

BOOST_AUTO_TEST_CASE(TestCalendarDaysInYear) {
    CalendarConfig config = CalendarConfig::createDefault();

    // 4 months x 30 days = 120 days
    BOOST_CHECK_EQUAL(config.getTotalDaysInYear(), 120);
}

BOOST_AUTO_TEST_CASE(TestCustomCalendarConfig) {
    CalendarConfig config;
    config.months = {
        {"Month1", 28, Season::Spring},
        {"Month2", 31, Season::Summer},
        {"Month3", 30, Season::Fall},
        {"Month4", 31, Season::Winter}
    };

    // 28 + 31 + 30 + 31 = 120 days
    BOOST_CHECK_EQUAL(config.getTotalDaysInYear(), 120);
}

BOOST_AUTO_TEST_CASE(TestEmptyCalendarConfig) {
    CalendarConfig config;
    config.months.clear();

    BOOST_CHECK_EQUAL(config.getTotalDaysInYear(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// MONTH PROGRESSION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(MonthProgressionTests, GameTimeManagerCalendarFixture)

BOOST_AUTO_TEST_CASE(TestInitialCalendarState) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Day 1 should be in month 0 (Bloomtide)
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 0);
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 1);
}

BOOST_AUTO_TEST_CASE(TestCurrentMonthName) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Day 1 is in Bloomtide
    std::string monthName(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(monthName, "Bloomtide");
}

BOOST_AUTO_TEST_CASE(TestDayOfMonthCalculation) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Day 1 -> dayOfMonth = 1
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);

    // Set to day 15 - setGameDay now updates calendar state
    gameTime->setGameDay(15);
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 15);
}

BOOST_AUTO_TEST_CASE(TestMonthProgressionByDays) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Day 1-30 = Bloomtide (month 0)
    gameTime->setGameDay(30);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 0);
    std::string month0(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month0, "Bloomtide");

    // Day 31 = Sunpeak (month 1)
    gameTime->setGameDay(31);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 1);
    std::string month1(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month1, "Sunpeak");
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);

    // Day 60 = last day of Sunpeak
    gameTime->setGameDay(60);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 1);
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 30);

    // Day 61 = first day of Harvestmoon (month 2)
    gameTime->setGameDay(61);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 2);
    std::string month2(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month2, "Harvestmoon");
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);

    // Day 91 = first day of Frosthold (month 3)
    gameTime->setGameDay(91);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 3);
    std::string month3(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month3, "Frosthold");
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);
}

BOOST_AUTO_TEST_CASE(TestDaysInCurrentMonth) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // All months in default calendar have 30 days
    BOOST_CHECK_EQUAL(gameTime->getDaysInCurrentMonth(), 30);

    // Move to second month
    gameTime->setGameDay(35);
    gameTime->update(0.0f);
    BOOST_CHECK_EQUAL(gameTime->getDaysInCurrentMonth(), 30);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// YEAR PROGRESSION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(YearProgressionTests, GameTimeManagerCalendarFixture)

BOOST_AUTO_TEST_CASE(TestYearProgression) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Year 1, Day 1
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 1);

    // Day 120 = last day of year 1
    gameTime->setGameDay(120);
    gameTime->update(0.0f);
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 1);

    // Day 121 = first day of year 2
    gameTime->setGameDay(121);
    gameTime->update(0.0f);
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 2);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 0);  // Back to Bloomtide
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);
}

BOOST_AUTO_TEST_CASE(TestMultiYearProgression) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Year 3 starts at day 241
    gameTime->setGameDay(241);
    gameTime->update(0.0f);
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 3);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 0);
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 1);

    // Year 5, month 2 (Harvestmoon), day 15
    // Year 5 starts at day 481
    // Month 2 starts at day 61 within a year
    // So day 481 + 60 + 14 = 555
    gameTime->setGameDay(555);
    gameTime->update(0.0f);
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 5);
    BOOST_CHECK_EQUAL(gameTime->getCurrentMonth(), 2);  // Harvestmoon
    BOOST_CHECK_EQUAL(gameTime->getDayOfMonth(), 15);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SEASON FROM MONTH TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SeasonFromMonthTests, GameTimeManagerCalendarFixture)

BOOST_AUTO_TEST_CASE(TestSeasonFromMonth) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Bloomtide = Spring
    gameTime->setGameDay(1);
    gameTime->update(0.0f);
    BOOST_CHECK(gameTime->getSeason() == Season::Spring);

    // Sunpeak = Summer
    gameTime->setGameDay(31);
    gameTime->update(0.0f);
    BOOST_CHECK(gameTime->getSeason() == Season::Summer);

    // Harvestmoon = Fall
    gameTime->setGameDay(61);
    gameTime->update(0.0f);
    BOOST_CHECK(gameTime->getSeason() == Season::Fall);

    // Frosthold = Winter
    gameTime->setGameDay(91);
    gameTime->update(0.0f);
    BOOST_CHECK(gameTime->getSeason() == Season::Winter);
}

BOOST_AUTO_TEST_CASE(TestSeasonCycleAcrossYears) {
    BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));

    // Year 2, Bloomtide = Spring again
    gameTime->setGameDay(121);
    gameTime->update(0.0f);
    BOOST_CHECK(gameTime->getSeason() == Season::Spring);
    BOOST_CHECK_EQUAL(gameTime->getGameYear(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// CUSTOM CALENDAR TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CustomCalendarTests, GameTimeManagerCalendarFixture)

BOOST_AUTO_TEST_CASE(TestSetCustomCalendar) {
    CalendarConfig customConfig;
    customConfig.months = {
        {"FirstMonth", 10, Season::Spring},
        {"SecondMonth", 20, Season::Summer},
        {"ThirdMonth", 15, Season::Fall}
    };

    gameTime->setCalendarConfig(customConfig);

    // Day 1 = FirstMonth
    gameTime->setGameDay(1);
    gameTime->update(0.0f);
    std::string month1(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month1, "FirstMonth");
    BOOST_CHECK_EQUAL(gameTime->getDaysInCurrentMonth(), 10);

    // Day 11 = SecondMonth
    gameTime->setGameDay(11);
    gameTime->update(0.0f);
    std::string month2(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month2, "SecondMonth");
    BOOST_CHECK_EQUAL(gameTime->getDaysInCurrentMonth(), 20);

    // Day 31 = ThirdMonth
    gameTime->setGameDay(31);
    gameTime->update(0.0f);
    std::string month3(gameTime->getCurrentMonthName());
    BOOST_CHECK_EQUAL(month3, "ThirdMonth");
    BOOST_CHECK_EQUAL(gameTime->getDaysInCurrentMonth(), 15);
}

BOOST_AUTO_TEST_SUITE_END()
