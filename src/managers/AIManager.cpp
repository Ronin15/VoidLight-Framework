/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/AIManager.hpp"
#include "ai/AICommandBus.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "ai/internal/Crowd.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "utils/SIMDMath.hpp"
#include <array>
#include <algorithm>
#include <format>
#include <unordered_map>

// Use SIMD abstraction layer
using namespace VoidLight::SIMD;

namespace {

constexpr size_t MAX_PENDING_BEHAVIOR_MESSAGES = 4;

struct PendingBehaviorMessageCandidate {
  uint8_t messageId{0};
  uint8_t param{0};
  uint64_t sequence{0};
};

constexpr size_t MAX_COMPACTED_BEHAVIOR_MESSAGES = 16;
constexpr float NPC_PROJECTILE_SPAWN_OFFSET = 20.0f;

void sortPendingBehaviorCandidates(
    std::array<PendingBehaviorMessageCandidate,
               MAX_COMPACTED_BEHAVIOR_MESSAGES>& candidates,
    size_t count) {
  for (size_t i = 1; i < count; ++i) {
    PendingBehaviorMessageCandidate value = candidates[i];
    size_t insertIdx = i;
    while (insertIdx > 0 &&
           candidates[insertIdx - 1].sequence > value.sequence) {
      candidates[insertIdx] = candidates[insertIdx - 1];
      --insertIdx;
    }
    candidates[insertIdx] = value;
  }
}

void upsertPendingBehaviorCandidate(
    std::array<PendingBehaviorMessageCandidate,
               MAX_COMPACTED_BEHAVIOR_MESSAGES>& candidates,
    size_t& count,
    const PendingBehaviorMessageCandidate& candidate) {
  auto begin = candidates.begin();
  auto end = begin + static_cast<std::ptrdiff_t>(count);
  auto existing = std::find_if(begin, end, [&](const auto& currentCandidate) {
    return currentCandidate.messageId == candidate.messageId;
  });
  if (existing != end) {
    *existing = candidate;
    return;
  }

  if (count < candidates.size()) {
    candidates[count++] = candidate;
  }
}

bool equipFirstAvailableMeleeWeapon(EntityDataManager& edm, EntityHandle handle) {
  if (!handle.isValid() || !handle.hasHealth()) {
    return false;
  }

  const size_t idx = edm.getIndex(handle);
  if (idx == SIZE_MAX) {
    return false;
  }

  const auto& charData = edm.getCharacterDataByIndex(idx);
  if (!charData.hasInventory()) {
    return false;
  }

  const size_t maxSlots = edm.getInventoryData(charData.inventoryIndex).maxSlots;
  auto& resourceManager = ResourceTemplateManager::Instance();
  for (size_t slot = 0; slot < maxSlots; ++slot) {
    const InventorySlotData inventorySlot =
        edm.getInventorySlot(charData.inventoryIndex, slot);
    if (inventorySlot.isEmpty()) {
      continue;
    }

    auto resourceTemplate =
        resourceManager.getResourceTemplate(inventorySlot.resourceHandle);
    const auto equipment =
        std::dynamic_pointer_cast<Equipment>(resourceTemplate);
    if (!equipment ||
        equipment->getEquipmentSlot() != Equipment::EquipmentSlot::Weapon ||
        equipment->getWeaponMode() != Equipment::WeaponMode::Melee) {
      continue;
    }

    return edm.equipCharacterItem(handle, inventorySlot.resourceHandle);
  }

  return false;
}

void dispatchResourceChange(EntityHandle ownerHandle,
                            const InventoryResourceChange& change,
                            const std::string& reason) {
  if (!change.isValid()) {
    return;
  }

  EventManager::Instance().triggerResourceChange(
      ownerHandle, change.resourceHandle, change.oldQuantity,
      change.newQuantity, reason);
}

} // namespace

bool AIManager::init() {
  if (m_initialized.load(std::memory_order_acquire)) {
    AI_INFO("AIManager already initialized");
    return true;
  }

  try {
    // Validate dependency initialization order
    // AIManager requires these managers to be initialized first to avoid
    // null pointer dereferences and initialization races
    if (!PathfinderManager::Instance().isInitialized()) {
      AI_ERROR("PathfinderManager must be initialized before AIManager");
      return false;
    }
    if (!CollisionManager::Instance().isInitialized()) {
      AI_ERROR("CollisionManager must be initialized before AIManager");
      return false;
    }

    // Cache manager references for hot path usage (avoid singleton lookups)
    mp_pathfinderManager = &PathfinderManager::Instance();

    // Pre-allocate storage for better performance
    constexpr size_t INITIAL_CAPACITY = 1000;
    m_storage.reserve(INITIAL_CAPACITY);
    m_handleToIndex.reserve(INITIAL_CAPACITY);

    // Reserve EDM-to-storage reverse mapping (grows dynamically based on EDM
    // indices)
    m_edmToStorageIndex.reserve(INITIAL_CAPACITY);

    m_activeIndicesBuffer.reserve(INITIAL_CAPACITY);

    m_initialized.store(true, std::memory_order_release);
    m_globallyPaused.store(false, std::memory_order_release);
    m_isShutdown = false;

    // Register default behaviors (Idle, Wander, Chase, Guard, Attack, Flee, Follow)
    registerDefaultBehaviors();

    AI_INFO("AIManager initialized successfully");
    return true;

  } catch (const std::exception &e) {
    AI_ERROR(std::format("Failed to initialize AIManager: {}", e.what()));
    return false;
  }
}

void AIManager::clean() {
  if (!m_initialized.load(std::memory_order_acquire) || m_isShutdown) {
    return;
  }

  AI_INFO("AIManager shutting down...");

  // Mark as shutting down
  m_isShutdown = true;
  m_initialized.store(false, std::memory_order_release);

  // Stop accepting new tasks
  m_globallyPaused.store(true, std::memory_order_release);
  VoidLight::AICommandBus::Instance().clearAll();
  m_pendingFactionChanges.clear();
  m_pendingBehaviorTransitions.clear();
  m_pendingBehaviorMessages.clear();
  m_pendingMeleeFallbackEquips.clear();
  m_pendingRangedAttacks.clear();

  {
    std::unique_lock<std::shared_mutex> entitiesLock(m_entitiesMutex);

    // Clear all storage (behaviors are data in EDM, no cleanup needed)
    m_storage.handles.clear();
    m_storage.lastUpdateTimes.clear();
    m_storage.edmIndices.clear();

    m_handleToIndex.clear();
    m_edmToStorageIndex.clear();
    m_guardEdmIndices.clear();
    for (auto& fv : m_factionEdmIndices) fv.clear();
  }

  // Clear cached manager references
  mp_pathfinderManager = nullptr;

  // Reset all counters
  m_totalBehaviorExecutions.store(0, std::memory_order_relaxed);
  m_totalAssignmentCount.store(0, std::memory_order_relaxed);
  m_frameCounter.store(0, std::memory_order_relaxed);

  AI_INFO("AIManager shutdown complete");
}

void AIManager::prepareForStateTransition() {
  AI_INFO("Preparing AIManager for state transition...");

  // Pause AI processing to prevent new tasks
  m_globallyPaused.store(true, std::memory_order_release);
  VoidLight::AICommandBus::Instance().clearAll();
  m_pendingFactionChanges.clear();
  m_pendingBehaviorTransitions.clear();
  m_pendingBehaviorMessages.clear();
  m_pendingMeleeFallbackEquips.clear();
  m_pendingRangedAttacks.clear();

  // Batches always complete within update() — no pending futures to wait for.

  // Clean up all entities safely (behaviors are data in EDM, no cleanup needed)
  {
    std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

    size_t entityCount = m_storage.size();

    // Clear all storage completely
    m_storage.handles.clear();
    m_storage.lastUpdateTimes.clear();
    m_storage.edmIndices.clear();
    m_handleToIndex.clear();
    m_edmToStorageIndex.clear();
    m_activeIndicesBuffer.clear();
    m_guardEdmIndices.clear();
    for (auto& fv : m_factionEdmIndices) fv.clear();

    AI_INFO_IF(entityCount > 0,
               std::format("Cleaned {} AI entities", entityCount));
    AI_DEBUG("Cleaned up all entities for state transition");
  }

  // Reset all counters and stats
  m_totalBehaviorExecutions.store(0, std::memory_order_relaxed);
  m_totalAssignmentCount.store(0, std::memory_order_relaxed);
  m_frameCounter.store(0, std::memory_order_relaxed);

  // Clear player reference completely
  {
    std::lock_guard<std::shared_mutex> lock(m_entitiesMutex);
    m_playerHandle = EntityHandle{};
  }

  // Reset pause state to false so next state starts unpaused
  m_globallyPaused.store(false, std::memory_order_release);

  AI_INFO("AIManager state transition complete - all state cleared and reset");
}

