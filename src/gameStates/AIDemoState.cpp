/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "gameStates/AIDemoState.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "gameStates/LoadingState.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/GameStateManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/ParticleManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ProjectileManager.hpp"
#include "managers/UIManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/EventManager.hpp"
#include "core/WorkerBudget.hpp"
#include "utils/Camera.hpp"
#include "utils/FrameProfiler.hpp"
#include "gpu/GPURenderer.hpp"
#include "gpu/SpriteBatch.hpp"
#include "utils/GPUSceneRecorder.hpp"

#include <cmath>
#include <cstddef>
#include <ctime>
#include <format>
#include <memory>

// Constructor/destructor defined here where GPUSceneRecorder is complete (for unique_ptr)
AIDemoState::AIDemoState() = default;

AIDemoState::~AIDemoState() {
  // Don't call virtual functions from destructors

  try {
    // Note: Proper cleanup should already have happened in exit()
    // This destructor is just a safety measure in case exit() wasn't called
    // Reset AI behaviors first to clear entity references
    // Don't call unassignBehaviorFromEntity here - it uses shared_from_this()
    // Clear NPCs without calling clean() on them
    AIManager::Instance().destroyAllNPCsForStateTransition();

    // Clean up player
    m_player.reset();

    GAMESTATE_INFO("Exiting AIDemoState in destructor...");
  } catch (const std::exception &e) {
    GAMESTATE_ERROR(
        std::format("Exception in AIDemoState destructor: {}", e.what()));
  } catch (...) {
    GAMESTATE_ERROR("Unknown exception in AIDemoState destructor");
  }
}

void AIDemoState::handleInput() {
  // Cache manager references for better performance
  InputManager const &inputMgr = InputManager::Instance();
  AIManager &aiMgr = AIManager::Instance();
  EntityDataManager &edm = EntityDataManager::Instance();

  // Use InputManager's new event-driven key press detection
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_SPACE)) {
    // Toggle pause/resume
    m_aiPaused = !m_aiPaused;

    // Set global AI pause state in AIManager
    aiMgr.setGlobalPause(m_aiPaused);

    // Simple feedback
    GAMESTATE_INFO(std::format("AI {}", m_aiPaused ? "PAUSED" : "RESUMED"));
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_B)) {
    GAMESTATE_INFO("Preparing to exit AIDemoState...");
    mp_stateManager->changeState(GameStateId::MAIN_MENU);
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_1)) {
    // Assign Wander behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to WANDER behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "Wander");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_2)) {
    // Assign Patrol behavior to all NPCs
    GAMESTATE_INFO(std::format(
        "Switching {} NPCs to PATROL behavior (batched processing)...",
        edm.getEntityCount(EntityKind::NPC)));
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "Patrol");
      }
    }
    GAMESTATE_INFO("Patrol assignments queued. Processing "
                   "instantly in parallel for optimal performance.");
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_3)) {
    // Assign Chase behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to CHASE behavior");

    // Chase behavior target is automatically maintained by AIManager
    // No manual target updates needed
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "Chase");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_4)) {
    // Assign SmallWander behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to SMALL WANDER behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "SmallWander");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_5)) {
    // Assign LargeWander behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to LARGE WANDER behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "LargeWander");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_6)) {
    // Assign EventWander behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to EVENT WANDER behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "EventWander");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_7)) {
    // Assign RandomPatrol behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to RANDOM PATROL behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "RandomPatrol");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_8)) {
    // Assign CirclePatrol behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to CIRCLE PATROL behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "CirclePatrol");
      }
    }
  }

  if (inputMgr.wasKeyPressed(SDL_SCANCODE_9)) {
    // Assign EventTarget behavior to all NPCs
    GAMESTATE_INFO("Switching all NPCs to EVENT TARGET behavior");
    for (size_t edmIdx : edm.getIndicesByKind(EntityKind::NPC)) {
      EntityHandle handle = edm.getHandle(edmIdx);
      if (handle.isValid()) {
        aiMgr.assignBehavior(handle, "EventTarget");
      }
    }
  }

  // Camera zoom controls
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_LEFTBRACKET) && m_camera) {
    m_camera->zoomIn(); // [ key = zoom in (objects larger)
  }
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_RIGHTBRACKET) && m_camera) {
    m_camera->zoomOut(); // ] key = zoom out (objects smaller)
  }

  // NPC spawning controls - use EventManager for unified spawning
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_N) ||
      inputMgr.wasKeyPressed(SDL_SCANCODE_M)) {
    auto &eventMgr = EventManager::Instance();

    if (inputMgr.wasKeyPressed(SDL_SCANCODE_N)) {
      // Spawn 2000 Villagers across entire world via events
      GAMESTATE_INFO("Spawning 2000 Villagers across world...");
      eventMgr.spawnNPC("Villager", 0, 0, 2000, 0, "Random", {}, true);
    }

    if (inputMgr.wasKeyPressed(SDL_SCANCODE_M)) {
      // Spawn 2000 random NPCs across entire world via events (random
      // race/class)
      GAMESTATE_INFO("Spawning 2000 random NPCs across world...");
      eventMgr.spawnNPC("Random", 0, 0, 2000, 0, "Random", {}, true);
    }
  }
}

