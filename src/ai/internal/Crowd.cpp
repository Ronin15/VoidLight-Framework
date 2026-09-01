/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/internal/Crowd.hpp"
#include "managers/CollisionManager.hpp"
#include <algorithm>
#include <array>
#include <atomic>

namespace AIInternal {

// AUTHORITATIVE FRAME COUNTER (shared, lock-free)
// The spatial query cache below is `thread_local` — one instance per worker
// thread in the persistent ThreadSystem pool. The per-frame freshness stamp must
// therefore be visible to every worker, not just the main thread. We keep a
// single file-scope atomic that the main thread writes once per frame (via
// InvalidateSpatialCache) and every worker reads at lookup/store time. Each
// worker's thread_local cache stamps and validates its entries against this
// value, so entries from a previous frame auto-invalidate the moment the frame
// advances — without any per-thread invalidation call that would never reach
// the workers. relaxed ordering is sufficient: the frame is published before the
// task queue dispatches batches (which establishes happens-before) and stays
// constant while workers run.
static std::atomic<uint64_t> g_authoritativeFrame{0};

// PERFORMANCE OPTIMIZATION: Spatial query cache to reduce CollisionManager load
// Caches queryArea results within the same frame to eliminate redundant spatial
// queries Key insight: Many nearby entities query the same spatial regions each
// frame
//
// MEMORY MANAGEMENT: Uses buffer reuse pattern to avoid per-frame allocations
// - Pre-allocated fixed-size array (no dynamic allocation per frame)
// - Marks entries as stale instead of clearing
// - Reuses vector capacity across frames (AGENTS.md requirement)
struct SpatialQueryCache {
  struct CacheEntry {
    uint64_t frameNumber{0};
    uint64_t queryKey{0}; // Store hash for fast validation (cheap integer compare)
    std::vector<EntityID> results{};
  };

  static constexpr size_t CACHE_SIZE = 64;
  std::array<CacheEntry, CACHE_SIZE> entries; // Fixed-size, no heap allocations

  SpatialQueryCache() {
    // Pre-allocate capacity for all vectors to avoid per-frame reallocations
    for (auto &entry : entries) {
      entry.results.reserve(32); // Typical query returns ~10-30 entities
      entry.frameNumber = 0;
      entry.queryKey = 0;
    }
  }

  // Simple hash for position+radius (quantize to reduce unique keys)
  static uint64_t hashQuery(const Vector2D &center, float radius) {
    // Quantize position to 8-pixel grid to increase cache hits
    int32_t const qx = static_cast<int32_t>(center.getX() / 8.0f);
    int32_t const qy = static_cast<int32_t>(center.getY() / 8.0f);
    int32_t const qr = static_cast<int32_t>(radius / 8.0f);
    // Combine into hash
    uint64_t hash = static_cast<uint64_t>(qx);
    hash ^= (static_cast<uint64_t>(qy) << 16);
    hash ^= (static_cast<uint64_t>(qr) << 32);
    return hash;
  }

  bool lookup(const Vector2D &center, float radius, uint64_t currentFrame,
              std::vector<EntityID> &outResults) {
    uint64_t key = hashQuery(center, radius);
    size_t index = key % CACHE_SIZE;

    const CacheEntry &entry = entries[index];
    // Frame-based validation: entry is valid only if it was stamped during the
    // authoritative current frame. Stamps from prior frames auto-invalidate.
    if (entry.frameNumber == currentFrame && entry.queryKey == key) {
      outResults = entry.results;
      return true;
    }
    return false;
  }

