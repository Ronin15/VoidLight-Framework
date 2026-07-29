/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "managers/GameStateManager.hpp"
#include "core/Logger.hpp"
#include "gameStates/GameState.hpp"
#include "gpu/GPURenderer.hpp"
#include "managers/UIManager.hpp"
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

void GameStateManager::clearUIForFullScreenReplace() {
  // Shared UI is global. Full-screen replaces (no underlying state left)
  // clear it once here so enter() only builds widgets. Overlay push/pop and
  // replace-top-while-stack-remains must NOT call this — underlying UI stays.
  auto &ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.prepareForStateTransition();
  }
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

    // Pause the current top state if it exists (underlying UI stays registered)
    std::shared_ptr<GameState> previousState;
    if (!m_activeStates.empty()) {
      previousState = m_activeStates.back();
      previousState->pause();
    }

    // Enter before stacking so we never render an uninitialized top state.
    // No full UI clear — this is an overlay / stacked transition.
    auto newState = it->second;
    if (!newState->enter()) {
      if (previousState) {
        previousState->resume();
      }
      GAMESTATE_ERROR(std::format("Failed to enter state: {}", static_cast<int>(stateId)));
      return;
    }

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

    // Overlay exit removes its own widgets; no full UI clear.
    auto currentState = m_activeStates.back();
    currentState->exit();
    m_activeStates.pop_back();
    GAMESTATE_INFO("Popped state");

    if (!m_activeStates.empty()) {
      m_activeStates.back()->resume();
    }
  }
}

void GameStateManager::changeState(GameStateId stateId) {
  auto it = m_registeredStates.find(stateId);
  if (it == m_registeredStates.end()) {
    GAMESTATE_ERROR(std::format("State not found: {}", static_cast<int>(stateId)));
    return;
  }

  // Suppress profiler hitch detection during state transition
  VoidLight::FrameProfiler::Instance().suppressFrames(5);

  // Standard order: exit old → (clear UI if full-screen) → enter new.
  // If enter fails, re-enter the previous top so the stack stays recoverable.
  auto newState = it->second;
  std::shared_ptr<GameState> previousState;
  if (!m_activeStates.empty()) {
    previousState = m_activeStates.back();
    previousState->exit();
    m_activeStates.pop_back();
  }

  // Full-screen replace when nothing remains underneath; stacked replace
  // (e.g. Pause → Settings over GamePlay) keeps underlying UI.
  const bool fullScreenReplace = m_activeStates.empty();
  if (fullScreenReplace) {
    clearUIForFullScreenReplace();
  }

  if (!newState->enter()) {
    GAMESTATE_ERROR(std::format("Failed to enter state: {}", static_cast<int>(stateId)));
    if (previousState) {
      if (fullScreenReplace) {
        clearUIForFullScreenReplace();
      }
      if (!previousState->enter()) {
        GAMESTATE_ERROR(std::format(
            "Failed to restore previous state {} after failed changeState to {}",
            static_cast<int>(previousState->getStateId()),
            static_cast<int>(stateId)));
        return;
      }
      m_activeStates.push_back(previousState);
    }
    return;
  }

  m_activeStates.push_back(newState);
  GAMESTATE_INFO(std::format("Changed state to: {}", static_cast<int>(stateId)));
}

void GameStateManager::changeStateClearingStack(GameStateId stateId) {
  auto it = m_registeredStates.find(stateId);
  if (it == m_registeredStates.end()) {
    GAMESTATE_ERROR(std::format("State not found: {}", static_cast<int>(stateId)));
    return;
  }

  // Suppress profiler hitch detection during state transition
  VoidLight::FrameProfiler::Instance().suppressFrames(5);

  auto newState = it->second;
  // Snapshot for failed-enter recovery (bottom → top re-entry).
  const std::vector<std::shared_ptr<GameState>> previousStack = m_activeStates;

  while (!m_activeStates.empty()) {
    auto currentState = m_activeStates.back();
    currentState->exit();
    m_activeStates.pop_back();
  }

  // Entire stack is gone — always a full-screen UI replace.
  clearUIForFullScreenReplace();

  if (!newState->enter()) {
    GAMESTATE_ERROR(std::format("Failed to enter state: {}", static_cast<int>(stateId)));
    // Best-effort restore: re-enter previous stack bottom → top. Each state
    // under the eventual top is pause()'d, matching pushState layering.
    clearUIForFullScreenReplace();
    for (size_t i = 0; i < previousStack.size(); ++i) {
      if (i > 0) {
        previousStack[i - 1]->pause();
      }
      if (!previousStack[i]->enter()) {
        GAMESTATE_ERROR(std::format(
            "Failed to restore state {} while recovering from failed "
            "changeStateClearingStack to {}",
            static_cast<int>(previousStack[i]->getStateId()),
            static_cast<int>(stateId)));
        break;
      }
      m_activeStates.push_back(previousStack[i]);
    }
    return;
  }

  m_activeStates.push_back(newState);
  GAMESTATE_INFO(std::format("Changed state (clearing stack) to: {}", static_cast<int>(stateId)));
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
