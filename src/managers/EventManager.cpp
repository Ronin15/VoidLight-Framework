/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/EventManager.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "events/CameraEvent.hpp"
#include "events/CollisionObstacleChangedEvent.hpp"
#include "events/EntityEvents.hpp"
#include "events/Event.hpp"
#include "events/MerchantSpawnEvent.hpp"
#include "events/NPCSpawnEvent.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "events/WorldEvent.hpp"
#include "events/WorldTriggerEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/GameTimeManager.hpp"

#include <algorithm>
#include <chrono>
#include <format>

// ==================== Core Singleton & Lifecycle ====================

namespace {
void registerBuiltInHandlers(EventManager& eventManager) {
  eventManager.registerPersistentHandler(EventTypeId::NPCSpawn, [](const EventData &data) {
    if (!data.isActive() || !data.event) return;
    auto npcEvent = std::dynamic_pointer_cast<NPCSpawnEvent>(data.event);
    if (npcEvent) {
      npcEvent->execute();
    }
  });
  eventManager.registerPersistentHandler(EventTypeId::MerchantSpawn, [](const EventData &data) {
    if (!data.isActive() || !data.event) return;
    auto merchantEvent = std::dynamic_pointer_cast<MerchantSpawnEvent>(data.event);
    if (merchantEvent) {
      merchantEvent->execute();
    }
  });
}
} // namespace

EventManager &EventManager::Instance() {
  static EventManager instance;
  return instance;
}

EventManager::EventManager() {
  // Pre-allocate handler vectors to avoid reallocation during registration
  for (auto &handlerVec : m_handlersByType) {
    handlerVec.reserve(16);
  }
}

EventManager::~EventManager() {
  if (!m_isShutdown) {
    clean();
  }
}

bool EventManager::isInitialized() const {
  return m_initialized.load(std::memory_order_acquire);
}

bool EventManager::isShutdown() const { return m_isShutdown; }

bool EventManager::init() {
  if (m_initialized.load()) {
    EVENT_WARN("EventManager already initialized");
    return true;
  }

  // Reset shutdown flag to allow re-initialization after clean()
  m_isShutdown = false;

  EVENT_INFO("Initializing EventManager (central event processing hub)");

  // Initialize handler containers
  for (auto &handlerContainer : m_handlersByType) {
    handlerContainer.clear();
    constexpr size_t HANDLER_CONTAINER_CAPACITY = 32;
    handlerContainer.reserve(HANDLER_CONTAINER_CAPACITY);
  }

  // Clear any deferred work queued before initialization/reset.
  clearPendingDispatchQueues();

  // Clear event pools after draining queued events so stale work cannot leak
  // into the next state or reinitialize into the next pool set.
  clearEventPools();

  // Configure event pools for trigger methods
  m_weatherPool.setCreator([]() {
    return std::make_shared<WeatherEvent>("trigger_weather", WeatherType::Clear);
  });
  m_npcSpawnPool.setCreator([]() {
    return std::make_shared<NPCSpawnEvent>("trigger_npc_spawn", SpawnParameters{});
  });
  m_merchantSpawnPool.setCreator([]() {
    return std::make_shared<MerchantSpawnEvent>("trigger_merchant_spawn",
                                                MerchantSpawnParameters{});
  });
  m_resourceChangePool.setCreator([]() {
    return std::make_shared<ResourceChangeEvent>(
        EntityHandle{}, VoidLight::ResourceHandle{}, 0, 0, "");
  });

  // Hot-path event pools
  m_particleEffectPool.setCreator([]() {
    return std::make_shared<ParticleEffectEvent>("pool_particle",
                                                 ParticleEffectType::Fire, 0.0f,
                                                 0.0f, 1.0f, -1.0f, "", "");
  });
  m_collisionObstacleChangedPool.setCreator([]() {
    return std::make_shared<CollisionObstacleChangedEvent>(
        CollisionObstacleChangedEvent::ChangeType::MODIFIED, Vector2D(0, 0),
        64.0f, "");
  });
  m_damagePool.setCreator([]() {
    return std::make_shared<DamageEvent>();
  });

  // Reset performance stats
  resetPerformanceStats();

  m_lastUpdateTime.store(getCurrentTimeNanos());
  m_initialized.store(true);

  registerBuiltInHandlers(*this);

  EVENT_INFO("EventManager initialized successfully");
  return true;
}

void EventManager::clean() {
  if (!m_initialized.load(std::memory_order_acquire) || m_isShutdown) {
    return;
  }

  // Set shutdown flags EARLY to prevent new work
  m_isShutdown = true;
  m_initialized.store(false, std::memory_order_release);

  // Wait for any pending async batches to complete before cleanup
  {
    std::vector<std::future<void>> localFutures;
    {
      std::lock_guard<std::mutex> lock(m_batchFuturesMutex);
      localFutures = std::move(m_batchFutures);
    }

    for (auto &future : localFutures) {
      if (future.valid()) {
        future.wait();
      }
    }
  }

  EVENT_INFO_IF(!m_isShutdown, "Cleaning up EventManager");

  // Clear all handlers
  clearAllHandlers();

  // Clear any deferred work queued before shutdown.
  clearPendingDispatchQueues();

  // Clear event pools after draining queued events to avoid carrying stale
  // combat state across shutdown and subsequent init().
  clearEventPools();

  // Reset performance stats
  resetPerformanceStats();
}

void EventManager::prepareForStateTransition() {
  EVENT_INFO("Preparing EventManager for state transition...");

  // Wait for any pending async batches
  {
    std::vector<std::future<void>> localFutures;
    {
      std::lock_guard<std::mutex> lock(m_batchFuturesMutex);
      localFutures = std::move(m_batchFutures);
    }

    for (auto &future : localFutures) {
      if (future.valid()) {
        future.wait();
      }
    }
  }

  // Clear transient handlers (state-level). Persistent handlers (manager-level,
  // registered via registerPersistentHandler) survive across transitions.
  clearTransientHandlers();

  // Clear any deferred work queued before the next state takes over.
  clearPendingDispatchQueues();

  // Clear event pools after queued events have been released and discarded.
  clearEventPools();

  // Reset performance stats
  resetPerformanceStats();

  EVENT_INFO("EventManager prepared for state transition");
}

