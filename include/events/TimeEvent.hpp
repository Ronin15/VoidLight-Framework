/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef TIME_EVENT_HPP
#define TIME_EVENT_HPP

#include "events/Event.hpp"
#include "events/WeatherEvent.hpp"
#include "managers/Season.hpp"
#include <string>
#include <string_view>

/**
 * @brief Time of day periods for visual effects
 */
enum class TimePeriod : uint8_t
{
    Morning = 0,   // 5:00 - 8:00
    Day = 1,       // 8:00 - 17:00
    Evening = 2,   // 17:00 - 21:00
    Night = 3      // 21:00 - 5:00
};

/**
 * @brief Event types for time-related changes
 */
enum class TimeEventType
{
    HourChanged,        // Every in-game hour
    DayChanged,         // When day advances
    MonthChanged,       // When month changes
    SeasonChanged,      // When season changes
    YearChanged,        // When year increments
    WeatherCheck,       // Periodic weather roll
    TimePeriodChanged   // When time period changes (Morning/Day/Evening/Night)
};

/**
 * @brief Base class for all time-related events
 */
class TimeEvent : public Event
{
public:
    explicit TimeEvent(TimeEventType eventType)
        : Event(), m_timeEventType(eventType) {}

    ~TimeEvent() override = default;

    TimeEventType getTimeEventType() const { return m_timeEventType; }

    std::string getTypeName() const override { return "TimeEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Time; }

    // Required Event interface implementations
    void update() override {}
    void execute() override {}
    void clean() override {}
    std::string getName() const override { return getTypeName(); }
    std::string getType() const override { return getTypeName(); }
    bool checkConditions() override { return true; }

    void reset() override
    {
        Event::resetCooldown();
        m_hasTriggered = false;
    }

protected:
    TimeEventType m_timeEventType;
};

/**
 * @brief Event fired when the game hour changes
 */
class HourChangedEvent : public TimeEvent
{
public:
    HourChangedEvent(int hour, bool isNight)
        : TimeEvent(TimeEventType::HourChanged),
          m_hour(hour), m_isNight(isNight) {}

    int getHour() const { return m_hour; }
    bool isNight() const { return m_isNight; }

    std::string getTypeName() const override { return "HourChangedEvent"; }
    std::string getName() const override { return "HourChangedEvent"; }
    std::string getType() const override { return "HourChangedEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_hour = 0;
        m_isNight = false;
    }

private:
    int m_hour{0};
    bool m_isNight{false};
};

/**
 * @brief Event fired when a new day begins
 */
class DayChangedEvent : public TimeEvent
{
public:
    DayChangedEvent(int day, int dayOfMonth, int month, const std::string& monthName)
        : TimeEvent(TimeEventType::DayChanged),
          m_day(day), m_dayOfMonth(dayOfMonth), m_month(month), m_monthName(monthName) {}

    int getDay() const { return m_day; }
    int getDayOfMonth() const { return m_dayOfMonth; }
    int getMonth() const { return m_month; }
    const std::string& getMonthName() const { return m_monthName; }

    std::string getTypeName() const override { return "DayChangedEvent"; }
    std::string getName() const override { return "DayChangedEvent"; }
    std::string getType() const override { return "DayChangedEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_day = 0;
        m_dayOfMonth = 0;
        m_month = 0;
        m_monthName.clear();
    }

private:
    int m_day{0};
    int m_dayOfMonth{0};
    int m_month{0};
    std::string m_monthName;
};

/**
 * @brief Event fired when the month changes
 */
class MonthChangedEvent : public TimeEvent
{
public:
    MonthChangedEvent(int month, const std::string& monthName, Season season)
        : TimeEvent(TimeEventType::MonthChanged),
          m_month(month), m_monthName(monthName), m_season(season) {}

    int getMonth() const { return m_month; }
    const std::string& getMonthName() const { return m_monthName; }
    Season getSeason() const { return m_season; }

    std::string getTypeName() const override { return "MonthChangedEvent"; }
    std::string getName() const override { return "MonthChangedEvent"; }
    std::string getType() const override { return "MonthChangedEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_month = 0;
        m_monthName.clear();
        m_season = Season::Spring;
    }

private:
    int m_month{0};
    std::string m_monthName;
    Season m_season{Season::Spring};
};

/**
 * @brief Event fired when the season changes
 */
class SeasonChangedEvent : public TimeEvent
{
public:
    SeasonChangedEvent(Season newSeason, Season previousSeason,
                       const std::string& seasonName)
        : TimeEvent(TimeEventType::SeasonChanged),
          m_season(newSeason), m_previousSeason(previousSeason),
          m_seasonName(seasonName) {}

