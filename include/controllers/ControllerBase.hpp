/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CONTROLLER_BASE_HPP
#define CONTROLLER_BASE_HPP

/**
 * @file ControllerBase.hpp
 * @brief Base class for lightweight state-scoped controllers
 *
 * Controllers are state-scoped helpers that monitor events, dispatch other
 * events, trigger manager actions, or coordinate state-local UI. They should
 * not own long-lived simulation/storage data; persistent data belongs in
 * managers or EDM.
 *
 * Key characteristics:
 * - Owned by GameState via ControllerRegistry
 * - Auto-unsubscribe on destruction
 * - Support suspend/resume for pause states
 * - Minimal state (subscription tokens, short-lived UI/view state, caches)
 *
 * Controller types:
 * - Frame-updatable: Inherit ControllerBase AND IUpdatable
 * - Event-only: Inherit ControllerBase only
 *
 * Use a Controller when:
 * - Bridging one event type to another
 * - Triggering manager actions on specific events
 * - Coordinating UI that is only relevant while a state is active
 * - Logic is only relevant while in certain game states
 *
 * Promote to Manager when:
 * - Significant data ownership required
 * - Complex simulation logic accumulates
 * - Multiple systems depend on it globally
 */

#include "managers/EventManager.hpp"
#include <string_view>
#include <vector>

class ControllerBase
{
public:
    /**
     * @brief Virtual destructor auto-unsubscribes from all events
     */
    virtual ~ControllerBase() { unsubscribe(); }

    // Non-copyable (event handlers capture 'this')
    ControllerBase(const ControllerBase&) = delete;
    ControllerBase& operator=(const ControllerBase&) = delete;

    // Movable
    ControllerBase(ControllerBase&& other) noexcept
        : m_subscribed(other.m_subscribed)
        , m_suspended(other.m_suspended)
        , m_handlerTokens(std::move(other.m_handlerTokens))
    {
        other.m_subscribed = false;
        other.m_suspended = false;
    }

    ControllerBase& operator=(ControllerBase&& other) noexcept
    {
        if (this != &other) {
            unsubscribe();  // Clean up current subscriptions
            m_subscribed = other.m_subscribed;
            m_suspended = other.m_suspended;
            m_handlerTokens = std::move(other.m_handlerTokens);
            other.m_subscribed = false;
            other.m_suspended = false;
        }
        return *this;
    }

    /**
     * @brief Subscribe to events. Must be implemented by derived classes.
     *
     * Called by ControllerRegistry::subscribeAll() during GameState::enter().
     * Implementations should register event handlers via addHandlerToken().
     */
    virtual void subscribe() = 0;

    /**
     * @brief Unsubscribe from all registered event handlers
     * @note Safe to call multiple times
     */
    void unsubscribe()
    {
        if (!m_subscribed) {
            return;
        }

        auto& eventMgr = EventManager::Instance();
        for (const auto& token : m_handlerTokens) {
            eventMgr.removeHandler(token);
        }
        m_handlerTokens.clear();
        m_subscribed = false;
    }

    /**
     * @brief Suspend controller when pause state is pushed
     *
     * Default implementation unsubscribes from events.
     * Override if custom suspend behavior is needed (e.g., keep listening
     * but don't process, or pause internal timers).
     */
    virtual void suspend()
    {
        if (m_suspended) {
            return;
        }
        unsubscribe();
        m_suspended = true;
    }

    /**
     * @brief Resume controller when pause state is popped
     *
     * Default implementation re-subscribes to events.
     * Override if custom resume behavior is needed.
     */
    virtual void resume()
    {
        if (!m_suspended) {
            return;
        }
        m_suspended = false;
        subscribe();
    }

    /**
     * @brief Get controller name for debugging and logging
     * @return String view of controller name (e.g., "CombatController")
     */
    [[nodiscard]] virtual std::string_view getName() const = 0;

    /**
     * @brief Check if currently subscribed to events
     * @return True if subscribed, false otherwise
     */
    [[nodiscard]] bool isSubscribed() const { return m_subscribed; }

    /**
     * @brief Check if controller is suspended (paused)
     * @return True if suspended, false otherwise
     */
    [[nodiscard]] bool isSuspended() const { return m_suspended; }

protected:
    ControllerBase() = default;

    /**
     * @brief Register a handler token for automatic cleanup
     * @param token The handler token from EventManager::registerHandlerWithToken
     */
    void addHandlerToken(const EventManager::HandlerToken& token)
    {
        m_handlerTokens.push_back(token);
    }

    /**
     * @brief Mark controller as subscribed
     * @param subscribed True when subscribed, false otherwise
     */
    void setSubscribed(bool subscribed) { m_subscribed = subscribed; }

    /**
     * @brief Check if already subscribed (for idempotent subscribe)
     * @return True if already subscribed
     */
    [[nodiscard]] bool checkAlreadySubscribed() const { return m_subscribed; }

private:
    bool m_subscribed{false};
    bool m_suspended{false};
    std::vector<EventManager::HandlerToken> m_handlerTokens;
};

#endif // CONTROLLER_BASE_HPP
