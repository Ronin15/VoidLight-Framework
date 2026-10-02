# PathfinderManager

## Overview

The PathfinderManager is a high-performance, centralized pathfinding service designed for the VoidLight-Framework. It provides thread-safe pathfinding capabilities that scale to support 10,000+ AI entities while maintaining 60+ FPS performance. The system features intelligent caching, dynamic obstacle integration, and seamless ThreadSystem integration.

## Architecture

### Design Patterns
- **Singleton Pattern**: Thread-safe singleton with proper lifecycle management
- **Producer-Consumer**: `requestPathToEDM` on workers, `commitCompletedPaths` on the main thread (no public callbacks)
- **Cache-Aside**: Intelligent path caching with automatic invalidation
- **Event-Driven**: Dynamic obstacle updates from CollisionManager

### Core Features
- **High Performance**: Optimized A* implementation with SIMD-friendly data structures
- **Thread Safety**: Lock-free request queuing with minimal contention
- **Smart Caching**: Path result caching with collision-aware invalidation
- **Dynamic Obstacles**: Real-time integration with CollisionManager changes
- **Priority Scheduling**: WorkerBudget integration for performance-critical requests

## EntityDataManager Integration

PathfinderManager integrates with EntityDataManager (EDM) to store path results directly in entity data, eliminating separate path storage and enabling persistent navigation state.

### EDM Path Storage

Path state lives in EDM, not in the manager. `PathData`
(`include/ai/BehaviorCommonState.hpp`) holds the per-entity navigation state
(`pathLength`, `navIndex`, timers, `currentWaypoint`, `hasPath`, and the atomic
`pathRequestPending` / `latestPathRequestId` request tokens). Waypoints are
stored in a fixed per-entity slot (`EDM::getWaypointSlot(index)`,
`FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY`).

### EDM-Based Path Requests

```cpp
uint64_t requestPathToEDM(size_t edmIndex, const Vector2D& start,
                          const Vector2D& goal,
                          Priority priority = Priority::Normal);
```

Behaviors call it from AI worker batches with the entity's EDM index. It
returns 0 when the manager is not initialized, is shut down, or has no grid.

### Path Result Delivery

Workers compute the path and enqueue a completion payload. `AIManager::update()`
calls `commitCompletedPaths()` on the main thread, which for each completion:

- skips it if the target handle is no longer valid or no longer maps to that EDM index
- skips it if its request token is not the entity's `latestPathRequestId` (stale)
- clears `pathRequestPending` when no path was found
- otherwise copies the waypoints into `EDM::getWaypointSlot()` and calls `EDM::finalizePath(index, length)`

## Public API Reference

### Initialization and Lifecycle

#### `static PathfinderManager& Instance()`
Gets the thread-safe singleton instance.
```cpp
PathfinderManager& pathfinder = PathfinderManager::Instance();
```

#### `bool init()`
Initializes the PathfinderManager and subscribes to collision events.
- **Returns**: `true` if successful, `false` otherwise
- **Thread Safety**: Safe to call from any thread

#### `bool isInitialized() const`
Checks if the manager has been properly initialized.

#### `void clean()`
Shuts down the pathfinder and releases all resources.

#### `void prepareForStateTransition()`
Waits for in-flight grid rebuilds, clears cached paths, and drops the
pathfinding grid. The manager stays initialized. Loading waits until the new
world's `StaticCollidersReady` rebuild makes `isGridReady()` true again.

### Request Management

#### Priority Levels
```cpp
enum class Priority : int {
    Critical = 0,  // Player movement, emergency AI
    High     = 1,  // Combat AI, important NPCs
    Normal   = 2,  // General AI movement
    Low      = 3   // Background/idle AI
};
```

#### `uint64_t requestPathToEDM(size_t edmIndex, const Vector2D& start, const Vector2D& goal, Priority priority = Priority::Normal)`

Production API. The worker computes the path; `commitCompletedPaths()` on the main thread writes `EDM::PathData`. There is no public callback `requestPath`.

```cpp
PathfinderManager::Instance().requestPathToEDM(
    edmIndex, start, goal, PathfinderManager::Priority::High);
// Later on the main thread (AIManager already does this):
PathfinderManager::Instance().commitCompletedPaths();
```

There is no public synchronous path API; `requestPath` (callback form) is private and used only for cache pre-warm.

### Configuration

#### `void setAllowDiagonal(bool allow)`
Enables or disables diagonal movement in pathfinding.

#### `void setMaxIterations(int maxIterations)`
Sets the maximum A* iterations to prevent runaway searches.

#### `void setMaxPathsPerFrame(int maxPaths)` / `void setCacheExpirationTime(float seconds)`
Request throttle and cached-path lifetime.

The grid cell size is fixed at 64 px (`m_cellSize`); there is no public setter.

### Performance Monitoring

