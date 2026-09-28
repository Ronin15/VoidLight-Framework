/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef PLAYER_RUNNING_STATE_HPP
#define PLAYER_RUNNING_STATE_HPP

#include "entities/EntityState.hpp"
#include <functional>

class Player;

class PlayerRunningState : public EntityState {
public:
    explicit PlayerRunningState(Player& player);
    ~PlayerRunningState() override = default;

    void enter() override;
    void update(float deltaTime) override;
    void exit() override;

private:
    void handleMovementInput(float deltaTime);
    void handleRunningAnimation(float deltaTime);
    bool hasInputDetected() const;

    // Non-owning reference to the player entity
    // The player entity is owned elsewhere in the application
    std::reference_wrapper<Player> m_player;
};

#endif // PLAYER_RUNNING_STATE_HPP
