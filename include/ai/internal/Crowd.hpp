/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

// Internal crowd utilities for lightweight separation steering.
#ifndef AI_INTERNAL_CROWD_HPP
#define AI_INTERNAL_CROWD_HPP

#include "core/Logger.hpp"
#include "entities/EntityHandle.hpp"  // EntityID
#include "utils/Vector2D.hpp"
#include <cstdint>
#include <vector>

namespace AIInternal {

VOIDLIGHT_STATS_ONLY(
struct CrowdStats {
  uint64_t queryCount{0};
  uint64_t cacheHits{0};
  uint64_t cacheMisses{0};
  uint64_t resultsCount{0};
};
)

// Counts nearby entities within a given area, filtering for actual entities only
// (excludes static objects, triggers, and self)
// - excludeId: entity ID to exclude from count (typically the querying entity)
// - center: center of query area
// - radius: query radius
// Returns: count of nearby dynamic/kinematic entities
int CountNearbyEntities(EntityID excludeId, const Vector2D &center, float radius);

// Gets nearby entities with their positions for crowd analysis
// - excludeId: entity ID to exclude from results (typically the querying entity)
// - center: center of query area
// - radius: query radius
// - outPositions: vector to fill with nearby entity positions
// Returns: count of nearby entities (same as outPositions.size())
int GetNearbyEntitiesWithPositions(EntityID excludeId, const Vector2D &center, float radius,
                                   std::vector<Vector2D> &outPositions);

// Invalidates spatial query cache for new frame
// Call this at the start of each AI update cycle to ensure cache freshness
// - frameNumber: current frame number for cache invalidation
void InvalidateSpatialCache(uint64_t frameNumber);

VOIDLIGHT_STATS_ONLY(
// Crowd query stats (aggregated across worker threads)
CrowdStats GetCrowdStats();
void ResetCrowdStats();
)

// Returns reference to thread-local position buffer for crowd queries
// Caller must call clear() before use. Avoids per-call allocations.
std::vector<Vector2D> &GetNearbyPositionBuffer();

} // namespace AIInternal

#endif // AI_INTERNAL_CROWD_HPP