#### `void update()`
Main-thread frame slot (`GameEngine` step 5, before `CollisionManager::update()`).
- Commits completed path requests to EDM (`commitCompletedPaths()`).
- Applies dirty grid cells: copies the published grid, rebuilds the dirty rows
  on the copy (`WorkerBudget` batches for `SystemType::Pathfinding`, joined
  before `update()` returns, execution reported), then publishes the copy.
- **Performance**: no allocation when nothing is dirty; a dirty frame pays one
  grid copy plus the dirty rows.
- **Thread Safety**: main thread only. Skipped while globally paused.

#### `PathfinderStats getStats() const` / `void resetStats()`
Snapshot of request, cache, and memory counters (`totalRequests`,
`completedRequests`, `failedRequests`, `cacheHits`, `cacheMisses`,
`requestsPerSecond`, `cacheHitRate`, `cacheSize`, `memoryUsageKB`, ...).
```cpp
auto stats = PathfinderManager::Instance().getStats();
PATHFIND_INFO(std::format("Pathfinding: {:.1f} req/sec, {:.1f}% cache hit rate",
    stats.requestsPerSecond, stats.cacheHitRate * 100.0f));
```

## Integration Examples

### AI System Integration

Behaviors call `requestPathToEDM(edmIndex, start, goal, priority)`. `AIManager` commits on the main thread via `commitCompletedPaths()`. Do not use worker callbacks.

### Collision Integration

PathfinderManager receives `CollisionObstacleChanged` / `TileChanged`, invalidates cached paths through the area, marks the cells dirty, and rebuilds them in `update()` (see Dirty-Cell Updates). No manual integration is required. There is no manager-level weight-field API; static bodies and tile data are the only grid inputs.

### Player Movement Integration

Player click-to-move is not a PathfinderManager callback. NPC/AI movement uses `requestPathToEDM` as above.

## Performance Considerations

### Threading Model
- **Request Thread**: AI worker batches (or the main thread) submit `requestPathToEDM`
- **Worker Threads**: ThreadSystem computes paths in the background
- **Result Delivery**: `commitCompletedPaths()` writes EDM on the main thread (no callbacks)
- **Update Thread**: path commits and dirty-row grid updates in `update()` on the main thread

### Cache System
- **Automatic Invalidation**: Cache entries invalidated when collision obstacles change
- **Expiration**: Cached paths expire after configurable timeout
- **Memory Management**: LRU eviction prevents unbounded memory growth

### Performance Metrics
- **Target Performance**: 10,000+ entities at 60+ FPS
- **Request Throughput**: 1,000+ pathfinding requests per second
- **Cache Hit Rate**: 60-90% (8192 entry cache with smart invalidation)
  - Large worlds (32K pixels, 2000+ entities): 60-75%
  - Medium worlds (12K pixels, 500-2000 entities): 75-85%
  - Small worlds (3K pixels, <500 entities): 85-90%
- **Memory Usage**: ~50KB per 100x100 grid, ~30MB for path cache (8192 entries, optimized for 32K worlds)

### Optimization Guidelines

#### Grid Resolution
The grid uses a fixed 64 px cell size; there is no runtime resolution setter.

#### Request Prioritization
```cpp
// Use appropriate priorities to ensure smooth gameplay
Priority::Critical  // Player movement, emergency AI (< 1% of requests)
Priority::High      // Combat AI, important NPCs (< 10% of requests)
Priority::Normal    // General AI movement (60-80% of requests)
Priority::Low       // Background AI, decorative NPCs (10-30% of requests)
```

#### Request Throttling
Behaviors gate re-requests with `PathData::pathRequestCooldown` and
`pathRequestPending`; `setMaxPathsPerFrame()` bounds per-frame processing.

## Grid Rebuild Architecture

The PathfinderManager builds the grid once per world load (detached, gated by LoadingState) and applies later obstacle/tile changes as dirty cells inside `update()`.

### Threaded Grid Rebuild System

Grid rebuilds execute on ThreadSystem workers using WorkerBudget allocation, enabling parallel cell processing for large grids.

#### Parallel Batching Strategy
```cpp
// Full rebuild into a new grid, published when done (isGridReady() tracks it).
// Production caller: StaticCollidersReady during a load. Tests/benches may call it.
void rebuildGrid();
```

The system determines the rebuild strategy from `WorkerBudgetManager::getBatchStrategy(SystemType::Pathfinding, gridHeight, ...)`:
- **One batch** (small grid or high queue pressure): sequential rebuild on a single ThreadSystem task
- **Multiple batches**: a coordinator task submits parallel row batches, waits for them, then publishes the grid

```cpp
// Example: 200x200 grid with 12-worker system
// - Calculates WorkerBudget allocation for pathfinding (~19% = 2-3 workers)
// - Divides grid rows into batches (e.g., 8 batches of 25 rows each)
// - Submits batch tasks with Low priority to avoid starving AI/Events
// - Coordinator task waits for all batches then atomically swaps grid
```

