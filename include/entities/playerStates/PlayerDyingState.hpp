/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PLAYER_DYING_STATE_HPP
#define PLAYER_DYING_STATE_HPP

#include "entities/EntityState.hpp"
#include <functional>

class Player;

class PlayerDyingState : public EntityState {
public:
    explicit PlayerDyingState(Player& player);

    void enter() override;
    void update(float deltaTime) override;
    void exit() override;

private:
    std::reference_wrapper<Player> m_player;
};

#endif // PLAYER_DYING_STATE_HPP
