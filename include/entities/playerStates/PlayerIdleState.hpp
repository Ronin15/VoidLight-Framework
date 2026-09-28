/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef PLAYER_IDLE_STATE_HPP
#define PLAYER_IDLE_STATE_HPP

#include "entities/EntityState.hpp"
#include <functional>

class Player;

class PlayerIdleState : public EntityState {
public:
    explicit PlayerIdleState(Player& player);

    void enter() override;
    void update(float deltaTime) override;
    void exit() override;

private:
    bool hasInputDetected() const;

    // Non-owning reference to the player entity
    // The player entity is owned elsewhere in the application
    std::reference_wrapper<Player> m_player;
};

#endif // PLAYER_IDLE_STATE_HPP
