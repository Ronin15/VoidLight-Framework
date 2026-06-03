/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef AI_MANAGER_HPP
#define AI_MANAGER_HPP

/**
 * @file AIManager.hpp
 * @brief High-performance AI data processor with WorkerBudget-driven threading
 *
 * AIManager is a pure data processor — it orchestrates behavior execution,
 * movement integration, and event collection without implementing AI logic:
 * - ThreadSystem and WorkerBudget integration for adaptive batch scaling
 * - SIMD movement batching (4-wide position + velocity + world clamping)
 * - Deferred event collection via thread-local buffers + enqueueBatch()
 * - Cache-friendly SoA layout with EDM as single source of truth
 * - Scales to 10k+ entities while maintaining 60+ FPS
 */

#include "ai/BehaviorConfig.hpp"
#include "ai/AICommandBus.hpp"
#include "entities/Entity.hpp"
#include "entities/EntityHandle.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include <array>
#include <atomic>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// PathfinderManager available for centralized pathfinding services
class PathfinderManager;

// BehaviorType enum is defined in EntityDataManager.hpp

/**
 * @brief High-performance AI data processor
 *
 * Orchestrates behavior execution and movement integration.
 * AI decision logic lives in Behaviors:: namespace, not here.
 */
class AIManager {
public:
  enum class SocialInteractionType : uint8_t {
    Trade,
    Gift,
    Greeting,
    Help,
    Theft,
    Insult
  };

  static AIManager &Instance() {
    static AIManager instance;
    return instance;
  }

  /**
   * @brief Initializes the AI Manager and its internal systems
   * @return true if initialization successful, false otherwise
   */
  bool init();

  /**
   * @brief Checks if the AI Manager has been initialized
   * @return true if initialized, false otherwise
   */
  bool isInitialized() const {
    return m_initialized.load(std::memory_order_acquire);
  }

  /**
   * @brief Cleans up all AI resources and marks manager as shut down
   */
  void clean();

  /**
   * @brief Prepares for state transition by safely cleaning up entities
   * @details Call this before exit() in game states to avoid deadlocks
   */
  void prepareForStateTransition();

  /**
   * @brief Updates all active AI entities
   *
   * Data processing pipeline:
   * 1. Gather active indices from EDM tier system
   * 2. Cache per-frame data (player, world bounds, game time)
   * 3. WorkerBudget-driven batch execution (single or multi-threaded)
   * 4. SIMD movement integration with world clamping
   * 5. Collect deferred events and submit via enqueueBatch()
   *
   * @param deltaTime Time elapsed since last update in seconds
   */
  void update(float deltaTime);

  /**
   * @brief Checks if AIManager has been shut down
   * @return true if manager is shut down, false otherwise
   */
  bool isShutdown() const { return m_isShutdown; }

  /**
   * @brief Registers all standard behavior types (Idle, Wander, Chase, Guard, Attack, Flee, Follow, Patrol)
   * @details Called by GameEngine after init(). Sets up m_behaviorTypeMap for name->type lookup.
   */
  void registerDefaultBehaviors();

  /**
   * @brief Checks if a behavior name is registered
   * @param name Name of the behavior to check
   * @return true if behavior name is known, false otherwise
   */
  bool hasBehavior(const std::string &name) const;

  /**
   * @brief Assigns a behavior to an entity by name
   * @param handle Entity to assign behavior to
   * @param behaviorName Name of the behavior (e.g., "Idle", "Attack")
   */
  void assignBehavior(EntityHandle handle, const std::string &behaviorName);

  /**
   * @brief Assigns a behavior to an entity with custom config
   * @param handle Entity to assign behavior to
   * @param config Behavior configuration (includes type)
   */
  void assignBehavior(EntityHandle handle, const VoidLight::BehaviorConfigData& config);

  /**
   * @brief Removes behavior assignment from an entity
   */
  void unassignBehavior(EntityHandle handle);

  /**
   * @brief Checks if an entity has an assigned behavior
   */
  bool hasBehavior(EntityHandle handle) const;


  // Player handle for AI targeting
  void setPlayerHandle(EntityHandle player);
  EntityHandle getPlayerHandle() const;
  Vector2D getPlayerPosition() const;
  bool isPlayerValid() const;

  // Entity registration (requires behavior - use assignBehavior() or registerEntity with behavior)
  void registerEntity(EntityHandle handle, const std::string &behaviorName);
  void unregisterEntity(EntityHandle handle);
  void destroyAllNPCsForStateTransition();
  void onEntityFactionChanged(size_t edmIndex, uint8_t oldFaction, uint8_t newFaction);
  void applySocialInteraction(EntityHandle npcHandle, EntityHandle subjectHandle,
                              SocialInteractionType interactionType,
                              float value);

