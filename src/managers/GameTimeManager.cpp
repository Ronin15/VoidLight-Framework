/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "managers/GameTimeManager.hpp"
#include "events/TimeEvent.hpp"
#include "managers/EventManager.hpp"
#include <cmath>
#include <format>
#include <numeric>
#include <random>

// ============================================================================
// SeasonConfig Implementation
// ============================================================================

SeasonConfig SeasonConfig::getDefault(Season season)
{
    SeasonConfig config;

    switch (season)
    {
        case Season::Spring:
            config.sunriseHour = 6.0f;
            config.sunsetHour = 19.0f;
            config.minTemperature = 45.0f;
            config.maxTemperature = 70.0f;
            config.weatherProbs.clear = 0.35f;
            config.weatherProbs.cloudy = 0.25f;
            config.weatherProbs.rainy = 0.25f;
            config.weatherProbs.stormy = 0.05f;
            config.weatherProbs.foggy = 0.05f;
            config.weatherProbs.snowy = 0.00f;
            config.weatherProbs.windy = 0.05f;
            break;

        case Season::Summer:
            config.sunriseHour = 5.0f;
            config.sunsetHour = 21.0f;
            config.minTemperature = 70.0f;
            config.maxTemperature = 95.0f;
            config.weatherProbs.clear = 0.50f;
            config.weatherProbs.cloudy = 0.20f;
            config.weatherProbs.rainy = 0.15f;
            config.weatherProbs.stormy = 0.10f;
            config.weatherProbs.foggy = 0.00f;
            config.weatherProbs.snowy = 0.00f;
            config.weatherProbs.windy = 0.05f;
            break;

        case Season::Fall:
            config.sunriseHour = 6.5f;
            config.sunsetHour = 18.0f;
            config.minTemperature = 40.0f;
            config.maxTemperature = 65.0f;
            config.weatherProbs.clear = 0.30f;
            config.weatherProbs.cloudy = 0.30f;
            config.weatherProbs.rainy = 0.20f;
            config.weatherProbs.stormy = 0.05f;
            config.weatherProbs.foggy = 0.10f;
            config.weatherProbs.snowy = 0.00f;
            config.weatherProbs.windy = 0.05f;
            break;

        case Season::Winter:
            config.sunriseHour = 7.5f;
            config.sunsetHour = 17.0f;
            config.minTemperature = 20.0f;
            config.maxTemperature = 45.0f;
            config.weatherProbs.clear = 0.25f;
            config.weatherProbs.cloudy = 0.25f;
            config.weatherProbs.rainy = 0.10f;
            config.weatherProbs.stormy = 0.05f;
            config.weatherProbs.foggy = 0.05f;
            config.weatherProbs.snowy = 0.25f;
            config.weatherProbs.windy = 0.05f;
            break;
    }

    return config;
}

// ============================================================================
// CalendarConfig Implementation
// ============================================================================

CalendarConfig CalendarConfig::createDefault()
{
    CalendarConfig config;
    config.months = {
        {"Bloomtide", 30, Season::Spring},
        {"Sunpeak", 30, Season::Summer},
        {"Harvestmoon", 30, Season::Fall},
        {"Frosthold", 30, Season::Winter}
    };
    return config;
}

int CalendarConfig::getTotalDaysInYear() const
{
    return std::accumulate(
        months.begin(),
        months.end(),
        0,
        [](int sum, const CalendarMonth& month) {
            return sum + month.dayCount;
        }
    );
}

// ============================================================================
// GameTimeManager Implementation
// ============================================================================

GameTimeManager::GameTimeManager() :
    m_currentHour(12.0f),
    m_currentDay(1),
    m_totalGameSeconds(43200.0f), // 12 hours in seconds
    m_timeScale(1.0f),
    m_sunriseHour(6.0f),
    m_sunsetHour(18.0f),
    m_lastUpdateTime(std::chrono::steady_clock::now()),
    m_calendarConfig(CalendarConfig::createDefault()),
    m_currentMonth(0),
    m_dayOfMonth(1),
    m_currentYear(1),
    m_currentSeason(Season::Spring),
    m_currentSeasonConfig(SeasonConfig::getDefault(Season::Spring)),
    m_previousHour(-1),
    m_previousDay(-1),
    m_previousMonth(-1),
    m_previousYear(-1),
    m_previousSeason(Season::Spring),
    m_weatherCheckInterval(4.0f),
    m_lastWeatherCheckHour(0.0f),
    m_autoWeatherEnabled(false)
{
    // Initialize calendar state from current day
    updateCalendarState();
}

