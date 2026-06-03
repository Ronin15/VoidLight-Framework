# CollisionManager

## Overview

The CollisionManager is a high-performance collision detection and response system designed for the VoidLight-Framework. It provides efficient spatial partitioning, batch processing capabilities, and tight integration with the AI and pathfinding systems. The manager supports 27,000+ collision bodies at 60+ FPS through hybrid dual-path threading, SOA storage, and SIMD optimizations.

## Architecture

### Design Patterns
- **Singleton Pattern**: Ensures single instance with global access
- **Hierarchical Spatial Hash**: Two-tier adaptive spatial hash system optimized for 10K+ bodies
- **SOA Storage**: Structure-of-Arrays layout for cache-friendly collision processing
- **Explicit Event Channels**: Emits world-trigger and obstacle-change events; general non-projectile collision pairs stay on the collision hot path
- **Batch Processing**: Optimized batch updates for AI entity kinematics
- **Hybrid Threading**: Dual-path (single/multi-threaded) for both broadphase and narrowphase

### EntityDataManager Integration

CollisionManager coordinates with EntityDataManager (EDM) for position synchronization and tier-aware collision detection.

#### EDM as Position Source

Entity positions are stored in EDM's `EntityHotData`. CollisionManager reads positions from EDM during collision detection:

```cpp
// CollisionManager reads positions from EDM during broadphase
void CollisionManager::syncPositionsFromEDM() {
    auto& edm = EntityDataManager::Instance();

    for (size_t i = 0; i < m_dynamicIndices.size(); ++i) {
        uint32_t edmIndex = m_edmMapping[i];
        const auto& transform = edm.getTransformByIndex(edmIndex);

        // Update SOA storage from EDM
        m_soaStorage.x[i] = transform.position.x;
        m_soaStorage.y[i] = transform.position.y;
    }
}
```

#### Tier-Aware Collision Detection

Only Active tier entities participate in full collision detection:

```cpp
// Build active indices filtering by tier
void CollisionManager::buildActiveIndices() {
    auto& edm = EntityDataManager::Instance();
    m_activeMovableIndices.clear();

    for (uint32_t edmIndex : m_registeredEntities) {
        const auto& hot = edm.getHotDataByIndex(edmIndex);

        // Skip hibernated entities entirely
        if (hot.tier == SimulationTier::Hibernated) continue;

        // Background tier: simplified collision (static only)
        // Active tier: full collision detection
        if (hot.tier == SimulationTier::Active) {
            m_activeMovableIndices.push_back(edmIndex);
        }
    }
}
```

#### Collision Data in EDM

Collision-related data is stored in EDM's `EntityHotData`:

```cpp
// In EntityHotData (64 bytes, cache-aligned)
struct EntityHotData {
    TransformData transform;        // Position, velocity
    float halfWidth, halfHeight;    // Collision dimensions
    // Collision flags stored here for fast access during detection
    uint8_t collisionLayer;
    uint8_t collisionMask;
    // ...
};
```

#### Registration via EDM Handle

Entities register collision bodies using their EDM handle:

```cpp
// Register collision body for EDM entity
void registerCollisionBody(EntityHandle handle, const Vector2D& halfSize,
                          BodyType type, uint32_t layer, uint32_t mask) {
    uint32_t edmIndex = EntityDataManager::Instance().getEdmIndex(handle);

    // Store collision dimensions in EDM
    auto& hot = EntityDataManager::Instance().getHotDataByIndex(edmIndex);
    hot.halfWidth = halfSize.x;
    hot.halfHeight = halfSize.y;

    // Add to SOA storage with EDM back-reference
    addToSOAStorage(edmIndex, type, layer, mask);
}
```

#### Performance Benefits