  /**
   * @brief Linear scan of active entities returning handles within radius (O(N))
   */
  void scanActiveHandlesInRadius(const Vector2D& center, float radius,
                                 std::vector<EntityHandle>& outHandles,
                                 bool excludePlayer = true) const;

  /**
   * @brief Linear scan of active entities returning EDM indices within radius (O(N))
   * Preferred API for behavior code - returns edmIndices directly, avoiding
   * redundant getIndex(handle) lookups at call sites.
   */
  void scanActiveIndicesInRadius(const Vector2D& center, float radius,
                                 std::vector<size_t>& outEdmIndices,
                                 bool excludePlayer = true) const;

  /**
   * @brief Scan only guard entities within radius (O(G) where G = guard count)
   * Uses incrementally maintained guard index — no per-frame rebuild.
   */
  void scanGuardsInRadius(const Vector2D& center, float radius,
                          std::vector<size_t>& outEdmIndices,
                          bool excludePlayer = true) const;

  /**
   * @brief Scan only same-faction entities within radius (O(F) where F = faction size)
   * Uses incrementally maintained faction index — no per-frame rebuild.
   */
  void scanFactionInRadius(uint8_t faction, const Vector2D& center, float radius,
                           std::vector<size_t>& outEdmIndices,
                           bool excludePlayer = true) const;

  // Global controls
  void setGlobalPause(bool paused);
  bool isGloballyPaused() const;

  // Priority from EDM CharacterData
  int getEntityPriority(EntityHandle handle) const;
  float getUpdateRangeMultiplier(int priority) const;
  static constexpr int AI_MIN_PRIORITY = 0;
  static constexpr int AI_MAX_PRIORITY = 9;
  static constexpr int DEFAULT_PRIORITY = 5;
  void resetBehaviors();

#ifndef NDEBUG
  // Threading configuration (benchmarking only - compiles out in release)
  void enableThreading(bool enable);
#endif

  // Performance monitoring
  size_t getBehaviorCount() const;
  size_t getBehaviorUpdateCount() const;

  // Thread-safe assignment tracking (atomic counter only)
  size_t getTotalAssignmentCount() const;

  /**
   * @brief Get direct access to PathfinderManager for optimal pathfinding
   * performance
   * @return Reference to PathfinderManager instance
   * @details Provides access to centralized pathfinding service for all AI
   * entities
   *
   * All pathfinding functionality has been moved to PathfinderManager.
   * Use PathfinderManager::Instance() to access pathfinding services.
   */
  PathfinderManager &getPathfinderManager() const;

private:
  AIManager() = default;
  ~AIManager();
  AIManager(const AIManager &) = delete;
  AIManager &operator=(const AIManager &) = delete;

  // Cache-efficient storage using Structure of Arrays (SoA)
  // Position/size data lives in EntityDataManager (single source of truth)
  // AIManager stores AI-specific data (behaviors, priorities) + cached EDM indices
  // Active/inactive state is tracked via m_edmToStorageIndex (SIZE_MAX = inactive)
  struct EntityStorage {
    std::vector<EntityHandle> handles;  // 8 bytes each
    std::vector<float> lastUpdateTimes;
    std::vector<size_t> edmIndices;  // Cached for O(1) batch access

    size_t size() const { return handles.size(); }
    void reserve(size_t capacity) {
      handles.reserve(capacity);
      lastUpdateTimes.reserve(capacity);
      edmIndices.reserve(capacity);
    }
  };

  EntityStorage m_storage;
  std::unordered_map<EntityHandle, size_t> m_handleToIndex;
  std::unordered_map<std::string, BehaviorType> m_behaviorTypeMap;

  // Named preset configs (SmallWander, LargeWander, etc.) - checked before m_behaviorTypeMap
  std::unordered_map<std::string, VoidLight::BehaviorConfigData> m_presetConfigs;

  // Reverse mapping: EDM index -> dense storage index for O(1) lookup in processBatch
  // SIZE_MAX = no behavior assigned. Much cheaper than shared_ptr (8 bytes vs 16, no atomic ops)
  std::vector<size_t> m_edmToStorageIndex;

  // Player handle
  EntityHandle m_playerHandle{};

  // Threading and state
  std::atomic<bool> m_initialized{false};
#ifndef NDEBUG
  std::atomic<bool> m_useThreading{true};
#endif
  std::atomic<bool> m_globallyPaused{false};

