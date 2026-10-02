/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef IUPDATABLE_HPP
#define IUPDATABLE_HPP

/**
 * @file IUpdatable.hpp
 * @brief Interface for controllers that require per-frame updates
 *
 * Controllers implement this interface when they need update() called
 * each frame. Event-only controllers (like WeatherController) do NOT
 * implement this - they react purely to EventManager events.
 *
 * Usage:
 * - Frame-updatable: class MyController : public ControllerBase, public IUpdatable
 * - Event-only:      class MyController : public ControllerBase
 *
 * The ControllerRegistry auto-detects IUpdatable at compile time via
 * std::is_base_of_v and only calls update() on controllers that implement it.
 */
class IUpdatable {
public:
    virtual ~IUpdatable() = default;

    /**
     * @brief Per-frame update for the controller
     * @param deltaTime Time elapsed since last frame in seconds
     *
     * Called by ControllerRegistry::updateAll() for all IUpdatable controllers.
     * Not called when controller is suspended (during pause states).
     */
    virtual void update(float deltaTime) = 0;
};

#endif // IUPDATABLE_HPP
