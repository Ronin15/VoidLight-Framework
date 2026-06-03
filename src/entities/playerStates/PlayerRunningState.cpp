/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "entities/playerStates/PlayerRunningState.hpp"
#include "entities/Player.hpp"
#include "managers/InputManager.hpp"
#include "managers/UIManager.hpp"
#include "utils/Camera.hpp"

PlayerRunningState::PlayerRunningState(Player& player) : m_player(player) {}

void PlayerRunningState::enter() {
    // Set animation for running using abstracted API
    m_player.get().playAnimation("running");
}

void PlayerRunningState::update(float deltaTime) {
    // Process input and calculate movement velocity
    handleMovementInput(deltaTime);

    // Update animation frames based on movement
    handleRunningAnimation(deltaTime);

    // Transition to idle state when no input is detected
    if (!hasInputDetected()) {
        m_player.get().changeState("idle");
    }
}

void PlayerRunningState::exit() {
    // Nothing needed on exit
}

void PlayerRunningState::handleMovementInput(float) {
    const float speed = m_player.get().getMovementSpeed();
    const InputManager& input = InputManager::Instance();

    Vector2D velocity(0.0f, 0.0f);
    bool hasInput = false;

    // Compose direction vector from four semantic move commands.
    // Both keyboard (W/A/S/D) and gamepad left stick deflection are handled
    // transparently by the command layer.
    if (input.isCommandDown(InputManager::Command::MoveRight)) {
        velocity.setX(velocity.getX() + speed);
        m_player.get().setFlip(SDL_FLIP_NONE);
        hasInput = true;
    }
    if (input.isCommandDown(InputManager::Command::MoveLeft)) {
        velocity.setX(velocity.getX() - speed);
        m_player.get().setFlip(SDL_FLIP_HORIZONTAL);
        hasInput = true;
    }
    if (input.isCommandDown(InputManager::Command::MoveDown)) {
        velocity.setY(velocity.getY() + speed);
        hasInput = true;
    }
    if (input.isCommandDown(InputManager::Command::MoveUp)) {
        velocity.setY(velocity.getY() - speed);
        hasInput = true;
    }

    // Mouse point-to-move (lowest priority — only when no directional command active)
    if (!hasInput && input.isCommandDown(InputManager::Command::WorldInteract)) {
        const Vector2D& mouseScreenPos = input.getMousePosition();

        // Don't move player when clicking on UI elements (inventory, buttons, etc.)
        if (!UIManager::Instance().isClickOnUI(mouseScreenPos)) {
            const VoidLight::Camera* camera = m_player.get().getCamera();
            if (camera) {
                Vector2D mouseWorldPos = camera->screenToWorld(mouseScreenPos);
                Vector2D playerPos = m_player.get().getPosition();
                Vector2D direction = mouseWorldPos - playerPos;

                if (direction.length() > 5.0f) {
                    direction.normalize();
                    velocity = direction * speed;
                    hasInput = true;

                    if (direction.getX() > 0) {
                        m_player.get().setFlip(SDL_FLIP_NONE);
                    } else if (direction.getX() < 0) {
                        m_player.get().setFlip(SDL_FLIP_HORIZONTAL);
                    }
                }
            }
        }
    }

    // Normalize diagonal movement so speed is consistent on all axes
    if (hasInput && velocity.lengthSquared() > speed * speed) {
        velocity.normalize();
        velocity = velocity * speed;
    }

    m_player.get().setVelocity(velocity);
    m_player.get().setAcceleration(Vector2D(0, 0));
}

void PlayerRunningState::handleRunningAnimation(float deltaTime) {
    Vector2D velocity = m_player.get().getVelocity();

    // Only animate when player is moving
    if (velocity.lengthSquared() > 1.0f) {
        // Accumulate deltaTime (m_animSpeed is in milliseconds, convert to seconds)
        float accumulator = m_player.get().getAnimationAccumulator() + deltaTime;
        float frameTime = m_player.get().getAnimSpeed() / 1000.0f;  // ms to seconds

        // Advance frame when accumulator exceeds frame time
        if (accumulator >= frameTime) {
            int currentFrame = m_player.get().getCurrentFrame();
            int numFrames = m_player.get().getNumFrames();
            m_player.get().setCurrentFrame((currentFrame + 1) % numFrames);
            accumulator -= frameTime;  // Preserve excess time for smooth timing
        }
        m_player.get().setAnimationAccumulator(accumulator);
    } else {
        m_player.get().setCurrentFrame(0);
        m_player.get().setAnimationAccumulator(0.0f);
    }
}

bool PlayerRunningState::hasInputDetected() const {
    const InputManager& input = InputManager::Instance();

    if (input.isCommandDown(InputManager::Command::MoveUp)    ||
        input.isCommandDown(InputManager::Command::MoveDown)  ||
        input.isCommandDown(InputManager::Command::MoveLeft)  ||
        input.isCommandDown(InputManager::Command::MoveRight)) {
        return true;
    }

    // Mouse world-click — only counts if not clicking on UI
    if (input.isCommandDown(InputManager::Command::WorldInteract)) {
        const Vector2D& mousePos = input.getMousePosition();
        return !UIManager::Instance().isClickOnUI(mousePos);
    }

    return false;
}
