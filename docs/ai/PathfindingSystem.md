# Pathfinding System

## Overview

The VoidLight-Framework pathfinding system provides high-performance A* pathfinding with advanced optimizations including hierarchical pathfinding, dynamic weight fields, and object pooling. The system is designed to scale efficiently from small tactical movements to large-scale navigation across entire game worlds.

## Architecture

### Core Components

#### PathfindingGrid
The main pathfinding grid that handles A* algorithm execution and world representation.
```cpp
class PathfindingGrid {
public:
    PathfindingGrid(int width, int height, float cellSize, const Vector2D& worldOffset, bool createCoarseGrid = true);

    // Main pathfinding interface
    PathfindingResult findPath(const Vector2D& start, const Vector2D& goal, std::vector<Vector2D>& outPath);
    PathfindingResult findPathHierarchical(const Vector2D& start, const Vector2D& goal, std::vector<Vector2D>& outPath);
    bool shouldUseHierarchicalPathfinding(const Vector2D& start, const Vector2D& goal) const;

    // Configuration
    void setAllowDiagonal(bool allow);
    void setMaxIterations(int maxIters);
    void setCosts(float straight, float diagonal);

    // Dynamic weighting
    void resetWeights(float defaultWeight = 1.0f);
    void addWeightCircle(const Vector2D& worldCenter, float worldRadius, float weightMultiplier);
};
```

#### PathfindingResult
Result enumeration for pathfinding operations.
```cpp
enum class PathfindingResult {
    SUCCESS,         // Path found successfully
    NO_PATH_FOUND,   // No valid path exists
    INVALID_START,   // Starting position is blocked
    INVALID_GOAL,    // Goal position is blocked
    TIMEOUT          // Maximum iterations exceeded
};
```

### Hierarchical Pathfinding

#### Two-Tier System
The system uses a dual-resolution approach for optimal performance:

**Fine Grid (Primary)**
- **Cell Size**: Configurable (typically 64 units)
- **Usage**: Detailed local navigation, precise movement
- **Performance**: Best for distances < 256 units

**Coarse Grid (Secondary)**
- **Cell Size**: 4x larger than fine grid (typically 256 units)
- **Usage**: Long-distance navigation, macro pathfinding
- **Performance**: 10x speedup for long paths
- **Optimization**: Used for early unreachability detection in large worlds

#### Automatic Selection
```cpp
bool shouldUseHierarchicalPathfinding(const Vector2D& start, const Vector2D& goal) const {
    float distance = (goal - start).magnitude();
    return distance > HIERARCHICAL_DISTANCE_THRESHOLD; // 256.0f units (optimized for large worlds)
}
```

## Algorithm Details

### A* Implementation

#### Core Algorithm
```cpp
PathfindingResult findPath(const Vector2D& start, const Vector2D& goal, std::vector<Vector2D>& outPath) {
    // Convert world coordinates to grid coordinates
    auto [startGX, startGY] = worldToGrid(start);
    auto [goalGX, goalGY] = worldToGrid(goal);

    // Validate start and goal positions
    if (isBlocked(startGX, startGY)) return PathfindingResult::INVALID_START;
    if (isBlocked(goalGX, goalGY)) return PathfindingResult::INVALID_GOAL;

    // A* algorithm with optimizations
    // - Priority queue for open set
    // - Closed set tracking
    // - Heuristic: Manhattan + Diagonal distance
    // - Object pooling for memory efficiency
}
```

#### Heuristic Function
```cpp
float calculateHeuristic(int x1, int y1, int x2, int y2) {
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);

    if (m_allowDiagonal) {
        // Diagonal heuristic for 8-directional movement
        return m_costStraight * std::max(dx, dy) +
               (m_costDiagonal - m_costStraight) * std::min(dx, dy);
    } else {
        // Manhattan distance for 4-directional movement
        return m_costStraight * (dx + dy);
    }
}
```

### Performance Optimizations

