/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE GameTimeManagerSeasonTests
#include <boost/test/unit_test.hpp>

#include "managers/GameTimeManager.hpp"
#include <cmath>
#include <string>

// Test tolerance for floating-point comparisons
constexpr float EPSILON = 0.001f;

bool approxEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// Test Fixture
// ============================================================================

class GameTimeManagerSeasonFixture {
public:
    GameTimeManagerSeasonFixture() {
        gameTime = &GameTimeManager::Instance();
        BOOST_REQUIRE(gameTime->init(12.0f, 1.0f));
    }

    ~GameTimeManagerSeasonFixture() {
        gameTime->setGlobalPause(false);
        BOOST_CHECK(gameTime->init(12.0f, 1.0f));
    }

protected:
    GameTimeManager* gameTime;
};

// ============================================================================
// SEASON ENUM TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SeasonEnumTests)

BOOST_AUTO_TEST_CASE(TestSeasonEnumValues) {
    // Verify enum values match expected integers
    BOOST_CHECK_EQUAL(static_cast<int>(Season::Spring), 0);
    BOOST_CHECK_EQUAL(static_cast<int>(Season::Summer), 1);
    BOOST_CHECK_EQUAL(static_cast<int>(Season::Fall), 2);
    BOOST_CHECK_EQUAL(static_cast<int>(Season::Winter), 3);
}

BOOST_AUTO_TEST_CASE(TestSeasonEnumCasting) {
    // Test casting from int to Season
    Season spring = static_cast<Season>(0);
    Season summer = static_cast<Season>(1);
    Season fall = static_cast<Season>(2);
    Season winter = static_cast<Season>(3);

    BOOST_CHECK(spring == Season::Spring);
    BOOST_CHECK(summer == Season::Summer);
    BOOST_CHECK(fall == Season::Fall);
    BOOST_CHECK(winter == Season::Winter);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SEASON CONFIG DEFAULTS TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SeasonConfigDefaultsTests)

BOOST_AUTO_TEST_CASE(TestSpringDefaults) {
    SeasonConfig config = SeasonConfig::getDefault(Season::Spring);

    BOOST_CHECK(approxEqual(config.sunriseHour, 6.0f));
    BOOST_CHECK(approxEqual(config.sunsetHour, 19.0f));
    BOOST_CHECK(approxEqual(config.minTemperature, 45.0f));
    BOOST_CHECK(approxEqual(config.maxTemperature, 70.0f));

    // Weather probabilities
    BOOST_CHECK(approxEqual(config.weatherProbs.clear, 0.35f));
    BOOST_CHECK(approxEqual(config.weatherProbs.rainy, 0.25f));
    BOOST_CHECK(approxEqual(config.weatherProbs.snowy, 0.00f));  // No snow in spring
}

BOOST_AUTO_TEST_CASE(TestSummerDefaults) {
    SeasonConfig config = SeasonConfig::getDefault(Season::Summer);

    BOOST_CHECK(approxEqual(config.sunriseHour, 5.0f));
    BOOST_CHECK(approxEqual(config.sunsetHour, 21.0f));
    BOOST_CHECK(approxEqual(config.minTemperature, 70.0f));
    BOOST_CHECK(approxEqual(config.maxTemperature, 95.0f));

    // Weather probabilities - summer has most clear days
    BOOST_CHECK(approxEqual(config.weatherProbs.clear, 0.50f));
    BOOST_CHECK(approxEqual(config.weatherProbs.snowy, 0.00f));  // No snow in summer
}

BOOST_AUTO_TEST_CASE(TestFallDefaults) {
    SeasonConfig config = SeasonConfig::getDefault(Season::Fall);

    BOOST_CHECK(approxEqual(config.sunriseHour, 6.5f));
    BOOST_CHECK(approxEqual(config.sunsetHour, 18.0f));
    BOOST_CHECK(approxEqual(config.minTemperature, 40.0f));
    BOOST_CHECK(approxEqual(config.maxTemperature, 65.0f));

    // Weather probabilities - fall has more fog
    BOOST_CHECK(approxEqual(config.weatherProbs.foggy, 0.10f));
    BOOST_CHECK(approxEqual(config.weatherProbs.snowy, 0.00f));  // No snow in fall
}