bool AIDemoState::enter() {
  // Cache GameEngine reference at function start
  auto &gameEngine = GameEngine::Instance();

  // Resume all game managers (may be paused from menu states)
  gameEngine.setGlobalPause(false);

  GAMESTATE_INFO("Entering AIDemoState...");

  // Reset transition flag when entering state
  m_transitioningToLoading = false;

  // Check if already initialized (resuming after LoadingState)
  if (m_initialized) {
    GAMESTATE_INFO("Already initialized - resuming AIDemoState");
    return true; // Skip all loading logic
  }

  // Check if world needs to be loaded
  if (!m_worldLoaded) {
    GAMESTATE_INFO("World not loaded yet - will transition to LoadingState on "
                   "first update");
    m_needsLoading = true;
    m_worldLoaded = true; // Mark as loaded to prevent loop on re-entry
    return true;          // Will transition to loading screen in update()
  }

  // World is loaded - proceed with normal initialization
  GAMESTATE_INFO("World already loaded - initializing AI demo");

  try {
    auto &worldManager = WorldManager::Instance();

    // Update world dimensions from loaded world
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    if (worldManager.getWorldBounds(minX, minY, maxX, maxY)) {
      m_worldWidth = std::max(0.0f, maxX - minX);
      m_worldHeight = std::max(0.0f, maxY - minY);
      GAMESTATE_INFO(std::format("World dimensions: {} x {} pixels",
                                 m_worldWidth, m_worldHeight));
    } else {
      // Fallback to screen dimensions if world bounds unavailable
      m_worldWidth = gameEngine.getWidthInPixels();
      m_worldHeight = gameEngine.getHeightInPixels();
    }

    // Create player first (the chase behavior will need it)
    m_player = std::make_shared<Player>();
    m_player->ensurePhysicsBodyRegistered();
    m_player->initializeInventory(); // Initialize inventory after construction
    m_player->setPosition(Vector2D(m_worldWidth / 2, m_worldHeight / 2));

    // Cache AIManager reference for better performance
    AIManager &aiMgr = AIManager::Instance();

    // Set player handle in AIManager for distance optimization
    // (EntityHandle-based API)
    aiMgr.setPlayerHandle(m_player->getHandle());

    // Chase behavior is now registered by AIManager::registerDefaultBehaviors()
    // Behaviors get player via AIManager::getPlayerHandle() or getPlayerPosition()

    // Create simple HUD UI (matches EventDemoState spacing pattern)
    auto &ui = UIManager::Instance();
    ui.createTitle("ai_title",
                   {0, UIConstants::TITLE_TOP_OFFSET,
                    gameEngine.getWidthInPixels(),
                    UIConstants::DEFAULT_TITLE_HEIGHT},
                   "AI Demo State");
    ui.setTitleAlignment("ai_title", UIAlignment::CENTER_CENTER);
    // Set auto-repositioning: top-aligned, full width
    ui.setComponentPositioning("ai_title", {UIPositionMode::TOP_ALIGNED, 0,
                                            UIConstants::TITLE_TOP_OFFSET, -1,
                                            UIConstants::DEFAULT_TITLE_HEIGHT});

    ui.createLabel(
        "ai_instructions_line1",
        {UIConstants::INFO_LABEL_MARGIN_X, UIConstants::INFO_FIRST_LINE_Y,
         gameEngine.getWidthInPixels() - 2 * UIConstants::INFO_LABEL_MARGIN_X,
         UIConstants::INFO_LABEL_HEIGHT},
        "Controls: [B] Exit | [SPACE] Pause/Resume | [N] Spawn 2K Standard | "
        "[M] Spawn 2K Random | [ ] Zoom");
    // Set auto-repositioning: top-aligned, full width minus margins
    ui.setComponentPositioning(
        "ai_instructions_line1",
        {UIPositionMode::TOP_ALIGNED, UIConstants::INFO_LABEL_MARGIN_X,
         UIConstants::INFO_FIRST_LINE_Y, -2 * UIConstants::INFO_LABEL_MARGIN_X,
         UIConstants::INFO_LABEL_HEIGHT});

    const int line2Y = UIConstants::INFO_FIRST_LINE_Y +
                       UIConstants::INFO_LABEL_HEIGHT +
                       UIConstants::INFO_LINE_SPACING;
    ui.createLabel(
        "ai_instructions_line2",
        {UIConstants::INFO_LABEL_MARGIN_X, line2Y,
         gameEngine.getWidthInPixels() - 2 * UIConstants::INFO_LABEL_MARGIN_X,
         UIConstants::INFO_LABEL_HEIGHT},
        "Behaviors: [1] Wander | [2] Patrol | [3] Chase | [4] Small | [5] "
        "Large | "
        "[6] Event | [7] Random | [8] Circle | [9] Target");
    // Set auto-repositioning: top-aligned, full width minus margins
    ui.setComponentPositioning("ai_instructions_line2",
                               {UIPositionMode::TOP_ALIGNED,
                                UIConstants::INFO_LABEL_MARGIN_X, line2Y,
                                -2 * UIConstants::INFO_LABEL_MARGIN_X,
                                UIConstants::INFO_LABEL_HEIGHT});

    const int statusY = line2Y + UIConstants::INFO_LABEL_HEIGHT +
                        UIConstants::INFO_LINE_SPACING +
                        UIConstants::INFO_STATUS_SPACING;
    ui.createLabel("ai_status",
                   {UIConstants::INFO_LABEL_MARGIN_X, statusY, 400,
                    UIConstants::INFO_LABEL_HEIGHT},
                   "FPS: -- | Entities: -- | AI: RUNNING");
    // Set auto-repositioning: top-aligned with calculated position (fixes
    // fullscreen transition)
    ui.setComponentPositioning("ai_status",
                               {UIPositionMode::TOP_ALIGNED,
                                UIConstants::INFO_LABEL_MARGIN_X, statusY, 400,
                                UIConstants::INFO_LABEL_HEIGHT});

    // Initialize camera (world is already loaded by LoadingState)
    initializeCamera();

    // Create GPU scene renderer for coordinated GPU rendering
    m_gpuSceneRecorder = std::make_unique<VoidLight::GPUSceneRecorder>();

    // Pre-allocate status buffer to avoid per-frame allocations
    m_statusBuffer.reserve(64);

    // Mark as fully initialized to prevent re-entering loading logic
    m_initialized = true;

    return true;
  } catch (const std::exception &e) {
    GAMESTATE_ERROR(
        std::format("Exception in AIDemoState::enter(): {}", e.what()));
    return false;
  } catch (...) {
    GAMESTATE_ERROR("Unknown exception in AIDemoState::enter()");
    return false;
  }
}

