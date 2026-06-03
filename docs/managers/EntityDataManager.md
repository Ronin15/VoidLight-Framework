# EntityDataManager

**Code:** `include/managers/EntityDataManager.hpp`, `src/managers/EntityDataManager.cpp`

**Singleton Access:** Use `EntityDataManager::Instance()` to access the manager.

## Overview

EntityDataManager is the **central data authority** for all entity data in VoidLight-Framework. It implements a Data-Oriented Design (DoD) using Structure-of-Arrays (SoA) storage for cache-optimal performance.

### Key Features

- **Single Source of Truth**: Eliminates 4x position duplication across managers
- **Cache-Optimal Storage**: 64-byte `EntityHotData` structs fit exactly one cache line
- **Simulation Tier System**: Active/Background/Hibernated for 100K+ entity support
- **Type-Specific Data Blocks**: CharacterData, ItemData, ProjectileData, etc.
- **Lock-Free Index Access**: Safe for parallel batch processing

### Design Philosophy

EntityDataManager is a **DATA STORE**, not a processor. It owns:
- All entity transform data (position, velocity, acceleration)
- Type-specific data blocks
- Simulation tier tracking

Processing systems read from and write to EntityDataManager:
- AIManager processes Active tier behaviors
- CollisionManager processes collision detection
- BackgroundSimulationManager processes Background tier entities

## Threading Contract

**CRITICAL:**
- **Structural operations** (create/destroy/register/getIndex) MUST be called from the main thread only
- **Index-based accessors** (`getHotDataByIndex`, `getTransformByIndex`) are lock-free and safe for parallel batch processing
- GameEngine::update() sequential order guarantees no concurrent structural changes

## Data Structures

### EntityHotData (64 bytes, cache-aligned)

Accessed every frame during collision detection and AI processing:

```cpp
struct EntityHotData {
    TransformData transform;        // 32 bytes: position, velocity, etc.
    float halfWidth;                // 4 bytes: Collision half-width
    float halfHeight;               // 4 bytes: Collision half-height
    EntityKind kind;                // 1 byte: Entity type
    SimulationTier tier;            // 1 byte: Active/Background/Hibernated
    uint8_t flags;                  // 1 byte: alive, dirty, pending destroy
    uint8_t reserved;               // 1 byte: padding; generation lives in m_generations
    uint32_t typeLocalIndex;        // 4 bytes: Index into type-specific array
    uint16_t collisionLayers;       // 2 bytes: Collision layer mask
    uint16_t collisionMask;         // 2 bytes: What layers to collide with
    uint8_t collisionFlags;         // 1 byte: COLLISION_ENABLED, IS_TRIGGER
    uint8_t triggerTag;             // 1 byte: TriggerTag for trigger entities
    uint8_t triggerType;            // 1 byte: TriggerType
    uint8_t _knockbackPad[9];       // 9 bytes: reserved; knockback moved to sidecar
};
static_assert(sizeof(EntityHotData) == 64, "One cache line");
```

Handle generation is tracked in `m_generations`, not in the hot cache line. Transient knockback is stored in `SparseSidecar<KnockbackData>` so only entities currently under knockback occupy dense state.

### TransformData (32 bytes)

```cpp
struct TransformData {
    Vector2D position;         // Current position
    Vector2D previousPosition; // For interpolation
    Vector2D velocity;         // Current velocity
    Vector2D acceleration;     // Current acceleration
};
```

### Type-Specific Data Blocks

| Type | Data Struct | Purpose |
|------|-------------|---------|
| Player/NPC | `CharacterData` | Health, stamina, faction, AI priority |
| DroppedItem | `ItemData` | ResourceHandle, quantity, pickup timer |
| Projectile | `ProjectileData` | Owner, damage, lifetime, speed |
| Container | `ContainerData` | Inventory ID, lock level |
| Harvestable | `HarvestableData` | Yield resource, respawn time |
| AreaEffect | `AreaEffectData` | Radius, damage/tick, duration |

### PathData

Stores pathfinding state for AI entities:

```cpp
struct PathData {
    uint16_t pathLength;            // Number of waypoints (max 32)
    uint16_t navIndex;              // Current waypoint index
    float pathUpdateTimer;          // Time since last path update
    Vector2D currentWaypoint;       // Cached for fast access
    bool hasPath;                   // Quick validity check
    std::atomic<uint8_t> pathRequestPending; // Path request in flight
};
```