bool GameTimeManager::init(float startHour, float timeScale)
{
    // Validate input parameters
    if (startHour < 0.0f || startHour >= 24.0f)
    {
        return false;
    }

    if (timeScale <= 0.0f)
    {
        return false;
    }

    // Set initial time values
    m_currentHour = startHour;
    m_currentDay = 1;
    m_timeScale = timeScale;

    // Calculate total seconds based on current hour
    m_totalGameSeconds = startHour * 3600.0f;

    // Initialize last update time
    m_lastUpdateTime = std::chrono::steady_clock::now();

    // Initialize calendar with default config if not already set
    if (m_calendarConfig.months.empty())
    {
        m_calendarConfig = CalendarConfig::createDefault();
    }

    // Initialize calendar state
    m_currentMonth = 0;
    m_dayOfMonth = 1;
    m_currentYear = 1;
    updateCalendarState();

    // Initialize previous state for change detection
    m_previousHour = static_cast<int>(startHour);
    m_previousDay = 1;
    m_previousMonth = 0;
    m_previousYear = 1;
    m_previousSeason = m_currentSeason;

    // Initialize weather check hour
    m_lastWeatherCheckHour = startHour;

    return true;
}

void GameTimeManager::update(float deltaTime)
{
    // Do nothing if globally paused
    if (m_globallyPaused.load(std::memory_order_acquire))
    {
        return;
    }

    // Store previous state for change detection
    m_previousHour = static_cast<int>(m_currentHour);
    m_previousDay = m_currentDay;
    m_previousMonth = m_currentMonth;
    m_previousYear = m_currentYear;
    m_previousSeason = m_currentSeason;

    // Convert delta time to game seconds based on time scale
    float const deltaGameSeconds = deltaTime * m_timeScale;

    // Advance game time
    advanceTime(deltaGameSeconds);

    // PERFORMANCE: Only recalculate calendar when day actually changes
    // Calendar state (month, year, season) only needs updating on day transitions
    if (m_currentDay != m_previousDay)
    {
        updateCalendarState();
    }

    // Dispatch time change events
    dispatchTimeEvents();

    // Check for automatic weather updates
    checkWeatherUpdate();
}

void GameTimeManager::setGlobalPause(bool paused)
{
    m_globallyPaused.store(paused, std::memory_order_release);
    if (!paused)
    {
        // Reset last update time to avoid time jump after resume
        m_lastUpdateTime = std::chrono::steady_clock::now();
    }
}

bool GameTimeManager::isGloballyPaused() const
{
    return m_globallyPaused.load(std::memory_order_acquire);
}

void GameTimeManager::advanceTime(float deltaGameSeconds)
{
    // Increment total game seconds
    m_totalGameSeconds += deltaGameSeconds;

    // Calculate new hour and day
    float const totalHours = m_totalGameSeconds / 3600.0f;
    float newHour = std::fmod(totalHours, 24.0f);
    int const newDay = static_cast<int>(totalHours / 24.0f) + 1;

    // Update hour and day
    m_currentHour = newHour;
    m_currentDay = newDay;
}

