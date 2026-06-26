/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef GAME_TIME_MANAGER_HPP
#define GAME_TIME_MANAGER_HPP

/**
 * @file GameTimeManager.hpp
 * @brief Management of game time, including day/night cycles, calendar, and seasons
 *
 * The GameTimeManager class provides functionality for:
 * - Tracking real-time vs. game time
 * - Day/night cycles with seasonal variations
 * - Fantasy calendar with custom months and years
 * - Season system with environmental parameters
 * - Automatic weather triggering based on season
 * - Time-based events and scheduling
 */

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

// Forward declaration
enum class WeatherType;

/**
 * @brief Type-safe season enumeration
 */
enum class Season : uint8_t
{
    Spring = 0,
    Summer = 1,
    Fall = 2,
    Winter = 3
};

/**
 * @brief Weather probability configuration for a season
 */
struct WeatherProbabilities
{
    float clear{0.40f};
    float cloudy{0.25f};
    float rainy{0.15f};
    float stormy{0.05f};
    float foggy{0.10f};
    float snowy{0.00f};
    float windy{0.05f};
};

/**
 * @brief Environmental configuration for a specific season
 */
struct SeasonConfig
{
    float sunriseHour{6.0f};
    float sunsetHour{18.0f};
    float minTemperature{50.0f};
    float maxTemperature{80.0f};
    WeatherProbabilities weatherProbs;

    /**
     * @brief Get default configuration for a specific season
     * @param season The season to get defaults for
     * @return SeasonConfig with appropriate defaults
     */
    static SeasonConfig getDefault(Season season);
};

/**
 * @brief Definition of a calendar month
 */
struct CalendarMonth
{
    std::string name;
    int dayCount{30};
    Season season{Season::Spring};
};

/**
 * @brief Calendar configuration with months
 */
struct CalendarConfig
{
    std::vector<CalendarMonth> months;

    /**
     * @brief Create default fantasy calendar (4 months, 30 days each)
     * @return CalendarConfig with Bloomtide, Sunpeak, Harvestmoon, Frosthold
     */
    static CalendarConfig createDefault();

    /**
     * @brief Get total days in a year
     * @return Sum of all month day counts
     */
    int getTotalDaysInYear() const;
};

class GameTimeManager {
public:
    /**
     * @brief Get the singleton instance of GameTimeManager
     * @return Reference to the GameTimeManager instance
     */
    static GameTimeManager& Instance() {
        static GameTimeManager instance;
        return instance;
    }

    /**
     * @brief Initialize the game time system
     * @param startHour Starting hour of game time (0-23)
     * @param timeScale Scale factor for time progression (1.0 = real time)
     * @return True if initialization succeeded, false otherwise
     */
    [[nodiscard]] bool init(float startHour = 12.0f, float timeScale = 1.0f);

    /**
     * @brief Update game time based on real elapsed time
     * @param deltaTime Real time elapsed in seconds since last update
     * @note Does nothing if time is paused
     */
    void update(float deltaTime);

    // ========================================================================
    // Global Pause Control (matching other managers)
    // ========================================================================

    /**
     * @brief Set global pause state for time progression
     * @param paused true to pause time updates, false to resume
     * @note When paused, update() will not advance time or dispatch events
     * @note Called by GameEngine::setGlobalPause() alongside other managers
     */
    void setGlobalPause(bool paused);

    /**
     * @brief Get current global pause state
     * @return true if time updates are globally paused
     */
    bool isGloballyPaused() const;

    /**
     * @brief Get current game hour (0-23.999)
     * @return Current hour including fractional part
     */
    float getGameHour() const { return m_currentHour; }

    /**
     * @brief Get current game day
     * @return Current day number (starts at 1)
     */
    int getGameDay() const { return m_currentDay; }

    /**
     * @brief Get current game season (0=Spring, 1=Summer, 2=Fall, 3=Winter)
     * @param daysPerSeason Number of game days per season (default: 30)
     * @return Current season index (0-3)
     */
    int getCurrentSeason(int daysPerSeason = 30) const;

    /**
     * @brief Get total elapsed game time in seconds
     * @return Total game seconds elapsed
     */
    float getTotalGameTimeSeconds() const { return m_totalGameSeconds; }

    /**
     * @brief Check if it's daytime in the game
     * @return True if current hour is between sunrise and sunset
     */
    bool isDaytime() const;

    /**
     * @brief Check if it's nighttime in the game
     * @return True if current hour is between sunset and sunrise
     */
    bool isNighttime() const;

    /**
     * @brief Get current time of day as string (Morning, Day, Evening, Night)
     * @return String view for time of day
     */
    std::string_view getTimeOfDayName() const;

    /**
     * @brief Set the time scale factor
     * @param scale Scale factor (1.0 = real time, 2.0 = twice as fast, etc.)
     */
    void setTimeScale(float scale) { m_timeScale = scale; }

    /**
     * @brief Get the current time scale factor
     * @return Current time scale
     */
    float getTimeScale() const { return m_timeScale; }

    /**
     * @brief Set the current game hour
     * @param hour Hour to set (0-23.999)
     */
    void setGameHour(float hour);

    /**
     * @brief Set the current game day
     * @param day Day to set (must be >= 1)
     * @note Updates calendar state (month, year, season) based on the new day
     */
    void setGameDay(int day);