| Aspect | Old Architecture | EDM Integration | Benefit |
|--------|-----------------|-----------------|---------|
| Position sync | Copy every frame | Read from EDM | Single source of truth |
| Tier filtering | Distance checks | Pre-built indices | O(1) tier lookup |
| Data locality | Scattered storage | EDM + local SOA | Cache-optimal |
| Entity removal | Multi-manager cleanup | EDM handles lifecycle | Automatic cleanup |

### Core Components

#### AABB (Axis-Aligned Bounding Box)
The fundamental collision primitive used throughout the system.
```cpp
namespace VoidLight-Framework {
    struct AABB {
        Vector2D center;    // World center position
        Vector2D halfSize;  // Half-width and half-height

        // Constructors
        AABB() = default;
        AABB(float cx, float cy, float hw, float hh);

        // Boundary methods
        float left() const;
        float right() const;
        float top() const;
        float bottom() const;

        // Collision detection
        bool intersects(const AABB& other) const;
        bool contains(const Vector2D& point) const;
        Vector2D closestPoint(const Vector2D& point) const;
    };
}
```

#### Body Types
```cpp
enum class BodyType {
    STATIC,     // Immovable world geometry (walls, obstacles)
    KINEMATIC,  // Movable by script/AI (NPCs, moving platforms)
    DYNAMIC,    // Dynamic gameplay bodies (player, projectiles)
    TRIGGER     // Non-solid areas that generate events
};
```

**Body Type Distinctions:**
- **STATIC**: World obstacles, buildings, triggers (never move, handled separately)
- **KINEMATIC**: NPCs, script-controlled entities (move via script, not physics)
- **DYNAMIC**: Player and projectile bodies tracked as movable entities in broadphase

Important runtime exception:
- Small projectiles are detected as dynamic overlap bodies, but they do **not** use blocking position resolution.
- Projectile contacts are delivered directly as `CollisionInfo` to the projectile hit sink registered by `ProjectileManager`.
- Projectile damage is then emitted as a deferred `DamageEvent`; there is no collision-event object subscription API for this path.

The collision system groups KINEMATIC + DYNAMIC as "movable" bodies for broadphase optimization, since both require collision detection against static geometry and each other.

#### Collision Layers
```cpp
namespace CollisionLayer {
    constexpr uint32_t Layer_Player = 0x01;
    constexpr uint32_t Layer_NPC = 0x02;
    constexpr uint32_t Layer_Environment = 0x04;
    constexpr uint32_t Layer_Projectile = 0x08;
    constexpr uint32_t Layer_Trigger = 0x10;
    constexpr uint32_t Layer_All = 0xFFFFFFFF;
}
```

---

## Threading Architecture

### Hybrid Dual-Path Threading

CollisionManager uses **adaptive hybrid threading** with WorkerBudget integration. Both broadphase and narrowphase have single-threaded and multi-threaded execution paths, selected dynamically based on workload size.

```
┌─────────────────────────────────────────────────────────────────┐
│                    CollisionManager::updateSOA()                │
├─────────────────────────────────────────────────────────────────┤
│  1. Build Active Indices (culling)                              │
│  2. Sync Dynamic Spatial Hash                                   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  BROADPHASE (broadphaseSOA)                             │    │
│  │  ┌───────────────────┐  ┌───────────────────────────┐   │    │
│  │  │ Single-Threaded   │  │ Multi-Threaded            │   │    │
│  │  │ (<500 movable)    │  │ (>=500 movable bodies)    │   │    │
│  │  │                   │  │ WorkerBudget batching     │   │    │
│  │  └───────────────────┘  └───────────────────────────┘   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  NARROWPHASE (narrowphaseSOA)                           │    │
│  │  ┌───────────────────┐  ┌───────────────────────────┐   │    │
│  │  │ Single-Threaded   │  │ Multi-Threaded            │   │    │
│  │  │ (<100 pairs)      │  │ (>=100 collision pairs)   │   │    │
│  │  │ 4-wide SIMD       │  │ Per-batch buffers         │   │    │
│  │  └───────────────────┘  └───────────────────────────┘   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                 │
│  3. Resolve Collisions                                          │
│  4. Process Trigger Events                                      │
│  5. Sync Entities                                               │
└─────────────────────────────────────────────────────────────────┘
```