void AIManager::update(float deltaTime) {
  if (!m_initialized.load(std::memory_order_acquire) ||
      m_globallyPaused.load(std::memory_order_acquire)) {
    return;
  }

  // Early exit if no AI-managed entities (e.g., just player with no NPCs)
  // This avoids all setup overhead when there's no behavior work to do
  if (m_storage.edmIndices.empty()) {
    return;
  }

  try {
    // Apply worker-computed path completions before behavior reads PathData.
    PathfinderManager::Instance().commitCompletedPaths();

    // Commit queued cross-thread commands before reading per-entity behavior state.
    // Order matters: faction changes keep indices coherent before scans;
    // equipment swaps update current combat stats before attack configs are read;
    // transitions first, then messages, so messages target current behavior.
    commitQueuedFactionChanges();
    commitQueuedMeleeFallbackEquips();
    commitQueuedBehaviorTransitions();
    commitQueuedBehaviorMessages();

    // Use getActiveIndices() to iterate only Active tier entities
    // This reduces iteration from 50K to ~468 (entities within active radius)
    auto &edm = EntityDataManager::Instance();
    auto activeSpan = edm.getActiveIndices();

    if (activeSpan.empty()) {
      return;
    }

    // Copy to local buffer (span may be invalidated during processing)
    // Reuse buffer to avoid per-frame allocation
    m_activeIndicesBuffer.clear();
    m_activeIndicesBuffer.insert(m_activeIndicesBuffer.end(),
                                 activeSpan.begin(), activeSpan.end());

    const size_t entityCount = m_activeIndicesBuffer.size();

    uint64_t currentFrame = m_frameCounter.load(std::memory_order_relaxed);

    // Invalidate spatial query cache for new frame
    // This ensures thread-local caches are fresh and don't use stale collision data
    AIInternal::InvalidateSpatialCache(currentFrame);
    VOIDLIGHT_DEBUG_ONLY(AIInternal::ResetCrowdStats();)

    // Query world bounds ONCE per frame (not per batch)
    float worldWidth = 32000.0f;
    float worldHeight = 32000.0f;
    if (mp_pathfinderManager) {
      float w, h;
      if (mp_pathfinderManager->getCachedWorldBounds(w, h) && w > 0 && h > 0) {
        worldWidth = w;
        worldHeight = h;
      }
    }

    // Cache world bounds for behaviors that need them during batch processing
    // (e.g., PatrolBehavior waypoint generation). Avoids WorldManager::Instance()
    // calls from worker threads.
    Behaviors::cacheWorldBounds();

    // Cache player info ONCE per frame (not per behavior call)
    // This eliminates shared_lock contention in getPlayerHandle()/getPlayerPosition()
    EntityHandle cachedPlayerHandle;
    Vector2D cachedPlayerPosition;
    Vector2D cachedPlayerVelocity;
    bool cachedPlayerValid = false;
    {
      std::shared_lock<std::shared_mutex> lock(m_entitiesMutex);
      cachedPlayerHandle = m_playerHandle;
      cachedPlayerValid = m_playerHandle.isValid();
      if (cachedPlayerValid) {
        size_t playerIdx = edm.getIndex(m_playerHandle);
        if (playerIdx != SIZE_MAX) {
          const auto &playerTransform = edm.getTransformByIndex(playerIdx);
          cachedPlayerPosition = playerTransform.position;
          cachedPlayerVelocity = playerTransform.velocity;
        }
      }
    }

    // Cache game time ONCE per frame for combat timing comparisons
    float cachedGameTime = GameTimeManager::Instance().getTotalGameTimeSeconds();

    // Cache player edmIndex once per frame (avoids hash lookup per query)
    m_cachedPlayerEdmIdx = m_playerHandle.isValid()
        ? edm.getIndex(m_playerHandle)
        : SIZE_MAX;

    // WorkerBudget manager — used per-type bucket below.
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& threadSystem = VoidLight::ThreadSystem::Instance();

    // Collect all deferred events across batches.
    m_allDamageEvents.clear();

    // Frame-level threading decision. WorkerBudget is the authoritative source:
    // one decision per frame so EMA / threshold learning sees the full workload.
    const auto frameDecision = budgetMgr.shouldUseThreading(
        VoidLight::SystemType::AI, entityCount);
    bool frameShouldThread = frameDecision.shouldThread;
    VOIDLIGHT_DEBUG_ONLY(
        if (!m_useThreading.load(std::memory_order_acquire)) {
            frameShouldThread = false;
        }
    )

    const size_t optimalWorkerCount = frameShouldThread
        ? budgetMgr.getOptimalWorkers(VoidLight::SystemType::AI, entityCount)
        : 0;

    // Frame-level batch execution timing.
    auto frameBatchStart = std::chrono::steady_clock::now();

    size_t totalBatchCount = 0;
    VOIDLIGHT_DEBUG_ONLY(
        bool logWasThreaded = false;
    )

    if (!frameShouldThread) {
        // Unified single-pass: one call covering all entities. Emotional decay
        // + behavior dispatch fused into the same loop inside processBatch.
        m_singleBatchEvents.clear();
        m_singleBatchKnockbackClears.clear();
        processBatch(m_activeIndicesBuffer, 0, entityCount, deltaTime,
                     worldWidth, worldHeight, cachedPlayerHandle,
                     cachedPlayerPosition, cachedPlayerVelocity,
                     cachedPlayerValid, cachedGameTime,
                     m_singleBatchEvents,
                     m_singleBatchKnockbackClears);
        if (!m_singleBatchEvents.empty()) {
            m_allDamageEvents.insert(m_allDamageEvents.end(),
                std::make_move_iterator(m_singleBatchEvents.begin()),
                std::make_move_iterator(m_singleBatchEvents.end()));
        }
        totalBatchCount = 1;
    } else {
        VOIDLIGHT_DEBUG_ONLY(logWasThreaded = true;)

        // Single global batch strategy against the full workload.
        auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
            VoidLight::SystemType::AI, entityCount, optimalWorkerCount);
        if (batchCount == 0) batchCount = 1;
        if (batchSize == 0) batchSize = 1;

        const size_t entitiesPerBatch = entityCount / batchCount;
        const size_t remainder        = entityCount % batchCount;
        totalBatchCount = batchCount;

        if (m_batchEventBuffers.size() < totalBatchCount) {
            m_batchEventBuffers.resize(totalBatchCount);
        }
        if (m_batchKnockbackClears.size() < totalBatchCount) {
            m_batchKnockbackClears.resize(totalBatchCount);
        }
        for (size_t i = 0; i < totalBatchCount; ++i) {
            m_batchEventBuffers[i].clear();
            m_batchKnockbackClears[i].clear();
        }

        m_batchFutures.clear();
        m_batchFutures.reserve(totalBatchCount);

        for (size_t i = 0; i < totalBatchCount; ++i) {
            const size_t bStart = i * entitiesPerBatch;
            const size_t bEnd   = bStart + entitiesPerBatch
                                 + (i == totalBatchCount - 1 ? remainder : 0);
            m_batchFutures.push_back(threadSystem.enqueueTaskWithResult(
                [this, i, bStart, bEnd, deltaTime, worldWidth, worldHeight,
                 cachedPlayerHandle, cachedPlayerPosition, cachedPlayerVelocity,
                 cachedPlayerValid, cachedGameTime]() {
                    processBatch(m_activeIndicesBuffer, bStart, bEnd, deltaTime,
                                 worldWidth, worldHeight, cachedPlayerHandle,
                                 cachedPlayerPosition, cachedPlayerVelocity,
                                 cachedPlayerValid, cachedGameTime,
                                 m_batchEventBuffers[i],
                                 m_batchKnockbackClears[i]);
                },
                VoidLight::TaskPriority::High, "AI_Batch"));
        }

        for (auto& future : m_batchFutures) {
            if (future.valid()) {
                future.get();
            }
        }

        for (size_t i = 0; i < totalBatchCount; ++i) {
            if (!m_batchEventBuffers[i].empty()) {
                m_allDamageEvents.insert(m_allDamageEvents.end(),
                    std::make_move_iterator(m_batchEventBuffers[i].begin()),
                    std::make_move_iterator(m_batchEventBuffers[i].end()));
            }
        }
    }

    // Drain knockback-expiry queues on the main thread. SparseSidecar::remove()
    // mutates shared m_dense/m_sparse via swap-pop and cannot be called from
    // worker threads. Workers enqueued edmIdx values during processBatch.
    {
        auto& edmDrain = EntityDataManager::Instance();
        for (uint32_t edmIdx : m_singleBatchKnockbackClears) {
            edmDrain.clearKnockback(edmIdx);
        }
        m_singleBatchKnockbackClears.clear();
        for (size_t i = 0; i < totalBatchCount && i < m_batchKnockbackClears.size(); ++i) {
            for (uint32_t edmIdx : m_batchKnockbackClears[i]) {
                edmDrain.clearKnockback(edmIdx);
            }
        }
    }

    // Deliver all deferred events collected across all type passes.
    if (!m_allDamageEvents.empty()) {
        EventManager::Instance().enqueueBatch(std::move(m_allDamageEvents));
    }

    auto frameBatchEnd = std::chrono::steady_clock::now();
    double totalUpdateTime = std::chrono::duration<double, std::milli>(
        frameBatchEnd - frameBatchStart).count();

    // Single frame-level reportExecution — matches the contract WorkerBudget's
    // EMA / threshold-learning assume (one sample per frame per system).
    budgetMgr.reportExecution(VoidLight::SystemType::AI,
        entityCount, frameShouldThread, totalBatchCount, totalUpdateTime);

    // Commit commands emitted by worker threads during this frame's batches.
    commitQueuedRangedAttacks();
    commitQueuedMeleeFallbackEquips();
    commitQueuedBehaviorTransitions();
    commitQueuedBehaviorMessages();

    m_frameCounter.fetch_add(1, std::memory_order_relaxed);

    VOIDLIGHT_DEBUG_ONLY(
        // Interval stats logging
        static thread_local uint64_t logFrameCounter = 0;
        if (++logFrameCounter % 1800 == 0 && entityCount > 0) {  // ~30 seconds at 60fps
            double entitiesPerSecond =
                totalUpdateTime > 0 ? (entityCount * 1000.0 / totalUpdateTime) : 0.0;
            const auto crowdStats = AIInternal::GetCrowdStats();
            double crowdHitRate =
                crowdStats.queryCount > 0
                    ? (100.0 * static_cast<double>(crowdStats.cacheHits) /
                       static_cast<double>(crowdStats.queryCount))
                    : 0.0;
            PathfinderManager::PathfinderStats pathStats{};
            if (mp_pathfinderManager) {
                pathStats = mp_pathfinderManager->getStats();
            }
            double pathHitRate = pathStats.cacheHitRate * 100.0;
            if (logWasThreaded) {
                AI_DEBUG(std::format(
                    "AI Summary - Active: {}, Update: {:.2f}ms, Throughput: {:.0f}/sec "
                    "[Threaded: {} total-batches] Crowd[q:{} hit:{:.0f}% res:{}] "
                    "Path[rps:{:.1f} hit:{:.0f}% cache:{}]",
                    entityCount, totalUpdateTime, entitiesPerSecond, totalBatchCount,
                    crowdStats.queryCount, crowdHitRate,
                    crowdStats.resultsCount, pathStats.requestsPerSecond, pathHitRate,
                    pathStats.cacheSize));
            } else {
                AI_DEBUG(std::format(
                    "AI Summary - Active: {}, Update: {:.2f}ms, Throughput: {:.0f}/sec "
                    "[Single-threaded] Crowd[q:{} hit:{:.0f}% res:{}] "
                    "Path[rps:{:.1f} hit:{:.0f}% cache:{}]",
                    entityCount, totalUpdateTime, entitiesPerSecond,
                    crowdStats.queryCount, crowdHitRate, crowdStats.resultsCount,
                    pathStats.requestsPerSecond, pathHitRate, pathStats.cacheSize));
            }
        }
    )

  } catch (const std::exception &e) {
    AI_ERROR(std::format("Exception in AIManager::update: {}", e.what()));
  }
}


