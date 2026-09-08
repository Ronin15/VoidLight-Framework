/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

# Spatial Query Optimization Plan

**Status:** Design Phase — Future Implementation. Not current architecture. Do not implement from this document unless a numbered slice adopts it.
**Created:** 2025-11-16
**Expected Benefit:** 15-25% reduction in spatial query overhead
**Risk Level:** Medium (requires refactoring AIManager and CollisionManager)

---

## Executive Summary

Currently, both **AIManager** and **CollisionManager** maintain separate spatial data structures for entity queries. This architectural plan proposes a **unified SpatialQueryManager** to eliminate redundancy, reduce memory footprint, and improve cache coherency.

---

## Current Architecture Analysis

### Redundant Spatial Structures

#### AIManager (AIManager.hpp:543-549)
```cpp
// Camera bounds cache for culling
struct CameraBoundsCache {
    float minX, minY, maxX, maxY;
    bool valid{false};
};
CameraBoundsCache m_cameraBoundsCache;

// Used for spatial queries during AI behavior processing
```

**Purpose:** Cull entities outside camera view during AI updates

#### CollisionManager (CollisionManager.hpp:471-472)
```cpp
HierarchicalSpatialHash m_staticHash;   // World geometry (rebuilt on tile changes)
HierarchicalSpatialHash m_dynamicHash;  // Moving entities (rebuilt per frame)
```

**Purpose:** Broad-phase collision detection and spatial queries

### Overlap Analysis

Both managers need to:
1. **Query entities by radius** (AI: nearby entities, Collision: potential colliders)
2. **Query entities by AABB** (AI: camera frustum, Collision: swept volumes)
3. **Update entity positions** (both maintain position caches)
4. **Handle static vs. dynamic entities** (world geometry vs. moving objects)

**Redundancy:** Two separate spatial hash implementations with similar query patterns.

---

## Proposed Architecture

### New Component: SpatialQueryManager

**File:** `include/managers/SpatialQueryManager.hpp`

