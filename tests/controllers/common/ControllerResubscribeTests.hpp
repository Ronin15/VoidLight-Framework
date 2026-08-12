/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CONTROLLER_RESUBSCRIBE_TESTS_HPP
#define CONTROLLER_RESUBSCRIBE_TESTS_HPP

/**
 * @file ControllerResubscribeTests.hpp
 * @brief Reusable check for controllers that dispatch a "current state"
 *        announcement event from subscribe().
 *
 * Some controllers (e.g. DayNightController) dispatch an event in
 * subscribe() so late subscribers learn the current state. Because
 * ControllerBase::resume() re-calls subscribe() after suspend(), a naive
 * implementation re-dispatches that announcement on every pause/resume
 * cycle even though nothing changed while suspended. Use
 * checkNoDuplicateAnnounceOnResume() in any controller test that has this
 * subscribe()-announces pattern to guard against that regression.
 */

#include "controllers/ControllerBase.hpp"
#include "managers/EventManager.hpp"
#include <boost/test/unit_test.hpp>
#include <functional>

inline void checkNoDuplicateAnnounceOnResume(
    ControllerBase& controller,
    EventTypeId announceEventType,
    const std::function<bool(const EventData&)>& matchesAnnounceEvent,
    int resumeCycles = 3)
{
    int dispatchCount = 0;
    EventManager::Instance().registerHandler(
        announceEventType,
        [&](const EventData& data) {
            if (matchesAnnounceEvent(data)) {
                ++dispatchCount;
            }
        });

    controller.subscribe();
    EventManager::Instance().update(); // flush the initial (deferred) announce
    int const countAfterInitialSubscribe = dispatchCount;

    for (int i = 0; i < resumeCycles; ++i) {
        controller.suspend();
        controller.resume();
    }
    EventManager::Instance().update(); // flush any deferred resubscribe announces

    BOOST_CHECK_EQUAL(dispatchCount, countAfterInitialSubscribe);
}

#endif // CONTROLLER_RESUBSCRIBE_TESTS_HPP
