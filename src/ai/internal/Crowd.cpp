/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/internal/Crowd.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include <algorithm>
#include <array>
#include <atomic>

namespace AIInternal {

// AUTHORITATIVE FRAME STAMP (shared, lock-free)
// The spatial query cache below is `thread_local` — one instance per worker
// thread in the persistent ThreadSystem pool. The per-frame freshness stamp must
// therefore be visible to every worker, not just the main thread. The main
// thread advances this stamp once per AI frame (InvalidateSpatialCache) and
// every worker reads it at lookup/store time, so entries from a previous frame
// auto-invalidate without a per-thread call. The stamp is owned here and never
// resets: AIManager's frame counter restarts at 0 on init() and state
// transitions while the thread_local caches survive, so reusing it would let a
// new state hit cached EDM indices from the old one. relaxed ordering is
// sufficient: the stamp is published before the task queue dispatches batches
// (which establishes happens-before) and stays constant while workers run.
// Starts at 1 so never-stored cache entries (stamp 0) never match.
static std::atomic<uint64_t> g_authoritativeFrame{1};

// PERFORMANCE OPTIMIZATION: Spatial query cache to reduce AIManager scan load
// Caches scanActiveIndicesInRadius results (EDM indices) within the same frame
// to eliminate redundant scans. Key insight: many nearby entities query the
// same spatial regions each frame
//
// MEMORY MANAGEMENT: Uses buffer reuse pattern to avoid per-frame allocations
// - Pre-allocated fixed-size array (no dynamic allocation per frame)
// - Marks entries as stale instead of clearing
// - Reuses vector capacity across frames (CLAUDE.md requirement)
struct SpatialQueryCache {
    struct CacheEntry {
        uint64_t frameNumber{0};
        uint64_t queryKey{0}; // Store hash for fast validation (cheap integer compare)
        std::vector<size_t> results{};
    };

    static constexpr size_t CACHE_SIZE = 64;
    std::array<CacheEntry, CACHE_SIZE> entries; // Fixed-size, no heap allocations

    SpatialQueryCache() {
        // Pre-allocate capacity for all vectors to avoid per-frame reallocations
        for (auto& entry : entries) {
            entry.results.reserve(32); // Typical query returns ~10-30 entities
            entry.frameNumber = 0;
            entry.queryKey = 0;
        }
    }

    // Simple hash for position+radius (quantize to reduce unique keys)
    static uint64_t hashQuery(const Vector2D& center, float radius) {
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

    bool lookup(const Vector2D& center, float radius, uint64_t currentFrame,
        std::vector<size_t>& outResults) {
        uint64_t key = hashQuery(center, radius);
        size_t index = key % CACHE_SIZE;

        const CacheEntry& entry = entries[index];
        // Frame-based validation: entry is valid only if it was stamped during the
        // authoritative current frame. Stamps from prior frames auto-invalidate.
        if (entry.frameNumber == currentFrame && entry.queryKey == key) {
            outResults = entry.results;
            return true;
        }
        return false;
    }

    void store(const Vector2D& center, float radius, uint64_t currentFrame,
        const std::vector<size_t>& results) {
        uint64_t key = hashQuery(center, radius);
        size_t index = key % CACHE_SIZE;

        CacheEntry& entry = entries[index];
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
    })

// Nearby active-tier NPC EDM indices from
// AIManager::scanActiveIndicesInRadius, through the per-frame thread-local cache.
// Called from AI batch workers: the scan reads AIManager's frame-built active
// index buffer and EDM transforms, the same read set as the other AIManager
// scan*InRadius helpers behaviors use.
static const std::vector<size_t>& queryNearbyIndices(const Vector2D& center,
    float radius) {
    VOIDLIGHT_STATS_ONLY(recordCrowdQuery();)

    static thread_local std::vector<size_t> queryResults;

    // Read the authoritative frame once; the thread-local cache self-invalidates
    // against it so stale cross-frame data is never returned on worker threads.
    uint64_t currentFrame = g_authoritativeFrame.load(std::memory_order_relaxed);

    bool cacheHit = g_spatialCache.lookup(center, radius, currentFrame, queryResults);
    VOIDLIGHT_STATS_ONLY(recordCrowdCache(cacheHit);)
    if (!cacheHit) {
        AIManager::Instance().scanActiveIndicesInRadius(center, radius, queryResults);
        // Crowds are NPCs; projectiles, area effects, and items are not.
        const auto& edm = EntityDataManager::Instance();
        std::erase_if(queryResults, [&edm](size_t idx) {
            return edm.getHotDataByIndex(idx).kind != EntityKind::NPC;
        });
        g_spatialCache.store(center, radius, currentFrame, queryResults);
    }
    return queryResults;
}

int CountNearbyEntities(size_t excludeEdmIndex, const Vector2D& center,
    float radius) {
    const auto& nearby = queryNearbyIndices(center, radius);
    int count = static_cast<int>(nearby.size()) -
        static_cast<int>(std::count(nearby.begin(), nearby.end(), excludeEdmIndex));
    VOIDLIGHT_STATS_ONLY(recordCrowdResults(count);)
    return count;
}

int GetNearbyEntitiesWithPositions(size_t excludeEdmIndex, const Vector2D& center,
    float radius,
    std::vector<Vector2D>& outPositions) {
    outPositions.clear();

    const auto& nearby = queryNearbyIndices(center, radius);
    const auto& edm = EntityDataManager::Instance();
    for (size_t idx : nearby) {
        if (idx != excludeEdmIndex) {
            outPositions.push_back(edm.getHotDataByIndex(idx).transform.position);
        }
    }

    int count = static_cast<int>(outPositions.size());
    VOIDLIGHT_STATS_ONLY(recordCrowdResults(count);)
    return count;
}

void InvalidateSpatialCache() {
    g_authoritativeFrame.fetch_add(1, std::memory_order_relaxed);
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
    })

std::vector<Vector2D>& GetNearbyPositionBuffer() { return g_nearbyPositionBuffer; }

} // namespace AIInternal