### Threading Thresholds

| Phase | Threshold | Rationale |
|-------|-----------|-----------|
| Broadphase | 500+ movable bodies | Threading overhead outweighs benefit for smaller counts |
| Narrowphase | 100+ collision pairs | SIMD single-threaded is fast enough below this |

### WorkerBudget Integration

CollisionManager queries WorkerBudgetManager for optimal batch configuration:

```cpp
// Dispatcher in broadphaseSOA()
auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
size_t optimalWorkers = budgetMgr.getOptimalWorkers(
    VoidLight::SystemType::Collision, movableIndices.size());

auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
    VoidLight::SystemType::Collision,
    movableIndices.size(),
    optimalWorkers);

// Adaptive threshold learns optimal cutoff based on measured throughput
bool useThreading = batchCount > 1 &&
    budgetMgr.shouldUseThreading(VoidLight::SystemType::Collision, movableIndices.size());

if (!useThreading) {
    broadphaseSingleThreaded(indexPairs);
} else {
    broadphaseMultiThreaded(indexPairs, batchCount, batchSize);
}
```

### Per-Batch Output Buffers (Zero Contention)

Multi-threaded narrowphase uses **per-batch output buffers** to eliminate lock contention:

```cpp
// Each worker thread has its own collision buffer
std::vector<std::vector<CollisionInfo>> m_batchCollisionBuffers;

// After all batches complete, results are merged
void mergeThreadResults() {
    for (auto& batchBuffer : m_batchCollisionBuffers) {
        collisions.insert(collisions.end(),
                         batchBuffer.begin(), batchBuffer.end());
    }
}
```

### Performance

- **2-4x speedup** for narrowphase with 10K+ bodies when multi-threaded
- **27K+ bodies @ 60 FPS** on Apple Silicon (M1/M2/M3)
- **Adaptive batching** converges to optimal batch size via WorkerBudget hill-climbing

---

## SOA Storage Architecture

CollisionManager uses Structure-of-Arrays (SOA) storage for optimal cache performance during collision detection.

### HotData (64 bytes, cache-aligned)
Accessed every frame during collision detection:
```cpp
struct HotData {
    Vector2D position;           // 8 bytes: Current position (center of AABB)
    Vector2D velocity;           // 8 bytes: Current velocity
    Vector2D halfSize;           // 8 bytes: Half-width and half-height

    // Cached AABB for performance
    mutable float aabbMinX, aabbMinY, aabbMaxX, aabbMaxY;  // 16 bytes

    uint16_t layers;             // 2 bytes: Layer mask
    uint16_t collidesWith;       // 2 bytes: Collision mask
    uint8_t bodyType;            // 1 byte: BodyType enum
    uint8_t triggerTag;          // 1 byte: TriggerTag enum
    uint8_t active;              // 1 byte: Participates in detection
    uint8_t isTrigger;           // 1 byte: Is trigger body
    mutable uint8_t aabbDirty;   // 1 byte: Cache invalidation flag

    int16_t coarseCellX, coarseCellY;  // 4 bytes: Cached grid coords
    uint8_t _reserved[5];        // 5 bytes: Future expansion
    uint8_t _padding[2];         // 2 bytes: Alignment
};
static_assert(sizeof(HotData) == 64, "HotData must be cache-line aligned");
```

### ColdData (Rarely Accessed)
Separated to avoid cache pollution:
```cpp
struct ColdData {
    EntityWeakPtr entityWeak;    // Back-reference to entity
    Vector2D acceleration;       // Acceleration (rarely used)
    Vector2D lastPosition;       // Previous position
    AABB fullAABB;              // Full AABB object
    float restitution;           // Bounce coefficient
    float friction;              // Surface friction
    float mass;                  // Mass (kg)
};
```