BOOST_AUTO_TEST_CASE(TestWinterDefaults) {
    SeasonConfig config = SeasonConfig::getDefault(Season::Winter);

    BOOST_CHECK(approxEqual(config.sunriseHour, 7.5f));
    BOOST_CHECK(approxEqual(config.sunsetHour, 17.0f));
    BOOST_CHECK(approxEqual(config.minTemperature, 20.0f));
    BOOST_CHECK(approxEqual(config.maxTemperature, 45.0f));

    // Weather probabilities - winter has snow
    BOOST_CHECK(approxEqual(config.weatherProbs.snowy, 0.25f));
    BOOST_CHECK_GT(config.weatherProbs.snowy, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestDaylightDurationBySeason) {
    // Summer should have longest days
    SeasonConfig summer = SeasonConfig::getDefault(Season::Summer);
    float summerDaylight = summer.sunsetHour - summer.sunriseHour;

    // Winter should have shortest days
    SeasonConfig winter = SeasonConfig::getDefault(Season::Winter);
    float winterDaylight = winter.sunsetHour - winter.sunriseHour;

    BOOST_CHECK_GT(summerDaylight, winterDaylight);

    // Spring and Fall should be in between
    SeasonConfig spring = SeasonConfig::getDefault(Season::Spring);
    float springDaylight = spring.sunsetHour - spring.sunriseHour;

    BOOST_CHECK_GT(summerDaylight, springDaylight);
    BOOST_CHECK_GT(springDaylight, winterDaylight);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SEASON NAME TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SeasonNameTests, GameTimeManagerSeasonFixture)

BOOST_AUTO_TEST_CASE(TestGetSeasonName) {
    // Spring (days 1-30)
    gameTime->setGameDay(1);
    BOOST_CHECK_EQUAL(std::string(gameTime->getSeasonName()), "Spring");

    // Summer (days 31-60)
    gameTime->setGameDay(31);
    BOOST_CHECK_EQUAL(std::string(gameTime->getSeasonName()), "Summer");

    // Fall (days 61-90)
    gameTime->setGameDay(61);
    BOOST_CHECK_EQUAL(std::string(gameTime->getSeasonName()), "Fall");

    // Winter (days 91-120)
    gameTime->setGameDay(91);
    BOOST_CHECK_EQUAL(std::string(gameTime->getSeasonName()), "Winter");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GET SEASON CONFIG TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(GetSeasonConfigTests, GameTimeManagerSeasonFixture)

BOOST_AUTO_TEST_CASE(TestGetSeasonConfig) {
    // Set to spring
    gameTime->setGameDay(1);
    gameTime->update(0.0f);

    const SeasonConfig& config = gameTime->getSeasonConfig();

    // Should match spring defaults
    BOOST_CHECK(approxEqual(config.sunriseHour, 6.0f));
    BOOST_CHECK(approxEqual(config.sunsetHour, 19.0f));
}

BOOST_AUTO_TEST_CASE(TestSeasonConfigUpdateOnSeasonChange) {
    // Start in spring
    gameTime->setGameDay(1);
    const SeasonConfig& springConfig = gameTime->getSeasonConfig();
    float springSunrise = springConfig.sunriseHour;

    // Move to summer
    gameTime->setGameDay(31);
    const SeasonConfig& summerConfig = gameTime->getSeasonConfig();
    float summerSunrise = summerConfig.sunriseHour;

    // Sunrise times should be different
    BOOST_CHECK_NE(springSunrise, summerSunrise);
    BOOST_CHECK(approxEqual(summerSunrise, 5.0f));  // Summer sunrise is earlier
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// CURRENT TEMPERATURE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CurrentTemperatureTests, GameTimeManagerSeasonFixture)

BOOST_AUTO_TEST_CASE(TestCurrentTemperatureRange) {
    // Set to spring
    gameTime->setGameDay(1);
    gameTime->update(0.0f);

    float temp = gameTime->getCurrentTemperature();
    const SeasonConfig& config = gameTime->getSeasonConfig();

    // Temperature should be between min and max
    BOOST_CHECK_GE(temp, config.minTemperature);
    BOOST_CHECK_LE(temp, config.maxTemperature);
}

BOOST_AUTO_TEST_CASE(TestTemperatureVariesWithTimeOfDay) {
    gameTime->setGameDay(1);

    // Check temperature at different hours
    BOOST_REQUIRE(gameTime->init(4.0f, 1.0f));  // 4 AM - coldest
    float tempAt4AM = gameTime->getCurrentTemperature();

    BOOST_REQUIRE(gameTime->init(14.0f, 1.0f));  // 2 PM - warmest
    float tempAt2PM = gameTime->getCurrentTemperature();

    // 2 PM should be warmer than 4 AM
    BOOST_CHECK_GT(tempAt2PM, tempAt4AM);
}

BOOST_AUTO_TEST_CASE(TestTemperatureChangesBySeason) {
    // Summer temperature
    gameTime->setGameDay(31);  // Summer
    gameTime->setGameHour(12.0f);
    float summerTemp = gameTime->getCurrentTemperature();

    // Winter temperature
    gameTime->setGameDay(91);  // Winter
    gameTime->setGameHour(12.0f);
    float winterTemp = gameTime->getCurrentTemperature();

    // Summer should be warmer than winter at the same time of day
    BOOST_CHECK_GT(summerTemp, winterTemp);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SEASON TRANSITION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SeasonTransitionTests, GameTimeManagerSeasonFixture)

BOOST_AUTO_TEST_CASE(TestSeasonTransitions) {
    // Start in spring
    gameTime->setGameDay(1);
    BOOST_CHECK(gameTime->getSeason() == Season::Spring);

    // Transition to summer
    gameTime->setGameDay(31);
    BOOST_CHECK(gameTime->getSeason() == Season::Summer);

    // Transition to fall
    gameTime->setGameDay(61);
    BOOST_CHECK(gameTime->getSeason() == Season::Fall);

    // Transition to winter
    gameTime->setGameDay(91);
    BOOST_CHECK(gameTime->getSeason() == Season::Winter);

    // Transition back to spring (new year)
    gameTime->setGameDay(121);
    BOOST_CHECK(gameTime->getSeason() == Season::Spring);
}

BOOST_AUTO_TEST_CASE(TestGetCurrentSeasonLegacyMethod) {
    // Test the legacy getCurrentSeason(daysPerSeason) method
    // This method calculates season directly from the day number

    // Spring: days 1-30
    gameTime->setGameDay(1);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 0);  // Spring
    gameTime->setGameDay(30);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 0);  // Still Spring

    // Summer: days 31-60
    gameTime->setGameDay(31);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 1);  // Summer
    gameTime->setGameDay(60);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 1);  // Still Summer

    // Fall: days 61-90
    gameTime->setGameDay(61);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 2);  // Fall
    gameTime->setGameDay(90);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 2);  // Still Fall

    // Winter: days 91-120
    gameTime->setGameDay(91);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 3);  // Winter
    gameTime->setGameDay(120);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 3);  // Still Winter

    // Year wraps: day 121 = Spring again
    gameTime->setGameDay(121);
    BOOST_CHECK_EQUAL(gameTime->getCurrentSeason(30), 0);  // Spring (new year)
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// WEATHER PROBABILITY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(WeatherProbabilityTests)