// ==================== Update (Dispatch Queue Processing) ====================

void EventManager::update() {
  if (!m_initialized.load() || m_isShutdown) {
    return;
  }

  // Skip update when globally paused
  if (m_globallyPaused.load(std::memory_order_acquire)) {
    return;
  }

  // Process the deferred dispatch queue
  drainDispatchQueueWithBudget();
}

void EventManager::drainAllDeferredEvents() {
  if (!m_initialized.load()) {
    return;
  }

  // Process until queue is empty (max 100 iterations for safety)
  constexpr int MAX_ITERATIONS = 100;
  for (int i = 0; i < MAX_ITERATIONS; ++i) {
    size_t pendingCount = 0;
    {
      std::lock_guard<std::mutex> lock(m_dispatchMutex);
      pendingCount = getPendingQueueSizeUnsafe();
    }

    if (pendingCount == 0) {
      break;
    }

    update();
  }
}

// ==================== Threading & Pause Controls ====================

#ifndef NDEBUG
void EventManager::enableThreading(bool enable) {
  m_threadingEnabled.store(enable);
}

bool EventManager::isThreadingEnabled() const {
  return m_threadingEnabled.load();
}
#endif

void EventManager::setGlobalPause(bool paused) {
  m_globallyPaused.store(paused, std::memory_order_release);
}

bool EventManager::isGloballyPaused() const {
  return m_globallyPaused.load(std::memory_order_acquire);
}

// ==================== Handler Registration ====================

void EventManager::registerHandler(EventTypeId typeId, FastEventHandler handler) {
  registerHandlerWithToken(typeId, std::move(handler));
}

void EventManager::registerPersistentHandler(EventTypeId typeId, FastEventHandler handler) {
  registerPersistentHandlerWithToken(typeId, std::move(handler));
}

EventManager::HandlerToken
EventManager::registerHandlerWithToken(EventTypeId typeId, FastEventHandler handler) {
  std::unique_lock<std::shared_mutex> lock(m_handlersMutex);
  const size_t idx = static_cast<size_t>(typeId);
  uint64_t id = m_nextHandlerId.fetch_add(1, std::memory_order_relaxed);

  m_handlersByType[idx].emplace_back(std::move(handler), id, false);
  return HandlerToken{typeId, id};
}

EventManager::HandlerToken
EventManager::registerPersistentHandlerWithToken(EventTypeId typeId, FastEventHandler handler) {
  std::unique_lock<std::shared_mutex> lock(m_handlersMutex);
  const size_t idx = static_cast<size_t>(typeId);
  uint64_t id = m_nextHandlerId.fetch_add(1, std::memory_order_relaxed);

  m_handlersByType[idx].emplace_back(std::move(handler), id, true);
  return HandlerToken{typeId, id};
}

bool EventManager::removeHandler(const HandlerToken &token) {
  std::unique_lock<std::shared_mutex> lock(m_handlersMutex);
  const size_t idx = static_cast<size_t>(token.typeId);
  if (idx >= m_handlersByType.size())
    return false;

  auto &entries = m_handlersByType[idx];
  auto it = std::find_if(
      entries.begin(), entries.end(),
      [&token](const HandlerEntry &entry) { return entry.id == token.id; });
  if (it != entries.end()) {
    // Swap-and-pop: O(1) removal without leaving holes
    if (it != entries.end() - 1) {
      *it = std::move(entries.back());
    }
    entries.pop_back();
    return true;
  }
  return false;
}

void EventManager::clearAllHandlers() {
  std::unique_lock<std::shared_mutex> lock(m_handlersMutex);
  for (size_t i = 0; i < m_handlersByType.size(); ++i) {
    m_handlersByType[i].clear();
  }
  EVENT_INFO("All event handlers cleared");
}

void EventManager::clearTransientHandlers() {
  std::unique_lock<std::shared_mutex> lock(m_handlersMutex);
  size_t removedCount = 0;
  for (size_t i = 0; i < m_handlersByType.size(); ++i) {
    auto& handlers = m_handlersByType[i];
    size_t before = handlers.size();
    std::erase_if(handlers, [](const HandlerEntry& e) { return !e.persistent; });
    removedCount += before - handlers.size();
  }
  EVENT_INFO(std::format("Cleared {} transient handlers (persistent handlers retained)",
                         removedCount));
}

size_t EventManager::getHandlerCount(EventTypeId typeId) const {
  std::shared_lock<std::shared_mutex> lock(m_handlersMutex);
  return m_handlersByType[static_cast<size_t>(typeId)].size();
}

void EventManager::removeHandlers(EventTypeId typeId) {
  std::unique_lock<std::shared_mutex> lock(m_handlersMutex);
  const size_t idx = static_cast<size_t>(typeId);
  m_handlersByType[idx].clear();
}

// ==================== Trigger Methods ====================

bool EventManager::triggerWeatherChange(const std::string &weatherType,
                                        float transitionTime) const {
  return changeWeather(weatherType, transitionTime);
}

bool EventManager::triggerNPCSpawn(const std::string &npcType, float x,
                                   float y, const std::string &npcRace) const {
  return spawnNPC(npcType, x, y, 1, 0.0f, npcRace);
}

