# EntityHandle

**Code:** `include/entities/EntityHandle.hpp`

## Overview

`EntityHandle` is a lightweight 16-byte struct for referencing entities in EntityDataManager. It provides:

- **Fast entity identification** via EntityID
- **Type information** via EntityKind (no RTTI needed)
- **Stale reference detection** via generation counter

Handles are the primary way to reference entities throughout the codebase. They are cheap to copy and compare, making them suitable for containers and passing by value.

## Structure

```cpp
struct EntityHandle {
    using IDType = uint64_t;
    using Generation = uint32_t;

    IDType id{INVALID_ID};                      // 8 bytes: Unique identifier
    EntityKind kind{EntityKind::NPC};           // 1 byte: Entity type
    uint8_t padding[3]{0, 0, 0};                // 3 bytes: Alignment
    Generation generation{INVALID_GENERATION};  // 4 bytes: Stale detection
    // Total: 16 bytes (8-byte aligned)
};
```

## EntityKind Enumeration

```cpp
enum class EntityKind : uint8_t {
    // Characters (have health, AI behaviors)
    Player = 0,
    NPC = 1,

    // Interactables (world objects)
    DroppedItem = 2,
    Container = 3,      // Chests, barrels, corpse loot
    Harvestable = 4,    // Trees, ore nodes

    // Combat (physics-driven, short-lived)
    Projectile = 5,
    AreaEffect = 6,     // AoE spell zones, traps

    // Environment
    Prop = 7,           // Decorations, animated objects
    Trigger = 8,        // Invisible trigger zones

    // World geometry
    StaticObstacle = 9, // Tiles, walls, terrain collision

    COUNT
};
```

## SimulationTier Enumeration

```cpp
enum class SimulationTier : uint8_t {
    Active = 0,      // Full update: AI, collision, render
    Background = 1,  // Simplified: position only, no collision
    Hibernated = 2   // Minimal: data stored, no updates
};
```

## Public API Reference

### Construction

```cpp
// Default constructor (invalid handle)
constexpr EntityHandle() noexcept;

// Full construction
constexpr EntityHandle(IDType entityId, EntityKind entityKind, Generation gen) noexcept;
```

### Validity Check

```cpp
[[nodiscard]] constexpr bool isValid() const noexcept;
```

Returns `true` if `id != INVALID_ID && generation != INVALID_GENERATION`.

### Accessors

```cpp
[[nodiscard]] constexpr IDType getId() const noexcept;
[[nodiscard]] constexpr EntityKind getKind() const noexcept;
[[nodiscard]] constexpr Generation getGeneration() const noexcept;
```

### Type Checking Helpers

```cpp
[[nodiscard]] constexpr bool isPlayer() const noexcept;
[[nodiscard]] constexpr bool isNPC() const noexcept;
[[nodiscard]] constexpr bool isItem() const noexcept;
[[nodiscard]] constexpr bool isProjectile() const noexcept;
```

### Trait Helpers

```cpp
[[nodiscard]] constexpr bool hasHealth() const noexcept;    // Player, NPC
[[nodiscard]] constexpr bool hasInventory() const noexcept; // Player, NPC, Container
[[nodiscard]] constexpr bool hasPhysics() const noexcept;   // Most types
[[nodiscard]] constexpr bool hasAI() const noexcept;        // NPC only
```

### Comparison Operators

```cpp
[[nodiscard]] constexpr bool operator==(const EntityHandle& other) const noexcept;
[[nodiscard]] constexpr bool operator!=(const EntityHandle& other) const noexcept;
[[nodiscard]] constexpr bool operator<(const EntityHandle& other) const noexcept;
```

### Hash Support

```cpp
[[nodiscard]] std::size_t hash() const noexcept;
```

Enables use in `std::unordered_map` and `std::unordered_set`.

### String Conversion

```cpp
[[nodiscard]] std::string toString() const;
// Returns: "EntityHandle(123:NPC:1)" or "EntityHandle::INVALID"
```

## EntityTraits Namespace

Helper functions for EntityKind:

