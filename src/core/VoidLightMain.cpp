/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "core/GameEngine.hpp"
#include "core/ThreadSystem.hpp"
#include "core/TimestepManager.hpp"
#include "core/Logger.hpp"
#include "managers/GameStateManager.hpp"
#include "utils/FrameProfiler.hpp"
#include "gameStates/AIDemoState.hpp"
#include "gameStates/AdvancedAIDemoState.hpp"
#include "gameStates/EventDemoState.hpp"
#include "gameStates/GamePlayState.hpp"
#include "gameStates/GameOverState.hpp"
#include "gameStates/LoadingState.hpp"
#include "gameStates/LogoState.hpp"
#include "gameStates/MainMenuState.hpp"
#include "gameStates/OverlayDemoState.hpp"
#include "gameStates/SettingsMenuState.hpp"
#include "gameStates/UIDemoState.hpp"
#include <array>
#include <chrono>
#include <format>
#include <memory>
#include <numeric>
#include <string_view>

constexpr std::string_view GAME_NAME{"Game Template"};

// Application composition root: registers all concrete game states with the
// GameStateManager. Lives here (not in Core/GameEngine) so the engine depends
// only on the GameState interface and GameStateManager, preserving the
// Core -> Managers -> GameStates dependency direction.
static void registerInitialStates(GameStateManager& stateManager) {
  stateManager.addState(std::make_unique<LogoState>());
  stateManager.addState(
      std::make_unique<LoadingState>()); // Shared loading screen state
  stateManager.addState(std::make_unique<MainMenuState>());
  stateManager.addState(std::make_unique<SettingsMenuState>());
  stateManager.addState(std::make_unique<GamePlayState>());
  stateManager.addState(std::make_unique<GameOverState>());
  stateManager.addState(std::make_unique<AIDemoState>());
  stateManager.addState(std::make_unique<AdvancedAIDemoState>());
  stateManager.addState(std::make_unique<EventDemoState>());
  stateManager.addState(std::make_unique<UIDemoState>());
  stateManager.addState(std::make_unique<OverlayDemoState>());
}