bool AIDemoState::exit() {
  GAMESTATE_INFO("Exiting AIDemoState...");

  // Cache manager references for better performance
  AIManager &aiMgr = AIManager::Instance();
  BackgroundSimulationManager &bgSimMgr = BackgroundSimulationManager::Instance();
  EntityDataManager &edm = EntityDataManager::Instance();
  CollisionManager &collisionMgr = CollisionManager::Instance();
  PathfinderManager &pathfinderMgr = PathfinderManager::Instance();
  UIManager &ui = UIManager::Instance();
  WorldManager &worldMgr = WorldManager::Instance();
  EventManager &eventMgr = EventManager::Instance();

  if (m_transitioningToLoading) {
    // Transitioning to LoadingState - do cleanup but preserve m_worldLoaded
    // flag This prevents infinite loop when returning from LoadingState

    // Reset the flag after using it
    m_transitioningToLoading = false;

    // Clear controllers before entity/manager cleanup
    m_controllers.clear();

    // CRITICAL: Clear NPCs and player BEFORE prepareForStateTransition()
    // NPCs hold EDM indices - must be destroyed while EDM data is still valid
    aiMgr.destroyAllNPCsForStateTransition();
    if (m_player) {
      m_player.reset();
    }

    aiMgr.prepareForStateTransition();
    ProjectileManager::Instance().prepareForStateTransition();
    bgSimMgr.prepareForStateTransition();
    worldMgr.prepareForStateTransition();

    eventMgr.prepareForStateTransition();

    if (collisionMgr.isInitialized() && !collisionMgr.isShutdown()) {
      collisionMgr.prepareForStateTransition();
    }

    if (pathfinderMgr.isInitialized() && !pathfinderMgr.isShutdown()) {
      pathfinderMgr.prepareForStateTransition();
    }

    edm.prepareForStateTransition();
    VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();

    WorldManager::Instance().setActiveCamera(nullptr);
    if (m_player) {
      m_player->setCamera(nullptr);
    }

    // Clean up camera and GPU scene renderer
    m_camera.reset();

    // Clean up UI
    ui.prepareForStateTransition();

    // Unload world (LoadingState will reload it)
    if (worldMgr.isInitialized() && worldMgr.hasActiveWorld()) {
      worldMgr.unloadWorld();
      // CRITICAL: DO NOT reset m_worldLoaded here - keep it true to prevent
      // infinite loop when LoadingState returns to this state
    }

    // Restore AI to unpaused state
    aiMgr.setGlobalPause(false);
    m_aiPaused = false;

    // Reset initialized flag so state re-initializes after loading
    m_initialized = false;

    // Keep m_worldLoaded = true to remember we've already been through loading
    GAMESTATE_INFO("AIDemoState cleanup for LoadingState transition complete");
    return true;
  }

  // Full exit (going to main menu, other states, or shutting down)

  // Clear controllers before entity/manager cleanup
  m_controllers.clear();

  // CRITICAL: Clear NPCs and player BEFORE prepareForStateTransition()
  // NPCs hold EDM indices - must be destroyed while EDM data is still valid
  aiMgr.destroyAllNPCsForStateTransition();
  if (m_player) {
    m_player->setCamera(nullptr);
    m_player.reset();
  }

  aiMgr.prepareForStateTransition();
  ProjectileManager::Instance().prepareForStateTransition();
  bgSimMgr.prepareForStateTransition();
  worldMgr.prepareForStateTransition();

  eventMgr.prepareForStateTransition();

  // Clean collision state
  if (collisionMgr.isInitialized() && !collisionMgr.isShutdown()) {
    collisionMgr.prepareForStateTransition();
  }

  if (pathfinderMgr.isInitialized() && !pathfinderMgr.isShutdown()) {
    pathfinderMgr.prepareForStateTransition();
  }

  edm.prepareForStateTransition();
  VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();

  WorldManager::Instance().setActiveCamera(nullptr);

  // Clean up camera and GPU scene renderer first to stop world rendering
  m_camera.reset();

  // Clean up UI components using simplified method
  ui.prepareForStateTransition();

  // Unload the world when fully exiting, but only if there's actually a world
  // loaded This matches EventDemoState's safety pattern and prevents crashes
  if (worldMgr.isInitialized() && worldMgr.hasActiveWorld()) {
    worldMgr.unloadWorld();
    // Reset m_worldLoaded when doing full exit (going to main menu, etc.)
    m_worldLoaded = false;
  }

  // Always restore AI to unpaused state when exiting the demo state
  // This prevents the global pause from affecting other states
  aiMgr.setGlobalPause(false);
  m_aiPaused = false;

  // Reset initialization flag for next fresh start
  m_initialized = false;

  GAMESTATE_INFO("AIDemoState exit complete");
  return true;
}

