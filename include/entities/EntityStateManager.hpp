/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef ENTITY_STATE_MANAGER_HPP
#define ENTITY_STATE_MANAGER_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include "entities/EntityState.hpp"

class EntityStateManager {

public:
    EntityStateManager();
    void addState(const std::string& stateName, std::unique_ptr<EntityState> state);
    void setState(const std::string& stateName);
    std::string getCurrentStateName() const;
    bool hasState(const std::string& stateName) const;

    void update(float deltaTime);
    ~EntityStateManager();

private:
    std::unordered_map<std::string, std::shared_ptr<EntityState>> m_states;
   // Non-owning observer to the current active state
   // This state is owned by the 'states' container above
    std::weak_ptr<EntityState> m_currentState;
};

#endif  // ENTITY_STATE_MANAGER_HPP