void AIManager::registerDefaultBehaviors() {
  // Initialize behavior name-to-type map for API compatibility
  m_behaviorTypeMap = {
    {"Idle", BehaviorType::Idle},
    {"Wander", BehaviorType::Wander},
    {"Chase", BehaviorType::Chase},
    {"Patrol", BehaviorType::Patrol},
    {"Guard", BehaviorType::Guard},
    {"Attack", BehaviorType::Attack},
    {"Flee", BehaviorType::Flee},
    {"Follow", BehaviorType::Follow}
  };

  // Register named preset configs (variants of base behaviors)
  m_presetConfigs["SmallWander"] = VoidLight::BehaviorConfigData::makeWander(
      VoidLight::WanderBehaviorConfig::createSmallWander());
  m_presetConfigs["LargeWander"] = VoidLight::BehaviorConfigData::makeWander(
      VoidLight::WanderBehaviorConfig::createLargeWander());
  m_presetConfigs["EventWander"] = VoidLight::BehaviorConfigData::makeWander(
      VoidLight::WanderBehaviorConfig::createEventWander());
  m_presetConfigs["RandomPatrol"] = VoidLight::BehaviorConfigData::makePatrol(
      VoidLight::PatrolBehaviorConfig::createRandomPatrol());
  m_presetConfigs["CirclePatrol"] = VoidLight::BehaviorConfigData::makePatrol(
      VoidLight::PatrolBehaviorConfig::createCirclePatrol());
  m_presetConfigs["EventTarget"] = VoidLight::BehaviorConfigData::makeChase(
      VoidLight::ChaseBehaviorConfig::createEventTarget());
  m_presetConfigs["RangedAttack"] = VoidLight::BehaviorConfigData::makeAttack(
      VoidLight::AttackBehaviorConfig::createRangedConfig());

  AI_INFO("Behavior system ready (8 types + 7 presets)");
}

bool AIManager::hasBehavior(const std::string &name) const {
  // Check preset configs first, then base behavior types
  if (m_presetConfigs.find(name) != m_presetConfigs.end()) {
    return true;
  }
  auto it = m_behaviorTypeMap.find(name);
  return it != m_behaviorTypeMap.end();
}