void AIDemoState::update(float deltaTime) {
  try {
    // Check if we need to transition to loading screen (do this in update, not
    // enter)
    if (m_needsLoading) {
      m_needsLoading = false; // Clear flag

      GAMESTATE_INFO("Transitioning to LoadingState for world generation");

      // Create world configuration for AI demo
      VoidLight::WorldGenerationConfig config;
      config.width = 500; // Massive world matching EventDemoState
      config.height = 500;
      config.seed = static_cast<int>(std::time(nullptr));
      config.elevationFrequency = 0.02f;   // Lower frequency = larger biome regions
      config.humidityFrequency = 0.015f;
      config.waterLevel = 0.25f;
      config.mountainLevel = 0.75f;

      // Configure LoadingState and transition to it
      auto *loadingState = dynamic_cast<LoadingState *>(
          mp_stateManager->getState(GameStateId::LOADING).get());
      if (loadingState) {
        loadingState->configure(GameStateId::AI_DEMO, config);
        // Set flag before transitioning to preserve m_worldLoaded in exit()
        m_transitioningToLoading = true;
        // Use changeState (called from update) to properly exit and re-enter
        mp_stateManager->changeState(GameStateId::LOADING);
      } else {
        GAMESTATE_ERROR("LoadingState not found in GameStateManager");
      }

      return; // Don't continue with rest of update
    }

    // Auto-spawning disabled - use keyboard triggers instead (N key for
    // standard spawn, M key for random behaviors) Collision bounds are set on
    // first spawn via keyboard trigger

    // Update player
    if (m_player) {
      m_player->update(deltaTime);
    }

    // Cache manager references for better performance
    UIManager &ui = UIManager::Instance();

    // Update Active tier NPCs only (animations and state machine)
    // AIManager handles behavior logic, BackgroundSimulationManager handles
    // non-Active Data-driven NPCs are updated via NPCRenderController
    m_npcRenderCtrl.update(deltaTime);

    // Cache entity count for render() (avoids EDM query in render path)
    m_cachedEntityCount = EntityDataManager::Instance().getEntityCount(EntityKind::NPC);

    // Update camera (follows player automatically)
    updateCamera(deltaTime);

    // Update UI (moved from render path for consistent frame timing)
    if (!ui.isShutdown()) {
      ui.update(deltaTime);
    }

  } catch (const std::exception &e) {
    GAMESTATE_ERROR(
        std::format("Exception in AIDemoState::update(): {}", e.what()));
  } catch (...) {
    GAMESTATE_ERROR("Unknown exception in AIDemoState::update()");
  }
}