```cpp
/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef SPATIAL_QUERY_MANAGER_HPP
#define SPATIAL_QUERY_MANAGER_HPP

#include "utils/Vector2D.hpp"
#include <vector>
#include <cstdint>
#include <shared_mutex>

/**
 * @brief Unified spatial query system for AI and collision subsystems
 *
 * Eliminates redundant spatial hash structures by providing a single
 * authoritative source for spatial queries. Supports both static (world
 * geometry) and dynamic (moving entities) spatial indexing.
 *
 * Thread Safety:
 * - Read queries use shared_lock (parallel safe)
 * - Position updates use unique_lock (exclusive)
 * - Rebuild operations use unique_lock (exclusive)
 */
class SpatialQueryManager {
public:
    // Singleton access
    static SpatialQueryManager& Instance();

    /**
     * @brief Initialize the spatial query system
     * @param worldWidth Width of the world in tiles
     * @param worldHeight Height of the world in tiles
     * @param cellSize Size of each spatial hash cell (default: 128 pixels)
     * @return true if initialization successful
     */
    bool init(int worldWidth, int worldHeight, int cellSize = 128);

    /**
     * @brief Clean up spatial query system
     */
    void clean();

    // ==================== Entity Registration ====================

    /**
     * @brief Register a dynamic entity for spatial tracking
     * @param entityId Unique entity identifier
     * @param position Initial position
     * @param radius Bounding radius for queries
     * @return Handle for future updates/removal
     */
    uint64_t registerDynamicEntity(uint64_t entityId, const Vector2D& position, float radius);

    /**
     * @brief Register a static entity (world geometry, obstacles)
     * @param entityId Unique entity identifier
     * @param aabbMin AABB minimum corner
     * @param aabbMax AABB maximum corner
     * @return Handle for future removal
     */
    uint64_t registerStaticEntity(uint64_t entityId, const Vector2D& aabbMin, const Vector2D& aabbMax);

    /**
     * @brief Remove entity from spatial tracking
     * @param handle Handle returned by register*Entity
     */
    void unregisterEntity(uint64_t handle);

    // ==================== Position Updates ====================

    /**
     * @brief Update dynamic entity position (thread-safe, batched internally)
     * @param handle Entity handle
     * @param newPosition New position
     */
    void updateEntityPosition(uint64_t handle, const Vector2D& newPosition);

    /**
     * @brief Batch update entity positions (more efficient than individual updates)
     * @param updates Vector of {handle, position} pairs
     */
    struct PositionUpdate {
        uint64_t handle;
        Vector2D position;
    };
    void batchUpdatePositions(const std::vector<PositionUpdate>& updates);

    // ==================== Spatial Queries ====================

    /**
     * @brief Query entities within radius of a point
     * @param center Query center
     * @param radius Query radius
     * @param outHandles Output vector of entity handles
     * @param includeDynamic Include dynamic entities (default: true)
     * @param includeStatic Include static entities (default: true)
     */
    void queryRadius(const Vector2D& center, float radius,
                     std::vector<uint64_t>& outHandles,
                     bool includeDynamic = true,
                     bool includeStatic = true) const;

    /**
     * @brief Query entities within AABB
     * @param aabbMin AABB minimum corner
     * @param aabbMax AABB maximum corner
     * @param outHandles Output vector of entity handles
     * @param includeDynamic Include dynamic entities (default: true)
     * @param includeStatic Include static entities (default: true)
     */
    void queryAABB(const Vector2D& aabbMin, const Vector2D& aabbMax,
                   std::vector<uint64_t>& outHandles,
                   bool includeDynamic = true,
                   bool includeStatic = true) const;

    /**
     * @brief Query nearest N entities to a point
     * @param center Query center
     * @param maxCount Maximum number of results
     * @param maxRadius Maximum search radius
     * @param outHandles Output vector of entity handles (sorted by distance)
     * @param outDistances Output vector of distances (optional)
     */
    void queryNearest(const Vector2D& center, size_t maxCount, float maxRadius,
                      std::vector<uint64_t>& outHandles,
                      std::vector<float>* outDistances = nullptr) const;

    // ==================== Specialized Queries ====================

    /**
     * @brief Get entity position by handle (fast lookup)
     * @param handle Entity handle
     * @return Position, or (0,0) if handle invalid
     */
    Vector2D getEntityPosition(uint64_t handle) const;

    /**
     * @brief Check if handle is valid
     */
    bool isHandleValid(uint64_t handle) const;

    // ==================== Rebuild Operations ====================

    /**
     * @brief Rebuild static spatial hash (call after world changes)
     * @param async If true, rebuild on ThreadSystem (default: false)
     */
    void rebuildStaticHash(bool async = false);

    /**
     * @brief Rebuild dynamic spatial hash (typically called per frame)
     * @note This is fast (O(N)) due to pre-allocated buffers
     */
    void rebuildDynamicHash();

    // ==================== Statistics ====================

    struct Statistics {
        size_t dynamicEntityCount;
        size_t staticEntityCount;
        size_t occupiedCells;
        size_t totalCells;
        double lastRebuildTimeMs;
        size_t queriesThisFrame;
    };

    Statistics getStatistics() const;

private:
    SpatialQueryManager() = default;
    ~SpatialQueryManager() = default;

    // Internal spatial hash implementation
    struct HierarchicalSpatialHash {
        // Cell grid (coarse + fine levels)
        std::vector<std::vector<uint64_t>> coarseCells;  // 512x512 cells
        std::vector<std::vector<uint64_t>> fineCells;    // Per-coarse-cell refinement

        int cellSize{128};
        int worldWidth{0};
        int worldHeight{0};

        void rebuild(const std::vector<PositionUpdate>& entities);
        void query(const Vector2D& aabbMin, const Vector2D& aabbMax,
                   std::vector<uint64_t>& outHandles) const;
    };

    HierarchicalSpatialHash m_staticHash;   // World geometry (rebuilt on demand)
    HierarchicalSpatialHash m_dynamicHash;  // Moving entities (rebuilt per frame)

    // Entity data storage
    struct EntityData {
        uint64_t entityId;
        Vector2D position;
        float radius;
        bool isStatic;
        bool valid;
    };
    std::vector<EntityData> m_entities;  // Handle = index into this vector

    // Thread safety
    mutable std::shared_mutex m_spatialMutex;

    // Statistics
    mutable Statistics m_stats;
};

#endif // SPATIAL_QUERY_MANAGER_HPP
```

---

## Migration Strategy

### Phase 1: Implement SpatialQueryManager (Week 1-2)

