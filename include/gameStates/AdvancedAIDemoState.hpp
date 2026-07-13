/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef ADVANCED_AI_DEMO_STATE_HPP
#define ADVANCED_AI_DEMO_STATE_HPP

#include "gameStates/GameState.hpp"
#include "controllers/ControllerRegistry.hpp"
#include "controllers/render/NPCRenderController.hpp"
#include "controllers/render/ProjectileRenderController.hpp"
#include "entities/EntityHandle.hpp"
#include "entities/Player.hpp"
#include "managers/EventManager.hpp"
#include "utils/Camera.hpp"

#include <memory>
#include <vector>

// Forward declarations with smart pointer types
class Player;
using PlayerPtr = std::shared_ptr<Player>;

namespace VoidLight {
class GPUSceneRecorder;
}

class AdvancedAIDemoState : public GameState {
public:
    AdvancedAIDemoState();  // Defined in .cpp for unique_ptr with forward-declared types
    ~AdvancedAIDemoState() override;

    void update(float deltaTime) override;
    void handleInput() override;

    bool enter() override;
    bool exit() override;

    GameStateId getStateId() const override { return GameStateId::ADVANCED_AI_DEMO; }

    // GPU rendering support
    void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                           float interpolationAlpha) override;
    void renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                        SDL_GPURenderPass* scenePass,
                        float interpolationAlpha) override;
    void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                     SDL_GPURenderPass* swapchainPass) override;
    bool supportsGPURendering() const override { return true; }

    // Get the player entity for AI behaviors to access
    EntityPtr getPlayer() const { return m_player; }

private:
    // Methods
    void setupTestVillage();  // Spawns merchant NPCs, guards, and villagers
    void initializeCamera();
    void updateCamera(float deltaTime);

    // Data-driven NPC rendering (velocity-based animation)
    NPCRenderController m_npcRenderCtrl{};
    ProjectileRenderController m_projectileRenderCtrl{};

    // Player entity
    PlayerPtr m_player{};
    std::unique_ptr<VoidLight::Camera> m_camera;

    // GPU scene recorder for coordinated scene-data recording
    std::unique_ptr<VoidLight::GPUSceneRecorder> m_gpuSceneRecorder{nullptr};

    float m_worldWidth{800.0f};
    float m_worldHeight{600.0f};

    // Track whether world has been loaded (prevents re-entering LoadingState)
    bool m_worldLoaded{false};

    // Track if we need to transition to loading screen on first update
    bool m_needsLoading{false};

    // Track if we're transitioning to LoadingState (prevents infinite loop)
    bool m_transitioningToLoading{false};
    bool m_transitioningToGameOver{false};

    // Track if state is fully initialized (after returning from LoadingState)
    bool m_initialized{false};

    // Controller registry (follows GamePlayState pattern)
    ControllerRegistry m_controllers;

    // AI pause state
    bool m_aiPaused{false};

    // Status display optimization - zero per-frame allocations (C++20 type-safe)
    std::string m_statusBuffer{};
    float m_lastDisplayedFPS{-1.0f};
    size_t m_lastDisplayedNPCCount{0};
    bool m_lastDisplayedPauseState{false};

    // Cached NPC count (updated in update(), used in render())
    size_t m_cachedNPCCount{0};
};

#endif // ADVANCED_AI_DEMO_STATE_HPP
