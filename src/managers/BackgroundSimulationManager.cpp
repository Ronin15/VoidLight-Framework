/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/BackgroundSimulationManager.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "managers/EntityDataManager.hpp"
#include <algorithm>
#include <chrono>
#include <format>


// ============================================================================
// LIFECYCLE
// ============================================================================

bool BackgroundSimulationManager::init() {
    if (m_initialized.load(std::memory_order_acquire)) {
        BGSIM_WARN("BackgroundSimulationManager already initialized");
        return true;
    }

    // Verify dependencies
    if (!EntityDataManager::Instance().isInitialized()) {
        BGSIM_ERROR("EntityDataManager must be initialized before BackgroundSimulationManager");
        return false;
    }

    // Reset state (clean() sets m_isShutdown=true, must reset here)
    m_isShutdown.store(false, std::memory_order_release);
    m_tiersDirty.store(true, std::memory_order_release);
    m_referencePointSet = false;
    m_framesSinceTierUpdate = 0;
    m_accumulator = 0.0;
    m_hasNonActiveEntities.store(false, std::memory_order_release);
    m_globallyPaused.store(false, std::memory_order_release);

    // Reserve buffers
    m_backgroundIndices.reserve(10000);  // Expect up to 10K background entities
    m_batchFutures.reserve(16);          // Reasonable batch count

    m_initialized.store(true, std::memory_order_release);
    BGSIM_INFO("BackgroundSimulationManager initialized successfully");
    return true;
}

void BackgroundSimulationManager::clean() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return;
    }

    BGSIM_INFO("Cleaning up BackgroundSimulationManager...");

    // Mark as shutdown to prevent new work
    m_isShutdown.store(true, std::memory_order_release);

    // Wait for any pending async work
    waitForAsyncCompletion();

    // Clear buffers
    m_backgroundIndices.clear();
    m_backgroundIndices.shrink_to_fit();

    m_initialized.store(false, std::memory_order_release);
    BGSIM_INFO("BackgroundSimulationManager cleaned up");
}

void BackgroundSimulationManager::prepareForStateTransition() {
    BGSIM_INFO("Preparing for state transition...");
    waitForAsyncCompletion();
    m_backgroundIndices.clear();
    m_tiersDirty.store(true, std::memory_order_release);
    m_framesSinceTierUpdate = 0;
    m_referencePointSet = false;  // Force reference point update on next state
    m_accumulator = 0.0;          // Reset timing for clean start
    BGSIM_INFO("State transition preparation complete");
}

// ============================================================================
// MAIN UPDATE - Unified tier recalc + background entity processing
// ============================================================================

void BackgroundSimulationManager::update(const Vector2D& referencePoint, float deltaTime) {
    // === CRITICAL: Early exit checks FIRST - match all other managers ===
    // When paused: NO work at all (no frame counter, no tier check, nothing)
    if (!m_initialized.load(std::memory_order_acquire) ||
        m_isShutdown.load(std::memory_order_acquire) ||
        m_globallyPaused.load(std::memory_order_acquire)) {
        return;  // Complete skip - zero CPU cycles when paused
    }

    // === PHASE 1: Periodic tier recalculation (every 60 frames) ===
    m_framesSinceTierUpdate++;
    bool needsTierUpdate = m_tiersDirty.load(std::memory_order_acquire) ||
                           m_framesSinceTierUpdate >= TIER_UPDATE_INTERVAL;

    if (needsTierUpdate) {
        setReferencePoint(referencePoint);
        updateTiers();
        m_framesSinceTierUpdate = 0;
        m_tiersDirty.store(false, std::memory_order_release);

        // Update work flag based on tier results
        const auto& edm = EntityDataManager::Instance();
        auto backgroundSpan = edm.getBackgroundIndices();
        m_hasNonActiveEntities.store(!backgroundSpan.empty(), std::memory_order_release);
    }

    // === PHASE 2: Background entity processing (10Hz, only if work exists) ===
    if (!m_hasNonActiveEntities.load(std::memory_order_acquire)) {
        return;  // No background entities - skip processing entirely
    }

    // Accumulator pattern for 10Hz updates
    m_accumulator += deltaTime;
    while (m_accumulator >= m_updateInterval) {
        m_accumulator -= m_updateInterval;
        processBackgroundEntities(m_updateInterval);
    }
}