void AIManager::assignBehavior(EntityHandle handle,
                               const std::string &behaviorName) {
  if (!handle.isValid()) {
    AI_ERROR("Cannot assign behavior to invalid handle");
    return;
  }

  // Check for preset config first (SmallWander, LargeWander, etc.)
  auto presetIt = m_presetConfigs.find(behaviorName);
  if (presetIt != m_presetConfigs.end()) {
    // Use preset config directly via the config-based overload
    assignBehavior(handle, presetIt->second);
    return;
  }

  // Fall back to default config for base behavior types
  auto typeIt = m_behaviorTypeMap.find(behaviorName);
  if (typeIt == m_behaviorTypeMap.end()) {
    AI_ERROR(std::format("Unknown behavior name: {}", behaviorName));
    return;
  }
  BehaviorType behaviorType = typeIt->second;

  // Get default config for this behavior type
  auto& edm = EntityDataManager::Instance();
  size_t edmIndex = edm.getIndex(handle);
  if (edmIndex == SIZE_MAX) {
    AI_ERROR("Cannot assign behavior: entity not in EDM");
    return;
  }

  auto config = Behaviors::getDefaultConfig(behaviorType);

  // For Attack behavior, customize config from CharacterData combat style
  const auto& hot = edm.getHotDataByIndex(edmIndex);
  if (behaviorType == BehaviorType::Attack &&
      (hot.kind == EntityKind::NPC || hot.kind == EntityKind::Player)) {
    const auto& charData = edm.getCharacterDataByIndex(edmIndex);
    if (charData.combatStyle == CharacterData::CombatStyle::Ranged) {
      config = VoidLight::BehaviorConfigData::makeAttack(
          VoidLight::AttackBehaviorConfig::createRangedConfig(charData.attackRange));
      config.params.attack.projectileSpeed = charData.projectileSpeed > 0.0f
          ? charData.projectileSpeed : 250.0f;
      config.params.attack.attackDamage = charData.attackDamage;
    } else {
      config = VoidLight::BehaviorConfigData::makeAttack(
          VoidLight::AttackBehaviorConfig::createMeleeConfig(charData.attackRange));
      config.params.attack.attackDamage = charData.attackDamage;
    }
  }

  if (!edm.hasMemoryData(edmIndex)) {
    edm.initMemoryData(edmIndex);
  }
  assert(edm.hasMemoryData(edmIndex) &&
         "Behavior-assigned AI entities must have valid memory data");

  // Acquire write lock — index updates and EDM config must be atomic
  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

  auto indexIt = m_handleToIndex.find(handle);
  if (indexIt != m_handleToIndex.end()) {
    // Update existing entity — remove old indices before overwriting config
    size_t index = indexIt->second;
    if (index < m_storage.size()) {
      BehaviorType oldType = edm.getBehaviorConfigRef(edmIndex).type;
      removeFromIndices(edmIndex, oldType);

      if (index < m_storage.edmIndices.size()) {
        m_storage.edmIndices[index] = edmIndex;
      }

      if (m_edmToStorageIndex.size() <= edmIndex) {
        m_edmToStorageIndex.resize(edmIndex + 1, SIZE_MAX);
      }
      m_edmToStorageIndex[edmIndex] = index;

      AI_INFO(std::format("Updated behavior for existing entity to: {}",
                          behaviorName));
    }
  } else {
    // Add new entity
    size_t newIndex = m_storage.size();

    m_storage.handles.push_back(handle);
    m_storage.lastUpdateTimes.push_back(0.0f);
    m_storage.edmIndices.push_back(edmIndex);

    if (m_edmToStorageIndex.size() <= edmIndex) {
      m_edmToStorageIndex.resize(edmIndex + 1, SIZE_MAX);
    }
    m_edmToStorageIndex[edmIndex] = newIndex;

    m_handleToIndex[handle] = newIndex;

    AI_INFO(std::format("Added new entity with behavior: {}", behaviorName));
  }

  // Set config in EDM and initialize state (after old indices removed)
  edm.reassignBehaviorConfig(edmIndex, config);
  Behaviors::init(edmIndex, config);

  // Add to guard/faction indices for the new behavior
  addToIndices(edmIndex, behaviorType);

  m_totalAssignmentCount.fetch_add(1, std::memory_order_relaxed);
}

void AIManager::assignBehavior(EntityHandle handle,
                               const VoidLight::BehaviorConfigData& config) {
  if (!handle.isValid()) {
    AI_ERROR("Cannot assign behavior to invalid handle");
    return;
  }
  if (config.type == BehaviorType::None) {
    AI_ERROR("Cannot assign behavior with type None");
    return;
  }

  auto& edm = EntityDataManager::Instance();
  size_t edmIndex = edm.getIndex(handle);
  if (edmIndex == SIZE_MAX) {
    AI_ERROR("Cannot assign behavior: entity not in EDM");
    return;
  }
  if (!edm.hasMemoryData(edmIndex)) {
    edm.initMemoryData(edmIndex);
  }
  assert(edm.hasMemoryData(edmIndex) &&
         "Behavior-assigned AI entities must have valid memory data");

  // Acquire write lock — index updates and EDM config must be atomic
  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

  auto indexIt = m_handleToIndex.find(handle);
  if (indexIt != m_handleToIndex.end()) {
    // Update existing entity — remove old indices before overwriting config
    size_t index = indexIt->second;
    if (index < m_storage.size()) {
      BehaviorType oldType = edm.getBehaviorConfigRef(edmIndex).type;
      removeFromIndices(edmIndex, oldType);

      if (index < m_storage.edmIndices.size()) {
        m_storage.edmIndices[index] = edmIndex;
      }
      if (m_edmToStorageIndex.size() <= edmIndex) {
        m_edmToStorageIndex.resize(edmIndex + 1, SIZE_MAX);
      }
      m_edmToStorageIndex[edmIndex] = index;

      AI_DEBUG(std::format("Assigned behavior directly: type={}",
                           static_cast<int>(config.type)));
    }
  } else {
    // Add new entity
    size_t newIndex = m_storage.size();

    m_storage.handles.push_back(handle);
    m_storage.lastUpdateTimes.push_back(0.0f);
    m_storage.edmIndices.push_back(edmIndex);

    if (m_edmToStorageIndex.size() <= edmIndex) {
      m_edmToStorageIndex.resize(edmIndex + 1, SIZE_MAX);
    }
    m_edmToStorageIndex[edmIndex] = newIndex;

    m_handleToIndex[handle] = newIndex;
    AI_INFO(std::format("Added new entity with behavior: type={}",
                        static_cast<int>(config.type)));
  }

  // Set config in EDM and initialize state (after old indices removed)
  edm.reassignBehaviorConfig(edmIndex, config);
  Behaviors::init(edmIndex, config);

  // Add to guard/faction indices for the new behavior
  addToIndices(edmIndex, config.type);

  m_totalAssignmentCount.fetch_add(1, std::memory_order_relaxed);
}

void AIManager::unassignBehavior(EntityHandle handle) {
  if (!handle.isValid())
    return;

  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

  auto it = m_handleToIndex.find(handle);
  if (it != m_handleToIndex.end()) {
    size_t index = it->second;
    if (index < m_storage.size()) {
      // Remove from indices then clear behavior config in EDM
      size_t edmIndex = m_storage.edmIndices[index];
      if (edmIndex != SIZE_MAX) {
        auto& edm = EntityDataManager::Instance();
        BehaviorType oldType = edm.getBehaviorConfigRef(edmIndex).type;
        removeFromIndices(edmIndex, oldType);
        edm.clearBehaviorConfig(edmIndex);

        if (edmIndex < m_edmToStorageIndex.size()) {
          m_edmToStorageIndex[edmIndex] = SIZE_MAX;
        }
      }
    }
  }
}

bool AIManager::hasBehavior(EntityHandle handle) const {
  if (!handle.isValid())
    return false;

  std::shared_lock<std::shared_mutex> lock(m_entitiesMutex);

  auto it = m_handleToIndex.find(handle);
  if (it != m_handleToIndex.end() && it->second < m_storage.size()) {
    // Check if entity has a valid behavior config in EDM
    size_t edmIndex = m_storage.edmIndices[it->second];
    if (edmIndex != SIZE_MAX) {
      const auto& edm = EntityDataManager::Instance();
      return edm.getBehaviorConfigRef(edmIndex).type != BehaviorType::None;
    }
  }

  return false;
}

void AIManager::setPlayerHandle(EntityHandle player) {
  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);
  m_playerHandle = player;
}

EntityHandle AIManager::getPlayerHandle() const {
  std::shared_lock<std::shared_mutex> lock(m_entitiesMutex);
  return m_playerHandle;
}

Vector2D AIManager::getPlayerPosition() const {
  std::shared_lock<std::shared_mutex> lock(m_entitiesMutex);
  if (m_playerHandle.isValid()) {
    auto &edm = EntityDataManager::Instance();
    size_t edmIndex = edm.getIndex(m_playerHandle);
    if (edmIndex != SIZE_MAX) {
      return edm.getTransformByIndex(edmIndex).position;
    }
  }
  return Vector2D{0.0f, 0.0f};
}

bool AIManager::isPlayerValid() const {
  std::shared_lock<std::shared_mutex> lock(m_entitiesMutex);
  return m_playerHandle.isValid();
}

void AIManager::unregisterEntity(EntityHandle handle) {
  if (!handle.isValid())
    return;

  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

  // Remove from indices and reverse mapping
  auto it = m_handleToIndex.find(handle);
  if (it != m_handleToIndex.end() && it->second < m_storage.size()) {
    size_t edmIndex = m_storage.edmIndices[it->second];
    if (edmIndex != SIZE_MAX) {
      auto& edm = EntityDataManager::Instance();
      BehaviorType oldType = edm.getBehaviorConfigRef(edmIndex).type;
      removeFromIndices(edmIndex, oldType);
      edm.clearBehaviorConfig(edmIndex);

      if (edmIndex < m_edmToStorageIndex.size()) {
        m_edmToStorageIndex[edmIndex] = SIZE_MAX;
      }
    }
  }
}

