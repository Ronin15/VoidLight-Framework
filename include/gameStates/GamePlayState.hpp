/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef GAME_PLAY_STATE_HPP
#define GAME_PLAY_STATE_HPP

#include "controllers/ControllerRegistry.hpp"
#include "controllers/render/NPCRenderController.hpp"
#include "controllers/render/ProjectileRenderController.hpp"
#include "events/TimeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "gameStates/GameState.hpp"
#include "managers/EventManager.hpp"
#include <memory>
#include <string>

// Forward declarations (full includes in .cpp)
class Player;
namespace VoidLight {
class Camera;
class GPUSceneRecorder;
}

class GamePlayState : public GameState {
public:
  GamePlayState();  // Defined in .cpp for unique_ptr with forward-declared types
  ~GamePlayState() override;
  bool enter() override;
  void update(float deltaTime) override;
  void handleInput() override;
  bool exit() override;
  void pause() override;
  void resume() override;
  GameStateId getStateId() const override { return GameStateId::GAME_PLAY; }

  // GPU rendering support
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                         float interpolationAlpha) override;
  void renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                      SDL_GPURenderPass* scenePass,
                      float interpolationAlpha) override;
  void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                   SDL_GPURenderPass* swapchainPass) override;
  bool supportsGPURendering() const override { return true; }

private:
  bool m_transitioningToLoading{
      false}; // Flag to indicate we're transitioning to loading state
  bool m_transitioningToGameOver{false}; // Prevent duplicate death transitions
  std::shared_ptr<Player> mp_Player{nullptr}; // Player object
  bool m_initialized{false}; // Flag to track if state is already initialized (for pause/resume)

  // Camera for world navigation and player following
  std::unique_ptr<VoidLight::Camera> m_camera{nullptr};

  // GPU scene recorder for coordinated scene-data recording
  std::unique_ptr<VoidLight::GPUSceneRecorder> m_gpuSceneRecorder{nullptr};

  // Track whether world has been loaded (prevents re-entering LoadingState)
  bool m_worldLoaded{false};

  // Track if we need to transition to loading screen on first update
  bool m_needsLoading{false};

  // FPS counter (toggled with F2)
  bool m_fpsVisible{false};
  std::string m_fpsBuffer{};
  float m_lastDisplayedFPS{-1.0f};

  // --- Controllers (owned by ControllerRegistry) ---
  ControllerRegistry m_controllers;

  // Data-driven NPC rendering (direct member like AdvancedAIDemoState)
  NPCRenderController m_npcRenderCtrl{};
  ProjectileRenderController m_projectileRenderCtrl{};

  // --- Time UI display buffer ---
  std::string m_statusBuffer{};  // Reusable buffer for status text (zero allocation)
  bool m_statusBarDirty{true};   // Flag to rebuild status bar only when events fire

  // Inventory UI methods
  void spawnStarterGearChest();
  bool tryOpenNearbyMerchantTrade();
  void toggleInventoryDisplay();
  void registerEventHandlers();
  void unregisterEventHandlers();

  // Camera management methods
  void initializeCamera();
  void updateCamera(float deltaTime);
  // Camera auto-manages world bounds; no state-level setup needed

  // Reusable buffer for nearby entity queries (avoids per-interaction allocation)
  std::vector<EntityHandle> m_nearbyHandlesBuffer;

  EventManager::HandlerToken m_dayNightEventToken;
  bool m_dayNightSubscribed{false};

  // Day/night event handlers and update
  void onTimePeriodChanged(const EventData& data);
  // Ambient particle effects (dust motes, fireflies) - managed per time period
  void updateAmbientParticles(TimePeriod period);
  void stopAmbientParticles();
  void onWeatherChanged(const EventData& data);
  uint32_t m_ambientDustEffectId{0};
  uint32_t m_ambientFireflyEffectId{0};
  TimePeriod m_lastAmbientPeriod{TimePeriod::Day};  // Track to avoid particle thrashing
  bool m_ambientParticlesActive{false};  // Whether ambient particles are currently running
  EventManager::HandlerToken m_weatherEventToken;
  bool m_weatherSubscribed{false};
  EventManager::HandlerToken m_harvestEventToken;
  bool m_harvestSubscribed{false};
  TimePeriod m_currentTimePeriod{TimePeriod::Day};  // Track current period for weather changes
  WeatherType m_lastWeatherType{WeatherType::Clear};  // Track to avoid redundant weather processing
};

#endif // GAME_PLAY_STATE_HPP