void BackgroundSimulationManager::processBackgroundEntities(float fixedDeltaTime) {
    auto t0 = std::chrono::steady_clock::now();

    // Get background tier indices from EntityDataManager
    // Note: Tier updates now happen in unified update() method
    auto& edm = EntityDataManager::Instance();
    auto backgroundSpan = edm.getBackgroundIndices();

    if (backgroundSpan.empty()) {
        m_perf.lastEntitiesProcessed = 0;
        m_perf.lastUpdateMs = 0.0;
        return;
    }

    // Copy to local buffer (span may be invalidated during processing)
    m_backgroundIndices.clear();
    m_backgroundIndices.insert(m_backgroundIndices.end(),
                               backgroundSpan.begin(), backgroundSpan.end());

    // Use background indices directly - processBatch already filters by kind/alive.
    // WorkerBudget learns from the full background count (close enough for threading decisions).
    const size_t entityCount = m_backgroundIndices.size();

    // Use centralized WorkerBudgetManager for smart worker allocation (follows AIManager pattern)
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    // Decide threading strategy first using adaptive threshold from WorkerBudget
    // WorkerBudget is the AUTHORITATIVE source - no manager overrides
    auto decision = budgetMgr.shouldUseThreading(
        VoidLight::SystemType::BackgroundSim, entityCount);
    bool useThreading = decision.shouldThread;

    size_t actualBatchCount = 1;

    // Per-path timing: single-threaded feeds threshold learning, batch feeds hill-climbing
    std::chrono::steady_clock::time_point batchStart;
    std::chrono::steady_clock::time_point batchEnd;

    if (useThreading) {
        // Compute batch strategy (not timed — only actual work is timed)
        size_t optimalWorkerCount = budgetMgr.getOptimalWorkers(
            VoidLight::SystemType::BackgroundSim, entityCount);
        auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
            VoidLight::SystemType::BackgroundSim, entityCount, optimalWorkerCount);

        batchStart = std::chrono::steady_clock::now();
        processMultiThreaded(fixedDeltaTime, m_backgroundIndices, batchCount, batchSize);
        batchEnd = std::chrono::steady_clock::now();
        m_perf.lastWasThreaded = true;
        m_perf.lastBatchCount = batchCount;
        actualBatchCount = batchCount;
    } else {
        batchStart = std::chrono::steady_clock::now();
        processSingleThreaded(fixedDeltaTime, m_backgroundIndices);
        batchEnd = std::chrono::steady_clock::now();
        m_perf.lastWasThreaded = false;
        m_perf.lastBatchCount = 1;
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double batchMs = std::chrono::duration<double, std::milli>(batchEnd - batchStart).count();

    m_perf.lastEntitiesProcessed = entityCount;
    m_perf.lastUpdateMs = elapsedMs;
    m_perf.updateAverage(elapsedMs);

    // Report ONLY batch time for adaptive tuning (not index retrieval/threading decision)
    budgetMgr.reportExecution(VoidLight::SystemType::BackgroundSim,
                              entityCount, useThreading,
                              actualBatchCount, batchMs);

    VOIDLIGHT_DEBUG_ONLY(
        // Rolling log every 60 seconds (600 updates at 10Hz)
        if (m_perf.totalUpdates % 600 == 0) {
            BGSIM_DEBUG(std::format(
                "Entities: {}, Avg: {:.2f}ms [{}]",
                entityCount, m_perf.avgUpdateMs,
                useThreading ? std::format("{} batches", actualBatchCount) : "single"));
        }
    )
}

void BackgroundSimulationManager::waitForAsyncCompletion() {
    std::lock_guard<std::mutex> lock(m_futuresMutex);
    for (auto& future : m_batchFutures) {
        if (future.valid()) {
            future.wait();
        }
    }
    m_batchFutures.clear();
}

// ============================================================================
// TIER MANAGEMENT
// ============================================================================

void BackgroundSimulationManager::setReferencePoint(const Vector2D& position) {
    // First call: always set reference point (m_referencePointSet starts false)
    if (!m_referencePointSet) {
        m_referencePoint = position;
        m_referencePointSet = true;
        m_tiersDirty.store(true, std::memory_order_release);
        return;
    }

    // Subsequent calls: only update if moved significantly (avoid excessive tier recalcs)
    float dx = position.getX() - m_referencePoint.getX();
    float dy = position.getY() - m_referencePoint.getY();
    float distSq = dx * dx + dy * dy;

    // Threshold: 32 units movement triggers tier recalculation (1-2 tiles)
    constexpr float MOVEMENT_THRESHOLD_SQ = 32.0f * 32.0f;

    if (distSq > MOVEMENT_THRESHOLD_SQ) {
        m_referencePoint = position;
        m_tiersDirty.store(true, std::memory_order_release);
    }
}

void BackgroundSimulationManager::updateTiers() {
    auto& edm = EntityDataManager::Instance();
    if (!edm.isInitialized()) {
        return;
    }

    // Delegate to EntityDataManager
    edm.updateSimulationTiers(m_referencePoint, m_activeRadius, m_backgroundRadius);
}

// ============================================================================
// BATCH PROCESSING (follows AIManager pattern)
// ============================================================================