  void store(const Vector2D &center, float radius, uint64_t currentFrame,
             const std::vector<EntityID> &results) {
    uint64_t key = hashQuery(center, radius);
    size_t index = key % CACHE_SIZE;

    CacheEntry &entry = entries[index];
    entry.frameNumber = currentFrame;
    entry.queryKey = key;
    entry.results = results; // Reuses existing capacity when possible
  }
};

// Thread-local cache instance (one per worker thread)
static thread_local SpatialQueryCache g_spatialCache;

// Thread-local position buffer for GetNearbyEntitiesWithPositions callers
// Avoids per-call allocations when callers use GetNearbyPositionBuffer()
static thread_local std::vector<Vector2D> g_nearbyPositionBuffer;

VOIDLIGHT_STATS_ONLY(
static std::atomic<uint64_t> g_queryCount{0};
static std::atomic<uint64_t> g_cacheHits{0};
static std::atomic<uint64_t> g_cacheMisses{0};
static std::atomic<uint64_t> g_resultsCount{0};

inline void recordCrowdQuery() {
  g_queryCount.fetch_add(1, std::memory_order_relaxed);
}

inline void recordCrowdCache(bool cacheHit) {
  if (cacheHit) {
    g_cacheHits.fetch_add(1, std::memory_order_relaxed);
  } else {
    g_cacheMisses.fetch_add(1, std::memory_order_relaxed);
  }
}

inline void recordCrowdResults(int count) {
  g_resultsCount.fetch_add(static_cast<uint64_t>(count),
                           std::memory_order_relaxed);
}
)

int CountNearbyEntities(EntityID excludeId, const Vector2D &center,
                        float radius) {
  const auto &cm = CollisionManager::Instance();

VOIDLIGHT_STATS_ONLY(recordCrowdQuery();)

  // Use thread-local vector to avoid repeated allocations
  static thread_local std::vector<EntityID> queryResults;
  queryResults.clear();

  // Read the authoritative frame once; the thread-local cache self-invalidates
  // against it so stale cross-frame data is never returned on worker threads.
  uint64_t currentFrame = g_authoritativeFrame.load(std::memory_order_relaxed);

  // PERFORMANCE: Check spatial cache before expensive queryArea call
  bool cacheHit = g_spatialCache.lookup(center, radius, currentFrame, queryResults);
VOIDLIGHT_STATS_ONLY(recordCrowdCache(cacheHit);)
  if (!cacheHit) {
    // Cache miss - perform actual collision query
    VoidLight::AABB area(center.getX() - radius, center.getY() - radius,
                            radius * 2.0f, radius * 2.0f);
    cm.queryArea(area, queryResults);

    // Store result in cache for subsequent queries in same frame
    g_spatialCache.store(center, radius, currentFrame, queryResults);
  }

  // Count only actual entities (dynamic/kinematic, non-trigger, excluding self)
  int count = static_cast<int>(std::count_if(
      queryResults.begin(), queryResults.end(), [excludeId, &cm](auto id) {
        return id != excludeId && (cm.isDynamic(id) || cm.isKinematic(id)) &&
               !cm.isTrigger(id);
      }));
VOIDLIGHT_STATS_ONLY(recordCrowdResults(count);)
  return count;
}

int GetNearbyEntitiesWithPositions(EntityID excludeId, const Vector2D &center,
                                   float radius,
                                   std::vector<Vector2D> &outPositions) {
  outPositions.clear();

  const auto &cm = CollisionManager::Instance();

VOIDLIGHT_STATS_ONLY(recordCrowdQuery();)

  // Use thread-local vector to avoid repeated allocations
  static thread_local std::vector<EntityID> queryResults;
  queryResults.clear();

  // Read the authoritative frame once; the thread-local cache self-invalidates
  // against it so stale cross-frame data is never returned on worker threads.
  uint64_t currentFrame = g_authoritativeFrame.load(std::memory_order_relaxed);

  // PERFORMANCE: Check spatial cache before expensive queryArea call
  bool cacheHit = g_spatialCache.lookup(center, radius, currentFrame, queryResults);
VOIDLIGHT_STATS_ONLY(recordCrowdCache(cacheHit);)
  if (!cacheHit) {
    // Cache miss - perform actual collision query
    VoidLight::AABB area(center.getX() - radius, center.getY() - radius,
                            radius * 2.0f, radius * 2.0f);
    cm.queryArea(area, queryResults);

    // Store result in cache for subsequent queries in same frame
    g_spatialCache.store(center, radius, currentFrame, queryResults);
  }

  // Collect positions of actual entities (dynamic/kinematic, non-trigger,
  // excluding self)
  for (auto id : queryResults) {
    if (id != excludeId && (cm.isDynamic(id) || cm.isKinematic(id)) &&
        !cm.isTrigger(id)) {
      Vector2D entityPos;
      if (cm.getBodyCenter(id, entityPos)) {
        outPositions.push_back(entityPos);
      }
    }
  }

  int count = static_cast<int>(outPositions.size());
VOIDLIGHT_STATS_ONLY(recordCrowdResults(count);)
  return count;
}

void InvalidateSpatialCache(uint64_t frameNumber) {
  // Publish the new authoritative frame for all worker threads. Each worker's
  // thread_local cache validates entries against this value, so prior-frame
  // stamps auto-invalidate without any per-thread call. Released before the AI
  // batch dispatch, which establishes happens-before for the workers' reads.
  g_authoritativeFrame.store(frameNumber, std::memory_order_relaxed);
}

VOIDLIGHT_STATS_ONLY(
CrowdStats GetCrowdStats() {
  CrowdStats stats{};
  stats.queryCount = g_queryCount.load(std::memory_order_relaxed);
  stats.cacheHits = g_cacheHits.load(std::memory_order_relaxed);
  stats.cacheMisses = g_cacheMisses.load(std::memory_order_relaxed);
  stats.resultsCount = g_resultsCount.load(std::memory_order_relaxed);
  return stats;
}

void ResetCrowdStats() {
  g_queryCount.store(0, std::memory_order_relaxed);
  g_cacheHits.store(0, std::memory_order_relaxed);
  g_cacheMisses.store(0, std::memory_order_relaxed);
  g_resultsCount.store(0, std::memory_order_relaxed);
}
)

std::vector<Vector2D> &GetNearbyPositionBuffer() { return g_nearbyPositionBuffer; }

} // namespace AIInternal