### BehaviorData (shared header)

Slimmed, per-entity shared state. Behavior type lives in `BehaviorConfigRef`; variant-specific state lives in per-variant archetype pools (see below):

```cpp
struct BehaviorData {
    uint8_t flags;                  // Valid, initialized
    float moveSpeed;                // Cached from CharacterData
    float separationTimer;
    Vector2D lastSepVelocity;
    float lastCrowdAnalysis;
    int cachedNearbyCount;
    Vector2D cachedClusterCenter;
    uint8_t pendingMessages[8];     // 4 {id, param} pairs
    uint8_t pendingMessageCount;
};
```

### Behavior archetype pools

Config and state for each variant live in dense per-variant vectors. A per-entity `BehaviorConfigRef` (8 bytes) names the active variant and the pool index:

```cpp
struct BehaviorConfigRef {
    BehaviorType type;              // which variant pool
    uint32_t index;                 // slot index in that pool
};

// EDM holds one pair per variant (Idle, Wander, Chase, Patrol, Flee, Follow, Guard, Attack).
// Config and state pools share the same index by invariant (managed lockstep by
// reassignBehaviorConfig / clearBehaviorConfig).
std::vector<WanderBehaviorConfig> m_wanderConfigs;
std::vector<WanderStateData>      m_wanderStates;
std::vector<size_t>               m_wanderOwners;   // owner edmIndex per slot
// ... repeated for each of the 8 variants.
```

Access:
```cpp
auto ref = edm.getBehaviorConfigRef(edmIdx);
if (ref.type == BehaviorType::Wander) {
    const auto& cfg   = edm.getWanderConfig(ref.index);
    auto&       state = edm.getWanderState(ref.index);
}
```

## Simulation Tiers

| Tier | Processing | Distance | Use Case |
|------|------------|----------|----------|
| **Active** | Full AI, collision, render | Near camera | Visible entities |
| **Background** | Position only @ 10Hz | Off-screen | Maintain world consistency |
| **Hibernated** | No updates, data stored | Far away | Memory-only storage |

```cpp
// Update tiers based on distance from player
EntityDataManager::Instance().updateSimulationTiers(playerPosition, 1500.0f, 10000.0f);

// Get indices for processing
auto activeIndices = edm.getActiveIndices();           // For AIManager
auto backgroundIndices = edm.getBackgroundIndices();   // For BackgroundSimManager
```

## Public API Reference

### Lifecycle

```cpp
static EntityDataManager& Instance();
bool init();
void clean();
void prepareForStateTransition();
[[nodiscard]] bool isInitialized() const noexcept;
```

### Entity Creation

```cpp
// Create new entities (returns handle)
EntityHandle createNPC(const Vector2D& position, float halfWidth = 16.0f, float halfHeight = 16.0f);
EntityHandle createPlayer(const Vector2D& position);
EntityHandle createDroppedItem(const Vector2D& position, ResourceHandle handle, int quantity = 1);
EntityHandle createContainer(const Vector2D& position, ContainerType type, uint16_t maxSlots = 20, uint8_t lockLevel = 0, const std::string& worldId = "");
EntityHandle createHarvestable(const Vector2D& position, ResourceHandle yieldResource, int yieldMin = 1, int yieldMax = 3, float respawnTime = 60.0f, const std::string& worldId = "", HarvestType harvestType = HarvestType::Gathering);
EntityHandle createProjectile(const Vector2D& position, const Vector2D& velocity, EntityHandle owner, float damage, float lifetime = 5.0f);
EntityHandle createAreaEffect(const Vector2D& position, float radius, EntityHandle owner, float damage, float duration);
EntityHandle createStaticBody(const Vector2D& position, float halfWidth, float halfHeight);
EntityHandle createTrigger(const Vector2D& position, float halfWidth, float halfHeight, TriggerTag tag, TriggerType type);

// Entity destruction
void destroyEntity(EntityHandle handle);
void processDestructionQueue();  // Call at end of frame
```

### Entity Registration (Legacy Support)

For entities created via legacy `Entity` subclass constructors:

