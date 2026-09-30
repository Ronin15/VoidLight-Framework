/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

// Internal crowd utilities for lightweight separation steering.
#ifndef AI_INTERNAL_CROWD_HPP
#define AI_INTERNAL_CROWD_HPP

#include "core/Logger.hpp"
#include "utils/Vector2D.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace AIInternal {

VOIDLIGHT_STATS_ONLY(
    struct CrowdStats {
        uint64_t queryCount{0};
        uint64_t cacheHits{0};
        uint64_t cacheMisses{0};
        uint64_t resultsCount{0};
    };)

// Crowd queries read AIManager::scanActiveIndicesInRadius filtered to
// EntityKind::NPC (results cached per frame per worker thread).
// Valid inside AIManager's update slot, where the active index buffer is built.

// Counts nearby active NPCs within radius of center, excluding
// excludeEdmIndex (typically the querying entity's EDM index).
int CountNearbyEntities(size_t excludeEdmIndex, const Vector2D& center, float radius);

// Same query as CountNearbyEntities; fills outPositions with the EDM positions
// of the nearby entities. Returns outPositions.size().
int GetNearbyEntitiesWithPositions(size_t excludeEdmIndex, const Vector2D& center,
    float radius, std::vector<Vector2D>& outPositions);

// Starts a new crowd-cache frame: advances a monotonic stamp (never reset) so
// every worker's thread_local cache drops earlier entries. AIManager::update()
// calls it on the main thread before dispatching batches.
void InvalidateSpatialCache();

VOIDLIGHT_STATS_ONLY(
    // Crowd query stats (aggregated across worker threads)
    CrowdStats GetCrowdStats();
    void ResetCrowdStats();)

// Returns reference to thread-local position buffer for crowd queries
// Caller must call clear() before use. Avoids per-call allocations.
std::vector<Vector2D>& GetNearbyPositionBuffer();

} // namespace AIInternal

#endif // AI_INTERNAL_CROWD_HPP