void AIDemoState::initializeCamera() {
  const auto &gameEngine = GameEngine::Instance();

  // Initialize camera at player's position to avoid any interpolation jitter
  Vector2D playerPosition = m_player ? m_player->getPosition() : Vector2D(0, 0);

  // Create camera starting at player position
  m_camera = std::make_unique<VoidLight::Camera>(
      playerPosition.getX(), playerPosition.getY(), // Start at player position
      static_cast<float>(gameEngine.getWidthInPixels()),
      static_cast<float>(gameEngine.getHeightInPixels()));

  // Configure camera to follow player
  if (m_player) {
    // Disable camera event firing for consistency with other demo states
    m_camera->setEventFiringEnabled(false);

    // Set target and enable follow mode
    std::weak_ptr<Entity> playerAsEntity =
        std::static_pointer_cast<Entity>(m_player);
    m_camera->setTarget(playerAsEntity);
    m_camera->setMode(VoidLight::Camera::Mode::Follow);

    // Camera follow tuning lives in Camera::Config defaults — uniform across states.

    // Provide camera to player for screen-to-world coordinate conversion
    m_player->setCamera(m_camera.get());

    // Camera auto-synchronizes world bounds on update
  }

  // Register camera with WorldManager for chunk texture updates
  WorldManager::Instance().setActiveCamera(m_camera.get());
}

