/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef EVENT_MANAGER_HPP
#define EVENT_MANAGER_HPP

/**
 * @file EventManager.hpp
 * @brief High-performance event processing hub
 *
 * EventManager provides:
 * - Type-indexed handler dispatch for cross-system coordination
 * - Deferred queue draining on the main thread
 * - Pooled hot-path events for stable batching
 * - Built-in processing for selected engine-level event types
 */

#include "entities/EntityHandle.hpp"
#include "events/EventTypeId.hpp"
#include "utils/ResourceHandle.hpp"
#include "utils/Vector2D.hpp"
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <limits>
#include <algorithm>

// Forward declarations
class Event;
namespace VoidLight { struct CollisionInfo; }
class WeatherEvent;
class NPCSpawnEvent;
class MerchantSpawnEvent;
class ResourceChangeEvent;
class WorldEvent;
class CameraEvent;
class CameraMovedEvent;
class CameraZoomChangedEvent;
class CameraShakeStartedEvent;
class CameraShakeEndedEvent;
class WorldTriggerEvent;
class HarvestResourceEvent;
class CollisionObstacleChangedEvent;
class ParticleEffectEvent;
class TimeEvent;
class DamageEvent;
class Entity;

using EventPtr = std::shared_ptr<Event>;
using EventWeakPtr = std::weak_ptr<Event>;
using EntityPtr = std::shared_ptr<Entity>;

/**
 * @brief Cache-friendly event data structure (data-oriented design)
 * Optimized for natural alignment and minimal padding
 */
struct EventData {
  EventPtr event;     // Smart pointer to event
  uint32_t flags;     // Active, dirty, etc.
  EventTypeId typeId; // Type for fast dispatch

  // Flags bit definitions
  static constexpr uint32_t FLAG_ACTIVE = 1 << 0;
  static constexpr uint32_t FLAG_DIRTY = 1 << 1;
  static constexpr uint32_t FLAG_PENDING_REMOVAL = 1 << 2;

  EventData()
      : event(nullptr), flags(0), typeId(EventTypeId::Custom) {}

  bool isActive() const { return flags & FLAG_ACTIVE; }
  void setActive(bool active) {
    if (active) flags |= FLAG_ACTIVE; else flags &= ~FLAG_ACTIVE;
  }
  bool isDirty() const { return flags & FLAG_DIRTY; }
  void setDirty(bool dirty) {
    if (dirty) flags |= FLAG_DIRTY; else flags &= ~FLAG_DIRTY;
  }
};

/**
 * @brief Fast event handler function type
 */
using FastEventHandler = std::function<void(const EventData &)>;

/**
 * @brief Handler entry combining callable with ID for token-based removal
 *
 * Handlers marked persistent survive prepareForStateTransition().
 * Manager-level handlers (registered in init()) should be persistent.
 * State-level handlers (registered in enter()) should be transient (default).
 */
struct HandlerEntry {
  FastEventHandler callable;
  uint64_t id = 0;
  bool persistent = false;

  HandlerEntry() = default;
  HandlerEntry(FastEventHandler c, uint64_t i, bool p = false)
    : callable(std::move(c)), id(i), persistent(p) {}

  explicit operator bool() const { return static_cast<bool>(callable); }
};

/**
 * @brief Event pool for memory-efficient event management
 */
template <typename EventType> class EventPool {
public:
  using Creator = std::function<std::shared_ptr<EventType>()>;
  explicit EventPool(Creator creator = Creator{})
      : m_creator(std::move(creator)) {}

  std::shared_ptr<EventType> acquire() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_available.empty()) {
      auto event = m_available.front();
      m_available.pop();
      return event;
    }

    // Create new event via creator if provided
    if (m_creator) {
      auto event = m_creator();
      m_allEvents.push_back(event);
      return event;
    }
    // No creator set; caller must handle nullptr
    return nullptr;
  }

  void release(std::shared_ptr<EventType> event) {
    if (!event)
      return;

    std::lock_guard<std::mutex> lock(m_mutex);
    event->reset(); // Reset event state
    m_available.push(event);
  }

  void clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_available.empty()) {
      m_available.pop();
    }
    m_allEvents.clear();
  }

  void setCreator(Creator creator) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_creator = std::move(creator);
  }

private:
  std::vector<std::shared_ptr<EventType>> m_allEvents;
  std::queue<std::shared_ptr<EventType>> m_available;
  std::mutex m_mutex;
  Creator m_creator;
};

/**
 * @brief Performance statistics for monitoring
 */
struct PerformanceStats {
  double totalTime{0.0};
  uint64_t callCount{0};
  double avgTime{0.0};
  double minTime{std::numeric_limits<double>::max()};
  double maxTime{0.0};