#### Object Pooling
```cpp
struct NodePool {
    // Pre-allocated containers to avoid repeated allocation/deallocation
    std::priority_queue<Node, std::vector<Node>, Cmp> openQueue;
    std::vector<float> gScoreBuffer;
    std::vector<float> fScoreBuffer;
    std::vector<int> parentBuffer;
    std::vector<uint8_t> closedBuffer;
    std::vector<Vector2D> pathBuffer;

    void ensureCapacity(int gridSize);
    void reset(); // Clear without deallocating
};
```

#### Memory Layout Optimization
```cpp
// Cache-friendly data structures
std::vector<uint8_t> m_blocked;  // 0 = walkable, 1 = blocked
std::vector<float> m_weight;     // Movement cost multipliers

// Grid coordinates stored as flat arrays for cache efficiency
int gridIndex(int x, int y) const { return y * m_w + x; }
```

## Configuration Options

### Movement Settings
```cpp
// Enable 8-directional movement (default: true)
grid.setAllowDiagonal(true);

// Set movement costs
grid.setCosts(
    1.0f,           // Straight movement cost
    1.41421356f     // Diagonal movement cost (√2)
);

// Set iteration limit to prevent infinite loops
grid.setMaxIterations(12000);  // Tuned for 200x200 grids
```

### Grid Resolution
```cpp
// High precision for tactical games
PathfindingGrid grid(mapWidth, mapHeight, 32.0f, worldOffset);

// Balanced for most games
PathfindingGrid grid(mapWidth, mapHeight, 64.0f, worldOffset);

// Fast for action games
PathfindingGrid grid(mapWidth, mapHeight, 128.0f, worldOffset);
```

## Dynamic Weight Fields

### Avoidance Areas
```cpp
// Create danger zone around explosion
grid.addWeightCircle(
    explosionCenter,  // World position
    100.0f,          // Radius in world units
    10.0f            // Cost multiplier (10x normal cost)
);

// Create slow zone for difficult terrain
grid.addWeightCircle(
    swampCenter,
    200.0f,          // Large area
    3.0f             // 3x movement cost
);

// Reset all weights to default
grid.resetWeights(1.0f);
```

### Temporary Obstacles
```cpp
// Add temporary obstacle avoidance
void createTemporaryAvoidanceZone(const Vector2D& center, float radius, float duration) {
    std::string fieldName = "temp_obstacle_" + std::to_string(UniqueID::generate());

    pathfindingGrid.addWeightCircle(center, radius, 8.0f);

    // Schedule removal
    Timer::schedule(duration, [fieldName, &pathfindingGrid]() {
        pathfindingGrid.resetWeights(1.0f); // or remove specific field
    });
}
```

## Integration Examples

### PathfinderManager Integration

```cpp
PathfinderManager::Instance().requestPathToEDM(
    edmIndex, startPosition, goalPosition, PathfinderManager::Priority::Normal);
// Main thread:
PathfinderManager::Instance().commitCompletedPaths();
// Result is in EDM PathData for that index.
```

There is no public callback `requestPath`.

### AI Behavior Integration

Wander, Patrol, Flee, Guard, Chase, and Follow call `requestPathToEDM` from the behavior executor. Path progress lives in EDM `PathData`. `AIManager` calls `commitCompletedPaths()` on the main thread. Do not store paths in behavior-local members and do not use worker callbacks.