```cpp
EntityHandle registerNPC(EntityID entityId, const Vector2D& position, float halfWidth, float halfHeight, float health, float maxHealth);
EntityHandle registerPlayer(EntityID entityId, const Vector2D& position, float halfWidth, float halfHeight);
EntityHandle registerDroppedItem(EntityID entityId, const Vector2D& position, ResourceHandle handle, int quantity);
void unregisterEntity(EntityID entityId);
```

### Handle Validation

```cpp
[[nodiscard]] bool isValidHandle(EntityHandle handle) const;
[[nodiscard]] size_t getIndex(EntityHandle handle) const;
[[nodiscard]] size_t findIndexByEntityId(EntityID entityId) const;
```

### Transform Access

```cpp
// By handle
TransformData& getTransform(EntityHandle handle);
const TransformData& getTransform(EntityHandle handle) const;

// By index (for batch processing - no map lookup)
TransformData& getTransformByIndex(size_t index);
const TransformData& getTransformByIndex(size_t index) const;
```

### Hot Data Access

```cpp
// By handle
EntityHotData& getHotData(EntityHandle handle);
const EntityHotData& getHotData(EntityHandle handle) const;

// By index (inlined for zero-overhead access)
EntityHotData& getHotDataByIndex(size_t index);
const EntityHotData& getHotDataByIndex(size_t index) const;

// Bulk access
std::span<const EntityHotData> getHotDataArray() const;
```

### Type-Specific Data Access

```cpp
CharacterData& getCharacterData(EntityHandle handle);
ItemData& getItemData(EntityHandle handle);
ProjectileData& getProjectileData(EntityHandle handle);
ContainerData& getContainerData(EntityHandle handle);
HarvestableData& getHarvestableData(EntityHandle handle);
AreaEffectData& getAreaEffectData(EntityHandle handle);

// By index (for batch processing)
CharacterData& getCharacterDataByIndex(size_t index);
```

### Path Data Access

```cpp
PathData& getPathData(size_t index);
const PathData& getPathData(size_t index) const;
bool hasPathData(size_t index) const noexcept;
void ensurePathData(size_t index);
void clearPathData(size_t index);

// Waypoint access (lock-free, per-entity slots)
Vector2D* getWaypointSlot(size_t index) noexcept;
void finalizePath(size_t index, uint16_t length) noexcept;
Vector2D getWaypoint(size_t entityIdx, size_t waypointIdx) const;
Vector2D getCurrentWaypoint(size_t entityIdx) const;
Vector2D getPathGoal(size_t entityIdx) const;
```

### Behavior Data Access

```cpp
BehaviorData& getBehaviorData(size_t index);
const BehaviorData& getBehaviorData(size_t index) const;
bool hasBehaviorData(size_t index) const noexcept;
void initBehaviorData(size_t index, BehaviorType type);
void clearBehaviorData(size_t index);
```

Variant-specific behavior config and state live in per-type dense pools. Use `getBehaviorConfigRef(index)` to read the active `BehaviorType` and pool index, and `reassignBehaviorConfig(...)` / `clearBehaviorConfig(...)` for structural changes. `BehaviorData` is shared cross-behavior state only.

### Knockback Sidecar

```cpp
KnockbackData& applyKnockback(size_t edmIdx);
KnockbackData* getKnockback(size_t edmIdx) noexcept;
bool hasKnockback(size_t edmIdx) const noexcept;
void clearKnockback(size_t edmIdx) noexcept;
size_t knockbackActiveCount() const noexcept;
SparseSidecar<KnockbackData>& knockbackSidecar() noexcept;
```

`EventManager` applies knockback when processing `DamageEvent`. `AIManager` and player movement consume and decay it during update. Expired entries are cleared on the main thread after worker batches join.

### Inventory Data

```cpp
uint32_t createInventory(uint16_t maxSlots, bool worldTracked = false);
bool initNPCAsMerchant(EntityHandle handle, uint16_t maxSlots = 20);
uint32_t getNPCInventoryIndex(EntityHandle handle) const;
bool addToInventory(uint32_t inventoryIndex, ResourceHandle handle, int quantity);
bool removeFromInventory(uint32_t inventoryIndex, ResourceHandle handle, int quantity);
int getInventoryQuantity(uint32_t inventoryIndex, ResourceHandle handle) const;
std::unordered_map<ResourceHandle, int> getInventoryResources(uint32_t inventoryIndex) const;
InventorySlotData getInventorySlot(uint32_t inventoryIndex, size_t slotIndex) const;
size_t getInventorySlots(uint32_t inventoryIndex, std::span<InventorySlotData> outSlots) const;
bool swapInventorySlots(uint32_t inventoryIndex, size_t sourceSlot, size_t targetSlot);
```