1. **Create base implementation**
   - Implement hierarchical spatial hash
   - Add entity registration/unregistration
   - Implement core query methods

2. **Add comprehensive tests**
   - Unit tests for registration/queries
   - Performance benchmarks vs. current approach
   - Thread safety stress tests

### Phase 2: Integrate with CollisionManager (Week 3)

1. **Replace CollisionManager spatial structures**
   - Remove `m_staticHash` and `m_dynamicHash`
   - Use `SpatialQueryManager::queryAABB()` for broad-phase
   - Batch position updates via `batchUpdatePositions()`

2. **Validate collision system**
   - Run full collision test suite
   - Benchmark performance vs. baseline
   - Ensure no regressions in accuracy

### Phase 3: Integrate with AIManager (Week 4)

1. **Replace AIManager camera bounds cache**
   - Remove `m_cameraBoundsCache`
   - Use `SpatialQueryManager::queryAABB()` for frustum culling
   - Use `queryNearest()` for behavior target selection

2. **Validate AI system**
   - Run AI scaling benchmarks
   - Verify entity update performance
   - Test 10K+ entity scenarios

### Phase 4: Optimization and Tuning (Week 5)

1. **SIMD optimization**
   - Apply SIMD to distance calculations in `queryNearest()`
   - Optimize AABB intersection tests

2. **Cache optimization**
   - Profile cache miss rates
   - Adjust cell sizes for optimal performance
   - Tune batch sizes

---

## Expected Performance Impact

### Memory Savings

**Before:**
```
AIManager camera cache:      ~48 bytes
CollisionManager static hash: ~2-4 MB (depends on world size)
CollisionManager dynamic hash: ~1-2 MB (depends on entity count)
Total:                       ~3-6 MB
```

**After:**
```
SpatialQueryManager unified hash: ~2-3 MB
Total:                           ~2-3 MB
```

**Savings:** 1-3 MB (30-50% reduction)

### Performance Benefits

1. **Reduced cache pollution** - Single spatial structure improves locality
2. **Eliminated duplicate queries** - AIManager and CollisionManager share results
3. **Batch update optimization** - Single rebuild instead of two separate rebuilds
4. **SIMD opportunities** - Centralized queries enable aggressive SIMD optimization

**Expected Speedup:**
- Spatial queries: 15-25% faster (fewer cache misses)
- Position updates: 10-20% faster (single batch instead of multiple)
- Memory bandwidth: 20-30% reduction (smaller working set)

---

## Risks and Mitigation

### Risk 1: Contention on Shared Spatial Structure
**Mitigation:** Use `shared_mutex` for read queries (parallel safe), unique_lock only for updates

### Risk 2: Query Interface Overhead
**Mitigation:** Inline hot-path methods, batch queries where possible

### Risk 3: Breaking Existing Behavior
**Mitigation:** Comprehensive test suite, side-by-side validation during migration

### Risk 4: Performance Regression
**Mitigation:** Benchmark at each phase, rollback if <10% improvement not achieved

---

## Success Criteria

1. ✅ **Memory reduction:** 30%+ savings in spatial data structures
2. ✅ **Performance improvement:** 15%+ speedup in spatial queries
3. ✅ **Test coverage:** All existing collision/AI tests pass
4. ✅ **Code clarity:** Reduced duplication, clearer ownership of spatial data

---

## Future Extensions

1. **Frustum culling service** - Unified camera frustum queries for rendering
2. **Pathfinding integration** - Use spatial queries for navigation graph updates
3. **Physics spatial queries** - Raycasts, sweeps, overlap tests
4. **Spatial audio** - Query nearby sound sources for 3D audio

---

## References

- **Current Implementation:**
  - `include/managers/AIManager.hpp:543-549` (camera bounds cache)
  - `include/managers/CollisionManager.hpp:440-473` (hierarchical spatial hash)

- **Related Systems:**
  - `include/managers/PathfinderManager.hpp` (potential future integration)
  - `include/utils/SIMDMath.hpp` (SIMD optimization opportunities)

---

**Decision Required:** Approve this plan for implementation in next development cycle?

**Estimated Effort:** 5 weeks (1 engineer)
**Priority:** Medium (optimization, not critical)
**Dependencies:** None (can be implemented in parallel with other features)
