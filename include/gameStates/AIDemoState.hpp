/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef AI_DEMO_STATE_HPP
#define AI_DEMO_STATE_HPP

#include "gameStates/GameState.hpp"
#include "controllers/ControllerRegistry.hpp"
#include "controllers/render/NPCRenderController.hpp"
#include "controllers/render/ProjectileRenderController.hpp"
#include "entities/EntityHandle.hpp"
#include "entities/Player.hpp"
#include "utils/Camera.hpp"

#include <memory>
#include <vector>

// Forward declarations with smart pointer types
class Player;
using PlayerPtr = std::shared_ptr<Player>;

namespace VoidLight {
class GPUSceneRecorder;
}

class AIDemoState : public GameState {
public:
    AIDemoState();  // Defined in .cpp for unique_ptr with forward-declared types
    ~AIDemoState() override;

    void update(float deltaTime) override;
    void handleInput() override;

    bool enter() override;
    bool exit() override;

    GameStateId getStateId() const override { return GameStateId::AI_DEMO; }

    bool hasGPUScene() const override { return true; }
    void recordGPUSceneVertices(VoidLight::GPURenderer& gpuRenderer,
                                float interpolationAlpha) override;
    void recordGPUUIVertices(VoidLight::GPURenderer& gpuRenderer) override;
    void renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                        SDL_GPURenderPass* scenePass,
                        float interpolationAlpha) override;
    void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                     SDL_GPURenderPass* swapchainPass) override;

    // Get the player entity for AI behaviors to access
    EntityPtr getPlayer() const { return m_player; }

private:
    // Methods
    void initializeCamera();
    void updateCamera(float deltaTime);

    // Controller registry (follows GamePlayState pattern)
    ControllerRegistry m_controllers;

    // Data-driven NPC rendering (velocity-based animation)
    NPCRenderController m_npcRenderCtrl{};
    ProjectileRenderController m_projectileRenderCtrl{};

    // Player entity
    PlayerPtr m_player{};

    // Demo settings
    float m_worldWidth{800.0f};
    float m_worldHeight{600.0f};

    // Track whether world has been loaded (prevents re-entering LoadingState)
    bool m_worldLoaded{false};

    // Track if we need to transition to loading screen on first update
    bool m_needsLoading{false};

    // Track if we're transitioning to LoadingState (prevents infinite loop)
    bool m_transitioningToLoading{false};

    // Track if state is fully initialized (after returning from LoadingState)
    bool m_initialized{false};

    // Camera for world navigation
    std::unique_ptr<VoidLight::Camera> m_camera{nullptr};

    // GPU scene recorder for coordinated scene-data recording
    std::unique_ptr<VoidLight::GPUSceneRecorder> m_gpuSceneRecorder{nullptr};

    // AI pause state
    bool m_aiPaused{false};
    // Status display optimization - zero per-frame allocations (C++20 type-safe)
    std::string m_statusBuffer{};
    float m_lastDisplayedFPS{-1.0f};
    size_t m_lastDisplayedEntityCount{0};
    bool m_lastDisplayedPauseState{false};

    // Cached entity count (updated in update(), used in render())
    size_t m_cachedEntityCount{0};
};

#endif // AI_DEMO_STATE_HPP