void AIDemoState::updateCamera(float deltaTime) {
  if (m_camera) {
    // Sync viewport with current window size (handles resize events)
    m_camera->syncViewportWithEngine();

    // Update camera position and following logic
    m_camera->update(deltaTime);
  }
}

void AIDemoState::recordGPUVertices(VoidLight::GPURenderer &gpuRenderer,
                                    float interpolationAlpha) {
  if (!m_camera || !m_gpuSceneRecorder) { return; }

  // Begin scene-data recording before the engine-owned scene pass opens
  auto ctx = m_gpuSceneRecorder->beginRecording(gpuRenderer, *m_camera, interpolationAlpha);
  if (!ctx) { return; }

  // Record world tiles to sprite batch
  auto &worldMgr = WorldManager::Instance();
  worldMgr.recordGPU(*ctx.spriteBatch, ctx.cameraX, ctx.cameraY,
                     ctx.viewWidth, ctx.viewHeight, ctx.zoom);

  // Record NPCs and projectiles to sprite batch (atlas-based)
  m_npcRenderCtrl.recordGPU(ctx);
  m_projectileRenderCtrl.recordGPU(ctx);

  // End sprite batch recording (finalizes atlas-based sprites)
  m_gpuSceneRecorder->endSpriteBatch();

  // Record player (entity batch - separate texture)
  if (m_player) {
    m_player->recordGPUVertices(gpuRenderer, ctx.cameraX, ctx.cameraY, interpolationAlpha);
  }

  // Update status text before recording UI vertices
  auto &ui = UIManager::Instance();
  {
    float currentFPS = mp_stateManager->getCurrentFPS();
    size_t entityCount = m_cachedEntityCount;

    if (std::abs(currentFPS - m_lastDisplayedFPS) > 0.05f ||
        entityCount != m_lastDisplayedEntityCount ||
        m_aiPaused != m_lastDisplayedPauseState) {

      m_statusBuffer.clear();
      std::format_to(std::back_inserter(m_statusBuffer),
                     "FPS: {:.1f} | Entities: {} | AI: {}", currentFPS, entityCount,
                     m_aiPaused ? "PAUSED" : "RUNNING");
      ui.setText("ai_status", m_statusBuffer);

      m_lastDisplayedFPS = currentFPS;
      m_lastDisplayedEntityCount = entityCount;
      m_lastDisplayedPauseState = m_aiPaused;
    }
  }

  // Record UI vertices
  ui.recordGPUVertices(gpuRenderer);

  m_gpuSceneRecorder->endRecording();
}

void AIDemoState::renderGPUScene(VoidLight::GPURenderer &gpuRenderer,
                                 SDL_GPURenderPass *scenePass,
                                 float) {
  if (!m_camera || !m_gpuSceneRecorder) { return; }

  // Render previously recorded scene data into the engine-owned scene pass
  m_gpuSceneRecorder->renderRecordedScene(gpuRenderer, scenePass);

  // Render player (entity batch)
  if (m_player) {
    m_player->renderGPU(gpuRenderer, scenePass);
  }
}

void AIDemoState::renderGPUUI(VoidLight::GPURenderer &gpuRenderer,
                              SDL_GPURenderPass *swapchainPass) {
  UIManager::Instance().renderGPU(gpuRenderer, swapchainPass);
}
