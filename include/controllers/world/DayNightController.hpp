/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef DAY_NIGHT_CONTROLLER_HPP
#define DAY_NIGHT_CONTROLLER_HPP

/**
 * @file DayNightController.hpp
 * @brief Controller that tracks time periods and manages GPU lighting
 *
 * Subscribes to HourChangedEvent and dispatches TimePeriodChangedEvent when the
 * time period changes (Morning/Day/Evening/Night). Manages GPU lighting by
 * interpolating ambient colors and updating GPURenderer each frame.
 *
 * Ownership: GameState owns the controller instance (not a singleton).
 *
 * Event flow:
 *   GameTimeManager::dispatchTimeEvents() -> HourChangedEvent (Deferred)
 *     -> DayNightController detects period change
 *     -> Sets target lighting values, dispatches TimePeriodChangedEvent
 *     -> update() interpolates current toward target each frame
 *     -> GPURenderer composite shader applies lighting
 */

#include "controllers/ControllerBase.hpp"
#include "events/TimeEvent.hpp"
#include <string_view>

class DayNightController : public ControllerBase
{
public:
    DayNightController() = default;
    ~DayNightController() override = default;

    // Movable (inherited from base)
    DayNightController(DayNightController&&) noexcept = default;
    DayNightController& operator=(DayNightController&&) noexcept = default;

    /**
     * @brief Subscribe to time events and start tracking time periods
     * @note Called by ControllerRegistry::subscribeAll()
     */
    void subscribe() override;

    /**
     * @brief Update lighting interpolation and GPU state
     * @param deltaTime Time since last frame in seconds
     * @note Call this each frame from the owning game state's update()
     */
    void update(float deltaTime);

    /**
     * @brief Get controller name for debugging
     * @return "DayNightController"
     */
    [[nodiscard]] std::string_view getName() const override { return "DayNightController"; }

    /**
     * @brief Get the current time period
     * @return Current TimePeriod enum value
     */
    [[nodiscard]] TimePeriod getCurrentPeriod() const { return m_currentPeriod; }

    /**
     * @brief Get the current time period as string (zero allocation)
     * @return String view: "Morning", "Day", "Evening", or "Night"
     */
    [[nodiscard]] std::string_view getCurrentPeriodString() const;

    /**
     * @brief Get a descriptive message for the current time period
     * @return String view: "Dawn approaches", "The sun rises high", etc.
     */
    [[nodiscard]] std::string_view getCurrentPeriodDescription() const;

    /**
     * @brief Get the visual configuration for the current period
     * @return TimePeriodVisuals with overlay color values
     */
    [[nodiscard]] TimePeriodVisuals getCurrentVisuals() const;

private:
    /**
     * @brief Handler for time events
     * @param data Event data containing the time event
     */
    void onTimeEvent(const EventData& data);

    /**
     * @brief Transition to a new time period and dispatch event
     * @param newPeriod The new time period
     */
    void transitionToPeriod(TimePeriod newPeriod);

    /**
     * @brief Determine time period from hour
     * @param hour Current game hour (0-23.999)
     * @return Corresponding TimePeriod
     */
    static TimePeriod hourToTimePeriod(float hour);

    /**
     * @brief Update GPU renderer with current lighting values
     */
    void updateGPULighting();

    // Current state
    TimePeriod m_currentPeriod{TimePeriod::Day};
    TimePeriod m_previousPeriod{TimePeriod::Day};

    // Lighting interpolation state (0-255 range, matching TimePeriodVisuals)
    float m_currentR{255.0f};
    float m_currentG{255.0f};
    float m_currentB{255.0f};
    float m_currentA{0.0f};
    float m_targetR{255.0f};
    float m_targetG{255.0f};
    float m_targetB{255.0f};
    float m_targetA{0.0f};

    // Transition timing
    static constexpr float TRANSITION_DURATION{30.0f};  // seconds for full transition

    // True once subscribe() has run at least once. Distinct from
    // ControllerBase's m_subscribed: that flag also flips on every
    // resume()-triggered resubscribe after suspend(). Combined with a
    // same-period check in subscribe(), this prevents re-snapping lighting
    // or re-dispatching TimePeriodChangedEvent when nothing actually changed.
    bool m_everInitialized{false};
};

#endif // DAY_NIGHT_CONTROLLER_HPP