### Benefits
- **Cache-friendly**: HotData fits in single cache line
- **Minimal pollution**: ColdData accessed only when needed
- **Fast iteration**: Sequential memory access during broadphase/narrowphase

---

## Hierarchical Spatial Hash

### Two-Tier Architecture

The collision system uses **two separate spatial hashes** for optimal performance:

#### Static Spatial Hash (`m_staticSpatialHash`)
- **Contains**: World geometry (buildings, obstacles, water triggers)
- **Rebuilt**: Only when world changes (tile edits, building placement)
- **Queried**: By dynamic/kinematic bodies during broadphase
- **Optimization**: Coarse-grid region cache (128x128 cells) reduces redundant queries

#### Dynamic Spatial Hash (`m_dynamicSpatialHash`)
- **Contains**: Moving entities (player, NPCs, projectiles)
- **Rebuilt**: Every frame from active culled bodies
- **Queried**: For dynamic-vs-dynamic collision detection
- **Optimization**: Only includes bodies within culling area

### Why Separation?
- Avoids rebuilding thousands of static tiles every frame
- Static bodies never initiate collision checks (optimization)
- Cache remains valid across frames for static geometry
- Culling only applies to dynamic hash, not static

### HierarchicalSpatialHash Configuration
```cpp
class HierarchicalSpatialHash {
public:
    static constexpr float COARSE_CELL_SIZE = 128.0f;    // Region-level culling
    static constexpr float FINE_CELL_SIZE = 32.0f;       // Precise collision detection
    static constexpr float MOVEMENT_THRESHOLD = 8.0f;    // Static body update threshold
    static constexpr size_t REGION_ACTIVE_THRESHOLD = 16; // Dynamic subdivision threshold

    // Core operations
    void insert(size_t bodyIndex, const AABB& aabb);
    void remove(size_t bodyIndex);
    void update(size_t bodyIndex, const AABB& oldAABB, const AABB& newAABB);
    void clear();

    // Query operations
    void queryRegion(const AABB& area, std::vector<size_t>& outBodyIndices) const;
    void queryRegionBounds(float minX, float minY, float maxX, float maxY,
                          std::vector<size_t>& outBodyIndices) const;

    // Batch operations
    void insertBatch(const std::vector<std::pair<size_t, AABB>>& bodies);
    void updateBatch(const std::vector<std::tuple<size_t, AABB, AABB>>& updates);
};
```

### Adaptive Subdivision
```cpp
// Low density region (<=16 bodies): Single coarse cell lookup
// High density region (>16 bodies): Fine grid subdivision for precision

struct Region {
    CoarseCoord coord;
    size_t bodyCount;
    bool hasFineSplit;  // true when bodyCount > REGION_ACTIVE_THRESHOLD (16)

    // Fine subdivision (only created for high-density regions)
    std::unordered_map<GridKey, std::vector<size_t>> fineCells;

    // Coarse body list (used for low-density regions)
    std::vector<size_t> bodyIndices;
};
```

---

## SIMD Processing

CollisionManager uses cross-platform SIMD operations from `SIMDMath.hpp` for high-performance collision detection.

### 4-Wide AABB Intersection Testing

Both single-threaded and multi-threaded narrowphase process collision pairs in batches of 4:

```cpp
// In narrowphaseSingleThreaded()
size_t i = 0;
const size_t simdEnd = (indexPairs.size() / 4) * 4;

for (; i < simdEnd; i += 4) {
    // Load indices for 4 pairs
    const auto& [aIdx0, bIdx0] = indexPairs[i];
    const auto& [aIdx1, bIdx1] = indexPairs[i+1];
    const auto& [aIdx2, bIdx2] = indexPairs[i+2];
    const auto& [aIdx3, bIdx3] = indexPairs[i+3];

    // Get AABB bounds for all 4 pairs
    alignas(16) float minXA[4], minYA[4], maxXA[4], maxYA[4];
    alignas(16) float minXB[4], minYB[4], maxXB[4], maxYB[4];

    // SIMD intersection tests...
}

// Scalar tail for remaining pairs
for (; i < indexPairs.size(); ++i) {
    processNarrowphasePairScalar(indexPairs[i], collisions);
}
```

