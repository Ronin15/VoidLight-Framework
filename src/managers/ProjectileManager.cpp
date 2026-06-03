/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/ProjectileManager.hpp"
#include "collisions/CollisionInfo.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "events/EntityEvents.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "utils/SIMDMath.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

using namespace VoidLight::SIMD;

constexpr float MIN_PROJECTILE_SPEED_SQ = 1.0f;

// ============================================================================
// LIFECYCLE
// ============================================================================

bool ProjectileManager::init()
{
    if (m_initialized.load(std::memory_order_acquire))
    {
        PROJ_WARN("ProjectileManager already initialized");
        return true;
    }

    if (!EntityDataManager::Instance().isInitialized())
    {
        PROJ_ERROR("EntityDataManager must be initialized before ProjectileManager");
        return false;
    }

    // Reset state
    m_isShutdown.store(false, std::memory_order_release);
    m_globallyPaused.store(false, std::memory_order_release);

    // Reserve buffers
    m_activeProjectileIndices.reserve(500);
    m_destroyQueue.reserve(64);
    m_batchFutures.reserve(16);
    m_batchDestroyQueues.reserve(16);
    m_singleDeferredEventBatch.reserve(1);

    // Register collision sink: keeps CollisionManager free of ProjectileManager dependency
    CollisionManager::Instance().setProjectileHitSink(
        [](const VoidLight::CollisionInfo& info)
        {
            ProjectileManager::Instance().handleProjectileCollision(info);
        });

    m_initialized.store(true, std::memory_order_release);
    PROJ_INFO("ProjectileManager initialized successfully");
    return true;
}

void ProjectileManager::clean()
{
    if (!m_initialized.load(std::memory_order_acquire))
    {
        return;
    }

    PROJ_INFO("Cleaning up ProjectileManager...");

    m_isShutdown.store(true, std::memory_order_release);

    // Wait for any pending async work
    for (auto& future : m_batchFutures)
    {
        if (future.valid())
        {
            future.wait();
        }
    }

    // Clear buffers
    m_activeProjectileIndices.clear();
    m_activeProjectileIndices.shrink_to_fit();
    m_destroyQueue.clear();
    m_destroyQueue.shrink_to_fit();
    m_batchFutures.clear();
    m_batchFutures.shrink_to_fit();
    m_singleDeferredEventBatch.clear();
    m_singleDeferredEventBatch.shrink_to_fit();
    m_batchDestroyQueues.clear();
    m_batchDestroyQueues.shrink_to_fit();

    // Deregister collision sink so CollisionManager doesn't hold a dangling reference
    CollisionManager::Instance().setProjectileHitSink(nullptr);

    m_perf = PerfStats{};
    m_initialized.store(false, std::memory_order_release);
    PROJ_INFO("ProjectileManager cleaned up");
}

void ProjectileManager::prepareForStateTransition()
{
    PROJ_INFO("Preparing for state transition...");

    // Wait for any pending async work
    for (auto& future : m_batchFutures)
    {
        if (future.valid())
        {
            future.wait();
        }
    }
    m_batchFutures.clear();

    // Destroy all active projectiles
    auto& edm = EntityDataManager::Instance();
    auto projectileSpan = edm.getIndicesByKind(EntityKind::Projectile);
    for (size_t idx : projectileSpan)
    {
        EntityHandle handle = edm.getHandle(idx);
        if (handle.isValid())
        {
            edm.destroyEntity(handle);
        }
    }

    // Clear buffers (keep capacity)
    m_activeProjectileIndices.clear();
    m_destroyQueue.clear();
    for (auto& queue : m_batchDestroyQueues)
    {
        queue.clear();
    }

    PROJ_INFO("State transition preparation complete");
}