void AIManager::destroyAllNPCsForStateTransition() {
  auto& edm = EntityDataManager::Instance();
  auto npcIndices = edm.getIndicesByKind(EntityKind::NPC);

  for (size_t idx : npcIndices) {
    EntityHandle handle = edm.getHandle(idx);
    if (!handle.isValid()) {
      continue;
    }

    unregisterEntity(handle);
    edm.destroyEntity(handle);
  }
}

void AIManager::onEntityFactionChanged(size_t edmIndex, uint8_t oldFaction, uint8_t newFaction) {
  if (oldFaction >= MAX_FACTIONS || newFaction >= MAX_FACTIONS || oldFaction == newFaction) {
    return;
  }
  auto& edm = EntityDataManager::Instance();
  VoidLight::AICommandBus::Instance().enqueueFactionChange(
      edm.getHandle(edmIndex), edmIndex, oldFaction, newFaction);
}

void AIManager::applySocialInteraction(EntityHandle npcHandle,
                                       EntityHandle subjectHandle,
                                       SocialInteractionType interactionType,
                                       float value) {
  if (!npcHandle.isValid()) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  size_t idx = edm.getIndex(npcHandle);
  if (idx == SIZE_MAX || !edm.hasMemoryData(idx)) {
    return;
  }

  MemoryEntry entry;
  entry.subject = subjectHandle;
  entry.location = edm.getHotDataByIndex(idx).transform.position;
  entry.timestamp = GameTimeManager::Instance().getTotalGameTimeSeconds();
  entry.value = value;
  entry.type = MemoryType::Interaction;
  entry.flags = MemoryEntry::FLAG_VALID;

  float importance = std::abs(value) * 50.0f;
  switch (interactionType) {
    case SocialInteractionType::Gift:
      importance += 50.0f;
      break;
    case SocialInteractionType::Help:
      importance += 75.0f;
      break;
    case SocialInteractionType::Theft:
      importance = 200.0f;
      break;
    case SocialInteractionType::Trade:
      importance += 25.0f;
      break;
    case SocialInteractionType::Greeting:
    case SocialInteractionType::Insult:
      importance += 10.0f;
      break;
  }
  entry.importance = static_cast<uint8_t>(std::min(255.0f, importance));
  edm.addMemory(idx, entry);

  float aggression = 0.0f;
  float fear = 0.0f;
  float curiosity = 0.0f;
  float suspicion = 0.0f;

  switch (interactionType) {
    case SocialInteractionType::Trade:
      suspicion = -0.05f * (value > 0 ? 1.0f : -0.5f);
      break;
    case SocialInteractionType::Gift:
      suspicion = -0.15f;
      aggression = -0.1f;
      fear = -0.05f;
      break;
    case SocialInteractionType::Greeting:
      suspicion = -0.02f;
      break;
    case SocialInteractionType::Help:
      suspicion = -0.2f;
      aggression = -0.15f;
      fear = -0.1f;
      break;
    case SocialInteractionType::Theft:
      suspicion = 0.4f;
      aggression = 0.3f;
      fear = 0.1f;
      break;
    case SocialInteractionType::Insult:
      aggression = 0.2f;
      suspicion = 0.15f;
      break;
  }

  edm.modifyEmotions(idx, aggression, fear, curiosity, suspicion);
}

// Thread safety: m_activeIndicesBuffer and m_cachedPlayerEdmIdx are written on
// the main thread before batch futures are submitted. Futures create a
// happens-before edge, so worker threads read consistent data without a lock.
void AIManager::scanActiveIndicesInRadius(const Vector2D &center, float radius,
                                        std::vector<size_t> &outEdmIndices,
                                        bool excludePlayer) const {
  outEdmIndices.clear();
  const float radiusSq = radius * radius;
  auto &edm = EntityDataManager::Instance();
  for (size_t idx : m_activeIndicesBuffer) {
    float distSq = Vector2D::distanceSquared(
        center, edm.getHotDataByIndex(idx).transform.position);
    if (distSq <= radiusSq) {
      outEdmIndices.push_back(idx);
    }
  }

  // Filter player using cached edmIndex (no hash lookup)
  if (excludePlayer && m_cachedPlayerEdmIdx != SIZE_MAX) {
    std::erase(outEdmIndices, m_cachedPlayerEdmIdx);
  }
}

void AIManager::scanActiveHandlesInRadius(const Vector2D &center, float radius,
                                     std::vector<EntityHandle> &outHandles,
                                     bool excludePlayer) const {
  outHandles.clear();

  thread_local std::vector<size_t> t_edmBuffer;
  scanActiveIndicesInRadius(center, radius, t_edmBuffer, excludePlayer);

  auto &edm = EntityDataManager::Instance();
  outHandles.reserve(t_edmBuffer.size());
  std::transform(t_edmBuffer.begin(), t_edmBuffer.end(), std::back_inserter(outHandles),
                 [&edm](size_t edmIdx) { return edm.getHandle(edmIdx); });
}

void AIManager::scanGuardsInRadius(const Vector2D &center, float radius,
                                    std::vector<size_t> &outEdmIndices,
                                    bool excludePlayer) const {
  outEdmIndices.clear();
  const float radiusSq = radius * radius;
  auto &edm = EntityDataManager::Instance();

  for (size_t edmIdx : m_guardEdmIndices) {
    if (edmIdx >= m_edmToStorageIndex.size()) {
      continue;
    }
    const size_t storageIdx = m_edmToStorageIndex[edmIdx];
    if (storageIdx == SIZE_MAX || storageIdx >= m_storage.size()) {
      continue;
    }
    const auto& hotData = edm.getHotDataByIndex(edmIdx);
    if (!hotData.isAlive()) {
      continue;
    }
    float distSq = Vector2D::distanceSquared(center, hotData.transform.position);
    if (distSq <= radiusSq) {
      outEdmIndices.push_back(edmIdx);
    }
  }

  if (excludePlayer && m_cachedPlayerEdmIdx != SIZE_MAX) {
    std::erase(outEdmIndices, m_cachedPlayerEdmIdx);
  }
}

void AIManager::scanFactionInRadius(uint8_t faction, const Vector2D &center,
                                     float radius,
                                     std::vector<size_t> &outEdmIndices,
                                     bool excludePlayer) const {
  outEdmIndices.clear();
  if (faction >= MAX_FACTIONS) return;
  const float radiusSq = radius * radius;
  auto &edm = EntityDataManager::Instance();
  for (size_t edmIdx : m_factionEdmIndices[faction]) {
    if (edmIdx >= m_edmToStorageIndex.size()) {
      continue;
    }
    const size_t storageIdx = m_edmToStorageIndex[edmIdx];
    if (storageIdx == SIZE_MAX || storageIdx >= m_storage.size()) {
      continue;
    }
    const auto& hotData = edm.getHotDataByIndex(edmIdx);
    if (!hotData.isAlive()) {
      continue;
    }
    float distSq = Vector2D::distanceSquared(
        center, hotData.transform.position);
    if (distSq <= radiusSq) {
      outEdmIndices.push_back(edmIdx);
    }
  }
  if (excludePlayer && m_cachedPlayerEdmIdx != SIZE_MAX) {
    std::erase(outEdmIndices, m_cachedPlayerEdmIdx);
  }
}

void AIManager::addToIndices(size_t edmIndex, BehaviorType behaviorType) {
  // Guard index
  if (behaviorType == BehaviorType::Guard) {
    if (std::find(m_guardEdmIndices.begin(), m_guardEdmIndices.end(), edmIndex)
        == m_guardEdmIndices.end()) {
      m_guardEdmIndices.push_back(edmIndex);
    }
  }
  // Faction index
  auto &edm = EntityDataManager::Instance();
  uint8_t faction = edm.getCharacterDataByIndex(edmIndex).faction;
  if (faction < MAX_FACTIONS) {
    auto &factionVec = m_factionEdmIndices[faction];
    if (std::find(factionVec.begin(), factionVec.end(), edmIndex)
        == factionVec.end()) {
      factionVec.push_back(edmIndex);
    }
  }
}

void AIManager::removeFromIndices(size_t edmIndex, BehaviorType oldBehaviorType) {
  if (oldBehaviorType == BehaviorType::Guard) {
    std::erase(m_guardEdmIndices, edmIndex);
  }
  auto &edm = EntityDataManager::Instance();
  uint8_t faction = edm.getCharacterDataByIndex(edmIndex).faction;
  if (faction < MAX_FACTIONS) {
    std::erase(m_factionEdmIndices[faction], edmIndex);
  }
}