  void addSample(double time) {
    totalTime += time;
    callCount++;
    avgTime = totalTime / callCount;
    minTime = std::min(minTime, time);
    maxTime = std::max(maxTime, time);
  }

  void reset() {
    totalTime = 0.0;
    callCount = 0;
    avgTime = 0.0;
    minTime = std::numeric_limits<double>::max();
    maxTime = 0.0;
  }
};

/**
 * @brief High-performance event processing hub
 *
 * EventManager owns handler registration, deferred queue draining,
 * and built-in processing for selected engine-level events.
 */
class EventManager {
public:
  static EventManager &Instance();

  // Dispatch control for handler execution
  enum class DispatchMode : uint8_t { Deferred = 0, Immediate = 1 };

  /**
   * @brief Initializes the EventManager and its internal systems
   * @return true if initialization successful, false otherwise
   */
  bool init();

  /**
   * @brief Checks if the Event Manager has been initialized
   * @return true if initialized, false otherwise
   */
  bool isInitialized() const;

  /**
   * @brief Cleans up all event resources
   */
  void clean();

  /**
   * @brief Prepares for state transition by safely cleaning up handlers
   * @details Call this before exit() in game states to avoid issues
   */
  void prepareForStateTransition();

  /**
   * @brief Processes the deferred dispatch queue
   */
  void update();

  /**
   * @brief Drains all deferred events from the dispatch queue
   * @details Calls update() multiple times until all deferred events are processed.
   *          Primarily intended for testing to ensure deterministic event processing.
   */
  void drainAllDeferredEvents();

  // ==================== Batch Enqueue (for AI/Combat workers) ====================

  /**
   * @brief Deferred event for batch enqueueing
   * @details Used by AI worker threads to accumulate events locally,
   *          then enqueue in a single batch with one lock acquisition.
   */
  struct DeferredEvent {
    EventTypeId typeId;
    EventData data;
  };

  /**
   * @brief Enqueues multiple deferred events with a single lock acquisition
   * @param events Vector of deferred events to enqueue (moved)
   * @details AI workers should accumulate events locally during batch processing,
   *          then call this once at the end. One lock per batch instead of per event.
   */
  void enqueueBatch(std::vector<DeferredEvent>&& events) const;

  /**
   * @brief Checks if EventManager has been shut down
   * @return true if manager is shut down, false otherwise
   */
  bool isShutdown() const;

  // ==================== Handler Registration ====================

  /**
   * @brief Registers a transient handler (cleared on state transition)
   */
  void registerHandler(EventTypeId typeId, FastEventHandler handler);

  /**
   * @brief Registers a persistent handler (survives state transitions)
   *
   * Use for manager-level infrastructure handlers registered in init().
   * State-level handlers should use registerHandler() instead.
   */
  void registerPersistentHandler(EventTypeId typeId, FastEventHandler handler);

  /**
   * @brief Removes all handlers for an event type
   */
  void removeHandlers(EventTypeId typeId);

  /**
   * @brief Clears all registered handlers (including persistent — for shutdown)
   */
  void clearAllHandlers();

  /**
   * @brief Clears only transient handlers (persistent handlers survive)
   *
   * Called during state transitions. Manager-level handlers registered
   * with registerPersistentHandler() are retained.
   */
  void clearTransientHandlers();

  /**
   * @brief Gets the handler count for an event type
   */
  size_t getHandlerCount(EventTypeId typeId) const;

  // Token-based handler management
  struct HandlerToken {
    EventTypeId typeId;
    uint64_t id;
  };

  /**
   * @brief Registers a transient handler and returns a token for removal
   */
  HandlerToken registerHandlerWithToken(EventTypeId typeId,
                                        FastEventHandler handler);

  /**
   * @brief Registers a persistent handler and returns a token for removal
   */
  HandlerToken registerPersistentHandlerWithToken(EventTypeId typeId,
                                                  FastEventHandler handler);

  /**
   * @brief Removes a handler using its token (works for both persistent and transient)
   */
  bool removeHandler(const HandlerToken &token);

  // ==================== Global Controls ====================

#ifndef NDEBUG
  // Threading control (benchmarking only - compiles out in release)
  void enableThreading(bool enable);
  bool isThreadingEnabled() const;
#endif

  /**
   * @brief Sets global pause state (for menu states)
   */
  void setGlobalPause(bool paused);

  /**
   * @brief Gets the global pause state
   */
  bool isGloballyPaused() const;

  // ==================== Trigger Methods (Dispatch-Only) ====================

  /**
   * @brief Triggers a weather change event
   */
  bool changeWeather(const std::string &weatherType,
                     float transitionTime = 5.0f,
                     DispatchMode mode = DispatchMode::Deferred) const;

