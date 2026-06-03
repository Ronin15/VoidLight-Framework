/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PROJECTILE_MANAGER_HPP
#define PROJECTILE_MANAGER_HPP

/**
 * @file ProjectileManager.hpp
 * @brief Position integration, lifetime management, and collision-to-damage
 *        wiring for Projectile entities
 *
 * Projectiles flow through the same entity pipeline as NPCs:
 * - Created via EntityDataManager::createProjectile() (Active tier, Layer_Projectile)
 * - CollisionManager detects projectile overlaps via getActiveIndices() broadphase
 * - Small projectiles do not participate in blocking position resolution
 * - CollisionManager routes projectile contacts directly to this manager
 * - This manager converts valid projectile hits into deferred DamageEvents
 *   via the same combat queue NPC melee/ranged combat uses
 * - Owner immunity is enforced here using the projectile's stored owner handle
 *
 * Threading Model (follows AIManager pattern):
 * - WorkerBudget-driven adaptive threading via SystemType::ProjectileSim
 * - SIMD 4-wide position integration
 * - Per-batch destroy queues for thread safety
 */

#include "entities/EntityHandle.hpp"
#include "managers/EventManager.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <vector>

// Forward declarations
class EntityDataManager;
struct EntityHotData;
struct TransformData;

namespace VoidLight {
    struct CollisionInfo;
}

class ProjectileManager
{
public:
    static ProjectileManager& Instance()
    {
        static ProjectileManager instance;
        return instance;
    }

    [[nodiscard]] bool init();
    void clean();
    void prepareForStateTransition();

    /**
     * @brief Main update — position integration + lifetime management
     *
     * Called between AIManager and ParticleManager in GameEngine::update().
     * Uses WorkerBudget for adaptive threading with SIMD 4-wide movement.
     */
    void update(float deltaTime);
    void handleProjectileCollision(const VoidLight::CollisionInfo& info);

    [[nodiscard]] bool isInitialized() const noexcept
    {
        return m_initialized.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool isShutdown() const noexcept
    {
        return m_isShutdown.load(std::memory_order_acquire);
    }

    void setGlobalPause(bool paused)
    {
        m_globallyPaused.store(paused, std::memory_order_release);
    }

    [[nodiscard]] bool isGloballyPaused() const noexcept
    {
        return m_globallyPaused.load(std::memory_order_acquire);
    }

    // Performance metrics
    struct PerfStats
    {
        double lastUpdateMs{0.0};
        double avgUpdateMs{0.0};
        size_t lastEntitiesProcessed{0};
        size_t lastBatchCount{0};
        uint64_t totalUpdates{0};
        bool lastWasThreaded{false};
    };

    [[nodiscard]] const PerfStats& getPerfStats() const { return m_perf; }

private:
    ProjectileManager() = default;
    ~ProjectileManager() = default;

    ProjectileManager(const ProjectileManager&) = delete;
    ProjectileManager& operator=(const ProjectileManager&) = delete;

    // Batch processing
    void processBatch(const std::vector<size_t>& indices,
                      size_t start, size_t end,
                      float deltaTime,
                      float worldWidth, float worldHeight,
                      std::vector<EntityHandle>& outDestroyQueue);

    void embedProjectile(size_t projectileIndex, const Vector2D& impactNormal,
                         EntityHandle embeddedTarget = INVALID_ENTITY_HANDLE);

    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_isShutdown{false};
    std::atomic<bool> m_globallyPaused{false};

    // Reusable buffers (avoid per-frame allocation)
    std::vector<size_t> m_activeProjectileIndices;
    std::vector<EntityHandle> m_destroyQueue;
    std::vector<std::future<void>> m_batchFutures;
    std::vector<EventManager::DeferredEvent> m_singleDeferredEventBatch;

    // Per-batch destroy queues for multi-threaded processing
    std::vector<std::vector<EntityHandle>> m_batchDestroyQueues;

    // Performance tracking
    PerfStats m_perf;
};

#endif // PROJECTILE_MANAGER_HPP