void AIManager::commitQueuedFactionChanges() {
  VoidLight::AICommandBus::Instance().drainFactionChanges(m_pendingFactionChanges);
  if (m_pendingFactionChanges.empty()) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

  for (const auto& cmd : m_pendingFactionChanges) {
    if (!cmd.targetHandle.isValid()) {
      continue;
    }

    const size_t edmIndex = edm.getIndex(cmd.targetHandle);
    if (edmIndex == SIZE_MAX || edmIndex != cmd.targetEdmIndex ||
        edmIndex >= m_edmToStorageIndex.size()) {
      continue;
    }

    const size_t storageIdx = m_edmToStorageIndex[edmIndex];
    if (storageIdx == SIZE_MAX || storageIdx >= m_storage.size()) {
      continue;
    }

    const uint8_t currentFaction = edm.getCharacterDataByIndex(edmIndex).faction;
    if (currentFaction >= MAX_FACTIONS) {
      continue;
    }

    for (auto& factionVec : m_factionEdmIndices) {
      std::erase(factionVec, edmIndex);
    }

    auto& targetFaction = m_factionEdmIndices[currentFaction];
    if (std::find(targetFaction.begin(), targetFaction.end(), edmIndex) == targetFaction.end()) {
      targetFaction.push_back(edmIndex);
    }
  }
}

void AIManager::commitQueuedMeleeFallbackEquips() {
  VoidLight::AICommandBus::Instance().drainMeleeFallbackEquips(
      m_pendingMeleeFallbackEquips);
  if (m_pendingMeleeFallbackEquips.empty()) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  std::sort(m_pendingMeleeFallbackEquips.begin(),
            m_pendingMeleeFallbackEquips.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.targetEdmIndex != rhs.targetEdmIndex) {
                return lhs.targetEdmIndex < rhs.targetEdmIndex;
              }
              return lhs.sequence > rhs.sequence;
            });

  size_t i = 0;
  while (i < m_pendingMeleeFallbackEquips.size()) {
    const auto& cmd = m_pendingMeleeFallbackEquips[i];
    const size_t edmIndex = cmd.targetEdmIndex;

    size_t runEnd = i + 1;
    while (runEnd < m_pendingMeleeFallbackEquips.size() &&
           m_pendingMeleeFallbackEquips[runEnd].targetEdmIndex == edmIndex) {
      ++runEnd;
    }

    if (cmd.targetHandle.isValid()) {
      const size_t resolvedEdmIndex = edm.getIndex(cmd.targetHandle);
      if (resolvedEdmIndex != SIZE_MAX && resolvedEdmIndex == edmIndex) {
        const bool equippedFallback =
            equipFirstAvailableMeleeWeapon(edm, cmd.targetHandle);
        AI_DEBUG_IF(!equippedFallback,
                    "No melee fallback weapon available for ranged attacker");
      }
    }

    i = runEnd;
  }
}

void AIManager::commitQueuedRangedAttacks() {
  VoidLight::AICommandBus::Instance().drainRangedAttacks(m_pendingRangedAttacks);
  if (m_pendingRangedAttacks.empty()) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  std::sort(m_pendingRangedAttacks.begin(), m_pendingRangedAttacks.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.attackerEdmIndex != rhs.attackerEdmIndex) {
                return lhs.attackerEdmIndex < rhs.attackerEdmIndex;
              }
              return lhs.sequence < rhs.sequence;
            });

  for (const auto& cmd : m_pendingRangedAttacks) {
    if (!cmd.attackerHandle.isValid() || cmd.projectileSpeed <= 0.0f) {
      continue;
    }

    const size_t resolvedEdmIndex = edm.getIndex(cmd.attackerHandle);
    if (resolvedEdmIndex == SIZE_MAX ||
        resolvedEdmIndex != cmd.attackerEdmIndex) {
      continue;
    }

    Vector2D direction = cmd.targetPos - cmd.attackerPos;
    const float dist = direction.length();
    if (dist < 1.0f) {
      continue;
    }
    direction = direction * (1.0f / dist);

    InventoryResourceChange ammoChange{};
    if (!edm.consumeRequiredAmmoForRangedAttack(cmd.attackerHandle,
                                                &ammoChange)) {
      const bool equippedFallback =
          equipFirstAvailableMeleeWeapon(edm, cmd.attackerHandle);
      VoidLight::AICommandBus::Instance().enqueueBehaviorMessage(
          cmd.attackerHandle, resolvedEdmIndex,
          BehaviorMessage::RANGED_ATTACK_FAILED);
      AI_DEBUG_IF(!equippedFallback,
                  "No melee fallback weapon available for ranged attacker");
      continue;
    }

    dispatchResourceChange(cmd.attackerHandle, ammoChange, "ammo_consumed");

    const Vector2D spawnPos =
        cmd.attackerPos + direction * NPC_PROJECTILE_SPAWN_OFFSET;
    const Vector2D velocity = direction * cmd.projectileSpeed;
    const float lifetime = (cmd.attackRange / cmd.projectileSpeed) + 0.5f;
    const EntityHandle projectile =
        edm.createProjectile(spawnPos, velocity, cmd.attackerHandle,
                             cmd.damage, lifetime);
    if (!projectile.isValid()) {
      VoidLight::AICommandBus::Instance().enqueueBehaviorMessage(
          cmd.attackerHandle, resolvedEdmIndex,
          BehaviorMessage::RANGED_ATTACK_FAILED);
    }
  }
}

void AIManager::commitQueuedBehaviorMessages() {
  VoidLight::AICommandBus::Instance().drainBehaviorMessages(m_pendingBehaviorMessages);
  if (m_pendingBehaviorMessages.empty()) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  size_t droppedCount = 0;
  std::sort(m_pendingBehaviorMessages.begin(), m_pendingBehaviorMessages.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.targetEdmIndex != rhs.targetEdmIndex) {
                return lhs.targetEdmIndex < rhs.targetEdmIndex;
              }
              return lhs.sequence < rhs.sequence;
            });

  size_t i = 0;
  while (i < m_pendingBehaviorMessages.size()) {
    const size_t edmIndex = m_pendingBehaviorMessages[i].targetEdmIndex;
    size_t runEnd = i + 1;
    while (runEnd < m_pendingBehaviorMessages.size() &&
           m_pendingBehaviorMessages[runEnd].targetEdmIndex == edmIndex) {
      ++runEnd;
    }

    if (edmIndex == SIZE_MAX || !edm.hasBehaviorData(edmIndex)) {
      i = runEnd;
      continue;
    }

    auto& data = edm.getBehaviorData(edmIndex);
    std::array<PendingBehaviorMessageCandidate,
               MAX_COMPACTED_BEHAVIOR_MESSAGES> candidates{};
    size_t candidateCount = 0;

    // Existing inbox entries are always older than commands drained this frame.
    for (uint8_t msgIdx = 0; msgIdx < data.pendingMessageCount; ++msgIdx) {
      upsertPendingBehaviorCandidate(
          candidates, candidateCount,
          {data.pendingMessages[msgIdx].messageId,
           data.pendingMessages[msgIdx].param,
           static_cast<uint64_t>(msgIdx)});
    }

    const uint64_t sequenceBias = static_cast<uint64_t>(data.pendingMessageCount);
    for (size_t runIdx = i; runIdx < runEnd; ++runIdx) {
      const auto& cmd = m_pendingBehaviorMessages[runIdx];
      if (!cmd.targetHandle.isValid()) {
        continue;
      }

      const size_t resolvedEdmIndex = edm.getIndex(cmd.targetHandle);
      if (resolvedEdmIndex == SIZE_MAX || resolvedEdmIndex != edmIndex) {
        continue;
      }

      upsertPendingBehaviorCandidate(
          candidates, candidateCount,
          {cmd.messageId, cmd.param, cmd.sequence + sequenceBias});
    }

    if (candidateCount == 0) {
      i = runEnd;
      continue;
    }

    sortPendingBehaviorCandidates(candidates, candidateCount);

    size_t firstKept = 0;
    if (candidateCount > MAX_PENDING_BEHAVIOR_MESSAGES) {
      droppedCount += candidateCount - MAX_PENDING_BEHAVIOR_MESSAGES;
      firstKept = candidateCount - MAX_PENDING_BEHAVIOR_MESSAGES;
    }

    data.pendingMessageCount = 0;
    for (size_t candidateIdx = firstKept; candidateIdx < candidateCount;
         ++candidateIdx) {
      data.pendingMessages[data.pendingMessageCount].messageId =
          candidates[candidateIdx].messageId;
      data.pendingMessages[data.pendingMessageCount].param =
          candidates[candidateIdx].param;
      data.pendingMessageCount++;
    }

    i = runEnd;
  }

  if (droppedCount > 0) {
    AI_WARN(std::format("Behavior message queue overflow: dropped {} messages", droppedCount));
  }
}

