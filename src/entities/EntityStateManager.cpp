/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "entities/EntityStateManager.hpp"
#include "entities/EntityState.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <format>
#include <stdexcept>

EntityStateManager::EntityStateManager() : m_currentState() {}

EntityStateManager::~EntityStateManager() {
    m_currentState.reset();
}

void EntityStateManager::addState(const std::string& stateName, std::unique_ptr<EntityState> state) {
    if (m_states.find(stateName) != m_states.end()) {
        ENTITYSTATE_ERROR(std::format("State already exists: {}", stateName));
        throw std::invalid_argument(std::format("VoidLight Engine - State already exists: {}", stateName));
    }
    // Convert unique_ptr to shared_ptr and add to container
    m_states[stateName] = std::shared_ptr<EntityState>(std::move(state));
}

void EntityStateManager::setState(const std::string& stateName) {
    // find new state
    auto it = m_states.find(stateName);
    if (it != m_states.end()) {
        // Exit current state if it exists
        if (auto current = m_currentState.lock()) {
            current->exit();
        }
        // Set new current state and enter
        m_currentState = it->second;
        if (auto current = m_currentState.lock()) {
            current->enter();
        }
    } else {
        // state not found - exit current state and reset weak_ptr
        if (auto current = m_currentState.lock()) {
            current->exit();
        }
        ENTITYSTATE_ERROR(std::format("State not found: {}", stateName));
        m_currentState.reset();
    }
}

std::string EntityStateManager::getCurrentStateName() const {
    if (auto current = m_currentState.lock()) {
        // Find the key for the current state pointer using std::find_if
        auto it = std::find_if(m_states.begin(), m_states.end(),
            [&current](const auto& pair) { return pair.second == current; });
        if (it != m_states.end()) {
            return it->first;
        }
    }
    return "";
}

bool EntityStateManager::hasState(const std::string& stateName) const {
    return m_states.find(stateName) != m_states.end();
}

void EntityStateManager::update(float deltaTime) {
    if (auto current = m_currentState.lock()) {
        current->update(deltaTime);
    }
}