void GameTimeManager::updateCalendarState()
{
    if (m_calendarConfig.months.empty())
    {
        // Fallback: use simple season calculation
        int const seasonIndex = ((m_currentDay - 1) / 30) % 4;
        m_currentSeason = static_cast<Season>(seasonIndex);
        m_currentSeasonConfig = SeasonConfig::getDefault(m_currentSeason);
        return;
    }

    int totalDaysInYear = m_calendarConfig.getTotalDaysInYear();
    if (totalDaysInYear <= 0)
    {
        return;
    }

    // Calculate year and day within year (0-based)
    int const daysSinceStart = m_currentDay - 1;  // Convert to 0-based
    m_currentYear = (daysSinceStart / totalDaysInYear) + 1;
    int dayInYear = daysSinceStart % totalDaysInYear;

    // Find which month this day falls into
    int accumulatedDays = 0;
    for (size_t i = 0; i < m_calendarConfig.months.size(); ++i)
    {
        const auto& month = m_calendarConfig.months[i];
        if (dayInYear < accumulatedDays + month.dayCount)
        {
            m_currentMonth = static_cast<int>(i);
            m_dayOfMonth = dayInYear - accumulatedDays + 1;  // Convert to 1-based
            break;
        }
        accumulatedDays += month.dayCount;
    }

    // Update season from current month
    updateSeasonFromCalendar();
}

void GameTimeManager::updateSeasonFromCalendar()
{
    if (m_calendarConfig.months.empty())
    {
        return;
    }

    // Get season from current month
    if (m_currentMonth >= 0 &&
        m_currentMonth < static_cast<int>(m_calendarConfig.months.size()))
    {
        Season newSeason = m_calendarConfig.months[m_currentMonth].season;
        if (newSeason != m_currentSeason)
        {
            m_currentSeason = newSeason;
            m_currentSeasonConfig = SeasonConfig::getDefault(m_currentSeason);

            // Update daylight hours based on season
            m_sunriseHour = m_currentSeasonConfig.sunriseHour;
            m_sunsetHour = m_currentSeasonConfig.sunsetHour;
        }
    }
}

void GameTimeManager::dispatchTimeEvents()
{
    // Check for hour change
    int const currentHourInt = static_cast<int>(m_currentHour);
    if (currentHourInt != m_previousHour && m_previousHour >= 0)
    {
        auto event = std::make_shared<HourChangedEvent>(currentHourInt, isNighttime());
        EventManager::Instance().dispatchEvent(event, EventManager::DispatchMode::Deferred);
    }

    // Check for day change
    if (m_currentDay != m_previousDay && m_previousDay >= 0)
    {
        // Convert string_view to string for event storage
        auto event = std::make_shared<DayChangedEvent>(
            m_currentDay, m_dayOfMonth, m_currentMonth, std::string(getCurrentMonthName()));
        EventManager::Instance().dispatchEvent(event, EventManager::DispatchMode::Deferred);
    }

    // Check for month change
    if (m_currentMonth != m_previousMonth && m_previousMonth >= 0)
    {
        // Convert string_view to string for event storage
        auto event = std::make_shared<MonthChangedEvent>(
            m_currentMonth, std::string(getCurrentMonthName()), m_currentSeason);
        EventManager::Instance().dispatchEvent(event, EventManager::DispatchMode::Deferred);
    }

    // Check for season change
    if (m_currentSeason != m_previousSeason)
    {
        auto event = std::make_shared<SeasonChangedEvent>(
            m_currentSeason, m_previousSeason, std::string(getSeasonName()));
        EventManager::Instance().dispatchEvent(event, EventManager::DispatchMode::Deferred);
    }

    // Check for year change
    if (m_currentYear != m_previousYear && m_previousYear >= 0)
    {
        auto event = std::make_shared<YearChangedEvent>(m_currentYear);
        EventManager::Instance().dispatchEvent(event, EventManager::DispatchMode::Deferred);
    }
}

void GameTimeManager::checkWeatherUpdate()
{
    if (!m_autoWeatherEnabled)
    {
        return;
    }

    // Calculate hours since last weather check
    float hoursSinceCheck = m_currentHour - m_lastWeatherCheckHour;
    if (hoursSinceCheck < 0.0f)
    {
        hoursSinceCheck += 24.0f;  // Handle midnight wraparound
    }

    // Also account for day changes
    if (m_currentDay != m_previousDay)
    {
        hoursSinceCheck += 24.0f * (m_currentDay - m_previousDay - 1);
    }

    if (hoursSinceCheck >= m_weatherCheckInterval)
    {
        m_lastWeatherCheckHour = m_currentHour;

        // Roll for recommended weather and dispatch WeatherCheckEvent
        // Subscribers (e.g., WeatherController) listen for this and call
        // EventManager::changeWeather() to actually change the weather
        WeatherType const newWeather = rollWeatherForSeason();
        auto event = std::make_shared<WeatherCheckEvent>(m_currentSeason, newWeather);
        EventManager::Instance().dispatchEvent(event, EventManager::DispatchMode::Deferred);
    }
}