### Layer Mask Filtering
```cpp
// Check collision layer masks for 4 bodies simultaneously
const Int4 maskVec = broadcast_int(dynamicCollidesWith);

for (; i < simdEnd; i += 4) {
    Int4 layers = load4_int(layerMasks);
    Int4 overlap = bitwise_and_int(layers, maskVec);
    // Non-zero means can collide
}
```

### Platform Support
- **x86-64**: SSE2 (baseline), AVX2 (advanced)
- **ARM64**: NEON (Apple Silicon M1/M2/M3)
- **Fallback**: Automatic scalar fallback for unsupported platforms

---

## Public API Reference

### Initialization and Lifecycle

#### `bool init()`
Initializes the CollisionManager singleton.
- **Returns**: `true` if successful, `false` otherwise
- **Side Effects**: Clears all bodies, subscribes to world events, sets up collision callbacks

#### `void clean()`
Shuts down the CollisionManager and cleans up all resources.

#### `void prepareForStateTransition()`
Prepares the manager for game state transitions.
- **CRITICAL**: Clears ALL bodies (static and dynamic) during state transitions
- Called automatically by GameStateManager before state transitions

### Body Management

#### SOA Body Management (Recommended)
```cpp
// Add body using SOA storage
size_t addCollisionBodySOA(EntityID id, const Vector2D& position,
                           const Vector2D& halfSize, BodyType type,
                           uint32_t layer = CollisionLayer::Layer_Default,
                           uint32_t collideMask = 0xFFFFFFFFu,
                           bool isTrigger = false, uint8_t triggerTag = 0);

// Remove body
void removeCollisionBodySOA(EntityID id);

// Update position/velocity
void updateCollisionBodyPositionSOA(EntityID id, const Vector2D& newPosition);
void updateCollisionBodyVelocitySOA(EntityID id, const Vector2D& newVelocity);
```

#### Layer Configuration
```cpp
// NPC that collides with players and environment
CollisionManager::Instance().setBodyLayer(
    npcId,
    CollisionLayer::Layer_NPC,
    CollisionLayer::Layer_Player | CollisionLayer::Layer_Environment
);
```

### Batch Updates

#### `void updateKinematicBatchSOA(const std::vector<KinematicUpdate>& updates)`
High-performance batch update for AI systems.
```cpp
struct KinematicUpdate {
    EntityID id;
    Vector2D position;
    Vector2D velocity;
};

std::vector<CollisionManager::KinematicUpdate> updates;
for (const auto& entity : aiEntities) {
    updates.emplace_back(entity.id, entity.position, entity.velocity);
}
CollisionManager::Instance().updateKinematicBatchSOA(updates);
```

#### `void applyBatchedKinematicUpdates(const std::vector<std::vector<KinematicUpdate>>& batchUpdates)`
Zero-contention batch updates where each AI batch has its own buffer.

### Trigger System

#### Trigger Creation
```cpp
// Create water trigger area
EntityID waterTriggerId = CollisionManager::Instance().createTriggerArea(
    AABB(100.0f, 100.0f, 50.0f, 50.0f),
    VoidLight::TriggerTag::Water,
    CollisionLayer::Layer_Environment,
    CollisionLayer::Layer_Player | CollisionLayer::Layer_NPC
);

// Create trigger at specific coordinates
EntityID triggerId = CollisionManager::Instance().createTriggerAreaAt(
    x, y, halfWidth, halfHeight,
    VoidLight::TriggerTag::Portal,
    CollisionLayer::Layer_Trigger,
    CollisionLayer::Layer_Player
);
```