Inventories are EDM-backed data, not legacy entity-owned inventory components. Containers auto-create inventories, merchants use EDM inventory indices, and world-tracked inventories register aggregate data with `WorldResourceManager`.

Inventory APIs are split by use case:

- `getInventoryResources(...)` returns aggregate quantities by `ResourceHandle` for world/resource/social systems that do not care about layout.
- `getInventorySlot(...)` and `getInventorySlots(...)` expose ordered physical slot contents. Prefer the span-based bulk read for UI refreshes so callers take one inventory lock and reuse caller-owned storage.
- `swapInventorySlots(...)` is a storage primitive only. It validates the inventory and slot indices, works across inline and overflow slots, preserves `usedSlots`, and marks the inventory dirty only when slot contents actually change.

Drag/drop, player policy, hotbar assignment, and UI feedback belong in `InventoryController`, not EDM.

### Simulation Tier Management

```cpp
void setSimulationTier(EntityHandle handle, SimulationTier tier);
void updateSimulationTiers(const Vector2D& referencePoint, float activeRadius = 1500.0f, float backgroundRadius = 10000.0f);

// Get indices by tier
std::span<const size_t> getActiveIndices() const;
std::span<const size_t> getBackgroundIndices() const;
std::span<const size_t> getActiveIndicesWithCollision() const;
std::span<const size_t> getTriggerDetectionIndices() const;
std::span<const size_t> getIndicesByKind(EntityKind kind) const;
```

### Active Indices API

The `getActiveIndices()` family of methods returns pre-computed lists of entity indices filtered by simulation tier and other criteria. These lists are updated by `updateSimulationTiers()` and enable efficient batch processing.

#### Why Use Active Indices?

Instead of iterating all entities and checking tier/kind per-entity:

```cpp
// SLOW: Check every entity, filter at runtime
for (size_t i = 0; i < edm.getEntityCount(); ++i) {
    const auto& hot = edm.getHotDataByIndex(i);
    if (hot.tier != SimulationTier::Active) continue;  // Branching per entity
    if (hot.kind != EntityKind::NPC) continue;
    processEntity(i);
}

// FAST: Use pre-filtered index list
for (size_t edmIndex : edm.getActiveIndices()) {
    // All indices are guaranteed Active tier
    processEntity(edmIndex);
}
```

#### Available Index Lists

| Method | Returns | Use Case |
|--------|---------|----------|
| `getActiveIndices()` | All Active tier entities | AIManager batch processing |
| `getBackgroundIndices()` | All Background tier entities | BackgroundSimulationManager |
| `getActiveIndicesWithCollision()` | Active entities with collision enabled | CollisionManager |
| `getTriggerDetectionIndices()` | Entities that detect triggers | Trigger overlap checks |
| `getIndicesByKind(kind)` | All entities of specific EntityKind | Type-specific processing |

#### Tier Update Frequency

Tier assignments are recalculated periodically (not every frame) for performance:

```cpp
// In GameEngine or BackgroundSimulationManager
// Called every ~60 frames (~1 second at 60Hz)
if (m_framesSinceTierUpdate++ >= TIER_UPDATE_INTERVAL) {
    edm.updateSimulationTiers(playerPosition, activeRadius, backgroundRadius);
    m_framesSinceTierUpdate = 0;
}
```

#### Batch Processing Pattern

```cpp
void AIManager::update(float dt)
{
    auto& edm = EntityDataManager::Instance();

    m_activeIndicesBuffer.assign(
        edm.getActiveIndices().begin(), edm.getActiveIndices().end());

    auto decision = WorkerBudgetManager::Instance().shouldUseThreading(
        SystemType::AI, m_activeIndicesBuffer.size());

    auto [batchCount, batchSize] = WorkerBudgetManager::Instance()
        .getBatchStrategy(SystemType::AI, m_activeIndicesBuffer.size(), workers);

    for (size_t batch = 0; batch < batchCount; ++batch)
    {
        size_t start = batch * batchSize;
        size_t end = std::min(start + batchSize, m_activeIndicesBuffer.size());

        threadSystem.enqueueTask([this, start, end, dt] {
            processBatch(dt, start, end);
        });
    }
}
```