bool EventManager::changeWeather(const std::string &weatherType,
                                 float transitionTime, DispatchMode mode) const {
  std::shared_ptr<WeatherEvent> weatherEvent = m_weatherPool.acquire();
  if (!weatherEvent)
    weatherEvent = std::make_shared<WeatherEvent>("trigger_weather", weatherType);
  else
    weatherEvent->setWeatherType(weatherType);

  WeatherParams params = weatherEvent->getWeatherParams();
  params.transitionTime = transitionTime;
  weatherEvent->setWeatherParams(params);

  EventData data;
  data.typeId = EventTypeId::Weather;
  data.setActive(true);
  data.event = weatherEvent;

  return dispatchEvent(EventTypeId::Weather, data, mode, "changeWeather");
}

bool EventManager::spawnNPC(const std::string &npcType, float x, float y,
                            int count, float spawnRadius,
                            const std::string &npcRace,
                            const std::vector<std::string> &aiBehaviors,
                            bool worldWide, DispatchMode mode) const {
  SpawnParameters params(npcType, count, spawnRadius, npcRace);
  params.aiBehaviors = aiBehaviors;
  params.worldWide = worldWide;

  auto npcEvent = m_npcSpawnPool.acquire();
  if (!npcEvent)
    npcEvent = std::make_shared<NPCSpawnEvent>("trigger_npc_spawn", params);
  else
    npcEvent->setSpawnParameters(params);
  npcEvent->addSpawnPoint(x, y);

  EventData data;
  data.typeId = EventTypeId::NPCSpawn;
  data.setActive(true);
  data.event = npcEvent;

  return dispatchEvent(EventTypeId::NPCSpawn, data, mode, "spawnNPC");
}

bool EventManager::spawnMerchant(const std::string& merchantClass,
                                 float x, float y,
                                 const std::string& merchantRace,
                                 int count,
                                 float spawnRadius,
                                 bool worldWide,
                                 DispatchMode mode) const {
  MerchantSpawnParameters params;
  params.merchantClass = merchantClass.empty() ? "GeneralMerchant" : merchantClass;
  params.merchantRace = merchantRace.empty() ? "Human" : merchantRace;
  params.count = count;
  params.spawnRadius = spawnRadius;
  params.worldWide = worldWide;

  auto merchantEvent = m_merchantSpawnPool.acquire();
  if (!merchantEvent) {
    merchantEvent = std::make_shared<MerchantSpawnEvent>("trigger_merchant_spawn",
                                                         params);
  } else {
    merchantEvent->reset();
    merchantEvent->setMerchantSpawnParameters(params);
  }
  merchantEvent->clearSpawnPoints();
  merchantEvent->addSpawnPoint(x, y);

  EventData data;
  data.typeId = EventTypeId::MerchantSpawn;
  data.setActive(true);
  data.event = merchantEvent;

  return dispatchEvent(EventTypeId::MerchantSpawn, data, mode, "spawnMerchant");
}

bool EventManager::triggerParticleEffect(const std::string &effectName, float x,
                                         float y, float intensity,
                                         float duration,
                                         const std::string &groupTag,
                                         DispatchMode mode) const {
  ParticleEffectType effectType = ParticleEffectEvent::stringToEffectType(effectName);

  auto pe = m_particleEffectPool.acquire();
  if (pe) {
    pe->setEffectType(effectType);
    pe->setPosition(x, y);
    pe->setIntensity(intensity);
    pe->setDuration(duration);
    pe->setGroupTag(groupTag);
  } else {
    pe = std::make_shared<ParticleEffectEvent>("trigger_particle_effect",
                                               effectType, x, y, intensity,
                                               duration, groupTag, "");
  }

  EventData data;
  data.typeId = EventTypeId::ParticleEffect;
  data.setActive(true);
  data.event = pe;

  return dispatchEvent(EventTypeId::ParticleEffect, data, mode, "triggerParticleEffect");
}

bool EventManager::triggerParticleEffect(const std::string &effectName,
                                         const Vector2D &position,
                                         float intensity, float duration,
                                         const std::string &groupTag,
                                         DispatchMode mode) const {
  return triggerParticleEffect(effectName, position.getX(), position.getY(),
                               intensity, duration, groupTag, mode);
}

bool EventManager::triggerResourceChange(
    EntityHandle ownerHandle, VoidLight::ResourceHandle resourceHandle,
    int oldQuantity, int newQuantity, const std::string &changeReason,
    DispatchMode mode) const {
  auto resourceEvent = m_resourceChangePool.acquire();
  if (resourceEvent) {
    resourceEvent->set(ownerHandle, resourceHandle, oldQuantity, newQuantity,
                       changeReason);
  } else {
    resourceEvent = std::make_shared<ResourceChangeEvent>(
        ownerHandle, resourceHandle, oldQuantity, newQuantity, changeReason);
  }

  EventData eventData;
  eventData.typeId = EventTypeId::ResourceChange;
  eventData.setActive(true);
  eventData.event = resourceEvent;

  return dispatchEvent(EventTypeId::ResourceChange, eventData, mode,
                       "triggerResourceChange");
}

bool EventManager::triggerWorldTrigger(const WorldTriggerEvent &event,
                                       DispatchMode mode) const {
  EventData eventData;
  eventData.typeId = EventTypeId::WorldTrigger;
  eventData.setActive(true);
  eventData.event = std::make_shared<WorldTriggerEvent>(event);

  return dispatchEvent(EventTypeId::WorldTrigger, eventData, mode, "triggerWorldTrigger");
}

bool EventManager::triggerCollisionObstacleChanged(
    const Vector2D &position, float radius, const std::string &description,
    DispatchMode mode) const {
  auto obstacleEvent = m_collisionObstacleChangedPool.acquire();
  if (obstacleEvent) {
    obstacleEvent->configure(
        CollisionObstacleChangedEvent::ChangeType::MODIFIED, position, radius,
        description);
  } else {
    obstacleEvent = std::make_shared<CollisionObstacleChangedEvent>(
        CollisionObstacleChangedEvent::ChangeType::MODIFIED, position, radius,
        description);
  }

  EventData eventData;
  eventData.typeId = EventTypeId::CollisionObstacleChanged;
  eventData.setActive(true);
  eventData.event = obstacleEvent;

  return dispatchEvent(EventTypeId::CollisionObstacleChanged, eventData, mode,
                       "triggerCollisionObstacleChanged");
}