  // Behavior execution tracking — worker threads fetch_add once per batch;
  // cache-line isolated to avoid false sharing with read-mostly atomics above.
  alignas(64) std::atomic<size_t> m_totalBehaviorExecutions{0};

  // Thread-safe assignment tracking
  std::atomic<size_t> m_totalAssignmentCount{0};

  // Frame counter for cache invalidation and distance staggering (operational)
  // Written once per frame on main thread; isolated from the worker-written counter above.
  alignas(64) std::atomic<uint64_t> m_frameCounter{0};

  // Thread synchronization
  mutable std::shared_mutex m_entitiesMutex;

  // Cached manager references (avoid singleton lookups in hot paths)
  PathfinderManager* mp_pathfinderManager{nullptr};

  // Batch futures for parallel processing - reused via clear() each frame
  std::vector<std::future<void>> m_batchFutures;

  // Pre-allocated per-batch event buffers (avoids per-frame allocation in threaded path)
  std::vector<std::vector<EventManager::DeferredEvent>> m_batchEventBuffers;

  // Reusable buffer for collecting damage events from batch futures
  std::vector<EventManager::DeferredEvent> m_allDamageEvents;

  // Reusable buffer for single-threaded path deferred events (avoids per-call allocation)
  std::vector<EventManager::DeferredEvent> m_singleBatchEvents;

  // Per-batch knockback-expiry queues. Workers enqueue edmIdx when framesRemaining
  // hits zero; main thread drains after futures join. Keeps SparseSidecar::remove()
  // off worker threads — its pop_back / cross-entity m_sparse patch is not race-safe.
  std::vector<std::vector<uint32_t>> m_batchKnockbackClears;
  std::vector<uint32_t> m_singleBatchKnockbackClears;

  // Reusable buffer for Active tier EDM indices (avoids per-frame allocation)
  std::vector<size_t> m_activeIndicesBuffer;

  // Cached player edmIndex (updated once per frame during update(), SIZE_MAX = no player)
  size_t m_cachedPlayerEdmIdx{SIZE_MAX};

  // Incrementally maintained behavior/faction indices for O(G)/O(F) radius scans.
  // Modified only on main thread (under m_entitiesMutex write lock in assignBehavior etc.),
  // read-only during batch processing — thread-safe by construction.
  static constexpr uint8_t MAX_FACTIONS = 16;
  std::vector<size_t> m_guardEdmIndices;                           // EDM indices of Guard-assigned entities
  std::array<std::vector<size_t>, MAX_FACTIONS> m_factionEdmIndices;  // Per-faction EDM indices
  std::vector<VoidLight::AICommandBus::BehaviorMessageCommand> m_pendingBehaviorMessages;
  std::vector<VoidLight::AICommandBus::BehaviorTransitionCommand> m_pendingBehaviorTransitions;
  std::vector<VoidLight::AICommandBus::BehaviorTransitionCommand> m_selectedTransitions;
  std::unordered_map<size_t, size_t> m_selectedTransitionsByEdmIndex;
  std::vector<VoidLight::AICommandBus::FactionChangeCommand> m_pendingFactionChanges;
  std::vector<VoidLight::AICommandBus::EquipmentSwapCommand> m_pendingMeleeFallbackEquips;
  std::vector<VoidLight::AICommandBus::RangedAttackCommand> m_pendingRangedAttacks;

  void addToIndices(size_t edmIndex, BehaviorType behaviorType);
  void removeFromIndices(size_t edmIndex, BehaviorType oldBehaviorType);
  void commitQueuedFactionChanges();
  void commitQueuedRangedAttacks();
  void commitQueuedMeleeFallbackEquips();
  void commitQueuedBehaviorMessages();
  void commitQueuedBehaviorTransitions();

  // Process batch of Active tier entities using EDM indices directly.
  // Runs emotional decay and behavior dispatch in a single fused pass.
  // Collects deferred events from this batch's thread-local buffer into outEvents.
  void processBatch(const std::vector<size_t>& activeIndices,
                    size_t start, size_t end,
                    float deltaTime,
                    float worldWidth, float worldHeight,
                    EntityHandle playerHandle, const Vector2D& playerPos,
                    const Vector2D& playerVel, bool playerValid,
                    float gameTime,
                    std::vector<EventManager::DeferredEvent>& outEvents,
                    std::vector<uint32_t>& outKnockbackClears);

  // Shutdown state
  bool m_isShutdown{false};
};

#endif // AI_MANAGER_HPP