void ProjectileManager::embedProjectile(size_t projectileIndex, const Vector2D& impactNormal,
                                        EntityHandle embeddedTarget)
{
    auto& edm = EntityDataManager::Instance();
    auto& projectileHot = edm.getHotDataByIndex(projectileIndex);
    auto& projectile = edm.getProjectileData(projectileHot.typeLocalIndex);

    if (projectile.isEmbedded())
    {
        return;
    }

    projectile.flags |= ProjectileData::FLAG_EMBEDDED;
    projectile.embeddedTarget = embeddedTarget;
    projectile.lifetime = ProjectileData::EMBEDDED_LIFETIME_SECONDS;
    if (embeddedTarget.isValid())
    {
        const auto& targetTransform = edm.getTransform(embeddedTarget);
        float offsetX = projectileHot.transform.position.getX() - targetTransform.position.getX();
        float offsetY = projectileHot.transform.position.getY() - targetTransform.position.getY();

        // Clamp offset to prevent the projectile from rendering inside the target.
        // The maximum valid offset is the sum of half-extents of both bodies.
        const size_t targetEdmIdx = edm.getIndex(embeddedTarget);
        if (targetEdmIdx != SIZE_MAX)
        {
            const auto& targetHot = edm.getHotDataByIndex(targetEdmIdx);
            const float maxOffset = projectileHot.halfWidth + targetHot.halfWidth;
            const float offsetLen = std::hypot(offsetX, offsetY);
            if (offsetLen > maxOffset && offsetLen > 0.0f)
            {
                const float scale = maxOffset / offsetLen;
                offsetX *= scale;
                offsetY *= scale;
            }
        }

        projectile.embeddedOffsetX = offsetX;
        projectile.embeddedOffsetY = offsetY;
    }
    else
    {
        projectile.embeddedOffsetX = impactNormal.getX() * projectileHot.halfWidth;
        projectile.embeddedOffsetY = impactNormal.getY() * projectileHot.halfHeight;
    }

    // Preserve flight angle so the renderer can orient the embedded sprite
    const auto& vel = projectileHot.transform.velocity;
    projectile.embeddedAngle = std::atan2(vel.getY(), vel.getX());

    projectileHot.transform.velocity = Vector2D(0.0f, 0.0f);
    projectileHot.transform.acceleration = Vector2D(0.0f, 0.0f);
    projectileHot.transform.previousPosition = projectileHot.transform.position;
    projectileHot.setCollisionEnabled(false);
    projectileHot.collisionMask = 0;
}