    /**
     * @brief Set sunrise and sunset hours
     * @param sunrise Hour when sun rises (0-23.999)
     * @param sunset Hour when sun sets (0-23.999)
     */
    void setDaylightHours(float sunrise, float sunset);

    /**
     * @brief Format current game time as a string
     * @param use24Hour Whether to use 24-hour format
     * @return View into internal buffer (e.g., "14:30" or "2:30 PM")
     * @note Non-const: modifies internal buffer
     */
    std::string_view formatCurrentTime(bool use24Hour = true);

    // ========================================================================
    // Calendar System
    // ========================================================================

    /**
     * @brief Set the calendar configuration
     * @param config Calendar configuration with months
     */
    void setCalendarConfig(const CalendarConfig& config);

    /**
     * @brief Get current month index (0-based)
     * @return Current month index within calendar
     */
    int getCurrentMonth() const { return m_currentMonth; }

    /**
     * @brief Get day of the current month (1-based)
     * @return Day within the current month
     */
    int getDayOfMonth() const { return m_dayOfMonth; }

    /**
     * @brief Get current game year
     * @return Current year (starts at 1)
     */
    int getGameYear() const { return m_currentYear; }

    /**
     * @brief Get the name of the current month
     * @return View into CalendarMonth name (e.g., "Bloomtide", "Sunpeak")
     */
    std::string_view getCurrentMonthName() const;

    /**
     * @brief Get number of days in the current month
     * @return Day count for current month
     */
    int getDaysInCurrentMonth() const;

    // ========================================================================
    // Type-Safe Season System
    // ========================================================================

    /**
     * @brief Get current season as type-safe enum
     * @return Current Season enum value
     */
    Season getSeason() const { return m_currentSeason; }

    /**
     * @brief Get current season name as string
     * @return String view: "Spring", "Summer", "Fall", or "Winter"
     */
    std::string_view getSeasonName() const;

    /**
     * @brief Get environmental configuration for current season
     * @return SeasonConfig with daylight hours, temperatures, weather probabilities
     */
    const SeasonConfig& getSeasonConfig() const;

    /**
     * @brief Get current temperature based on season and time of day
     * @return Temperature value interpolated between season min/max
     */
    float getCurrentTemperature() const;

    // ========================================================================
    // Automatic Weather System
    // ========================================================================

    /**
     * @brief Enable or disable automatic weather changes
     * @param enable True to enable, false to disable
     */
    void enableAutoWeather(bool enable) { m_autoWeatherEnabled = enable; }

    /**
     * @brief Check if automatic weather is enabled
     * @return True if auto weather is active
     */
    bool isAutoWeatherEnabled() const { return m_autoWeatherEnabled; }

    /**
     * @brief Set interval between weather rolls (in game hours)
     * @param gameHours Hours between automatic weather checks
     */
    void setWeatherCheckInterval(float gameHours);

    /**
     * @brief Roll for weather based on current season probabilities
     * @return WeatherType based on probability roll
     */
    WeatherType rollWeatherForSeason() const;

    /**
     * @brief Roll for weather based on specific season probabilities
     * @param season Season to use for probability lookup
     * @return WeatherType based on probability roll
     */
    WeatherType rollWeatherForSeason(Season season) const;

private:
    // Singleton constructor/destructor
    GameTimeManager();
    ~GameTimeManager() = default;

    // Prevent copying
    GameTimeManager(const GameTimeManager&) = delete;
    GameTimeManager& operator=(const GameTimeManager&) = delete;

    // Time tracking
    float m_currentHour{12.0f};       // Current hour (0-23.999)
    int m_currentDay{1};              // Current day (starts at 1)
    float m_totalGameSeconds{0.0f};   // Total game seconds elapsed

    // Time progression
    float m_timeScale{1.0f};          // Scale factor for time progression

    // Daylight settings
    float m_sunriseHour{6.0f};        // Hour when sun rises
    float m_sunsetHour{18.0f};        // Hour when sun sets

    // Real-time tracking
    std::chrono::steady_clock::time_point m_lastUpdateTime;

    // Calendar state
    CalendarConfig m_calendarConfig;
    int m_currentMonth{0};            // 0-based month index
    int m_dayOfMonth{1};              // 1-based day within month
    int m_currentYear{1};             // Year counter (starts at 1)
    Season m_currentSeason{Season::Spring};
    SeasonConfig m_currentSeasonConfig;

    // Previous state for change detection
    int m_previousHour{-1};
    int m_previousDay{-1};
    int m_previousMonth{-1};
    int m_previousYear{-1};
    Season m_previousSeason{Season::Spring};

    // Weather system
    float m_weatherCheckInterval{4.0f};  // Game hours between weather rolls
    float m_lastWeatherCheckHour{0.0f};
    bool m_autoWeatherEnabled{false};

    // Global pause state (atomic for thread safety, matching other managers)
    std::atomic<bool> m_globallyPaused{false};

    // Format buffer for formatCurrentTime() - C++20 type-safe, zero allocations after init
    std::string m_timeFormatBuffer{};

    // Helper methods
    void advanceTime(float deltaGameSeconds);
    void updateCalendarState();
    void updateSeasonFromCalendar();
    void dispatchTimeEvents();
    void checkWeatherUpdate();
};

#endif // GAME_TIME_MANAGER_HPP