#### Trigger Tags
```cpp
enum class TriggerTag {
    None,
    Water,       // Slows movement, special effects
    Fire,        // Damage over time
    Ice,         // Slippery surfaces
    Portal,      // Teleportation points
    Checkpoint,  // Save points
    Pickup,      // Item collection
    Obstacle     // Movement penalties
};
```

#### Trigger Cooldowns
```cpp
CollisionManager::Instance().setTriggerCooldown(triggerId, 2.0f);
CollisionManager::Instance().setDefaultTriggerCooldown(1.0f);
```

### Queries

```cpp
// Test if two bodies overlap
bool overlaps(EntityID a, EntityID b) const;

// Find all bodies within area
void queryArea(const AABB& area, std::vector<EntityID>& out) const;

// Get body center position
bool getBodyCenter(EntityID id, Vector2D& outCenter) const;

// Type checking
bool isDynamic(EntityID id) const;
bool isKinematic(EntityID id) const;
bool isStatic(EntityID id) const;
bool isTrigger(EntityID id) const;
```

### World Integration

```cpp
// Rebuild all static collision bodies from world
void rebuildStaticFromWorld();

// Create triggers for water tiles
size_t createTriggersForWaterTiles(TriggerTag tag = TriggerTag::Water);

// Create triggers for movement-affecting obstacles
size_t createTriggersForObstacles();

// Create static collision bodies for solid obstacles
size_t createStaticObstacleBodies();

// Update collision state when tile changes
void onTileChanged(int x, int y);
```

### World Boundary Handling

CollisionManager automatically synchronizes with world boundaries when worlds are loaded or generated.

#### Automatic Boundary Updates

When `WorldLoadedEvent` or `WorldGeneratedEvent` fires, CollisionManager:
1. Queries `WorldManager::getWorldBounds()` for min/max coordinates
2. Updates internal `m_worldBounds` AABB
3. Rebuilds static colliders from world data

```cpp
// In event handler (automatic)
const auto& worldManager = WorldManager::Instance();
float minX, minY, maxX, maxY;
if (worldManager.getWorldBounds(minX, minY, maxX, maxY)) {
    this->setWorldBounds(minX, minY, maxX, maxY);
}
```

#### Manual Boundary Configuration

```cpp
// Set world boundaries manually (in pixels)
CollisionManager::Instance().setWorldBounds(0.0f, 0.0f, 3200.0f, 3200.0f);
```

#### Entity Boundary Clamping

Entity boundary clamping is NOT handled by CollisionManager. Instead:
- **Player**: Clamps position in `Player::update()` using cached world bounds
- **NPCs**: Use `PathfinderManager::clampInsideExtents()` for boundary-safe movement
- **Projectiles**: Destroyed when leaving world bounds

#### Spatial Hash Behavior at World Edges

The HierarchicalSpatialHash handles world edge cases:
- **Static hash**: Includes all obstacles within world bounds
- **Dynamic hash**: Only includes active entities near camera
- **Edge queries**: Return empty results outside world bounds (no wraparound)

---

## Integration Examples

### World Loading
```cpp
void GameState::loadLevel() {
    WorldManager::Instance().loadWorld("level1.json");

    CollisionManager::Instance().rebuildStaticFromWorld();

    size_t waterTriggers = CollisionManager::Instance().createTriggersForWaterTiles();
    size_t obstacleTriggers = CollisionManager::Instance().createTriggersForObstacles();

    GAMEENGINE_INFO(std::format("Created {} water, {} obstacle triggers",
                                waterTriggers, obstacleTriggers));
}
```

### AI System Integration
```cpp
// In AIManager - batch update all kinematic bodies
std::vector<CollisionManager::KinematicUpdate> kinematicUpdates;
kinematicUpdates.reserve(m_activeEntities.size());

for (const auto& entity : m_activeEntities) {
    Vector2D newPosition = entity->getPosition() + entity->getVelocity() * deltaTime;
    kinematicUpdates.emplace_back(entity->getId(), newPosition, entity->getVelocity());
}

CollisionManager::Instance().updateKinematicBatchSOA(kinematicUpdates);
```