`AIManager::processBatch(...)` reads EDM indices from `m_activeIndicesBuffer`.
The fused per-entity loop then builds a `BehaviorContext`, switches on the
entity's `BehaviorConfigRef::type`, calls the typed behavior executor with the
matching dense config/state pool entries, accumulates movement, and consumes
knockback sidecar state. Structural outputs such as behavior transitions,
ranged attacks, and equipment fallback requests go through `AICommandBus` and
are committed after worker batches join.

### Queries

```cpp
void queryEntitiesInRadius(const Vector2D& center, float radius, std::vector<EntityHandle>& outHandles, EntityKind kindFilter = EntityKind::COUNT) const;
size_t getEntityCount() const noexcept;
size_t getEntityCount(EntityKind kind) const noexcept;
size_t getEntityCount(SimulationTier tier) const noexcept;
EntityID getEntityId(size_t index) const;
EntityHandle getHandle(size_t index) const;
```

## Usage Examples

### Creating Entities

```cpp
auto& edm = EntityDataManager::Instance();

// Create NPC
EntityHandle npc = edm.createNPC(Vector2D(100, 200), 16.0f, 16.0f);

// Access data
auto& transform = edm.getTransform(npc);
transform.velocity = Vector2D(50, 0);

auto& character = edm.getCharacterData(npc);
character.health = 80.0f;
character.faction = 1;  // Enemy
```

### Batch Processing (AI/Collision)

```cpp
void AIManager::processBatch(float dt, size_t start, size_t end)
{
    auto& edm = EntityDataManager::Instance();

    for (size_t i = start; i < end; ++i)
    {
        size_t edmIndex = m_activeIndicesBuffer[i];

        // Direct index access - no map lookups
        EntityHotData& hot = edm.getHotDataByIndex(edmIndex);
        if (!hot.isAlive() || hot.kind != EntityKind::NPC) continue;

        BehaviorData& behavior = edm.getBehaviorData(edmIndex);
        BehaviorConfigRef ref = edm.getBehaviorConfigRef(edmIndex);

        switch (ref.type) {
        case BehaviorType::Wander:
            Behaviors::executeWander(ctx,
                edm.getWanderConfig(ref.index),
                edm.getWanderState(ref.index));
            break;
        default:
            break;
        }
    }
}
```

### Tier-Based Processing

```cpp
void GameEngine::update(float dt) {
    auto& edm = EntityDataManager::Instance();

    // Update tiers periodically
    edm.updateSimulationTiers(playerPosition);

    // AIManager processes Active tier
    AIManager::Instance().update(dt);

    // CollisionManager processes Active tier with collision
    CollisionManager::Instance().update(dt);

    // BackgroundSimManager processes Background tier at 10Hz
    BackgroundSimulationManager::Instance().update(playerPosition, dt);
}
```

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `getHotDataByIndex()` | O(1) | Inlined, zero overhead |
| `getTransformByIndex()` | O(1) | Inlined, zero overhead |
| `getIndex(handle)` | O(1) | Map lookup (main thread only) |
| `createNPC()` | O(1) amortized | May grow vectors |
| `destroyEntity()` | O(1) | Queued, processed end of frame |
| `updateSimulationTiers()` | O(n) | Called periodically, not every frame |

### Memory Layout

- **Dynamic entities**: Contiguous in `m_hotData` (~5MB for 10K entities)
- **Static entities**: Separate `m_staticHotData` (never tiered)
- **Type-specific data**: Indexed by `typeLocalIndex` in hot data

## Related Documentation

- **[EntityHandle](../entities/EntityHandle.md)** - Lightweight entity references
- **[BackgroundSimulationManager](BackgroundSimulationManager.md)** - Off-screen entity processing
- **[AIManager](../ai/AIManager.md)** - AI behavior processing
- **[CollisionManager](CollisionManager.md)** - Collision detection