void AIManager::commitQueuedBehaviorTransitions() {
  VoidLight::AICommandBus::Instance().drainBehaviorTransitions(m_pendingBehaviorTransitions);
  if (m_pendingBehaviorTransitions.empty()) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  std::unique_lock<std::shared_mutex> lock(m_entitiesMutex);

  // Coalesce to one transition per target for this commit pass.
  // Multiple worker threads can enqueue conflicting transitions for the same
  // entity in one frame. Resolve by latest logical enqueue sequence.
  m_selectedTransitions.clear();
  m_selectedTransitions.reserve(m_pendingBehaviorTransitions.size());
  m_selectedTransitionsByEdmIndex.clear();
  m_selectedTransitionsByEdmIndex.reserve(m_pendingBehaviorTransitions.size());

  for (const auto& cmd : m_pendingBehaviorTransitions) {
    if (!cmd.targetHandle.isValid()) {
      continue;
    }

    const size_t resolvedEdmIndex = edm.getIndex(cmd.targetHandle);
    if (resolvedEdmIndex == SIZE_MAX || resolvedEdmIndex != cmd.targetEdmIndex ||
        resolvedEdmIndex >= m_edmToStorageIndex.size()) {
      continue;
    }

    auto it = m_selectedTransitionsByEdmIndex.find(resolvedEdmIndex);
    if (it == m_selectedTransitionsByEdmIndex.end()) {
      m_selectedTransitionsByEdmIndex.emplace(resolvedEdmIndex, m_selectedTransitions.size());
      m_selectedTransitions.push_back(cmd);
      continue;
    }

    auto& existing = m_selectedTransitions[it->second];
    if (cmd.sequence > existing.sequence) {
      existing = cmd;
    }
  }

  std::sort(m_selectedTransitions.begin(), m_selectedTransitions.end(),
            [](const auto& a, const auto& b) {
              return a.targetEdmIndex < b.targetEdmIndex;
            });

  for (const auto& cmd : m_selectedTransitions) {
    if (!cmd.targetHandle.isValid()) {
      continue;
    }

    const size_t edmIndex = edm.getIndex(cmd.targetHandle);
    if (edmIndex == SIZE_MAX || edmIndex != cmd.targetEdmIndex ||
        edmIndex >= m_edmToStorageIndex.size()) {
      continue;
    }

    size_t storageIdx = m_edmToStorageIndex[edmIndex];
    if (storageIdx == SIZE_MAX || storageIdx >= m_storage.size()) {
      continue;
    }

    const auto oldRef = edm.getBehaviorConfigRef(edmIndex);
    removeFromIndices(edmIndex, oldRef.type);

    edm.clearBehaviorData(edmIndex);
    edm.reassignBehaviorConfig(edmIndex, cmd.config);
    Behaviors::init(edmIndex, cmd.config);

    addToIndices(edmIndex, cmd.config.type);
  }
}

void AIManager::setGlobalPause(bool paused) {
  m_globallyPaused.store(paused, std::memory_order_release);
  AI_INFO((paused ? "AI processing paused" : "AI processing resumed"));
}

bool AIManager::isGloballyPaused() const {
  return m_globallyPaused.load(std::memory_order_acquire);
}

void AIManager::resetBehaviors() {
  AI_INFO("Resetting all AI behaviors");

  std::unique_lock<std::shared_mutex> entitiesLock(m_entitiesMutex);

  // Clear all data (behaviors are data in EDM, no cleanup needed)
  m_storage.handles.clear();
  m_storage.lastUpdateTimes.clear();
  m_storage.edmIndices.clear();
  m_handleToIndex.clear();
  m_edmToStorageIndex.clear();
  m_guardEdmIndices.clear();
  for (auto& fv : m_factionEdmIndices) fv.clear();

  // Reset counters
  m_totalBehaviorExecutions.store(0, std::memory_order_relaxed);
}

#ifndef NDEBUG
void AIManager::enableThreading(bool enable) {
  m_useThreading.store(enable, std::memory_order_release);
  AI_INFO(std::format("Threading {}", enable ? "enabled" : "disabled"));
}
#endif

size_t AIManager::getBehaviorCount() const {
  // m_behaviorTypeMap is immutable after init() — no lock needed
  return m_behaviorTypeMap.size();
}

size_t AIManager::getBehaviorUpdateCount() const {
  return m_totalBehaviorExecutions.load(std::memory_order_relaxed);
}

size_t AIManager::getTotalAssignmentCount() const {
  return m_totalAssignmentCount.load(std::memory_order_relaxed);
}