void ProjectileManager::handleProjectileCollision(const VoidLight::CollisionInfo& info)
{
    if (!m_initialized.load(std::memory_order_acquire) ||
        m_isShutdown.load(std::memory_order_acquire))
    {
        return;
    }

    auto& edm = EntityDataManager::Instance();

    // Trigger collisions for projectiles reach here when a projectile overlaps a physical
    // trigger body. They are dispatched separately via processTriggerEvents(); this branch
    // is a defensive guard to prevent double-processing projectile-vs-trigger contacts.
    if (info.trigger)
    {
        return;
    }

    if (info.indexA == SIZE_MAX)
    {
        return;
    }

    size_t projIdx = SIZE_MAX;
    size_t targetIdx = SIZE_MAX;
    Vector2D knockbackNormal = info.normal;

    const auto& hotA = edm.getHotDataByIndex(info.indexA);
    if (!hotA.isAlive())
    {
        return;
    }

    if (!info.isMovableMovable)
    {
        if (hotA.kind != EntityKind::Projectile)
        {
            return;
        }

        embedProjectile(info.indexA, info.normal);
        return;
    }

    if (info.indexB == SIZE_MAX)
    {
        return;
    }

    const auto& hotB = edm.getHotDataByIndex(info.indexB);

    // Skip if either entity is already dead (destroyed between collision detect and event dispatch)
    if (!hotB.isAlive())
    {
        return;
    }

    // Identify which entity is the projectile (if any)
    if (hotA.kind == EntityKind::Projectile && hotB.kind != EntityKind::Projectile)
    {
        projIdx = info.indexA;
        targetIdx = info.indexB;
    }
    else if (hotB.kind == EntityKind::Projectile && hotA.kind != EntityKind::Projectile)
    {
        projIdx = info.indexB;
        targetIdx = info.indexA;
        knockbackNormal = info.normal * -1.0f;
    }
    else
    {
        // Neither is a projectile, or both are — skip
        return;
    }

    auto& projHot = edm.getHotDataByIndex(projIdx);
    if (!projHot.isAlive())
    {
        return;
    }

    const auto& proj = edm.getProjectileData(projHot.typeLocalIndex);
    if (proj.isEmbedded())
    {
        return;
    }

    EntityHandle targetHandle = edm.getHandle(targetIdx);
    if (targetHandle == proj.owner)
    {
        return;
    }

    // Only damage entities that have health (Player, NPC)
    const auto& targetHot = edm.getHotDataByIndex(targetIdx);
    if (!EntityTraits::hasHealth(targetHot.kind))
    {
        // Hit non-damageable entity — embed and stop participating in collision/damage.
        embedProjectile(projIdx, knockbackNormal, targetHandle);
        return;
    }

    // Projectile contacts are interpreted during the collision pass, but the
    // projectile may already have been zeroed by embed/lifetime logic before
    // the queued DamageEvent is processed. Fall back to the stored launch
    // speed when computing hit force.
    const float actualSpeedSq = projHot.transform.velocity.lengthSquared();
    float effectiveSpeedSq = actualSpeedSq;
    if (effectiveSpeedSq < MIN_PROJECTILE_SPEED_SQ)
    {
        effectiveSpeedSq = proj.speed * proj.speed;
    }

    if (effectiveSpeedSq < MIN_PROJECTILE_SPEED_SQ)
    {
        return;
    }

    // Create and enqueue DamageEvent (deferred — same pipeline as NPC combat)
    auto& eventMgr = EventManager::Instance();
    auto damageEvent = eventMgr.acquireDamageEvent();

    // Knockback scaled by impact speed — faster projectiles hit harder
    constexpr float KNOCKBACK_BASE = 30.0f;
    constexpr float KNOCKBACK_SPEED_FACTOR = 0.1f;
    const float effectiveSpeed = std::sqrt(effectiveSpeedSq);
    const float knockbackForce = KNOCKBACK_BASE + effectiveSpeed * KNOCKBACK_SPEED_FACTOR;
    Vector2D knockback = knockbackNormal * knockbackForce;

    damageEvent->configure(proj.owner, targetHandle, proj.damage, knockback);

    EventData damageData;
    damageData.typeId = EventTypeId::Combat;
    damageData.setActive(true);
    damageData.event = damageEvent;

    m_singleDeferredEventBatch.clear();
    m_singleDeferredEventBatch.push_back({EventTypeId::Combat, std::move(damageData)});
    eventMgr.enqueueBatch(std::move(m_singleDeferredEventBatch));
    m_singleDeferredEventBatch.reserve(1); // Restore capacity lost by move

    // Destroy projectile (unless piercing)
    if (!(proj.flags & ProjectileData::FLAG_PIERCING))
    {
        embedProjectile(projIdx, knockbackNormal, targetHandle);
    }
}


// ============================================================================
// MAIN UPDATE — Position integration + lifetime management
// ============================================================================