void GameTimeManager::setGameHour(float hour)
{
    // Validate and set hour
    if (hour >= 0.0f && hour < 24.0f)
    {
        float const oldHour = m_currentHour;
        m_currentHour = hour;

        // Adjust total game seconds to match new hour
        float hourDiff = hour - oldHour;
        if (hourDiff < 0.0f)
        {
            hourDiff += 24.0f; // Handle wraparound (e.g., 23:00 to 01:00)
        }

        m_totalGameSeconds += hourDiff * 3600.0f;
    }
}

void GameTimeManager::setDaylightHours(float sunrise, float sunset)
{
    // Validate input
    if (sunrise >= 0.0f && sunrise < 24.0f &&
        sunset >= 0.0f && sunset < 24.0f &&
        sunrise != sunset)
    {
        m_sunriseHour = sunrise;
        m_sunsetHour = sunset;
    }
}

void GameTimeManager::setGameDay(int day)
{
    m_currentDay = (day >= 1) ? day : 1;
    // Update totalGameSeconds to match the new day (preserving current hour)
    // This ensures advanceTime() doesn't overwrite the day on next update()
    m_totalGameSeconds = (static_cast<float>(m_currentDay - 1) * 24.0f + m_currentHour) * 3600.0f;
    updateCalendarState();
}

bool GameTimeManager::isDaytime() const
{
    if (m_sunriseHour < m_sunsetHour)
    {
        // Simple case: sunrise is before sunset
        return m_currentHour >= m_sunriseHour && m_currentHour < m_sunsetHour;
    }
    else
    {
        // Complex case: sunrise is after sunset (e.g., sunrise at 22:00, sunset at 4:00)
        return m_currentHour >= m_sunriseHour || m_currentHour < m_sunsetHour;
    }
}

bool GameTimeManager::isNighttime() const
{
    return !isDaytime();
}

std::string_view GameTimeManager::getTimeOfDayName() const
{
    switch (hourToTimePeriod(m_currentHour)) {
        case TimePeriod::Morning: return "Morning";
        case TimePeriod::Day:     return "Day";
        case TimePeriod::Evening: return "Evening";
        case TimePeriod::Night:   return "Night";
    }
    return "Day";
}

int GameTimeManager::getCurrentSeason(int daysPerSeason) const
{
    // Validate input
    if (daysPerSeason <= 0)
    {
        daysPerSeason = 30; // Default fallback
    }

    // Calculate which season we're in based on current day
    // Seasons: 0=Spring, 1=Summer, 2=Fall, 3=Winter
    int const seasonIndex = ((m_currentDay - 1) / daysPerSeason) % 4;
    return seasonIndex;
}

std::string_view GameTimeManager::formatCurrentTime(bool use24Hour)
{
    int const hours = static_cast<int>(m_currentHour);
    int const minutes = static_cast<int>((m_currentHour - hours) * 60.0f);

    // Ensure capacity on first call (C++20 type-safe, zero allocations after first use)
    if (m_timeFormatBuffer.capacity() < 16) {
        m_timeFormatBuffer.reserve(16);
    }
    m_timeFormatBuffer.clear();

    if (use24Hour)
    {
        // 24-hour format (e.g., "14:30")
        std::format_to(std::back_inserter(m_timeFormatBuffer),
                       "{:02d}:{:02d}", hours, minutes);
    }
    else
    {
        // 12-hour format (e.g., "2:30 PM")
        int displayHour = hours % 12;
        if (displayHour == 0)
            displayHour = 12;

        std::format_to(std::back_inserter(m_timeFormatBuffer),
                       "{}:{:02d} {}", displayHour, minutes, hours >= 12 ? "PM" : "AM");
    }

    return m_timeFormatBuffer;
}