// ==================== Combat Triggers ====================

bool EventManager::triggerDamage(DispatchMode mode) const {
  // TODO: Add parameters and configure event when combat is designed
  auto damageEvent = m_damagePool.acquire();
  if (!damageEvent) {
    damageEvent = std::make_shared<DamageEvent>();
  }

  EventData eventData;
  eventData.typeId = EventTypeId::Combat;
  eventData.setActive(true);
  eventData.event = damageEvent;

  return dispatchEvent(EventTypeId::Combat, eventData, mode, "triggerDamage");
}

// World triggers
bool EventManager::triggerWorldLoaded(const std::string &worldId, int width,
                                      int height, DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::World;
  data.setActive(true);
  data.event = std::make_shared<WorldLoadedEvent>(worldId, width, height);
  return dispatchEvent(EventTypeId::World, data, mode, "triggerWorldLoaded");
}

bool EventManager::triggerWorldUnloaded(const std::string &worldId,
                                        DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::World;
  data.setActive(true);
  data.event = std::make_shared<WorldUnloadedEvent>(worldId);
  return dispatchEvent(EventTypeId::World, data, mode, "triggerWorldUnloaded");
}

bool EventManager::triggerTileChanged(int x, int y,
                                      const std::string &changeType,
                                      DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::World;
  data.setActive(true);
  data.event = std::make_shared<TileChangedEvent>(x, y, changeType);
  return dispatchEvent(EventTypeId::World, data, mode, "triggerTileChanged");
}

bool EventManager::triggerWorldGenerated(const std::string &worldId, int width,
                                         int height, float generationTime,
                                         DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::World;
  data.setActive(true);
  data.event = std::make_shared<WorldGeneratedEvent>(worldId, width, height,
                                                     generationTime);
  return dispatchEvent(EventTypeId::World, data, mode, "triggerWorldGenerated");
}

bool EventManager::triggerStaticCollidersReady(size_t solidBodyCount,
                                               size_t triggerCount,
                                               DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::World;
  data.setActive(true);
  data.event = std::make_shared<StaticCollidersReadyEvent>(solidBodyCount, triggerCount);
  return dispatchEvent(EventTypeId::World, data, mode, "triggerStaticCollidersReady");
}

// Camera triggers
bool EventManager::triggerCameraMoved(const Vector2D &newPos,
                                      const Vector2D &oldPos,
                                      DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::Camera;
  data.setActive(true);
  data.event = std::make_shared<CameraMovedEvent>(newPos, oldPos);
  return dispatchEvent(EventTypeId::Camera, data, mode, "triggerCameraMoved");
}

bool EventManager::triggerCameraModeChanged(int newMode, int oldMode,
                                            DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::Camera;
  data.setActive(true);
  data.event = std::make_shared<CameraModeChangedEvent>(
      static_cast<CameraModeChangedEvent::Mode>(newMode),
      static_cast<CameraModeChangedEvent::Mode>(oldMode));
  return dispatchEvent(EventTypeId::Camera, data, mode, "triggerCameraModeChanged");
}

bool EventManager::triggerCameraShakeStarted(float duration, float intensity,
                                             DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::Camera;
  data.setActive(true);
  data.event = std::make_shared<CameraShakeStartedEvent>(duration, intensity);
  return dispatchEvent(EventTypeId::Camera, data, mode, "triggerCameraShakeStarted");
}

bool EventManager::triggerCameraShakeEnded(DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::Camera;
  data.setActive(true);
  data.event = std::make_shared<CameraShakeEndedEvent>();
  return dispatchEvent(EventTypeId::Camera, data, mode, "triggerCameraShakeEnded");
}

bool EventManager::triggerCameraTargetChanged(const std::weak_ptr<Entity>& newTarget,
                                              const std::weak_ptr<Entity>& oldTarget,
                                              DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::Camera;
  data.setActive(true);
  data.event = std::make_shared<CameraTargetChangedEvent>(newTarget, oldTarget);
  return dispatchEvent(EventTypeId::Camera, data, mode, "triggerCameraTargetChanged");
}

bool EventManager::triggerCameraZoomChanged(float newZoom, float oldZoom,
                                            DispatchMode mode) const {
  EventData data;
  data.typeId = EventTypeId::Camera;
  data.setActive(true);
  data.event = std::make_shared<CameraZoomChangedEvent>(newZoom, oldZoom);
  return dispatchEvent(EventTypeId::Camera, data, mode, "triggerCameraZoomChanged");
}

// Public dispatch for EventPtr
bool EventManager::dispatchEvent(const EventPtr& event, DispatchMode mode) const {
  if (!event) {
    EVENT_ERROR("dispatchEvent called with null event");
    return false;
  }

  EventTypeId typeId = event->getTypeId();
  EventData data;
  data.typeId = typeId;
  data.setActive(true);
  data.event = event;

  return dispatchEvent(typeId, data, mode, "dispatchEvent(EventPtr)");
}

// ==================== Internal Dispatch ====================