### World Integration
```cpp
void PathfindingGrid::rebuildFromWorld() {
    WorldManager& worldMgr = WorldManager::Instance();

    // Update grid from world tiles
    for (int y = 0; y < m_h; ++y) {
        for (int x = 0; x < m_w; ++x) {
            Vector2D worldPos = gridToWorld(x, y);
            int worldX = static_cast<int>(worldPos.x / TILE_SIZE);
            int worldY = static_cast<int>(worldPos.y / TILE_SIZE);

            // Check if tile blocks movement
            auto tileType = worldMgr.getTileType(worldX, worldY);
            bool blocked = (tileType == TileType::WALL ||
                           tileType == TileType::OBSTACLE ||
                           tileType == TileType::WATER);

            setBlocked(x, y, blocked);

            // Set movement cost based on terrain
            float weight = getTerrainWeight(tileType);
            setWeight(x, y, weight);
        }
    }

    // Update coarse grid for hierarchical pathfinding
    updateCoarseGrid();
}

float getTerrainWeight(TileType type) {
    switch (type) {
        case TileType::GRASS:    return 1.0f;   // Normal movement
        case TileType::DIRT:     return 1.2f;   // Slightly slower
        case TileType::SAND:     return 1.5f;   // Slow movement
        case TileType::SWAMP:    return 3.0f;   // Very slow
        case TileType::ROAD:     return 0.8f;   // Fast movement
        default:                 return 1.0f;
    }
}
```

## Performance Characteristics

### Scaling Metrics
- **Grid Size**: Up to 500x500 cells efficiently
- **Path Length**: Up to 1000+ waypoints
- **Requests/Second**: 1000+ concurrent pathfinding operations
- **Memory Usage**: ~4 bytes per cell + object pool overhead

### Hierarchical Performance
```cpp
// Performance comparison for 200x200 grid:
//
// Direct A* (long distance):
// - Distance: 800 units
// - Time: ~8ms
// - Iterations: ~6000
//
// Hierarchical A* (same distance):
// - Coarse path: ~1ms (50x50 grid)
// - Refinement: ~2ms per segment
// - Total: ~3ms
// - Speedup: ~2.5x
```

### Memory Optimization
```cpp
// Object pool reduces allocation overhead
struct PathfindingStats {
    uint64_t totalRequests{0};
    uint64_t successfulPaths{0};
    uint64_t timeouts{0};
    uint64_t totalIterations{0};
    uint32_t avgPathLength{0};
};

// Typical performance profile:
// - 95% success rate
// - < 0.1% timeouts
// - Average 50-100 iterations per path
// - Average path length 20-50 waypoints
```

## Path Smoothing

### Line-of-Sight Optimization
```cpp
void smoothPath(std::vector<Vector2D>& path) {
    if (path.size() <= 2) return;

    std::vector<Vector2D> smoothedPath;
    smoothedPath.push_back(path[0]);

    int currentIndex = 0;
    while (currentIndex < path.size() - 1) {
        int furthestVisible = currentIndex + 1;

        // Find furthest waypoint with line of sight
        for (int i = currentIndex + 2; i < path.size(); ++i) {
            if (hasLineOfSight(path[currentIndex], path[i])) {
                furthestVisible = i;
            } else {
                break;
            }
        }

        smoothedPath.push_back(path[furthestVisible]);
        currentIndex = furthestVisible;
    }

    path = std::move(smoothedPath);
}

bool hasLineOfSight(const Vector2D& start, const Vector2D& end) {
    // Bresenham's line algorithm to check for obstacles
    auto [x0, y0] = worldToGrid(start);
    auto [x1, y1] = worldToGrid(end);

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int x = x0, y = y0;
    int stepX = (x0 < x1) ? 1 : -1;
    int stepY = (y0 < y1) ? 1 : -1;
    int error = dx - dy;

    while (true) {
        if (isBlocked(x, y)) return false;
        if (x == x1 && y == y1) break;

        int error2 = 2 * error;
        if (error2 > -dy) { error -= dy; x += stepX; }
        if (error2 < dx) { error += dx; y += stepY; }
    }

    return true;
}
```

## Error Handling and Recovery

