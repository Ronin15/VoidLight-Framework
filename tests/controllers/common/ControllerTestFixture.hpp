/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CONTROLLER_TEST_FIXTURE_HPP
#define CONTROLLER_TEST_FIXTURE_HPP

/**
 * @file ControllerTestFixture.hpp
 * @brief Template base fixture for controller tests
 *
 * Provides common setup/teardown for all controller tests:
 * - EventManager reset and initialization
 * - GameTimeManager initialization
 * - Automatic cleanup
 *
 * Usage:
 *   using MyControllerFixture = ControllerTestFixture<MyController>;
 *   BOOST_FIXTURE_TEST_CASE(TestSomething, MyControllerFixture) { ... }
 */

#include "../events/EventManagerTestAccess.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"

/**
 * @brief Template fixture for controller tests
 * @tparam ControllerType The controller class to test
 *
 * Handles EventManager and GameTimeManager setup/teardown.
 * Provides m_controller member of the specified type.
 */
template <typename ControllerType>
class ControllerTestFixture
{
public:
    ControllerTestFixture()
    {
        // Reset EventManager to clean state
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());

        // Initialize GameTime to noon (Day period) - safe default
        BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));
    }

    ~ControllerTestFixture()
    {
        // Clean up event handlers
        EventManager::Instance().clean();
    }

    // Non-copyable
    ControllerTestFixture(const ControllerTestFixture&) = delete;
    ControllerTestFixture& operator=(const ControllerTestFixture&) = delete;

protected:
    ControllerType m_controller;
};

#endif // CONTROLLER_TEST_FIXTURE_HPP
