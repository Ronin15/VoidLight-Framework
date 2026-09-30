/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef GAME_OVER_STATE_HPP
#define GAME_OVER_STATE_HPP

#include "gameStates/GameState.hpp"

class GameOverState : public GameState {
public:
    bool enter() override;
    void update(float deltaTime) override;
    void handleInput() override;
    bool exit() override;
    GameStateId getStateId() const override { return GameStateId::GAME_OVER; }

    // Sets which state Retry returns to. Only GamePlayState routes here today
    // and sets GAME_PLAY. The value is not reset on exit, so a caller must set
    // it before every transition into GameOverState. Main Menu always goes to
    // MAIN_MENU regardless of this value.
    void setReturnState(GameStateId state) { m_returnState = state; }

    void recordGPUUIVertices(VoidLight::GPURenderer& gpuRenderer) override;
    void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
        SDL_GPURenderPass* swapchainPass) override;

private:
    // Retry destination, set via setReturnState() by GamePlayState (the only
    // caller). Defaults to GAME_PLAY.
    GameStateId m_returnState = GameStateId::GAME_PLAY;
};

#endif // GAME_OVER_STATE_HPP