void AIManager::processBatch(
                             const std::vector<size_t> &activeIndices,
                             size_t start, size_t end, float deltaTime,
                             float worldWidth, float worldHeight,
                             EntityHandle playerHandle,
                             const Vector2D &playerPos,
                             const Vector2D &playerVel, bool playerValid,
                             float gameTime,
                             std::vector<EventManager::DeferredEvent> &outEvents,
                             std::vector<uint32_t> &outKnockbackClears) {
  // Process batch of Active tier entities using EDM indices directly.
  // Emotional decay and behavior dispatch are fused into a single pass so Debug
  // builds touch each entity's hot data once per frame.
  size_t batchExecutions = 0;
  auto &edm = EntityDataManager::Instance();

  // No lock needed: m_edmToStorageIndex is read-only during batch window
  // - Behavior assignments happen synchronously via assignBehavior() before
  // batch processing
  // - Entity removals only mark inactive (don't modify vector structure)

  auto updateMovementScalar = [&](TransformData &transform,
                                  const EntityHotData &edmHotData) {
    Vector2D pos = transform.position + (transform.velocity * deltaTime);

    float halfW = edmHotData.halfWidth;
    float halfH = edmHotData.halfHeight;
    float minX = halfW;
    float maxX = worldWidth - halfW;
    float minY = halfH;
    float maxY = worldHeight - halfH;
    if (maxX < minX) {
      minX = worldWidth * 0.5f;
      maxX = minX;
    }
    if (maxY < minY) {
      minY = worldHeight * 0.5f;
      maxY = minY;
    }
    Vector2D clamped(std::clamp(pos.getX(), minX, maxX),
                     std::clamp(pos.getY(), minY, maxY));
    transform.position = clamped;

    if (clamped.getX() != pos.getX()) {
      transform.velocity.setX(0.0f);
    }
    if (clamped.getY() != pos.getY()) {
      transform.velocity.setY(0.0f);
    }
  };

  std::array<TransformData *, 4> batchTransforms{};
  std::array<const EntityHotData *, 4> batchHotData{};
  size_t batchCount = 0;

  auto flushMovementBatch = [&]() {
    if (batchCount == 0) {
      return;
    }
    if (batchCount < 4) {
      for (size_t lane = 0; lane < batchCount; ++lane) {
        updateMovementScalar(*batchTransforms[lane], *batchHotData[lane]);
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

    for (size_t lane = 0; lane < 4; ++lane) {
      TransformData *transform = batchTransforms[lane];
      const EntityHotData *hotData = batchHotData[lane];
      posX[lane] = transform->position.getX();
      posY[lane] = transform->position.getY();
      velX[lane] = transform->velocity.getX();
      velY[lane] = transform->velocity.getY();
      float laneMinX = hotData->halfWidth;
      float laneMaxX = worldWidth - hotData->halfWidth;
      float laneMinY = hotData->halfHeight;
      float laneMaxY = worldHeight - hotData->halfHeight;
      if (laneMaxX < laneMinX) {
        laneMinX = worldWidth * 0.5f;
        laneMaxX = laneMinX;
      }
      if (laneMaxY < laneMinY) {
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

    for (size_t lane = 0; lane < 4; ++lane) {
      TransformData *transform = batchTransforms[lane];
      transform->position.setX(posX[lane]);
      transform->position.setY(posY[lane]);

      if ((clampXMask >> lane) & 0x1) {
        transform->velocity.setX(0.0f);
      }
      if ((clampYMask >> lane) & 0x1) {
        transform->velocity.setY(0.0f);
      }
    }

    batchCount = 0;
  };

  // Unified single pass: emotional decay + behavior dispatch + SIMD movement
  // accumulation fused into one traversal of activeIndices[start, end). Each
  // entity's hot data is touched exactly once per frame.
  for (size_t i = start; i < end && i < activeIndices.size(); ++i) {
    size_t edmIdx = activeIndices[i];

    edm.updateEmotionalDecay(edmIdx, deltaTime);

    // Get storage index from reverse mapping - O(1) lookup, no atomic overhead
    if (edmIdx >= m_edmToStorageIndex.size()) {
      continue; // No behavior registered for this entity (e.g., Player)
    }
    size_t storageIdx = m_edmToStorageIndex[edmIdx];
    if (storageIdx == SIZE_MAX || storageIdx >= m_storage.size()) {
      continue; // No behavior assigned for this entity
    }

    if (!edm.hasBehaviorData(edmIdx)) {
      continue;
    }
    const auto ref = edm.getBehaviorConfigRef(edmIdx);
    if (ref.type == BehaviorType::None || ref.type == BehaviorType::COUNT) {
      continue;
    }

    auto &edmHotData = edm.getHotDataByIndex(edmIdx);
    auto &transform = edmHotData.transform;
    auto &behaviorData = edm.getBehaviorData(edmIdx);
    PathData *pathData = &edm.getPathData(edmIdx);

    assert(edm.hasMemoryData(edmIdx) &&
           "Behavior-executed entities must have initialized memory data");
    auto &memoryData = edm.getMemoryData(edmIdx);

    const CharacterData &characterData = edm.getCharacterDataByIndex(edmIdx);

    // Store previous position for interpolation
    transform.previousPosition = transform.position;

    BehaviorContext ctx(
        transform, edmHotData, m_storage.handles[storageIdx].getId(), edmIdx,
        deltaTime, playerHandle, playerPos, playerVel, playerValid,
        behaviorData, pathData, memoryData, characterData,
        0.0f, 0.0f, worldWidth, worldHeight, true, gameTime,
        edm.knockbackSidecar());

    switch (ref.type) {
      case BehaviorType::Idle:
        Behaviors::executeIdle(ctx, edm.getIdleConfig(ref.index), edm.getIdleState(ref.index));
        break;
      case BehaviorType::Wander:
        Behaviors::executeWander(ctx, edm.getWanderConfig(ref.index), edm.getWanderState(ref.index));
        break;
      case BehaviorType::Chase:
        Behaviors::executeChase(ctx, edm.getChaseConfig(ref.index), edm.getChaseState(ref.index));
        break;
      case BehaviorType::Patrol:
        Behaviors::executePatrol(ctx, edm.getPatrolConfig(ref.index), edm.getPatrolState(ref.index));
        break;
      case BehaviorType::Guard:
        Behaviors::executeGuard(ctx, edm.getGuardConfig(ref.index), edm.getGuardState(ref.index));
        break;
      case BehaviorType::Attack:
      {
        auto attackConfig = edm.getAttackConfig(ref.index);
        attackConfig.attackDamage = characterData.attackDamage;
        attackConfig.attackRange = characterData.attackRange;
        attackConfig.attackMode =
            (characterData.combatStyle == CharacterData::CombatStyle::Ranged)
                ? 1
                : 0;
        if (characterData.projectileSpeed > 0.0f) {
          attackConfig.projectileSpeed = characterData.projectileSpeed;
        }
        Behaviors::executeAttack(ctx, attackConfig, edm.getAttackState(ref.index));
        break;
      }
      case BehaviorType::Flee:
        Behaviors::executeFlee(ctx, edm.getFleeConfig(ref.index), edm.getFleeState(ref.index));
        break;
      case BehaviorType::Follow:
        Behaviors::executeFollow(ctx, edm.getFollowConfig(ref.index), edm.getFollowState(ref.index));
        break;
      default:
        break;
    }

    // Frame 1 (first tick after the hit): REPLACE behavior velocity with the
    // impulse scaled by KICK_MULTIPLIER so chase/attack velocity can't swallow
    // the shove — a pure `+=` at the base 30 px/s impulse reads as "no knockback"
    // against ~100 px/s chase. Frames 2..N: ADD the decayed impulse on top of
    // the behavior-produced velocity so the NPC resumes chasing immediately and
    // the tail stays invisible — a pure `=` drained velocity to ~0 over 8 frames
    // and read as a slow slide.
    //
    // Gated by a single sparse-array load — entities without knockback pay nothing.
    // Expiry is deferred to the main thread via outKnockbackClears: SparseSidecar::remove()
    // performs swap-pop on shared vectors and patches the displaced entity's m_sparse slot,
    // so it is not race-safe across worker threads.
    if (auto* kb = ctx.knockback.get(static_cast<uint32_t>(edmIdx)))
    {
      constexpr float KICK_MULTIPLIER = 6.0f;
      // Cap frame-1 velocity so a fast projectile, a low-mass NPC, or an
      // accumulated restrike can't shove the target across the screen. The
      // impulse direction is preserved; only the magnitude is clamped.
      constexpr float MAX_KICK_SPEED = 250.0f;
      constexpr float MAX_KICK_SPEED_SQ = MAX_KICK_SPEED * MAX_KICK_SPEED;
      if (kb->justApplied)
      {
        kb->justApplied = false;
        float kickX = kb->impulseX * KICK_MULTIPLIER;
        float kickY = kb->impulseY * KICK_MULTIPLIER;
        const float magSq = kickX * kickX + kickY * kickY;
        if (magSq > MAX_KICK_SPEED_SQ)
        {
          const float scale = MAX_KICK_SPEED / std::sqrt(magSq);
          kickX *= scale;
          kickY *= scale;
          // Float rounding on `x * (cap/x)` can leave |kick*| a few ULPs above
          // the cap (e.g. 250.000015). Clamp components defensively so the
          // post-clamp magnitude is bounded by MAX_KICK_SPEED on either axis.
          kickX = std::clamp(kickX, -MAX_KICK_SPEED, MAX_KICK_SPEED);
          kickY = std::clamp(kickY, -MAX_KICK_SPEED, MAX_KICK_SPEED);
        }
        transform.velocity.setX(kickX);
        transform.velocity.setY(kickY);
      }
      else
      {
        transform.velocity.setX(transform.velocity.getX() + kb->impulseX);
        transform.velocity.setY(transform.velocity.getY() + kb->impulseY);
      }
      kb->impulseX *= Knockback::DECAY;
      kb->impulseY *= Knockback::DECAY;
      if (--kb->framesRemaining == 0)
      {
        outKnockbackClears.push_back(static_cast<uint32_t>(edmIdx));
      }
    }

    batchTransforms[batchCount] = &transform;
    batchHotData[batchCount] = &edmHotData;
    ++batchCount;
    if (batchCount == 4) {
      flushMovementBatch();
    }

    ++batchExecutions;
  }

  flushMovementBatch();

  if (batchExecutions > 0) {
    m_totalBehaviorExecutions.fetch_add(batchExecutions,
                                        std::memory_order_relaxed);
  }

  // Collect deferred events from this batch's thread-local buffers into caller's vector
  Behaviors::collectDeferredDamageEvents(outEvents);
}

int AIManager::getEntityPriority(EntityHandle handle) const {
  if (!handle.isValid())
    return DEFAULT_PRIORITY;

  // Read priority from EDM CharacterData (single source of truth)
  auto &edm = EntityDataManager::Instance();
  size_t edmIndex = edm.getIndex(handle);
  if (edmIndex != SIZE_MAX) {
    const auto &charData = edm.getCharacterDataByIndex(edmIndex);
    return charData.priority;
  }
  return DEFAULT_PRIORITY;
}

float AIManager::getUpdateRangeMultiplier(int priority) const {
  // Higher priority = larger update range multiplier
  return 1.0f + (std::max(0, std::min(9, priority)) * 0.1f);
}

void AIManager::registerEntity(EntityHandle handle,
                               const std::string &behaviorName) {
  // Assign behavior directly - no queue delay
  assignBehavior(handle, behaviorName);
}

AIManager::~AIManager() {
  if (!m_isShutdown) {
    clean();
  }
}

PathfinderManager &AIManager::getPathfinderManager() const {
  return PathfinderManager::Instance();
}