bool EventManager::dispatchEvent(EventTypeId typeId, EventData &eventData,
                                 DispatchMode mode, std::string_view errorContext) const {
  if (mode == DispatchMode::Immediate) {
    const PendingDispatch pendingDispatch{0, typeId, eventData};
    const auto damageEvent = typeId == EventTypeId::Combat && eventData.event
        ? std::dynamic_pointer_cast<DamageEvent>(eventData.event)
        : nullptr;
    if (damageEvent) {
      const float gameTime = GameTimeManager::Instance().getTotalGameTimeSeconds();
      commitPreparedCombatEvent(pendingDispatch, PreparedCombatEvent{}, gameTime);
      dispatchPendingEvent(pendingDispatch, errorContext);
    } else {
      dispatchPendingEvent(pendingDispatch, errorContext);
    }
    releaseEventToPool(typeId, eventData.event);
    if (damageEvent) {
      return true;
    }
    std::shared_lock<std::shared_mutex> lock(m_handlersMutex);
    return !m_handlersByType[static_cast<size_t>(typeId)].empty();
  }

  // Deferred dispatch
  enqueueDispatch(typeId, std::move(eventData));
  return true;
}

size_t EventManager::getPendingQueueSizeUnsafe() const {
  return m_pendingDispatch.size() + m_pendingCombatDispatch.size();
}

void EventManager::dropOldestPendingUnsafe() const {
  if (m_pendingDispatch.empty()) {
    if (!m_pendingCombatDispatch.empty()) {
      releaseEventToPool(EventTypeId::Combat, m_pendingCombatDispatch.front().data.event);
      m_pendingCombatDispatch.pop_front();
    }
    return;
  }

  if (m_pendingCombatDispatch.empty()) {
    releaseEventToPool(m_pendingDispatch.front().typeId, m_pendingDispatch.front().data.event);
    m_pendingDispatch.pop_front();
    return;
  }

  if (m_pendingCombatDispatch.front().sequence < m_pendingDispatch.front().sequence) {
    releaseEventToPool(EventTypeId::Combat, m_pendingCombatDispatch.front().data.event);
    m_pendingCombatDispatch.pop_front();
  } else {
    releaseEventToPool(m_pendingDispatch.front().typeId, m_pendingDispatch.front().data.event);
    m_pendingDispatch.pop_front();
  }
}

void EventManager::enqueueDispatch(EventTypeId typeId, EventData&& data) const {
  std::lock_guard<std::mutex> lock(m_dispatchMutex);
  if (getPendingQueueSizeUnsafe() >= m_maxDispatchQueue) {
    dropOldestPendingUnsafe();
  }

  PendingDispatch pendingDispatch{m_nextDeferredSequence++, typeId, std::move(data)};
  if (typeId == EventTypeId::Combat) {
    m_pendingCombatDispatch.emplace_back(std::move(pendingDispatch));
  } else {
    m_pendingDispatch.emplace_back(std::move(pendingDispatch));
  }
}

void EventManager::enqueueBatch(std::vector<DeferredEvent>&& events) const {
  if (events.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_dispatchMutex);

  // Drop oldest queued events if we'd exceed the queue limit.
  size_t droppedCount = 0;
  while (getPendingQueueSizeUnsafe() + events.size() > m_maxDispatchQueue &&
         getPendingQueueSizeUnsafe() > 0) {
    dropOldestPendingUnsafe();
    ++droppedCount;
  }

  // If the incoming batch itself is larger than the queue cap, keep only the newest
  // tail that fits and release the overflowed pooled events immediately.
  if (events.size() > m_maxDispatchQueue) {
    const size_t overflowCount = events.size() - m_maxDispatchQueue;
    for (size_t i = 0; i < overflowCount; ++i) {
      releaseEventToPool(events[i].typeId, events[i].data.event);
    }
    events.erase(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(overflowCount));
    droppedCount += overflowCount;
  }

  // One final guard in case the queue still cannot fit the incoming tail.
  while (getPendingQueueSizeUnsafe() + events.size() > m_maxDispatchQueue &&
         getPendingQueueSizeUnsafe() > 0) {
    dropOldestPendingUnsafe();
    ++droppedCount;
  }

  if (droppedCount > 0) {
    EVENT_WARN(std::format("Queue overflow: dropped {} events", droppedCount));
  }

  for (auto& event : events) {
    PendingDispatch pendingDispatch{
        m_nextDeferredSequence++, event.typeId, std::move(event.data)};
    if (pendingDispatch.typeId == EventTypeId::Combat) {
      m_pendingCombatDispatch.emplace_back(std::move(pendingDispatch));
    } else {
      m_pendingDispatch.emplace_back(std::move(pendingDispatch));
    }
  }
}

void EventManager::dispatchPendingEvent(const PendingDispatch& pendingDispatch,
                                        std::string_view errorContext) const {
  std::shared_lock<std::shared_mutex> lock(m_handlersMutex);
  const auto& typeHandlers =
      m_handlersByType[static_cast<size_t>(pendingDispatch.typeId)];

  dispatchPendingEventWithHandlers(pendingDispatch, typeHandlers, errorContext);
}

void EventManager::dispatchPendingEventWithHandlers(
    const PendingDispatch& pendingDispatch,
    const std::vector<HandlerEntry>& typeHandlers,
    std::string_view errorContext) const {
  if (typeHandlers.empty()) {
    return;
  }

  for (const auto& entry : typeHandlers) {
    if (entry) {
      try {
        entry.callable(pendingDispatch.data);
      } catch (const std::exception& e) {
        EVENT_ERROR(
            std::format("Handler exception in {}: {}", errorContext, e.what()));
      } catch (...) {
        EVENT_ERROR(std::format("Unknown handler exception in {}", errorContext));
      }
    }
  }
}