void BackgroundSimulationManager::processSingleThreaded(float deltaTime,
                                                        const std::vector<size_t>& indices) {
    processBatch(deltaTime, indices, 0, indices.size());
}

void BackgroundSimulationManager::processMultiThreaded(float deltaTime,
                                                       const std::vector<size_t>& indices,
                                                       size_t batchCount,
                                                       size_t batchSize) {
    auto& threadSystem = VoidLight::ThreadSystem::Instance();

    // Clear previous futures (reuse capacity)
    {
        std::lock_guard<std::mutex> lock(m_futuresMutex);
        m_batchFutures.clear();
        m_batchFutures.reserve(batchCount);
    }

    const size_t totalCount = indices.size();
    size_t remainingEntities = totalCount % batchCount;

    for (size_t batch = 0; batch < batchCount; ++batch) {
        size_t startIdx = batch * batchSize;
        size_t endIdx = startIdx + batchSize;

        // Add remaining entities to last batch
        if (batch == batchCount - 1) {
            endIdx += remainingEntities;
        }

        if (startIdx >= endIdx || startIdx >= totalCount) continue;
        endIdx = std::min(endIdx, totalCount);

        // Submit task with future for completion tracking (follows AIManager pattern)
        auto future = threadSystem.enqueueTaskWithResult(
            [this, deltaTime, &indices, startIdx, endIdx]() -> void {
                try {
                    processBatch(deltaTime, indices, startIdx, endIdx);
                } catch (const std::exception& e) {
                    BGSIM_ERROR(std::format("Exception in background sim batch: {}", e.what()));
                } catch (...) {
                    BGSIM_ERROR("Unknown exception in background sim batch");
                }
            },
            VoidLight::TaskPriority::Low,  // Background sim is low priority
            "BGSim_Batch"
        );

        {
            std::lock_guard<std::mutex> lock(m_futuresMutex);
            m_batchFutures.push_back(std::move(future));
        }
    }

    // Wait for all batches to complete
    waitForAsyncCompletion();
}

void BackgroundSimulationManager::processBatch(float deltaTime,
                                               const std::vector<size_t>& indices,
                                               size_t startIdx,
                                               size_t endIdx) {
    auto& edm = EntityDataManager::Instance();

    for (size_t i = startIdx; i < endIdx; ++i) {
        size_t index = indices[i];

        // Get entity kind to determine simulation type
        const auto& hot = edm.getHotDataByIndex(index);

        if (!hot.isAlive()) continue;

        switch (hot.kind) {
            case EntityKind::NPC:
                simulateNPC(deltaTime, index);
                break;
            case EntityKind::DroppedItem:
                simulateItem(deltaTime, index);
                break;
            // Projectiles and AreaEffects should always be Active tier
            // Containers, Harvestables, Props, Triggers don't need background sim
            default:
                break;
        }
    }
}

// ============================================================================
// TYPE-SPECIFIC SIMPLIFIED SIMULATION
// ============================================================================

void BackgroundSimulationManager::simulateNPC(float deltaTime, size_t index) {
    auto& edm = EntityDataManager::Instance();
    auto& transform = edm.getTransformByIndex(index);

    // Simplified NPC simulation for background tier:
    // - Apply basic velocity to position (no collision detection)
    // - Gradual velocity decay (simulates slowing down)
    // - No AI behavior execution (too expensive)

    // Store previous position for interpolation (if entity becomes Active again)
    transform.previousPosition = transform.position;

    // Apply velocity with decay
    constexpr float VELOCITY_DECAY = 0.98f;  // 2% decay per frame
    constexpr float MIN_VELOCITY_SQ = 0.1f;  // Stop if velocity is negligible

    float velMagSq = transform.velocity.getX() * transform.velocity.getX() +
                     transform.velocity.getY() * transform.velocity.getY();

    if (velMagSq > MIN_VELOCITY_SQ) {
        // Apply velocity to position
        transform.position = transform.position + (transform.velocity * deltaTime);

        // Decay velocity
        transform.velocity = transform.velocity * VELOCITY_DECAY;
    } else {
        // Stop movement
        transform.velocity = Vector2D(0.0f, 0.0f);
    }
}

void BackgroundSimulationManager::simulateItem(float, size_t) {
    // Items in background tier:
    // - Update pickup timer if still counting down
    // - Could implement despawn timer for dropped items
    //
    // processBatch already validated aliveness and routed only DroppedItem
    // entities here, so no re-fetch or kind re-check is needed.

    // Items don't need position simulation - they stay where dropped
    // The main purpose of background item simulation is:
    // 1. Despawn after long time
    // 2. Keep timers updated

    // Note: Actual despawn logic would use ItemData from EntityDataManager
    // For now, items in background tier are just preserved
}