// ============================================================================
// Calendar Methods
// ============================================================================

void GameTimeManager::setCalendarConfig(const CalendarConfig& config)
{
    m_calendarConfig = config;
    updateCalendarState();
}

std::string_view GameTimeManager::getCurrentMonthName() const
{
    if (m_calendarConfig.months.empty())
        return "Unknown";

    if (m_currentMonth >= 0 &&
        m_currentMonth < static_cast<int>(m_calendarConfig.months.size()))
    {
        return m_calendarConfig.months[m_currentMonth].name;
    }

    return "Unknown";
}

int GameTimeManager::getDaysInCurrentMonth() const
{
    if (m_calendarConfig.months.empty())
    {
        return 30;  // Default
    }

    if (m_currentMonth >= 0 &&
        m_currentMonth < static_cast<int>(m_calendarConfig.months.size()))
    {
        return m_calendarConfig.months[m_currentMonth].dayCount;
    }

    return 30;
}

// ============================================================================
// Season Methods
// ============================================================================

std::string_view GameTimeManager::getSeasonName() const
{
    switch (m_currentSeason)
    {
        case Season::Spring: return "Spring";
        case Season::Summer: return "Summer";
        case Season::Fall:   return "Fall";
        case Season::Winter: return "Winter";
        default:             return "Unknown";
    }
}

const SeasonConfig& GameTimeManager::getSeasonConfig() const
{
    return m_currentSeasonConfig;
}

float GameTimeManager::getCurrentTemperature() const
{
    const auto& config = m_currentSeasonConfig;

    // Temperature varies throughout the day
    // Coldest at 4 AM, warmest at 2 PM (14:00)
    // Create a temperature curve: min at 4AM, max at 2PM
    constexpr float coldestHour = 4.0f;

    float const tempRange = config.maxTemperature - config.minTemperature;
    float hoursSinceColdest = m_currentHour - coldestHour;
    if (hoursSinceColdest < 0.0f)
    {
        hoursSinceColdest += 24.0f;
    }

    // Use cosine for smooth temperature curve
    float const phase = (hoursSinceColdest / 24.0f) * 2.0f * 3.14159f;
    float tempFactor = (1.0f - std::cos(phase)) / 2.0f;

    return config.minTemperature + (tempRange * tempFactor);
}

// ============================================================================
// Weather Methods
// ============================================================================

void GameTimeManager::setWeatherCheckInterval(float gameHours)
{
    if (gameHours > 0.0f)
    {
        m_weatherCheckInterval = gameHours;
    }
}

WeatherType GameTimeManager::rollWeatherForSeason() const
{
    return rollWeatherForSeason(m_currentSeason);
}

WeatherType GameTimeManager::rollWeatherForSeason(Season season) const
{
    // Get weather probabilities for the season
    SeasonConfig const config = SeasonConfig::getDefault(season);
    const auto& probs = config.weatherProbs;

    // Generate random number - use thread_local for thread safety
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float roll = dist(gen);

    // Accumulate probabilities and pick weather type
    float accumulated = 0.0f;

    accumulated += probs.clear;
    if (roll < accumulated) return WeatherType::Clear;

    accumulated += probs.cloudy;
    if (roll < accumulated) return WeatherType::Cloudy;

    accumulated += probs.rainy;
    if (roll < accumulated) return WeatherType::Rainy;

    accumulated += probs.stormy;
    if (roll < accumulated) return WeatherType::Stormy;

    accumulated += probs.foggy;
    if (roll < accumulated) return WeatherType::Foggy;

    accumulated += probs.snowy;
    if (roll < accumulated) return WeatherType::Snowy;

    accumulated += probs.windy;
    if (roll < accumulated) return WeatherType::Windy;

    // Default to Clear if probabilities don't sum to 1.0
    return WeatherType::Clear;
}