BOOST_AUTO_TEST_CASE(TestWeatherProbabilitiesSumToOne) {
    // Check that weather probabilities sum to approximately 1.0 for each season
    for (int i = 0; i < 4; ++i) {
        Season season = static_cast<Season>(i);
        SeasonConfig config = SeasonConfig::getDefault(season);
        const auto& probs = config.weatherProbs;

        float sum = probs.clear + probs.cloudy + probs.rainy +
                    probs.stormy + probs.foggy + probs.snowy + probs.windy;

        BOOST_CHECK(approxEqual(sum, 1.0f, 0.01f));
    }
}

BOOST_AUTO_TEST_CASE(TestWinterHasSnowProbability) {
    SeasonConfig winterConfig = SeasonConfig::getDefault(Season::Winter);
    BOOST_CHECK_GT(winterConfig.weatherProbs.snowy, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestNonWinterSeasonsNoSnow) {
    SeasonConfig springConfig = SeasonConfig::getDefault(Season::Spring);
    SeasonConfig summerConfig = SeasonConfig::getDefault(Season::Summer);
    SeasonConfig fallConfig = SeasonConfig::getDefault(Season::Fall);

    BOOST_CHECK(approxEqual(springConfig.weatherProbs.snowy, 0.0f));
    BOOST_CHECK(approxEqual(summerConfig.weatherProbs.snowy, 0.0f));
    BOOST_CHECK(approxEqual(fallConfig.weatherProbs.snowy, 0.0f));
}

BOOST_AUTO_TEST_SUITE_END()