### Event Integration

Collision-to-event traffic is limited to explicit event channels such as world triggers and obstacle changes. Projectile gameplay uses the projectile hit sink instead:

```cpp
CollisionManager::Instance().setProjectileHitSink(
    [](const CollisionInfo& info) {
        ProjectileManager::Instance().handleProjectileCollision(info);
    });
```

General non-projectile collision pairs are not forwarded through `EventManager`. If a future system needs collision-derived behavior, add a narrow manager-owned sink or explicit event contract instead of restoring broad per-collision forwarding.

### AI Obstacle Avoidance
```cpp
bool AIBehavior::canMoveToPosition(const Vector2D& targetPos) {
    AABB testAABB(targetPos.x, targetPos.y, m_entity->getHalfWidth(), m_entity->getHalfHeight());

    std::vector<EntityID> obstacles;
    CollisionManager::Instance().queryArea(testAABB, obstacles);

    for (EntityID obstacleId : obstacles) {
        if (CollisionManager::Instance().isDynamic(obstacleId) ||
            CollisionManager::Instance().isKinematic(obstacleId)) {
            continue; // Ignore dynamic entities
        }
        if (!CollisionManager::Instance().isTrigger(obstacleId)) {
            return false; // Blocked by static geometry
        }
    }
    return true;
}
```

---

## Performance Metrics

### Measured Performance
| Metric | Value |
|--------|-------|
| Max Bodies @ 60 FPS | 27,000+ (Apple Silicon) |
| Average Update Time (10K bodies) | 0.25ms |
| Memory per Body | ~200 bytes |
| CPU Usage (typical) | 2-4% |

### Scaling Characteristics
| Body Count | Update Time |
|------------|-------------|
| 100 | <0.1ms |
| 1,000 | 0.05-0.08ms |
| 10,000+ | 0.2-0.3ms |

### Threading Speedup
| Workload | Speedup |
|----------|---------|
| 10K+ bodies (narrowphase) | 2-4x |
| Broadphase spatial queries | 1.5-2x |

---

## Thread Safety

### Lock Strategy
- **`std::shared_mutex` (`m_storageMutex`)**: Read-write locking for storage access
  - `std::shared_lock` for concurrent reads (queryArea, overlaps, getBodyCenter)
  - `std::unique_lock` for exclusive writes (attachEntity, prepareForStateTransition)
- **Command Queue**: Deferred add/remove operations via `m_commandQueueMutex`
- **Per-Batch Buffers**: Narrowphase uses isolated buffers per worker thread

### Safe Access Patterns
- **Update Thread**: Main collision detection runs on update thread
- **Render Thread**: Query operations are thread-safe
- **Background Threads**: PathfinderManager can safely query collision state

---

## Debug and Testing

### Debug Features
```cpp
// Enable verbose collision logging
CollisionManager::Instance().setVerboseLogging(true);

// Get performance statistics
const auto& stats = CollisionManager::Instance().getPerfStats();
```

### Performance Stats Structure
```cpp
struct PerfStats {
    double lastBroadphaseMs;
    double lastNarrowphaseMs;
    double lastTotalMs;
    double avgTotalMs;
    size_t lastPairs;
    size_t lastCollisions;
    size_t lastActiveBodies;
    size_t lastDynamicBodiesCulled;
    size_t lastStaticBodiesCulled;
};
```

### Unit Tests
```bash
# Run collision system tests
./tests/test_scripts/run_collision_tests.sh

# Individual test executables
./bin/debug/collision_system_tests
./bin/debug/collision_config_tests

# Performance benchmarks
./bin/debug/collision_benchmark
./tests/test_scripts/run_collision_benchmark.sh
```

For more details on testing, see [TESTING.md](../../tests/TESTING.md).
