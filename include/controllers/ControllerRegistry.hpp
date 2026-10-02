/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CONTROLLER_REGISTRY_HPP
#define CONTROLLER_REGISTRY_HPP

/**
 * @file ControllerRegistry.hpp
 * @brief Type-erased container for managing GameState controllers
 *
 * The ControllerRegistry provides:
 * - Heterogeneous storage of controller types
 * - Batch subscribe/unsubscribe/suspend/resume operations
 * - Automatic IUpdatable detection and update dispatch
 * - Type-safe retrieval via get<T>()
 *
 * Ownership: GameState owns the ControllerRegistry, which owns the controllers.
 *
 * Usage:
 * @code
 * class MyGameState : public GameState {
 *     ControllerRegistry m_controllers;
 *
 *     bool enter() override {
 *         m_controllers.add<WeatherController>();
 *         m_controllers.add<CombatController>(mp_Player);  // Constructor args forwarded
 *         m_controllers.subscribeAll();
 *         return true;
 *     }
 *
 *     void update(float dt) override {
 *         m_controllers.updateAll(dt);
 *     }
 *
 *     void pause() override { m_controllers.suspendAll(); }
 *     void resume() override { m_controllers.resumeAll(); }
 *     bool exit() override { m_controllers.clear(); return true; }
 * };
 * @endcode
 */

#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

class ControllerRegistry {
public:
    ControllerRegistry() = default;
    ~ControllerRegistry() = default;

    // Non-copyable (controllers own handlers)
    ControllerRegistry(const ControllerRegistry&) = delete;
    ControllerRegistry& operator=(const ControllerRegistry&) = delete;

    // Movable
    ControllerRegistry(ControllerRegistry&&) noexcept = default;
    ControllerRegistry& operator=(ControllerRegistry&&) noexcept = default;

    /**
     * @brief Add a controller of type T
     * @tparam T Controller type (must derive from ControllerBase)
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Reference to the created controller
     *
     * If a controller of type T already exists, returns the existing one.
     * Automatically detects IUpdatable interface and adds to update list.
     */
    template <typename T, typename... Args>
    T& add(Args&&... args) {
        static_assert(std::is_base_of_v<ControllerBase, T>,
            "T must derive from ControllerBase");

        std::type_index const typeIdx(typeid(T));

        // Check if already registered
        auto it = m_typeToIndex.find(typeIdx);
        if (it != m_typeToIndex.end()) {
            return *static_cast<T*>(m_controllers[it->second].get());
        }

        // Create and store
        auto controller = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *controller;

        size_t index = m_controllers.size();
        m_controllers.push_back(std::move(controller));
        m_typeToIndex[typeIdx] = index;

        // Cache IUpdatable interface if present (compile-time detection)
        if constexpr (std::is_base_of_v<IUpdatable, T>) {
            m_updatables.push_back({static_cast<IUpdatable*>(&ref),
                static_cast<ControllerBase*>(&ref)});
        }

        return ref;
    }

    /**
     * @brief Get a controller of type T
     * @tparam T Controller type to retrieve
     * @return Pointer to controller, or nullptr if not found
     */
    template <typename T>
    T* get() {
        static_assert(std::is_base_of_v<ControllerBase, T>,
            "T must derive from ControllerBase");

        auto it = m_typeToIndex.find(std::type_index(typeid(T)));
        if (it != m_typeToIndex.end()) {
            return static_cast<T*>(m_controllers[it->second].get());
        }
        return nullptr;
    }

    /**
     * @brief Get a controller of type T (const version)
     */
    template <typename T>
    const T* get() const {
        return const_cast<ControllerRegistry*>(this)->get<T>();
    }

    /**
     * @brief Check if a controller of type T is registered
     */
    template <typename T>
    [[nodiscard]] bool has() const {
        return m_typeToIndex.find(std::type_index(typeid(T))) != m_typeToIndex.end();
    }

    // --- Batch Operations ---

    /**
     * @brief Subscribe all registered controllers to their events
     * Called in GameState::enter()
     */
    void subscribeAll() {
        for (auto& controller : m_controllers) {
            controller->subscribe();
        }
    }

    /**
     * @brief Unsubscribe all controllers from their events
     * Called in GameState::exit()
     */
    void unsubscribeAll() {
        for (auto& controller : m_controllers) {
            controller->unsubscribe();
        }
    }

    /**
     * @brief Suspend all controllers (called when pause state pushed)
     * Called in GameState::pause()
     */
    void suspendAll() {
        for (auto& controller : m_controllers) {
            controller->suspend();
        }
    }

    /**
     * @brief Resume all controllers (called when pause state popped)
     * Called in GameState::resume()
     */
    void resumeAll() {
        for (auto& controller : m_controllers) {
            controller->resume();
        }
    }

    /**
     * @brief Update all IUpdatable controllers
     * @param deltaTime Frame delta time in seconds
     *
     * Only calls update() on controllers that:
     * 1. Implement IUpdatable interface
     * 2. Are not currently suspended
     *
     * Called in GameState::update()
     */
    void updateAll(float deltaTime) {
        for (auto& [updatable, base] : m_updatables) {
            if (!base->isSuspended()) {
                updatable->update(deltaTime);
            }
        }
    }

    /**
     * @brief Get count of registered controllers
     */
    [[nodiscard]] size_t size() const { return m_controllers.size(); }

    /**
     * @brief Check if registry is empty
     */
    [[nodiscard]] bool empty() const { return m_controllers.empty(); }

    /**
     * @brief Clear all controllers (unsubscribes first)
     */
    void clear() {
        unsubscribeAll();
        m_controllers.clear();
        m_updatables.clear();
        m_typeToIndex.clear();
    }

private:
    /**
     * @brief Entry for IUpdatable controllers
     * Caches both interfaces to avoid repeated dynamic_cast
     */
    struct UpdatableEntry {
        IUpdatable* updatable; // For calling update()
        ControllerBase* base; // For checking isSuspended()
    };

    std::vector<std::unique_ptr<ControllerBase>> m_controllers;
    std::vector<UpdatableEntry> m_updatables;
    std::unordered_map<std::type_index, size_t> m_typeToIndex;
};

#endif // CONTROLLER_REGISTRY_HPP