EventManager::PreparedCombatEvent
EventManager::prepareCombatEvent(const PendingDispatch& pendingDispatch) const {
  PreparedCombatEvent preparedCombat;
  if (!pendingDispatch.data.isActive() || !pendingDispatch.data.event) {
    return preparedCombat;
  }

  auto damageEvent = std::dynamic_pointer_cast<DamageEvent>(pendingDispatch.data.event);
  if (!damageEvent) {
    return preparedCombat;
  }

  const auto& edm = EntityDataManager::Instance();
  const EntityHandle targetHandle = damageEvent->getTarget();
  const EntityHandle attackerHandle = damageEvent->getSource();
  preparedCombat.targetHandle = targetHandle;
  preparedCombat.attackerHandle = attackerHandle;
  const size_t targetIdx = edm.getIndex(targetHandle);
  preparedCombat.targetIdx = targetIdx;
  preparedCombat.damage = damageEvent->getDamage();
  preparedCombat.knockback = damageEvent->getKnockback();
  preparedCombat.targetIsNPC = targetHandle.isNPC();
  preparedCombat.destroyOnLethal = !targetHandle.isPlayer();

  if (targetIdx == SIZE_MAX) {
    return preparedCombat;
  }

  preparedCombat.valid = true;
  return preparedCombat;
}

void EventManager::prepareCombatBatch(size_t startCombatIndex,
                                      size_t endCombatIndex) const {
  if (startCombatIndex >= endCombatIndex ||
      startCombatIndex >= m_localCombatDispatchBuffer.size()) {
    return;
  }

  for (size_t combatIndex = startCombatIndex; combatIndex < endCombatIndex; ++combatIndex) {
    m_preparedCombatBuffer[combatIndex] =
        prepareCombatEvent(m_localCombatDispatchBuffer[combatIndex]);
  }
}

void EventManager::commitPreparedCombatEvent(const PendingDispatch& pendingDispatch,
                                             const PreparedCombatEvent& preparedCombat,
                                             float gameTime) const {
  if (!pendingDispatch.data.isActive() || !pendingDispatch.data.event) {
    return;
  }

  auto damageEvent = std::dynamic_pointer_cast<DamageEvent>(pendingDispatch.data.event);
  if (!damageEvent) {
    return;
  }

  auto& edm = EntityDataManager::Instance();
  const EntityHandle targetHandle = preparedCombat.valid
      ? preparedCombat.targetHandle
      : damageEvent->getTarget();
  const EntityHandle attackerHandle = preparedCombat.valid
      ? preparedCombat.attackerHandle
      : damageEvent->getSource();
  const bool targetIsNPC = preparedCombat.valid
      ? preparedCombat.targetIsNPC
      : targetHandle.isNPC();
  const bool destroyOnLethal = preparedCombat.valid
      ? preparedCombat.destroyOnLethal
      : !targetHandle.isPlayer();
  const size_t targetIdx = preparedCombat.valid
      ? preparedCombat.targetIdx
      : edm.getIndex(targetHandle);
  if (targetIdx == SIZE_MAX) {
    return;
  }

  auto& hotData = edm.getHotDataByIndex(targetIdx);
  auto& charData = edm.getCharacterDataByIndex(targetIdx);
  if (!hotData.isAlive()) {
    damageEvent->setRemainingHealth(charData.health);
    damageEvent->setWasLethal(false);
    return;
  }

  if (charData.health <= 0.0f) {
    damageEvent->setRemainingHealth(charData.health);
    damageEvent->setWasLethal(false);
    return;
  }

  const float damage = preparedCombat.valid
      ? preparedCombat.damage
      : damageEvent->getDamage();
  const float armorDefense = std::max(0.0f, charData.armorDefense);
  const float mitigatedDamage = damage > 0.0f
      ? std::max(1.0f, damage * (100.0f / (100.0f + armorDefense)))
      : 0.0f;
  charData.health = std::max(0.0f, charData.health - mitigatedDamage);

  const Vector2D knockback = preparedCombat.valid
      ? preparedCombat.knockback
      : damageEvent->getKnockback();
  const float knockbackScale = 1.0f / std::max(0.1f, charData.mass);
  auto& kb = edm.applyKnockback(targetIdx);
  kb.impulseX += knockback.getX() * knockbackScale;
  kb.impulseY += knockback.getY() * knockbackScale;
  kb.framesRemaining = static_cast<uint8_t>(Knockback::FRAMES);
  kb.justApplied = true;

  if (attackerHandle.isValid() && targetIsNPC) {
    edm.recordCombatEvent(targetIdx, attackerHandle, targetHandle,
                          mitigatedDamage, true, gameTime);
  }

  const bool wasLethal = (charData.health <= 0.0f);

  if (wasLethal) {
    hotData.flags &= ~EntityHotData::FLAG_ALIVE;
    if (destroyOnLethal) {
      edm.destroyEntity(targetHandle);
    }
  }

  damageEvent->setRemainingHealth(charData.health);
  damageEvent->setWasLethal(wasLethal);
}