    Season getSeason() const { return m_season; }
    Season getPreviousSeason() const { return m_previousSeason; }
    const std::string& getSeasonName() const { return m_seasonName; }

    std::string getTypeName() const override { return "SeasonChangedEvent"; }
    std::string getName() const override { return "SeasonChangedEvent"; }
    std::string getType() const override { return "SeasonChangedEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_season = Season::Spring;
        m_previousSeason = Season::Spring;
        m_seasonName.clear();
    }

private:
    Season m_season{Season::Spring};
    Season m_previousSeason{Season::Spring};
    std::string m_seasonName;
};

/**
 * @brief Event fired when a new year begins
 */
class YearChangedEvent : public TimeEvent
{
public:
    explicit YearChangedEvent(int year)
        : TimeEvent(TimeEventType::YearChanged), m_year(year) {}

    int getYear() const { return m_year; }

    std::string getTypeName() const override { return "YearChangedEvent"; }
    std::string getName() const override { return "YearChangedEvent"; }
    std::string getType() const override { return "YearChangedEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_year = 0;
    }

private:
    int m_year{0};
};

/**
 * @brief Event fired when automatic weather should be checked/updated
 */
class WeatherCheckEvent : public TimeEvent
{
public:
    WeatherCheckEvent(Season season, WeatherType recommendedWeather)
        : TimeEvent(TimeEventType::WeatherCheck),
          m_season(season), m_recommendedWeather(recommendedWeather) {}

    Season getSeason() const { return m_season; }
    WeatherType getRecommendedWeather() const { return m_recommendedWeather; }

    std::string getTypeName() const override { return "WeatherCheckEvent"; }
    std::string getName() const override { return "WeatherCheckEvent"; }
    std::string getType() const override { return "WeatherCheckEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_season = Season::Spring;
        // m_recommendedWeather will be set on next use
    }

private:
    Season m_season{Season::Spring};
    WeatherType m_recommendedWeather;
};

/**
 * @brief Visual configuration for a time period (overlay tint)
 */
struct TimePeriodVisuals
{
    uint8_t overlayR{0};
    uint8_t overlayG{0};
    uint8_t overlayB{0};
    uint8_t overlayA{0};  // Alpha 0 = no tint

    // Factory methods for default configurations
    static TimePeriodVisuals getMorning() { return {255, 140, 80, 30}; }    // Red-orange dawn
    static TimePeriodVisuals getDay() { return {255, 255, 200, 8}; }        // Slight yellow
    static TimePeriodVisuals getEvening() { return {255, 80, 40, 40}; }     // Orange-red sunset
    static TimePeriodVisuals getNight() { return {20, 20, 60, 90}; }        // Darker blue/purple

    static TimePeriodVisuals getForPeriod(TimePeriod period)
    {
        switch (period) {
            case TimePeriod::Morning: return getMorning();
            case TimePeriod::Day:     return getDay();
            case TimePeriod::Evening: return getEvening();
            case TimePeriod::Night:   return getNight();
            default:                  return getDay();
        }
    }
};

/**
 * @brief Event fired when the time period changes (Morning/Day/Evening/Night)
 */
class TimePeriodChangedEvent : public TimeEvent
{
public:
    TimePeriodChangedEvent(TimePeriod newPeriod, TimePeriod previousPeriod,
                           const TimePeriodVisuals& visuals)
        : TimeEvent(TimeEventType::TimePeriodChanged),
          m_period(newPeriod), m_previousPeriod(previousPeriod), m_visuals(visuals) {}

    TimePeriod getPeriod() const { return m_period; }
    TimePeriod getPreviousPeriod() const { return m_previousPeriod; }
    const TimePeriodVisuals& getVisuals() const { return m_visuals; }

    std::string_view getPeriodName() const
    {
        switch (m_period) {
            case TimePeriod::Morning: return "Morning";
            case TimePeriod::Day:     return "Day";
            case TimePeriod::Evening: return "Evening";
            case TimePeriod::Night:   return "Night";
            default:                  return "Unknown";
        }
    }

    std::string getTypeName() const override { return "TimePeriodChangedEvent"; }
    std::string getName() const override { return "TimePeriodChangedEvent"; }
    std::string getType() const override { return "TimePeriodChangedEvent"; }

    void reset() override
    {
        TimeEvent::reset();
        m_period = TimePeriod::Day;
        m_previousPeriod = TimePeriod::Day;
        m_visuals = TimePeriodVisuals::getDay();
    }

private:
    TimePeriod m_period{TimePeriod::Day};
    TimePeriod m_previousPeriod{TimePeriod::Day};
    TimePeriodVisuals m_visuals;
};

#endif // TIME_EVENT_HPP