// maybe_unused is just a hint to the compiler that the variable is not used.
// with -Wall -Wextra flags
int main(int, char*[]) {
  GAMEENGINE_INFO(std::format("Initializing {}", GAME_NAME));
  THREADSYSTEM_INFO("Initializing Thread System");

  // Initialize the thread system with default capacity
  // Cache ThreadSystem reference for better performance
  VoidLight::ThreadSystem& threadSystem = VoidLight::ThreadSystem::Instance();

  // Initialize thread system first
  try {
    if (!threadSystem.init()) {
      THREADSYSTEM_CRITICAL("Failed to initialize thread system");
      return -1;
    }
  } catch (const std::exception& e) {
    THREADSYSTEM_CRITICAL(std::format("Exception during thread system init: {}", e.what()));
    return -1;
  }

  THREADSYSTEM_INFO(std::format("Thread system initialized with {} worker threads and capacity for {} parallel tasks",
                                threadSystem.getThreadCount(), threadSystem.getQueueCapacity()));

  // Cache GameEngine reference
  GameEngine& gameEngine = GameEngine::Instance();

  // Initialize GameEngine (SDL, ResourcePath, settings, window, and all managers)
  if (!gameEngine.init(GAME_NAME)) {
    GAMEENGINE_CRITICAL(std::format("Init {} Failed", GAME_NAME));

    // CRITICAL: Always clean up on init failure to prevent memory corruption
    // during static destruction of partially initialized managers
    GAMEENGINE_INFO("Cleaning up after initialization failure");
    gameEngine.clean();

    return -1;
  }

  GAMEENGINE_INFO(std::format("Frame timing configured: {}",
                              gameEngine.isUsingSoftwareFrameLimiting()
                              ? "software frame limiting"
                              : "hardware VSync"));

  // Register all concrete game states now that the engine and its managers are
  // fully initialized, then push the initial state before starting main loop.
  registerInitialStates(*gameEngine.getGameStateManager());
  gameEngine.getGameStateManager()->pushState(GameStateId::LOGO);

  // Suppress hitch detection for first few frames while engine stabilizes
  VoidLight::FrameProfiler::Instance().suppressFrames(10);

  GAMEENGINE_INFO("Starting Main Loop");

  // Get TimestepManager reference for main loop
  TimestepManager& ts = gameEngine.getTimestepManager();

  VOIDLIGHT_DEBUG_ONLY(
  // Update performance tracking (DEBUG only)
  static constexpr size_t PERF_SAMPLE_COUNT = 10;
  std::array<double, PERF_SAMPLE_COUNT> updateSamples{};
  size_t sampleIndex = 0;
  size_t intervalUpdateIterations = 0;
  auto lastPerfLogTime = std::chrono::high_resolution_clock::now();
  )

  // Main game loop - classic fixed timestep pattern
  // Updates drain accumulator, THEN render reads alpha - no race conditions
  while (gameEngine.isRunning()) {
    PROFILE_FRAME_BEGIN();

    // Start frame timing (adds delta to accumulator)
    ts.startFrame();

    // Process SDL events (must be on main thread)
    {
      PROFILE_PHASE(VoidLight::FramePhase::Events);
      gameEngine.handleEvents();
    }
    if (!gameEngine.isRunning()) {
      break;
    }

    // Fixed timestep updates - run until accumulator is drained
    VOIDLIGHT_DEBUG_ONLY(
    auto updateStart = std::chrono::high_resolution_clock::now();
    size_t updateIterations = 0;
    )

    {
      PROFILE_PHASE(VoidLight::FramePhase::Update);
      while (gameEngine.isRunning() && ts.shouldUpdate()) {
        gameEngine.update(ts.getUpdateDeltaTime());
        VOIDLIGHT_DEBUG_ONLY(++updateIterations;)
      }
    }
    if (!gameEngine.isRunning()) {
      break;
    }

    VOIDLIGHT_DEBUG_ONLY(
    auto updateEnd = std::chrono::high_resolution_clock::now();
    double updateMs = std::chrono::duration<double, std::milli>(updateEnd - updateStart).count();
    updateSamples[sampleIndex++ % PERF_SAMPLE_COUNT] = updateMs;
    intervalUpdateIterations += updateIterations;

    double secondsSinceLastLog = std::chrono::duration<double>(updateEnd - lastPerfLogTime).count();
    if (secondsSinceLastLog >= TimestepManager::PERF_LOG_INTERVAL_SECONDS) {
      lastPerfLogTime = updateEnd;
      double avgMs = std::accumulate(updateSamples.begin(), updateSamples.end(), 0.0) / PERF_SAMPLE_COUNT;
      double frameBudgetMs = 1000.0 / ts.getTargetFPS();
      double utilizationPercent = (avgMs / frameBudgetMs) * 100.0;
      GAMEENGINE_DEBUG(std::format("Update performance: {:.2f}ms avg ({:.1f}% frame budget)",
                                   avgMs, utilizationPercent));
      GAMEENGINE_DEBUG(std::format("Update stats: iterations:{}, frameMs:{}, vsync:{}, softwareLimit:{}",
                                   intervalUpdateIterations, ts.getFrameTimeMs(),
                                   gameEngine.isVSyncEnabled(),
                                   ts.isUsingSoftwareFrameLimiting()));
      intervalUpdateIterations = 0;
    }
    )

    // Render with interpolation alpha (calculated from remaining accumulator)
    {
      PROFILE_PHASE(VoidLight::FramePhase::Render);
      gameEngine.render();
    }

    // Present (vsync wait) - separate from render for accurate profiling
    {
      PROFILE_PHASE(VoidLight::FramePhase::Present);
      gameEngine.present();
    }

    // End-of-frame cleanup (entity destruction queue, deferred work)
    // Runs after render/present while GPU finishes — uses idle CPU time
    gameEngine.processBackgroundTasks();

    // End frame (VSync or software frame limiting)
    ts.endFrame();

    PROFILE_FRAME_END();  // Hitch check + console log happens here
  }

  GAMEENGINE_INFO(std::format("Game {} shutting down", GAME_NAME));

  gameEngine.clean();

  return 0;
}