void EventManager::drainDispatchQueueWithBudget() {
  // Extract all pending events under lock - worker threads may be enqueueing
  // events concurrently (e.g., WorldManager::loadNewWorld on worker thread)
  {
    std::lock_guard<std::mutex> lock(m_dispatchMutex);
    const size_t pendingCount = getPendingQueueSizeUnsafe();
    if (pendingCount == 0) {
      return;
    }

    const size_t nonCombatCount = m_pendingDispatch.size();
    const size_t combatCount = m_pendingCombatDispatch.size();
    m_localNonCombatBuffer.clear();
    m_localNonCombatBuffer.reserve(nonCombatCount);
    for (size_t i = 0; i < nonCombatCount; ++i) {
      m_localNonCombatBuffer.emplace_back(std::move(m_pendingDispatch.front()));
      m_pendingDispatch.pop_front();
    }

    m_localCombatDispatchBuffer.clear();
    m_localCombatDispatchBuffer.reserve(combatCount);
    for (size_t i = 0; i < combatCount; ++i) {
      m_localCombatDispatchBuffer.emplace_back(std::move(m_pendingCombatDispatch.front()));
      m_pendingCombatDispatch.pop_front();
    }

    m_localDispatchBuffer.clear();
    if (!m_localNonCombatBuffer.empty() && !m_localCombatDispatchBuffer.empty()) {
      m_localDispatchBuffer.reserve(pendingCount);
      size_t nonCombatIndex = 0;
      size_t combatIndex = 0;
      while (nonCombatIndex < m_localNonCombatBuffer.size() &&
             combatIndex < m_localCombatDispatchBuffer.size()) {
        if (m_localNonCombatBuffer[nonCombatIndex].sequence <
            m_localCombatDispatchBuffer[combatIndex].sequence) {
          m_localDispatchBuffer.emplace_back(
              m_localNonCombatBuffer[nonCombatIndex++]);
        } else {
          m_localDispatchBuffer.emplace_back(
              m_localCombatDispatchBuffer[combatIndex++]);
        }
      }

      while (nonCombatIndex < m_localNonCombatBuffer.size()) {
        m_localDispatchBuffer.emplace_back(
            m_localNonCombatBuffer[nonCombatIndex++]);
      }
      while (combatIndex < m_localCombatDispatchBuffer.size()) {
        m_localDispatchBuffer.emplace_back(
            m_localCombatDispatchBuffer[combatIndex++]);
      }
    } else if (!m_localNonCombatBuffer.empty()) {
      m_localDispatchBuffer.reserve(nonCombatCount);
      m_localDispatchBuffer.insert(m_localDispatchBuffer.end(),
                                   m_localNonCombatBuffer.begin(),
                                   m_localNonCombatBuffer.end());
    }
  }
  // Lock released - process events without holding lock

  const size_t eventCount =
      m_localNonCombatBuffer.size() + m_localCombatDispatchBuffer.size();
  const bool allCombatEvents =
      (eventCount > 0 && m_localNonCombatBuffer.empty());

  const float cachedGameTime = GameTimeManager::Instance().getTotalGameTimeSeconds();
  auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
  const size_t combatEventCount = m_localCombatDispatchBuffer.size();
  bool useThreading = false;

  if (combatEventCount > 0) {
    auto decision = budgetMgr.shouldUseThreading(
        VoidLight::SystemType::Event, combatEventCount);
    useThreading = decision.shouldThread;
    VOIDLIGHT_DEBUG_ONLY(
        if (!m_threadingEnabled.load(std::memory_order_acquire)) {
          useThreading = false;
        }
    )
  }

  m_preparedCombatBuffer.clear();
  m_preparedCombatBuffer.resize(combatEventCount);
  size_t actualBatchCount = 1;
  bool actualWasThreaded = false;

  // Per-path timing: single-threaded feeds threshold learning, batch feeds hill-climbing
  std::chrono::steady_clock::time_point combatPrepStart;
  std::chrono::steady_clock::time_point combatPrepEnd;

  if (combatEventCount > 0) {
    if (useThreading) {
      // Compute batch strategy (not timed — only actual work is timed)
      auto& threadSystem = VoidLight::ThreadSystem::Instance();
      size_t optimalWorkerCount = budgetMgr.getOptimalWorkers(
          VoidLight::SystemType::Event, combatEventCount);
      auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
          VoidLight::SystemType::Event, combatEventCount, optimalWorkerCount);

      if (batchCount > 1) {
        actualWasThreaded = true;
        actualBatchCount = batchCount;
        m_combatPrepFutures.clear();
        m_combatPrepFutures.reserve(batchCount);

        // Start timing batch work (enqueue + wait) — feeds hill-climbing
        combatPrepStart = std::chrono::steady_clock::now();

        for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
          const size_t startCombatIndex = batchIndex * batchSize;
          if (startCombatIndex >= combatEventCount) {
            break;
          }

          const size_t endCombatIndex =
              std::min(startCombatIndex + batchSize, combatEventCount);
          m_combatPrepFutures.push_back(threadSystem.enqueueTaskWithResult(
              [this, startCombatIndex, endCombatIndex]() {
                try {
                  prepareCombatBatch(startCombatIndex, endCombatIndex);
                } catch (const std::exception& e) {
                  EVENT_ERROR(std::format("Exception in combat prep batch: {}", e.what()));
                } catch (...) {
                  EVENT_ERROR("Unknown exception in combat prep batch");
                }
              },
              VoidLight::TaskPriority::High, "Event_CombatPrep"));
        }

        for (auto& future : m_combatPrepFutures) {
          if (!future.valid()) {
            continue;
          }

          future.get();
        }
        combatPrepEnd = std::chrono::steady_clock::now();
      }
    }

    if (!actualWasThreaded) {
      // Single-threaded — timing feeds threshold learning
      combatPrepStart = std::chrono::steady_clock::now();
      prepareCombatBatch(0, combatEventCount);
      combatPrepEnd = std::chrono::steady_clock::now();
    }
  }

  {
    std::shared_lock<std::shared_mutex> handlerLock(m_handlersMutex);
    if (allCombatEvents) {
      const auto& combatHandlers =
          m_handlersByType[static_cast<size_t>(EventTypeId::Combat)];
      const bool hasCombatHandlers = !combatHandlers.empty();

      for (size_t combatIndex = 0; combatIndex < combatEventCount; ++combatIndex) {
        const auto& pendingDispatch = m_localCombatDispatchBuffer[combatIndex];
        if (combatIndex < m_preparedCombatBuffer.size()) {
          commitPreparedCombatEvent(pendingDispatch,
                                    m_preparedCombatBuffer[combatIndex],
                                    cachedGameTime);
        } else {
          commitPreparedCombatEvent(pendingDispatch, PreparedCombatEvent{}, cachedGameTime);
        }

        if (hasCombatHandlers) {
          dispatchPendingEventWithHandlers(pendingDispatch, combatHandlers,
                                           "deferred dispatch");
        }
      }
    } else {
      size_t preparedCombatCursor = 0;
      for (size_t dispatchIndex = 0; dispatchIndex < eventCount; ++dispatchIndex) {
        const auto& pendingDispatch = m_localDispatchBuffer[dispatchIndex];
        if (pendingDispatch.typeId == EventTypeId::Combat) {
          if (preparedCombatCursor < m_preparedCombatBuffer.size()) {
            commitPreparedCombatEvent(pendingDispatch,
                                      m_preparedCombatBuffer[preparedCombatCursor],
                                      cachedGameTime);
            ++preparedCombatCursor;
          } else {
            commitPreparedCombatEvent(pendingDispatch, PreparedCombatEvent{}, cachedGameTime);
          }
        }

        const auto& typeHandlers =
            m_handlersByType[static_cast<size_t>(pendingDispatch.typeId)];
        if (!typeHandlers.empty()) {
          dispatchPendingEventWithHandlers(pendingDispatch, typeHandlers,
                                           "deferred dispatch");
        }
      }
    }
  }

  double combatPrepMs = std::chrono::duration<double, std::milli>(
      combatPrepEnd - combatPrepStart).count();

  if (combatEventCount > 0 && combatPrepMs > 0.0) {
    budgetMgr.reportExecution(VoidLight::SystemType::Event, combatEventCount,
                              actualWasThreaded, actualBatchCount,
                              combatPrepMs);
  }

  VOIDLIGHT_DEBUG_ONLY(
      // Periodic debug logging (~35 seconds at 60fps)
      static thread_local uint64_t logFrameCounter = 0;
      if (++logFrameCounter % 2100 == 0 && eventCount > 0) {
        EVENT_DEBUG(std::format("Dispatch: {} events ({} combat) [{}, {:.2f}ms prep]",
                                eventCount, combatEventCount,
                                actualWasThreaded ? std::format("{} batches", actualBatchCount) : "single",
                                combatPrepMs));
      }
  )

  // Release pooled events back to pools (after all processing complete)
  if (allCombatEvents) {
    for (const auto& pd : m_localCombatDispatchBuffer) {
      releaseEventToPool(pd.typeId, pd.data.event);
    }
  } else {
    for (const auto& pd : m_localDispatchBuffer) {
      releaseEventToPool(pd.typeId, pd.data.event);
    }
  }
}