void ProjectileManager::update(float deltaTime)
{
    if (!m_initialized.load(std::memory_order_acquire) ||
        m_isShutdown.load(std::memory_order_acquire) ||
        m_globallyPaused.load(std::memory_order_acquire))
    {
        return;
    }

    auto& edm = EntityDataManager::Instance();
    auto projectileSpan = edm.getIndicesByKind(EntityKind::Projectile);

    if (projectileSpan.empty())
    {
        m_perf.lastEntitiesProcessed = 0;
        m_perf.lastUpdateMs = 0.0;
        return;
    }

    auto t0 = std::chrono::steady_clock::now();

    // Copy to local buffer (span may be invalidated during processing)
    m_activeProjectileIndices.clear();
    m_activeProjectileIndices.insert(m_activeProjectileIndices.end(),
                                     projectileSpan.begin(), projectileSpan.end());

    const size_t entityCount = m_activeProjectileIndices.size();

    // Query world bounds ONCE per frame
    float worldWidth = 32000.0f;
    float worldHeight = 32000.0f;
    auto& pathMgr = PathfinderManager::Instance();
    if (pathMgr.isInitialized())
    {
        float w, h;
        if (pathMgr.getCachedWorldBounds(w, h) && w > 0 && h > 0)
        {
            worldWidth = w;
            worldHeight = h;
        }
    }

    // WorkerBudget threading decision (follows AIManager pattern)
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto decision = budgetMgr.shouldUseThreading(
        VoidLight::SystemType::ProjectileSim, entityCount);
    bool useThreading = decision.shouldThread;

    size_t actualBatchCount = 1;
    bool actualWasThreaded = false;

    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    if (useThreading)
    {
        size_t optimalWorkerCount = budgetMgr.getOptimalWorkers(
            VoidLight::SystemType::ProjectileSim, entityCount);
        auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
            VoidLight::SystemType::ProjectileSim, entityCount, optimalWorkerCount);

        actualWasThreaded = true;
        actualBatchCount = batchCount;

        size_t entitiesPerBatch = entityCount / batchCount;
        size_t remainingEntities = entityCount % batchCount;

        // Ensure per-batch destroy queues are sized and cleared
        if (m_batchDestroyQueues.size() < batchCount)
        {
            m_batchDestroyQueues.resize(batchCount);
            for (size_t i = 0; i < batchCount; ++i)
            {
                m_batchDestroyQueues[i].reserve(32);
            }
        }
        for (size_t i = 0; i < batchCount; ++i)
        {
            m_batchDestroyQueues[i].clear();
        }

        m_batchFutures.clear();
        m_batchFutures.reserve(batchCount);

        auto& threadSystem = VoidLight::ThreadSystem::Instance();

        startTime = std::chrono::steady_clock::now();

        for (size_t i = 0; i < batchCount; ++i)
        {
            size_t start = i * entitiesPerBatch;
            size_t end = start + entitiesPerBatch;

            if (i == batchCount - 1)
            {
                end += remainingEntities;
            }

            m_batchFutures.push_back(
                threadSystem.enqueueTaskWithResult(
                    [this, start, end, deltaTime, worldWidth, worldHeight, i]()
                    {
                        processBatch(m_activeProjectileIndices, start, end,
                                     deltaTime, worldWidth, worldHeight,
                                     m_batchDestroyQueues[i]);
                    },
                    VoidLight::TaskPriority::Normal, "Proj_Batch"));
        }

        // Wait for all batches
        for (auto& future : m_batchFutures)
        {
            if (future.valid())
            {
                future.get();
            }
        }

        endTime = std::chrono::steady_clock::now();

        // Collect and process destroy queues from all batches
        for (size_t i = 0; i < batchCount; ++i)
        {
            for (const auto& handle : m_batchDestroyQueues[i])
            {
                if (handle.isValid())
                {
                    edm.destroyEntity(handle);
                }
            }
        }
    }
    else
    {
        // Single-threaded processing
        actualWasThreaded = false;
        actualBatchCount = 1;
        m_destroyQueue.clear();

        startTime = std::chrono::steady_clock::now();

        processBatch(m_activeProjectileIndices, 0, entityCount,
                     deltaTime, worldWidth, worldHeight, m_destroyQueue);

        endTime = std::chrono::steady_clock::now();

        // Destroy expired projectiles
        for (const auto& handle : m_destroyQueue)
        {
            if (handle.isValid())
            {
                edm.destroyEntity(handle);
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double batchMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    m_perf.lastEntitiesProcessed = entityCount;
    m_perf.lastUpdateMs = elapsedMs;
    m_perf.lastBatchCount = actualBatchCount;
    m_perf.lastWasThreaded = actualWasThreaded;

    // EMA update for rolling average
    constexpr double PERF_ALPHA = 0.05;
    if (m_perf.totalUpdates == 0)
    {
        m_perf.avgUpdateMs = elapsedMs;
    }
    else
    {
        m_perf.avgUpdateMs = PERF_ALPHA * elapsedMs + (1.0 - PERF_ALPHA) * m_perf.avgUpdateMs;
    }
    m_perf.totalUpdates++;

    // Report ONLY batch time for adaptive tuning
    budgetMgr.reportExecution(VoidLight::SystemType::ProjectileSim,
                              entityCount, actualWasThreaded,
                              actualBatchCount, batchMs);

    VOIDLIGHT_DEBUG_ONLY(
        // Rolling log every 60 seconds (3600 updates at 60Hz)
        if (m_perf.totalUpdates % 3600 == 0 && entityCount > 0)
        {
            PROJ_DEBUG(std::format(
                "Entities: {}, Avg: {:.2f}ms [{}]",
                entityCount, m_perf.avgUpdateMs,
                actualWasThreaded ? std::format("{} batches", actualBatchCount) : "single"));
        }
    )
}


// ============================================================================
// BATCH PROCESSING — SIMD 4-wide position integration + lifetime
// ============================================================================

void ProjectileManager::processBatch(const std::vector<size_t>& indices,
                                     size_t start, size_t end,
                                     float deltaTime,
                                     float worldWidth, float worldHeight,
                                     std::vector<EntityHandle>& outDestroyQueue)
{
    auto& edm = EntityDataManager::Instance();

    // SIMD 4-wide movement batch (follows AIManager pattern)
    std::array<TransformData*, 4> batchTransforms{};
    std::array<const EntityHotData*, 4> batchHotData{};
    std::array<size_t, 4> batchEdmIndices{};
    size_t batchCount = 0;

    // Boundary-hit lanes are collected here and drained after the main loop
    // so embedProjectile()'s cold-data writes don't pollute the SIMD flush.
    thread_local std::vector<std::pair<size_t, Vector2D>> pendingEmbeds;
    pendingEmbeds.clear();

    // Returns true if projectile hit world boundary and should embed there.
    auto updateMovementScalar = [&](TransformData& transform,
                                    const EntityHotData& hotData) -> bool
    {
        Vector2D pos = transform.position + (transform.velocity * deltaTime);

        float halfW = hotData.halfWidth;
        float halfH = hotData.halfHeight;
        float minX = halfW;
        float maxX = worldWidth - halfW;
        float minY = halfH;
        float maxY = worldHeight - halfH;
        if (maxX < minX)
        {
            minX = worldWidth * 0.5f;
            maxX = minX;
        }
        if (maxY < minY)
        {
            minY = worldHeight * 0.5f;
            maxY = minY;
        }
        Vector2D clamped(std::clamp(pos.getX(), minX, maxX),
                         std::clamp(pos.getY(), minY, maxY));
        transform.position = clamped;

        bool hitBoundary = false;
        if (pos.getX() < minX || pos.getX() > maxX)
        {
            transform.velocity.setX(0.0f);
            hitBoundary = true;
        }
        if (pos.getY() < minY || pos.getY() > maxY)
        {
            transform.velocity.setY(0.0f);
            hitBoundary = true;
        }
        return hitBoundary;
    };

    auto flushMovementBatch = [&]()
    {
        if (batchCount == 0)
        {
            return;
        }
        if (batchCount < 4)
        {
            for (size_t lane = 0; lane < batchCount; ++lane)
            {
                if (updateMovementScalar(*batchTransforms[lane], *batchHotData[lane]))
                {
                    TransformData* transform = batchTransforms[lane];
                    Vector2D impactNormal(0.0f, 0.0f);
                    if (transform->position.getX() <= batchHotData[lane]->halfWidth)
                    {
                        impactNormal.setX(-1.0f);
                    }
                    else if (transform->position.getX() >= worldWidth - batchHotData[lane]->halfWidth)
                    {
                        impactNormal.setX(1.0f);
                    }
                    if (transform->position.getY() <= batchHotData[lane]->halfHeight)
                    {
                        impactNormal.setY(-1.0f);
                    }
                    else if (transform->position.getY() >= worldHeight - batchHotData[lane]->halfHeight)
                    {
                        impactNormal.setY(1.0f);
                    }
                    pendingEmbeds.emplace_back(batchEdmIndices[lane], impactNormal);
                }
            }
            batchCount = 0;
            return;
        }

        alignas(16) float posX[4];
        alignas(16) float posY[4];
        alignas(16) float velX[4];
        alignas(16) float velY[4];
        alignas(16) float minX[4];
        alignas(16) float maxX[4];
        alignas(16) float minY[4];
        alignas(16) float maxY[4];

        for (size_t lane = 0; lane < 4; ++lane)
        {
            TransformData* transform = batchTransforms[lane];
            const EntityHotData* hotData = batchHotData[lane];
            posX[lane] = transform->position.getX();
            posY[lane] = transform->position.getY();
            velX[lane] = transform->velocity.getX();
            velY[lane] = transform->velocity.getY();
            float laneMinX = hotData->halfWidth;
            float laneMaxX = worldWidth - hotData->halfWidth;
            float laneMinY = hotData->halfHeight;
            float laneMaxY = worldHeight - hotData->halfHeight;
            if (laneMaxX < laneMinX)
            {
                laneMinX = worldWidth * 0.5f;
                laneMaxX = laneMinX;
            }
            if (laneMaxY < laneMinY)
            {
                laneMinY = worldHeight * 0.5f;
                laneMaxY = laneMinY;
            }
            minX[lane] = laneMinX;
            maxX[lane] = laneMaxX;
            minY[lane] = laneMinY;
            maxY[lane] = laneMaxY;
        }

        const Float4 deltaTimeVec = broadcast(deltaTime);
        Float4 posXv = load4_aligned(posX);
        Float4 posYv = load4_aligned(posY);
        const Float4 velXv = load4_aligned(velX);
        const Float4 velYv = load4_aligned(velY);

        posXv = madd(velXv, deltaTimeVec, posXv);
        posYv = madd(velYv, deltaTimeVec, posYv);

        const Float4 minXv = load4_aligned(minX);
        const Float4 maxXv = load4_aligned(maxX);
        const Float4 minYv = load4_aligned(minY);
        const Float4 maxYv = load4_aligned(maxY);
        const Float4 clampedXv = clamp(posXv, minXv, maxXv);
        const Float4 clampedYv = clamp(posYv, minYv, maxYv);

        const Float4 xDiff =
            bitwise_or(cmplt(clampedXv, posXv), cmplt(posXv, clampedXv));
        const Float4 yDiff =
            bitwise_or(cmplt(clampedYv, posYv), cmplt(posYv, clampedYv));
        const int clampXMask = movemask(xDiff);
        const int clampYMask = movemask(yDiff);

        store4_aligned(posX, clampedXv);
        store4_aligned(posY, clampedYv);

        const int boundaryMask = clampXMask | clampYMask;
        for (size_t lane = 0; lane < 4; ++lane)
        {
            TransformData* transform = batchTransforms[lane];
            transform->position.setX(posX[lane]);
            transform->position.setY(posY[lane]);

            if ((clampXMask >> lane) & 0x1)
            {
                transform->velocity.setX(0.0f);
            }
            if ((clampYMask >> lane) & 0x1)
            {
                transform->velocity.setY(0.0f);
            }

            // Embed projectiles that hit the world boundary (deferred)
            if ((boundaryMask >> lane) & 0x1)
            {
                Vector2D impactNormal(0.0f, 0.0f);
                if ((clampXMask >> lane) & 0x1)
                {
                    impactNormal.setX((velX[lane] > 0.0f) ? 1.0f : -1.0f);
                }
                if ((clampYMask >> lane) & 0x1)
                {
                    impactNormal.setY((velY[lane] > 0.0f) ? 1.0f : -1.0f);
                }
                pendingEmbeds.emplace_back(batchEdmIndices[lane], impactNormal);
            }
        }

        batchCount = 0;
    };

    // Main processing loop
    for (size_t i = start; i < end && i < indices.size(); ++i)
    {
        size_t edmIdx = indices[i];
        auto& hot = edm.getHotDataByIndex(edmIdx);

        if (!hot.isAlive()) continue;

        auto& proj = edm.getProjectileData(hot.typeLocalIndex);
        proj.lifetime -= deltaTime;

        if (proj.lifetime <= 0.0f)
        {
            outDestroyQueue.push_back(edm.getHandle(edmIdx));
            continue;
        }

        // Embedded projectiles are stationary — position is invariant and
        // previousPosition was stamped once in embedProjectile(), so skip
        // the redundant per-frame write and SIMD batching entirely.
        if (proj.isEmbedded())
        {
            continue;
        }

        auto& transform = hot.transform;
        transform.previousPosition = transform.position;

        // Accumulate for SIMD batch movement
        batchTransforms[batchCount] = &transform;
        batchHotData[batchCount] = &hot;
        batchEdmIndices[batchCount] = edmIdx;
        ++batchCount;

        if (batchCount == 4)
        {
            flushMovementBatch();
        }
    }

    // Flush remaining SIMD batch
    flushMovementBatch();

    // Drain deferred boundary-embed events outside the hot SIMD loop.
    for (const auto& [projectileIndex, impactNormal] : pendingEmbeds)
    {
        embedProjectile(projectileIndex, impactNormal);
    }
    pendingEmbeds.clear();
}