#### State Transition Coordination
```cpp
// Called before game state changes (e.g., PlayState → MenuState).
// Internally waits for in-flight rebuild tasks (private waitForGridRebuildCompletion()).
void prepareForStateTransition();
```

### Dirty-Cell Updates

Obstacle and tile changes after load are applied in the manager's own frame
slot. Handlers never rebuild.

#### Event Subscriptions
PathfinderManager subscribes to EventManager (persistent handlers):
- **StaticCollidersReady**: full detached `rebuildGrid()` for the new world
- **WorldUnloaded**: joins rebuild futures and drops the grid
- **CollisionObstacleChanged**: invalidates cached paths through the area and marks the cells dirty
- **TileChanged**: invalidates cached paths near the tile and marks its cell dirty

#### Dirty-Cell Flow
```cpp
// 1. addStaticBody/removeCollisionBody or WorldManager tile update fires a
//    deferred CollisionObstacleChanged / TileChanged
// 2. EventManager::update() (main thread) delivers it; the handler marks dirty
//    cells on the current grid (PathfindingGrid::markDirtyRegion)
// 3. PathfinderManager::update() takes the dirty rows (takeDirtyRows), copies
//    the grid, rebuilds those rows with rebuildFromWorld(rowStart, rowEnd) --
//    the same row worker the full rebuild uses -- joins the batches, updates
//    the coarse grid, and publishes the copy with the locked grid swap
// 4. In-flight path requests keep the grid snapshot they captured
```

Collision statics are read by the dirty-row batches while the main thread is
blocked in `update()`, so no static mutation overlaps them. Published grids
are never mutated in place.

Accepted gap: a path computed on the pre-publish grid can be cached just after
publish (cache invalidation ran at mark time). For harvests this is a weight
difference only.

### Performance Impact

| Operation | Description | Typical Time |
|-----------|-------------|--------------|
| Full Sequential | Single-threaded grid rebuild | 50-100ms (200×200) |
| Full Parallel | WorkerBudget parallel batching | 15-30ms (200×200) |
| Dirty cells | Dirty rows on a grid copy, in `update()` | grid copy + rows |
| State Transition | prepareForStateTransition() | <1ms (waits for completion) |

### LoadingState Integration

The LoadingState uses PathfinderManager's synchronization to ensure grid availability:

```cpp
// In LoadingState (global pause held; EventManager keeps draining):
// 1. Submit world generation to ThreadSystem
// 2. WorldManager posts WorldLoadedEvent (deferred)
// 3. CollisionManager rebuilds statics (no per-body CollisionObstacleChanged)
//    and fires StaticCollidersReady
// 4. PathfinderManager starts the detached full grid rebuild
// 5. LoadingState waits for isGridReady() before transitioning
```

## Error Handling

### Pathfinding Results
`PathfindingGrid` (`include/ai/pathfinding/PathfindingGrid.hpp`) reports:
```cpp
enum class PathfindingResult : uint8_t {
    SUCCESS,
    NO_PATH_FOUND,
    INVALID_START,
    INVALID_GOAL,
    TIMEOUT
};
```
`requestPathToEDM` does not surface the result; a failed request clears
`PathData::pathRequestPending` without writing waypoints, and the behavior
decides whether to retry after its cooldown.

## Testing

### Unit Tests
```bash
# Run pathfinding system tests
./tests/test_scripts/run_pathfinding_tests.sh

# Individual test suites
./bin/debug/pathfinding_system_tests
./bin/debug/pathfinder_manager_tests
```

### Performance Tests
```bash
# Pathfinding performance benchmarks
./tests/test_scripts/run_pathfinder_benchmark.sh
./bin/debug/pathfinder_benchmark

# Collision system benchmarks (separate)
./tests/test_scripts/run_collision_benchmark.sh
./bin/debug/collision_scaling_benchmark
```

### Integration Tests
- **AI Integration**: Validates batch pathfinding with AIManager
- **Collision Integration**: Tests dynamic obstacle response
- **Threading**: Validates thread safety under load

## Advanced Features

### Custom Heuristics
```cpp
// The system uses optimized A* with Manhattan + Diagonal heuristic
// Grid automatically optimizes for different movement patterns
```

### Memory Management
- **Pool Allocation**: Internal path storage uses object pools
- **Cache Management**: Automatic cache size management prevents memory leaks
- **Grid Lifetime**: `prepareForStateTransition()` and `WorldUnloaded` drop the grid; each world load rebuilds it

### Debug Features
Use `getStats()` for counters; `PATHFIND_DEBUG` / `PATHFIND_INFO` log rebuild and cache events.

For more information on pathfinding algorithms and grid implementation, see [PathfindingSystem.md](../ai/PathfindingSystem.md).