void EventManager::releaseEventToPool(EventTypeId typeId, const EventPtr& event) const {
  if (!event) return;

  switch (typeId) {
    case EventTypeId::Weather:
      if (auto we = std::dynamic_pointer_cast<WeatherEvent>(event)) {
        m_weatherPool.release(we);
      }
      break;
    case EventTypeId::NPCSpawn:
      if (auto nse = std::dynamic_pointer_cast<NPCSpawnEvent>(event)) {
        m_npcSpawnPool.release(nse);
      }
      break;
    case EventTypeId::MerchantSpawn:
      if (auto mse = std::dynamic_pointer_cast<MerchantSpawnEvent>(event)) {
        m_merchantSpawnPool.release(mse);
      }
      break;
    case EventTypeId::ResourceChange:
      if (auto rce = std::dynamic_pointer_cast<ResourceChangeEvent>(event)) {
        m_resourceChangePool.release(rce);
      }
      break;
    case EventTypeId::World:
      if (auto we = std::dynamic_pointer_cast<WorldEvent>(event)) {
        m_worldPool.release(we);
      }
      break;
    case EventTypeId::Camera:
      if (auto ce = std::dynamic_pointer_cast<CameraEvent>(event)) {
        m_cameraPool.release(ce);
      }
      break;
    case EventTypeId::ParticleEffect:
      if (auto pe = std::dynamic_pointer_cast<ParticleEffectEvent>(event)) {
        m_particleEffectPool.release(pe);
      }
      break;
    case EventTypeId::Combat:
      if (auto damageEvent = std::dynamic_pointer_cast<DamageEvent>(event)) {
        m_damagePool.release(damageEvent);
      }
      break;
    case EventTypeId::CollisionObstacleChanged:
      if (auto coce = std::dynamic_pointer_cast<CollisionObstacleChangedEvent>(event)) {
        m_collisionObstacleChangedPool.release(coce);
      }
      break;
    default:
      // Non-pooled event types
      break;
  }
}

// ==================== Performance & Diagnostics ====================

PerformanceStats EventManager::getPerformanceStats(EventTypeId typeId) const {
  std::lock_guard<std::mutex> lock(m_perfMutex);
  return m_performanceStats[static_cast<size_t>(typeId)];
}

void EventManager::resetPerformanceStats() const {
  std::lock_guard<std::mutex> lock(m_perfMutex);
  for (auto &stats : m_performanceStats) {
    stats.reset();
  }
}

size_t EventManager::getPendingEventCount() const {
  std::lock_guard<std::mutex> lock(m_dispatchMutex);
  return getPendingQueueSizeUnsafe();
}

void EventManager::clearEventPools() {
  m_weatherPool.clear();
  m_npcSpawnPool.clear();
  m_merchantSpawnPool.clear();
  m_resourceChangePool.clear();
  m_worldPool.clear();
  m_cameraPool.clear();
  m_particleEffectPool.clear();
  m_collisionObstacleChangedPool.clear();
  m_damagePool.clear();
}

// ==================== Helper Methods ====================

void EventManager::clearPendingDispatchQueues() const {
  std::lock_guard<std::mutex> lock(m_dispatchMutex);

  while (!m_pendingDispatch.empty()) {
    releaseEventToPool(m_pendingDispatch.front().typeId,
                       m_pendingDispatch.front().data.event);
    m_pendingDispatch.pop_front();
  }

  while (!m_pendingCombatDispatch.empty()) {
    releaseEventToPool(EventTypeId::Combat,
                       m_pendingCombatDispatch.front().data.event);
    m_pendingCombatDispatch.pop_front();
  }
}

uint64_t EventManager::getCurrentTimeNanos() const {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}
