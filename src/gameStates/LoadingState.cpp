/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "gameStates/LoadingState.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/GameStateManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/UIManager.hpp"
#include "managers/WorldManager.hpp"

#include "gpu/GPURenderer.hpp"

#include <format>

void LoadingState::configure(
    GameStateId targetStateId,
    const VoidLight::WorldGenerationConfig &worldConfig) {
  m_targetStateId = targetStateId;
  m_worldConfig = worldConfig;

  // Reset state for reuse
  m_progress.store(0.0f, std::memory_order_release);
  m_loadComplete.store(false, std::memory_order_release);
  m_loadFailed.store(false, std::memory_order_release);
  m_waitingForPathfinding.store(false, std::memory_order_release);
  setStatusText("Initializing...");

  // Clear any previous error
  {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError.clear();
  }
}

bool LoadingState::enter() {
  // Validate that LoadingState was properly configured
  if (m_targetStateId == GameStateId::COUNT) {
    GAMESTATE_ERROR(
        "LoadingState not configured - call configure() before pushing state");
    return false;
  }

  GAMESTATE_INFO(
      std::format("Entering LoadingState - Target: {}", static_cast<int>(m_targetStateId)));

  // Pause game time during loading (time shouldn't advance while loading)
  GameTimeManager::Instance().setGlobalPause(true);

  // Initialize loading screen UI
  initializeUI();

  // Start async world loading
  startAsyncWorldLoad();

  return true;
}

void LoadingState::update(float) {
  // Update UI state (progress bar and status text)
  auto &ui = UIManager::Instance();
  float currentProgress = m_progress.load(std::memory_order_acquire);
  ui.updateProgressBar("loading_progress", currentProgress);
  ui.setText("loading_status", getStatusText());

  // Fast path: Check with relaxed ordering first (no memory barrier)
  if (m_loadComplete.load(std::memory_order_relaxed)) {
    // Slow path: Verify with acquire barrier only when likely true
    if (m_loadComplete.load(std::memory_order_acquire)) {
      // Check if we need to wait for pathfinding grid
      if (!m_waitingForPathfinding.load(std::memory_order_acquire)) {
        // World loading just completed - now wait for pathfinding
        m_waitingForPathfinding.store(true, std::memory_order_release);
        setStatusText("Preparing pathfinding grid...");
        GAMESTATE_INFO("World loading complete - waiting for pathfinding grid");
        return; // Wait for next frame to check grid readiness
      }

      // Check if pathfinding grid is ready
      const auto &pathfinderManager = PathfinderManager::Instance();
      if (!pathfinderManager.isGridReady()) {
        // Grid still building - keep waiting
        return;
      }

      // All ready - proceed with transition
      if (m_loadFailed.load(std::memory_order_acquire)) {
        std::string errorMsg = "World loading failed - transitioning anyway";
        GAMESTATE_ERROR(errorMsg);

        // Store error if not already set by the loadTask lambda
        if (!hasError()) {
          std::lock_guard<std::mutex> lock(m_errorMutex);
          m_lastError = errorMsg;
        }
        // Continue to target state even on failure (matches current behavior)
      } else {
        GAMESTATE_INFO(
            std::format("World and pathfinding ready - transitioning to {}",
                        static_cast<int>(m_targetStateId)));
      }

      // Transition to target state
      if (mp_stateManager->hasState(m_targetStateId)) {
        mp_stateManager->changeState(m_targetStateId);
      } else {
        GAMESTATE_ERROR(
            std::format("Target state not found: {}", static_cast<int>(m_targetStateId)));

        // Store error for diagnostic purposes
        {
          std::lock_guard<std::mutex> lock(m_errorMutex);
          m_lastError = std::format("Target state not found: {}", static_cast<int>(m_targetStateId));
        }
      }
    }
  }
}

void LoadingState::handleInput() {
  // LoadingState doesn't accept input - loading must complete
}

bool LoadingState::exit() {
  GAMESTATE_INFO("Exiting LoadingState");

  // Cleanup loading UI
  cleanupUI();

  // Wait for async task to complete if still running
  if (m_loadTask.valid()) {
    try {
      m_loadTask.wait();
    } catch (const std::exception &e) {
      std::string errorMsg =
          std::format("Exception while waiting for load task: {}", e.what());
      GAMESTATE_ERROR(errorMsg);

      // Store error for diagnostic purposes
      {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastError = errorMsg;
      }
    }
  }

  return true;
}


