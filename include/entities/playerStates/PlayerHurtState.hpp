/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PLAYER_HURT_STATE_HPP
#define PLAYER_HURT_STATE_HPP

#include "entities/EntityState.hpp"
#include <functional>

class Player;

class PlayerHurtState : public EntityState {
public:
    explicit PlayerHurtState(Player& player);

    void enter() override;
    void update(float deltaTime) override;
    void exit() override;

private:
    std::reference_wrapper<Player> m_player;
    float m_animationDuration{0.0f};
    float m_elapsedTime{0.0f};
};

#endif // PLAYER_HURT_STATE_HPP