  /**
   * @brief Triggers an NPC spawn event
   */
  bool spawnNPC(const std::string &npcType, float x, float y,
                int count = 1, float spawnRadius = 0.0f,
                const std::string &npcRace = "",
                const std::vector<std::string> &aiBehaviors = {},
                bool worldWide = false,
                DispatchMode mode = DispatchMode::Deferred) const;
  bool spawnMerchant(const std::string& merchantClass, float x, float y,
                     const std::string& merchantRace = "Human",
                     int count = 1, float spawnRadius = 0.0f,
                     bool worldWide = false,
                     DispatchMode mode = DispatchMode::Deferred) const;

  /**
   * @brief Triggers a particle effect
   */
  bool triggerParticleEffect(const std::string &effectName, float x, float y,
                             float intensity = 1.0f, float duration = -1.0f,
                             const std::string &groupTag = "",
                             DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerParticleEffect(const std::string &effectName,
                             const Vector2D &position, float intensity = 1.0f,
                             float duration = -1.0f,
                             const std::string &groupTag = "",
                             DispatchMode mode = DispatchMode::Deferred) const;

  /**
   * @brief Triggers a resource change event
   */
  bool triggerResourceChange(EntityHandle ownerHandle,
                             VoidLight::ResourceHandle resourceHandle,
                             int oldQuantity, int newQuantity,
                             const std::string &changeReason = "",
                             DispatchMode mode = DispatchMode::Deferred) const;

  /**
   * @brief Triggers a world trigger event (OnEnter style)
   */
  bool triggerWorldTrigger(const WorldTriggerEvent &event,
                           DispatchMode mode = DispatchMode::Deferred) const;

  /**
   * @brief Triggers a collision obstacle changed event
   */
  bool triggerCollisionObstacleChanged(const Vector2D& position,
                                       float radius = 64.0f,
                                       const std::string& description = "",
                                       DispatchMode mode = DispatchMode::Deferred) const;

  // ==================== Combat Event Triggers ====================

  /**
   * @brief Triggers a damage event (stub - define parameters when combat is designed)
   * @details Pool and dispatch infrastructure ready. Update signature as needed.
   */
  bool triggerDamage(DispatchMode mode = DispatchMode::Deferred) const;

  // World event triggers
  bool triggerWorldLoaded(const std::string &worldId, int width, int height,
                          DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerWorldUnloaded(const std::string &worldId,
                            DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerTileChanged(int x, int y, const std::string &changeType,
                          DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerWorldGenerated(const std::string &worldId, int width, int height,
                             float generationTime,
                             DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerStaticCollidersReady(size_t solidBodyCount, size_t triggerCount,
                                   DispatchMode mode = DispatchMode::Deferred) const;

  // Camera event triggers
  bool triggerCameraMoved(const Vector2D &newPos, const Vector2D &oldPos,
                          DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerCameraModeChanged(int newMode, int oldMode,
                                DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerCameraShakeStarted(float duration, float intensity,
                                 DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerCameraShakeEnded(DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerCameraTargetChanged(const std::weak_ptr<Entity>& newTarget,
                                  const std::weak_ptr<Entity>& oldTarget,
                                  DispatchMode mode = DispatchMode::Deferred) const;
  bool triggerCameraZoomChanged(float newZoom, float oldZoom,
                                DispatchMode mode = DispatchMode::Deferred) const;

  // Compatibility aliases
  bool triggerWeatherChange(const std::string &weatherType,
                            float transitionTime = 5.0f) const;
  bool triggerNPCSpawn(const std::string &npcType, float x, float y,
                       const std::string &npcRace = "") const;

  /**
   * @brief Dispatches an event directly without registration
   * @param event Shared pointer to the event to dispatch
   * @param mode Deferred (processed in update()) or Immediate
   * @return true if dispatch successful, false otherwise
   */
  bool dispatchEvent(const EventPtr& event, DispatchMode mode = DispatchMode::Deferred) const;

  // ==================== Performance & Diagnostics ====================

  /**
   * @brief Gets performance stats for an event type
   */
  PerformanceStats getPerformanceStats(EventTypeId typeId) const;

  /**
   * @brief Resets all performance statistics
   */
  void resetPerformanceStats() const;

  /**
   * @brief Gets the number of pending events in the dispatch queue
   */
  size_t getPendingEventCount() const;

  /**
   * @brief Clears all event pools
   */
  void clearEventPools();

  // ==================== Pool Acquisition (for deferred event creation) ====================

  /**
   * @brief Acquire a DamageEvent from pool (avoids per-event allocation)
   */
  std::shared_ptr<DamageEvent> acquireDamageEvent() const { return m_damagePool.acquire(); }

private:
  struct PreparedCombatEvent {
    EntityHandle attackerHandle{};
    EntityHandle targetHandle{};
    size_t targetIdx{SIZE_MAX};
    float damage{0.0f};
    Vector2D knockback{};
    bool targetIsNPC{false};
    bool destroyOnLethal{false};
    bool valid{false};
  };

  EventManager(); // Constructor pre-allocates handler vectors

  // Shutdown state (main thread access only - game loop guarantees sequential updates)
  bool m_isShutdown{false};
  ~EventManager();
  EventManager(const EventManager &) = delete;
  EventManager &operator=(const EventManager &) = delete;

  // Event pools for trigger methods (reuse event objects)
  mutable EventPool<WeatherEvent> m_weatherPool;
  mutable EventPool<NPCSpawnEvent> m_npcSpawnPool;
  mutable EventPool<MerchantSpawnEvent> m_merchantSpawnPool;
  mutable EventPool<ResourceChangeEvent> m_resourceChangePool;
  mutable EventPool<WorldEvent> m_worldPool;
  mutable EventPool<CameraEvent> m_cameraPool;

  // Hot-path event pools (triggered frequently during gameplay)
  mutable EventPool<ParticleEffectEvent> m_particleEffectPool;
  mutable EventPool<CollisionObstacleChangedEvent> m_collisionObstacleChangedPool;
  mutable EventPool<DamageEvent> m_damagePool;

  // Handler storage (type-indexed)
  std::array<std::vector<HandlerEntry>, static_cast<size_t>(EventTypeId::COUNT)>
      m_handlersByType;
  std::atomic<uint64_t> m_nextHandlerId{1};

  // Threading and synchronization
  mutable std::shared_mutex m_handlersMutex;
  std::atomic<bool> m_threadingEnabled{true};
  std::atomic<bool> m_initialized{false};
  std::atomic<bool> m_globallyPaused{false};

  // Performance monitoring
  mutable std::array<PerformanceStats, static_cast<size_t>(EventTypeId::COUNT)>
      m_performanceStats;
  mutable std::mutex m_perfMutex;

  // Timing
  std::atomic<uint64_t> m_lastUpdateTime{0};

  // Deferred dispatch queue
  struct PendingDispatch {
    uint64_t sequence{0};
    EventTypeId typeId;
    EventData data;
  };
  mutable std::mutex m_dispatchMutex;  // Protects concurrent enqueue from AI workers

  // Async batch tracking for safe shutdown
  std::vector<std::future<void>> m_batchFutures;
  std::vector<std::future<void>> m_reusableBatchFutures;
  std::mutex m_batchFuturesMutex;
  mutable std::deque<PendingDispatch> m_pendingDispatch;
  mutable std::deque<PendingDispatch> m_pendingCombatDispatch;
  mutable uint64_t m_nextDeferredSequence{0};
  size_t m_maxDispatchQueue{8192};

  // Reusable buffer for drainDispatchQueueWithBudget
  mutable std::vector<PendingDispatch> m_localDispatchBuffer;
  mutable std::vector<PendingDispatch> m_localNonCombatBuffer;
  mutable std::vector<PendingDispatch> m_localCombatDispatchBuffer;
  mutable std::vector<PreparedCombatEvent> m_preparedCombatBuffer;
  mutable std::vector<std::future<void>> m_combatPrepFutures;

  void dispatchPendingEvent(const PendingDispatch& pendingDispatch,
                            std::string_view errorContext) const;
  void dispatchPendingEventWithHandlers(const PendingDispatch& pendingDispatch,
                                        const std::vector<HandlerEntry>& typeHandlers,
                                        std::string_view errorContext) const;
  PreparedCombatEvent prepareCombatEvent(const PendingDispatch& pendingDispatch) const;
  void prepareCombatBatch(size_t startCombatIndex, size_t endCombatIndex) const;
  void commitPreparedCombatEvent(const PendingDispatch& pendingDispatch,
                                 const PreparedCombatEvent& preparedCombat,
                                 float gameTime) const;
  uint64_t getCurrentTimeNanos() const;
  void enqueueDispatch(EventTypeId typeId, EventData&& data) const;
  size_t getPendingQueueSizeUnsafe() const;
  void dropOldestPendingUnsafe() const;
  void clearPendingDispatchQueues() const;
  void drainDispatchQueueWithBudget();

  // Consolidated dispatch helper
  bool dispatchEvent(EventTypeId typeId, EventData& eventData, DispatchMode mode,
                     std::string_view errorContext = "dispatchEvent") const;

  // Release pooled events back to their pools after dispatch
  void releaseEventToPool(EventTypeId typeId, const EventPtr& event) const;
};

#endif // EVENT_MANAGER_HPP