void LoadingState::startAsyncWorldLoad() {
  auto &threadSystem = VoidLight::ThreadSystem::Instance();
  auto &worldManager = WorldManager::Instance();

  // Capture necessary data by value for thread safety
  auto worldConfig = m_worldConfig;

  // Create lambda that will run on background thread
  auto loadTask = [this, worldConfig, &worldManager]() -> bool {
    GAMESTATE_INFO("Starting async world generation on background thread");

    // Progress callback that updates our atomic progress
    auto progressCallback = [this](float percent, const std::string &status) {
      // Update atomic progress (thread-safe)
      m_progress.store(percent, std::memory_order_release);

      // Update status text (mutex-protected)
      setStatusText(status);
    };

    // Load world with progress callback
    bool success = worldManager.loadNewWorld(worldConfig, progressCallback);

    if (success) {
      GAMESTATE_INFO("Async world generation completed successfully");
    } else {
      std::string errorMsg = "Async world generation failed";
      GAMESTATE_ERROR(errorMsg);

      // Store error for diagnostic purposes
      {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastError = errorMsg;
      }
    }

    // Mark loading as complete
    m_loadFailed.store(!success, std::memory_order_release);
    m_loadComplete.store(true, std::memory_order_release);

    return success;
  };

  // Enqueue task with high priority and get future
  m_loadTask = threadSystem.enqueueTaskWithResult(
      loadTask, VoidLight::TaskPriority::High,
      "LoadingState_WorldGeneration");
}

void LoadingState::setStatusText(const std::string &status) {
  std::lock_guard<std::mutex> lock(m_statusMutex);
  m_statusText = status;
}

std::string LoadingState::getStatusText() const {
  std::lock_guard<std::mutex> lock(m_statusMutex);
  return m_statusText;
}

std::string LoadingState::getLastError() const {
  std::lock_guard<std::mutex> lock(m_errorMutex);
  return m_lastError;
}

bool LoadingState::hasError() const {
  std::lock_guard<std::mutex> lock(m_errorMutex);
  return !m_lastError.empty();
}

void LoadingState::initializeUI() {
  auto &ui = UIManager::Instance();
  auto &gameEngine = GameEngine::Instance();
  int windowWidth = gameEngine.getWidthInPixels();
  int windowHeight = gameEngine.getHeightInPixels();

  // Create loading overlay
  ui.createOverlay();

  // Create title - centered both, 60px above center (accounts for 40px height)
  // Using CENTERED_BOTH: y = (height - 40) / 2 + offsetY, we want y = height/2
  // - 80 So offsetY = -80 + 20 = -60
  ui.createTitle("loading_title", {0, windowHeight / 2 - 80, windowWidth, 40},
                 "Loading World...");
  ui.setTitleAlignment("loading_title", UIAlignment::CENTER_CENTER);
  ui.setComponentPositioning("loading_title", {UIPositionMode::CENTERED_BOTH, 0,
                                               -60, windowWidth, 40});

  // Create progress bar - centered both, at vertical center
  // Using CENTERED_BOTH: y = (height - 30) / 2 + offsetY, we want y = height/2
  // So offsetY = 15
  constexpr int progressBarWidth = 400;
  constexpr int progressBarHeight = 30;
  ui.createProgressBar(
      "loading_progress",
      {0, windowHeight / 2, progressBarWidth, progressBarHeight}, 0.0f, 100.0f);
  ui.setComponentPositioning("loading_progress",
                             {UIPositionMode::CENTERED_BOTH, 0, 15,
                              progressBarWidth, progressBarHeight});

  // Create status text - centered both, 50px below progress bar center
  // Using CENTERED_BOTH: y = (height - 30) / 2 + offsetY, we want y = height/2
  // + 50 So offsetY = 50 + 15 = 65
  ui.createTitle("loading_status", {0, windowHeight / 2 + 50, windowWidth, 30},
                 "Initializing...");
  ui.setTitleAlignment("loading_status", UIAlignment::CENTER_CENTER);
  ui.setComponentPositioning("loading_status", {UIPositionMode::CENTERED_BOTH,
                                                0, 65, windowWidth, 30});

  m_uiInitialized = true;

  GAMESTATE_INFO("Loading screen UI initialized");
}

void LoadingState::cleanupUI() {
  if (!m_uiInitialized) {
    return;
  }

  auto &ui = UIManager::Instance();

  ui.removeOverlay();
  ui.removeComponent("loading_title");
  ui.removeComponent("loading_progress");
  ui.removeComponent("loading_status");

  m_uiInitialized = false;

  GAMESTATE_INFO("Loading screen UI cleaned up");
}

void LoadingState::recordGPUVertices(VoidLight::GPURenderer &gpuRenderer,
                                     float) {

  // Record UI vertices for GPU rendering
  auto &ui = UIManager::Instance();
  ui.recordGPUVertices(gpuRenderer);
}

void LoadingState::renderGPUUI(VoidLight::GPURenderer &gpuRenderer,
                               SDL_GPURenderPass *swapchainPass) {
  // Render UI to swapchain
  auto &ui = UIManager::Instance();
  ui.renderGPU(gpuRenderer, swapchainPass);
}
