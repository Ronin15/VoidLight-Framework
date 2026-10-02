/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PLAYER_ATTACKING_STATE_HPP
#define PLAYER_ATTACKING_STATE_HPP

#include "entities/EntityState.hpp"
#include <functional>

class Player;

class PlayerAttackingState : public EntityState {
public:
    explicit PlayerAttackingState(Player& player);

    void enter() override;
    void update(float deltaTime) override;
    void exit() override;

private:
    // Non-owning reference to the player entity
    std::reference_wrapper<Player> m_player;

    // Track attack animation duration
    float m_attackDuration{0.0f};
    static constexpr float ATTACK_ANIMATION_TIME{0.3f}; // seconds
};

#endif // PLAYER_ATTACKING_STATE_HPP