```cpp
namespace EntityTraits {
    constexpr bool hasHealth(EntityKind kind) noexcept;
    constexpr bool hasInventory(EntityKind kind) noexcept;
    constexpr bool hasPhysics(EntityKind kind) noexcept;
    constexpr bool hasAI(EntityKind kind) noexcept;
    constexpr bool isRenderable(EntityKind kind) noexcept;
    constexpr const char* kindToString(EntityKind kind) noexcept;
    constexpr const char* tierToString(SimulationTier tier) noexcept;
}
```

## Constants

```cpp
// Invalid handle constant
inline constexpr EntityHandle INVALID_ENTITY_HANDLE{};

// Special values
static constexpr IDType INVALID_ID = 0;
static constexpr Generation INVALID_GENERATION = 0;
```

## Usage Examples

### Creating and Using Handles

```cpp
// Create entity via EntityDataManager
auto& edm = EntityDataManager::Instance();
EntityHandle npc = edm.createNPC(Vector2D(100, 200));

// Check validity
if (npc.isValid()) {
    // Access data
    auto& transform = edm.getTransform(npc);
    transform.velocity = Vector2D(50, 0);
}

// Type checking (no RTTI)
if (npc.isNPC()) {
    auto& character = edm.getCharacterData(npc);
    character.health -= 10.0f;
}
```

### Using Trait Helpers

```cpp
void processEntity(EntityHandle handle) {
    if (handle.hasHealth()) {
        auto& character = edm.getCharacterData(handle);
        // Process health/combat...
    }

    if (handle.hasAI()) {
        auto& behavior = edm.getBehaviorData(edm.getIndex(handle));
        // Process AI...
    }
}
```

### Storing in Containers

```cpp
// Vector of handles
std::vector<EntityHandle> enemies;
enemies.push_back(edm.createNPC(pos1));
enemies.push_back(edm.createNPC(pos2));

// Unordered map (hash support)
std::unordered_map<EntityHandle, float> damageDealt;
damageDealt[npc] = 50.0f;

// Set (comparison support)
std::set<EntityHandle> processedEntities;
processedEntities.insert(npc);
```

### Generation Counter for Stale Detection

```cpp
EntityHandle savedHandle = npc;  // Save handle

// Later... entity might have been destroyed and slot reused
if (edm.isValidHandle(savedHandle)) {
    // Handle is still valid, same entity
    auto& data = edm.getHotData(savedHandle);
} else {
    // Entity was destroyed, or slot reused with new generation
    // Handle is stale, don't use it
}
```

### Switch on EntityKind

```cpp
void renderEntity(EntityHandle handle) {
    switch (handle.getKind()) {
        case EntityKind::Player:
            renderPlayer(handle);
            break;
        case EntityKind::NPC:
            renderNPC(handle);
            break;
        case EntityKind::DroppedItem:
            renderItem(handle);
            break;
        case EntityKind::Trigger:
            // Triggers are invisible, skip
            break;
        default:
            renderGeneric(handle);
            break;
    }
}
```

## Performance Characteristics

| Aspect | Value | Notes |
|--------|-------|-------|
| Size | 16 bytes | 8-byte aligned |
| Copy cost | Trivial | Pass by value is fine |
| Comparison | O(1) | Direct member comparison |
| Hash | O(1) | Combines id, kind, generation |

## Why Use EntityHandle?

### vs. Raw Pointers
- **Safer**: Generation counter detects stale references
- **Lighter**: 16 bytes vs 8 bytes, but includes type info
- **Type info**: No RTTI needed for type checking

### vs. EntityID Only
- **Includes type**: Fast `isNPC()` without map lookup
- **Generation tracking**: Detects slot reuse
- **Self-describing**: Debug output shows type

### vs. Entity* References
- **Decoupled**: Doesn't require Entity class hierarchy
- **DOD friendly**: Works with SoA storage in EDM
- **Serializable**: Can be saved/loaded easily

## Related Documentation

- **[EntityDataManager](../managers/EntityDataManager.md)** - Creates and validates handles
- **[Entity](README.md)** - Entity base class documentation
