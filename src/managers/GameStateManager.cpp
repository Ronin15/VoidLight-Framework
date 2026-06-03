/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "managers/GameStateManager.hpp"
#include "core/Logger.hpp"
#include "gameStates/GameState.hpp"
#include "gpu/GPURenderer.hpp"
#include "utils/FrameProfiler.hpp"
#include <algorithm>
#include <format>
#include <stdexcept>

// GameStateManager Implementation
GameStateManager::GameStateManager() {
  // Reserve capacity for typical number of game states (performance
  // optimization)
  m_registeredStates.reserve(8);
  m_activeStates.reserve(3); // For the active stack
}

void GameStateManager::addState(std::unique_ptr<GameState> state) {
  const auto id = state->getStateId();
  if (hasState(id)) {
    GAMESTATE_ERROR(std::format("State {} already exists", static_cast<int>(id)));
    throw std::runtime_error(std::format("VoidLight Engine - State {} already exists", static_cast<int>(id)));
  }
  // Set state manager reference so state can access transitions and frame data
  state->setStateManager(this);
  // Move the state into the map as shared_ptr
  m_registeredStates[id] = std::shared_ptr<GameState>(std::move(state));
}

void GameStateManager::pushState(GameStateId stateId) {
  auto it = m_registeredStates.find(stateId);
  if (it != m_registeredStates.end()) {
    // Suppress profiler hitch detection during state transition
    VoidLight::FrameProfiler::Instance().suppressFrames(5);

    // Pause the current top state if it exists
    std::shared_ptr<GameState> previousState;
    if (!m_activeStates.empty()) {
      previousState = m_activeStates.back();
      previousState->pause();
    }

    // CRITICAL: Enter the state BEFORE adding to active stack to prevent rendering before initialization
    auto newState = it->second;
    if (!newState->enter()) {
      if (previousState) {
        previousState->resume();
      }
      GAMESTATE_ERROR(std::format("Failed to enter state: {}", static_cast<int>(stateId)));
      return;
    }

    // Now push the fully initialized state onto the stack
    m_activeStates.push_back(newState);
    GAMESTATE_INFO(std::format("Pushed state: {}", static_cast<int>(stateId)));
  } else {
    GAMESTATE_ERROR(std::format("State not found: {}", static_cast<int>(stateId)));
  }
}

void GameStateManager::popState() {
  if (!m_activeStates.empty()) {
    // Suppress profiler hitch detection during state transition
    VoidLight::FrameProfiler::Instance().suppressFrames(5);

    // CRITICAL: Wait for exit to complete BEFORE removing from stack
    auto currentState = m_activeStates.back();
    currentState->exit(); // Wait for exit to complete fully

    // Only remove after exit is complete
    m_activeStates.pop_back();
    GAMESTATE_INFO("Popped state");

    // Resume the new top state if it exists
    if (!m_activeStates.empty()) {
      m_activeStates.back()->resume();
    }
  }
}

void GameStateManager::changeState(GameStateId stateId) {
  // Pop the current state if one exists (waits for exit to complete)
  if (!m_activeStates.empty()) {
    popState();
  }
  // Push the new state (waits for enter to complete)
  pushState(stateId);
}

void GameStateManager::changeStateClearingStack(GameStateId stateId) {
  if (!m_activeStates.empty()) {
    // Suppress profiler hitch detection during state transition
    VoidLight::FrameProfiler::Instance().suppressFrames(5);
  }

  while (!m_activeStates.empty()) {
    auto currentState = m_activeStates.back();
    currentState->exit();
    m_activeStates.pop_back();
  }

  pushState(stateId);
}

void GameStateManager::update(float deltaTime) {
  m_lastDeltaTime = deltaTime; // Store deltaTime for render

  // Only update the top state when multiple states are active (e.g., PauseState over GamePlayState)
  // This prevents underlying states from processing game logic when paused
  if (!m_activeStates.empty()) {
    m_activeStates.back()->update(deltaTime);
  }
}

void GameStateManager::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                          float interpolationAlpha) {
  if (!m_activeStates.empty()) {
    m_activeStates.back()->recordGPUVertices(gpuRenderer, interpolationAlpha);
  }
}

void GameStateManager::renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                                        SDL_GPURenderPass* scenePass,
                                        float interpolationAlpha) {
  if (!m_activeStates.empty()) {
    m_activeStates.back()->renderGPUScene(gpuRenderer, scenePass, interpolationAlpha);
  }
}

void GameStateManager::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                     SDL_GPURenderPass* swapchainPass) {
  if (!m_activeStates.empty()) {
    m_activeStates.back()->renderGPUUI(gpuRenderer, swapchainPass);
  }
}

void GameStateManager::handleInput() {
  // Only the top state handles input
  if (!m_activeStates.empty()) {
    m_activeStates.back()->handleInput();
  }
}

bool GameStateManager::hasState(GameStateId stateId) const {
  return m_registeredStates.find(stateId) != m_registeredStates.end();
}

std::shared_ptr<GameState>
GameStateManager::getState(GameStateId stateId) const {
  auto it = m_registeredStates.find(stateId);
  return it != m_registeredStates.end() ? it->second : nullptr;
}

void GameStateManager::removeState(GameStateId stateId) {
  // First, remove the state from the active stack if it's there
  m_activeStates.erase(
      std::remove_if(m_activeStates.begin(), m_activeStates.end(),
                     [&](const std::shared_ptr<GameState> &state) {
                       if (state->getStateId() == stateId) {
                         state->exit();
                         return true;
                       }
                       return false;
                     }),
      m_activeStates.end());

  // Resume the new top state if it exists
  if (!m_activeStates.empty()) {
    m_activeStates.back()->resume();
  }

  // Remove from the registered states map
  m_registeredStates.erase(stateId);
}

void GameStateManager::clearAllStates() {
  // Exit active states from top to bottom without resuming intermediate states.
  while (!m_activeStates.empty()) {
    auto currentState = m_activeStates.back();
    currentState->exit();
    m_activeStates.pop_back();
  }
  m_registeredStates.clear();
}