### Blocked Position Recovery
```cpp
Vector2D snapToNearestOpenWorld(const Vector2D& pos, float maxWorldRadius) const {
    auto [gx, gy] = worldToGrid(pos);
    int maxGridRadius = static_cast<int>(maxWorldRadius / m_cell);

    int openGX, openGY;
    if (findNearestOpen(gx, gy, maxGridRadius, openGX, openGY)) {
        return gridToWorld(openGX, openGY);
    }

    // Fallback: return original position
    return pos;
}

bool findNearestOpen(int gx, int gy, int maxRadius, int& outGX, int& outGY) const {
    // Spiral search pattern for nearest open cell
    for (int radius = 1; radius <= maxRadius; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) == radius || std::abs(dy) == radius) {
                    int testX = gx + dx;
                    int testY = gy + dy;

                    if (inBounds(testX, testY) && !isBlocked(testX, testY)) {
                        outGX = testX;
                        outGY = testY;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
```

### Pathfinding Failure Handling
```cpp
void handlePathfindingFailure(PathfindingResult result, EntityID entityId) {
    switch (result) {
        case PathfindingResult::NO_PATH_FOUND:
            // Try pathfinding to nearest reachable point
            attemptPartialPath(entityId);
            break;

        case PathfindingResult::INVALID_START:
            // Move entity to nearest open cell
            Vector2D currentPos = entity.getPosition();
            Vector2D openPos = grid.snapToNearestOpenWorld(currentPos, 64.0f);
            entity.setPosition(openPos);
            break;

        case PathfindingResult::INVALID_GOAL:
            // Find alternative goal nearby
            findAlternativeGoal(entityId);
            break;

        case PathfindingResult::TIMEOUT:
            // Reduce path complexity or use direct movement
            useSimplifiedNavigation(entityId);
            break;
    }
}
```

## Testing and Debugging

### Unit Tests
```bash
# Run pathfinding system tests
./tests/test_scripts/run_pathfinding_tests.sh

# Individual test executables
./bin/debug/pathfinding_system_tests     # Core A* algorithm tests
./bin/debug/pathfinding_performance_tests # Scaling and performance tests
```

### Debug Visualization

There is no `renderPathfindingDebug(SDL_Renderer*)` path. Production rendering is GPU-only (`GameEngine` / `GPURenderer`). Path debug, if added, belongs on the GPU scene/UI hooks — do not reintroduce SDL_Renderer.

Historical sketch (do not copy):

```cpp
void renderPathfindingDebug(SDL_Renderer* renderer, const Camera& camera) {
    // Render grid
    for (int y = 0; y < grid.getHeight(); ++y) {
        for (int x = 0; x < grid.getWidth(); ++x) {
            Vector2D worldPos = grid.gridToWorld(x, y);
            SDL_Color color;

            if (grid.isBlocked(x, y)) {
                color = {255, 0, 0, 128}; // Red for blocked
            } else {
                float weight = grid.getWeight(x, y);
                int intensity = static_cast<int>(128 + weight * 64);
                color = {0, intensity, 0, 64}; // Green intensity for weight
            }

            renderGridCell(renderer, worldPos, color, camera);
        }
    }

    // Render active paths
    for (const auto& path : activePaths) {
        renderPath(renderer, path, {0, 255, 255, 255}, camera);
    }
}
```

### Performance Profiling
```cpp
void profilePathfinding() {
    auto stats = grid.getStats();

    GAMEENGINE_INFO("Pathfinding Statistics:");
    GAMEENGINE_INFO("  Total Requests: " + std::to_string(stats.totalRequests));
    GAMEENGINE_INFO("  Success Rate: " +
                   std::to_string((float)stats.successfulPaths / stats.totalRequests * 100.0f) + "%");
    GAMEENGINE_INFO("  Avg Iterations: " +
                   std::to_string(stats.totalIterations / stats.totalRequests));
    GAMEENGINE_INFO("  Avg Path Length: " + std::to_string(stats.avgPathLength));
    GAMEENGINE_INFO("  Timeout Rate: " +
                   std::to_string((float)stats.timeouts / stats.totalRequests * 100.0f) + "%");
}
```

For more information on pathfinding integration and management, see [PathfinderManager.md](../managers/PathfinderManager.md).
