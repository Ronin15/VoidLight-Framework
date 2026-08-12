/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef ENTITY_DATA_TYPES_HPP
#define ENTITY_DATA_TYPES_HPP

/**
 * @file EntityDataTypes.hpp
 * @brief Free-standing SoA component data types for EntityDataManager
 *
 * These are plain value types with no dependency on the EntityDataManager
 * class itself — extracted from EntityDataManager.hpp so lightweight
 * consumers (e.g. BehaviorExecutors.hpp) can use them without pulling in
 * the ~1900-line manager class, its singleton/threading machinery, and
 * everything EntityDataManager.hpp transitively includes for that.
 *
 * EntityDataManager.hpp includes this header and IS the processing/storage
 * layer built on top of these types.
 */

#include "ai/BehaviorConfig.hpp"        // BehaviorType
#include "collisions/CollisionBody.hpp" // CollisionLayer
#include "collisions/TriggerTag.hpp"    // TriggerType
#include "entities/Entity.hpp"          // EntityKind, SimulationTier, EntityHandle, AnimationConfig
#include "utils/ResourceHandle.hpp"     // ResourceHandle
#include "utils/Vector2D.hpp"
#include "world/HarvestType.hpp"        // HarvestType
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <string>
#include <vector>

// ============================================================================
// CONSTANTS
// ============================================================================

/// Invalid inventory index constant (defined early for use in struct defaults)
static constexpr uint32_t INVALID_INVENTORY_INDEX = std::numeric_limits<uint32_t>::max();
static constexpr size_t CHARACTER_EQUIPMENT_SLOT_COUNT = 9;

/**
 * @brief Per-entity archetype reference for behavior config storage (8 bytes)
 *
 * Replaces the old 388-byte BehaviorConfigData per-entity slot.
 * Points into one of the per-variant dense pools owned by EDM.
 * index == UINT32_MAX when type == BehaviorType::None (no active config).
 */
struct BehaviorConfigRef
{
    BehaviorType type{BehaviorType::None}; // 1 byte
    uint8_t _pad[3]{};                     // 3 bytes alignment
    uint32_t index{std::numeric_limits<uint32_t>::max()}; // 4 bytes
};
static_assert(sizeof(BehaviorConfigRef) == 8, "BehaviorConfigRef must be exactly 8 bytes");

/**
 * @brief Transform data for entity movement (32 bytes)
 */
struct TransformData {
    Vector2D position{0.0f, 0.0f};         // Current position (8 bytes)
    Vector2D previousPosition{0.0f, 0.0f}; // For interpolation (8 bytes)
    Vector2D velocity{0.0f, 0.0f};         // Current velocity (8 bytes)
    Vector2D acceleration{0.0f, 0.0f};     // Current acceleration (8 bytes)
};

static_assert(sizeof(TransformData) == 32, "TransformData should be 32 bytes");

/**
 * @brief Per-entity transient knockback state — stored in SparseSidecar<KnockbackData>.
 *
 * Only entities that are currently being knocked back occupy space in the dense array.
 * framesRemaining is a fixed-timestep frame count (see Knockback::FRAMES / DECAY).
 */
struct KnockbackData
{
    float   impulseX{0.0f};         // 4 bytes: knockback impulse X component
    float   impulseY{0.0f};         // 4 bytes: knockback impulse Y component
    uint8_t framesRemaining{0};     // 1 byte:  remaining fixed-timestep frames
    bool    justApplied{false};     // 1 byte:  true on the first tick after a hit is applied;
                                    //           cleared by AIManager after consuming the REPLACE path
};

/**
 * @brief Hot data accessed every frame (64 bytes, one cache line)
 *
 * Packed for sequential access during batch processing.
 * All frequently-accessed data in one contiguous array.
 *
 * NOTE: This is for DYNAMIC entities (Player, NPC, Projectile, etc.) that:
 * - Move around and have AI/physics
 * - Are managed by the tier system (Active/Background/Hibernated)
 * - Only Active tier entities participate in collision detection
 *
 * STATIC obstacles (walls, buildings, terrain) are NOT stored here.
 * They live in CollisionManager's m_staticBodies storage and are always
 * checked for collision regardless of tier. This separation allows:
 * - Statics to never be iterated unnecessarily
 * - Statics to be in a compact spatial hash for O(1) queries
 * - Dynamic entities to be tier-filtered efficiently
 */
struct alignas(64) EntityHotData {
    TransformData transform;        // 32 bytes
    float halfWidth{16.0f};         // 4 bytes: Half-width for collision
    float halfHeight{16.0f};        // 4 bytes: Half-height for collision
    EntityKind kind{EntityKind::NPC};           // 1 byte
    SimulationTier tier{SimulationTier::Active}; // 1 byte
    uint8_t flags{0};               // 1 byte: alive, dirty, etc.
    uint8_t reserved{0};           // 1 byte: padding (generation lives in m_generations vector)
    uint32_t typeLocalIndex{0};     // 4 bytes: Index into type-specific array

    // Collision data (only for entities that participate in collision)
    uint16_t collisionLayers{VoidLight::CollisionLayer::Layer_Default};  // 2 bytes: Which layer(s) this entity is on
    uint16_t collisionMask{0xFFFF};  // 2 bytes: Which layers this entity collides with
    uint8_t collisionFlags{0};       // 1 byte: COLLISION_ENABLED, IS_TRIGGER
    uint8_t triggerTag{0};           // 1 byte: TriggerTag for trigger entities
    uint8_t triggerType{0};          // 1 byte: TriggerType (EventOnly, Physical)
    // 9 bytes freed by moving knockback to SparseSidecar<KnockbackData> m_knockback on EDM.
    // KnockbackData (impulseX 4B + impulseY 4B + framesRemaining 1B = 9B) was removed from
    // the hot line because only a small fraction of entities are knocked back at any time.
    // Explicit padding preserves the 64-byte cache-line size and makes the removal visible in diffs.
    uint8_t _knockbackPad[9]{};      // 9 bytes: reserved — formerly knockback inline fields

    // Entity flag constants
    static constexpr uint8_t FLAG_ALIVE = 0x01;
    static constexpr uint8_t FLAG_DIRTY = 0x02;
    static constexpr uint8_t FLAG_PENDING_DESTROY = 0x04;

    // Collision flag constants
    static constexpr uint8_t COLLISION_ENABLED = 0x01;
    static constexpr uint8_t IS_TRIGGER = 0x02;
    static constexpr uint8_t NEEDS_TRIGGER_DETECTION = 0x04;

    [[nodiscard]] bool isAlive() const noexcept { return flags & FLAG_ALIVE; }
    [[nodiscard]] bool isDirty() const noexcept { return flags & FLAG_DIRTY; }
    [[nodiscard]] bool isPendingDestroy() const noexcept {
        return flags & FLAG_PENDING_DESTROY;
    }
    [[nodiscard]] bool hasCollision() const noexcept {
        return collisionFlags & COLLISION_ENABLED;
    }
    [[nodiscard]] bool isTrigger() const noexcept {
        return collisionFlags & IS_TRIGGER;
    }
    [[nodiscard]] bool needsTriggerDetection() const noexcept {
        return collisionFlags & NEEDS_TRIGGER_DETECTION;
    }

    void setAlive(bool alive) noexcept {
        if (alive) flags |= FLAG_ALIVE;
        else flags &= ~FLAG_ALIVE;
    }
    void setDirty(bool dirty) noexcept {
        if (dirty) flags |= FLAG_DIRTY;
        else flags &= ~FLAG_DIRTY;
    }
    void markForDestruction() noexcept { flags |= FLAG_PENDING_DESTROY; }

    void setCollisionEnabled(bool enabled) noexcept {
        if (enabled) collisionFlags |= COLLISION_ENABLED;
        else collisionFlags &= ~COLLISION_ENABLED;
    }
    void setTrigger(bool trigger) noexcept {
        if (trigger) collisionFlags |= IS_TRIGGER;
        else collisionFlags &= ~IS_TRIGGER;
    }
    void setTriggerDetection(bool enabled) noexcept {
        if (enabled) collisionFlags |= NEEDS_TRIGGER_DETECTION;
        else collisionFlags &= ~NEEDS_TRIGGER_DETECTION;
    }

    [[nodiscard]] bool isEventOnlyTrigger() const noexcept {
        return isTrigger() && triggerType == static_cast<uint8_t>(VoidLight::TriggerType::EventOnly);
    }
};

static_assert(sizeof(EntityHotData) == 64, "EntityHotData should be 64 bytes (one cache line)");
static_assert(alignof(EntityHotData) == 64, "EntityHotData should be 64-byte aligned");

// ============================================================================
// TYPE-SPECIFIC DATA BLOCKS
// ============================================================================

/**
 * @brief Creature category for distinguishing NPCs, Monsters, and Animals
 *
 * Used by CharacterData to identify the creature composition system in use.
 */
enum class CreatureCategory : uint8_t {
    NPC = 0,      // Humanoid characters (race + class)
    Monster = 1,  // Hostile creatures (type + variant)
    Animal = 2    // Wildlife (species + role)
};

/**
 * @brief Biological sex for creatures
 */
enum class Sex : uint8_t {
    Male = 0,
    Female = 1,
    Unknown = 2   // For creatures where sex is undefined/irrelevant
};

/**
 * @brief Character data for Player, NPC, Monster, and Animal entities
 *
 * Unified character data for all creature types. The category field
 * distinguishes NPCs (race+class), Monsters (type+variant), and Animals (species+role).
 * typeId and subtypeId reference the appropriate registries based on category.
 */
struct CharacterData {
    // Stats (computed from base × modifier at creation)
    float health{100.0f};
    float baseMaxHealth{100.0f};
    float maxHealth{100.0f};
    float stamina{100.0f};
    float maxStamina{100.0f};
    float baseAttackDamage{10.0f};
    float attackDamage{10.0f};
    float attackRange{50.0f};
    float baseAttackRange{50.0f};
    float baseMoveSpeed{100.0f};
    float moveSpeed{100.0f};   // Effective movement speed
    float armorDefense{0.0f};  // Effective defense from equipped gear
    float mass{1.0f};          // Physical mass (affects knockback resistance)
    float projectileSpeed{0.0f}; // Ranged projectile speed (px/s), 0 = melee
    float baseProjectileSpeed{0.0f};

    // Identity (creature composition)
    CreatureCategory category{CreatureCategory::NPC};  // NPC, Monster, or Animal
    Sex sex{Sex::Unknown};     // Male, Female, or Unknown
    uint8_t typeId{0};         // raceId / monsterTypeId / speciesId
    uint8_t subtypeId{0};      // classId / variantId / roleId

    // Faction and AI
    uint8_t faction{0};        // 0=Friendly, 1=Enemy, 2=Neutral
    uint8_t behaviorType{0};   // BehaviorType enum
    uint8_t priority{5};       // AI priority (0-9)
    uint8_t stateFlags{0};     // alive, stunned, invulnerable, etc.
    enum CombatStyle : uint8_t { Melee = 0, Ranged = 1 };
    uint8_t combatStyle{CombatStyle::Melee};
    uint8_t baseCombatStyle{CombatStyle::Melee};

    // Inventory (for merchants and NPCs that carry items)
    uint32_t inventoryIndex{INVALID_INVENTORY_INDEX};  // EDM inventory index
    std::array<VoidLight::ResourceHandle, CHARACTER_EQUIPMENT_SLOT_COUNT>
        equippedItems{};

    // Emotional resilience from class (affects emotion changes)
    float emotionalResilience{0.5f};

    static constexpr uint8_t FLAG_MERCHANT = 0x08;    // Can trade with player

    [[nodiscard]] bool isMerchant() const noexcept {
        return (stateFlags & FLAG_MERCHANT) != 0;
    }

    [[nodiscard]] bool hasInventory() const noexcept {
        return inventoryIndex != INVALID_INVENTORY_INDEX;
    }
};

/**
 * @brief Item data for DroppedItem entities
 */
struct ItemData {
    VoidLight::ResourceHandle resourceHandle;  // Item template reference
    int quantity{1};
    float pickupTimer{0.5f};    // Delay before pickup allowed
    float bobTimer{0.0f};       // Visual bobbing effect
    uint8_t flags{0};

    static constexpr uint8_t FLAG_CAN_PICKUP = 0x01;
    static constexpr uint8_t FLAG_IS_STACKED = 0x02;

    [[nodiscard]] bool canPickup() const noexcept {
        return (flags & FLAG_CAN_PICKUP) && quantity > 0;
    }
};

/**
 * @brief Projectile data for Projectile entities
 */
struct ProjectileData {
    EntityHandle owner;         // Who fired this projectile
    EntityHandle embeddedTarget; // Dynamic target this projectile is stuck into
    float damage{10.0f};
    float lifetime{5.0f};       // Time until despawn
    float speed{200.0f};
    float embeddedOffsetX{0.0f};
    float embeddedOffsetY{0.0f};
    float embeddedAngle{0.0f};  // Flight angle (radians) preserved at embed time
    uint8_t damageType{0};      // Physical, Fire, Ice, etc.
    uint8_t flags{0};

    static constexpr uint8_t FLAG_PIERCING = 0x01;
    static constexpr uint8_t FLAG_HOMING = 0x02;
    static constexpr uint8_t FLAG_EXPLOSIVE = 0x04;
    static constexpr uint8_t FLAG_EMBEDDED = 0x08;

    static constexpr float EMBEDDED_LIFETIME_SECONDS = 1.25f;
    static constexpr float EMBEDDED_FADE_SECONDS = 0.45f;

    [[nodiscard]] bool isEmbedded() const noexcept {
        return flags & FLAG_EMBEDDED;
    }
};

/**
 * @brief Container data for Container entities (chests, barrels)
 */
/**
 * @brief Container types for chests, barrels, corpses, etc.
 */
enum class ContainerType : uint8_t {
    Chest = 0,
    Barrel = 1,
    Corpse = 2,
    Crate = 3,
    COUNT
};

/**
 * @brief Container data for Container entities (chests, barrels)
 */
struct ContainerData {
    uint32_t inventoryIndex{INVALID_INVENTORY_INDEX};  // EDM inventory index
    uint16_t maxSlots{20};
    uint8_t containerType{0};   // ContainerType enum value
    uint8_t lockLevel{0};       // 0 = unlocked, 1-10 = lock difficulty

    // Container state flags
    static constexpr uint8_t FLAG_IS_OPEN = 0x01;
    static constexpr uint8_t FLAG_IS_LOCKED = 0x02;
    static constexpr uint8_t FLAG_WAS_LOOTED = 0x04;
    uint8_t flags{0};

    [[nodiscard]] bool isOpen() const noexcept { return flags & FLAG_IS_OPEN; }
    [[nodiscard]] bool isLocked() const noexcept { return flags & FLAG_IS_LOCKED; }
    [[nodiscard]] bool wasLooted() const noexcept { return flags & FLAG_WAS_LOOTED; }

    void setOpen(bool v) noexcept {
        if (v) flags |= FLAG_IS_OPEN;
        else flags &= ~FLAG_IS_OPEN;
    }

    void setLocked(bool v) noexcept {
        if (v) flags |= FLAG_IS_LOCKED;
        else flags &= ~FLAG_IS_LOCKED;
    }

    void setLooted(bool v) noexcept {
        if (v) flags |= FLAG_WAS_LOOTED;
        else flags &= ~FLAG_WAS_LOOTED;
    }
};

/**
 * @brief Harvestable data for resource nodes (trees, ore, gems)
 *
 * Used by HarvestController for progress-based harvesting.
 * harvestType determines duration and action verb via HarvestConfig.
 */
struct HarvestableData {
    VoidLight::ResourceHandle yieldResource;
    int yieldMin{1};
    int yieldMax{3};
    float respawnTime{60.0f};   // Seconds until respawn
    float currentRespawn{0.0f}; // Time remaining
    VoidLight::HarvestType harvestType{VoidLight::HarvestType::Gathering};
    bool isDepleted{false};
};

// ============================================================================
// INVENTORY DATA STRUCTURES
// ============================================================================

/**
 * @brief Single inventory slot data (12 bytes)
 *
 * Compact slot for inventory storage. ResourceHandle provides type-safe
 * resource identification via ResourceTemplateManager.
 */
struct InventorySlotData {
    VoidLight::ResourceHandle resourceHandle;  // 8 bytes: Type-safe resource reference (6 + padding)
    int16_t quantity{0};                          // 2 bytes: Stack quantity
    int16_t _pad{0};                              // 2 bytes: Padding for alignment

    [[nodiscard]] bool isEmpty() const noexcept { return quantity <= 0 || !resourceHandle.isValid(); }
    void clear() noexcept { resourceHandle = VoidLight::ResourceHandle{}; quantity = 0; _pad = 0; }
};

// InventorySlotData is ~12 bytes (ResourceHandle 8 + quantity 2 + pad 2)

/**
 * @brief Inventory data with inline slots (128 bytes, 2 cache lines)
 *
 * Stores up to INLINE_SLOT_COUNT slots inline. Larger inventories use
 * InventoryOverflow for additional slots beyond the inline capacity.
 *
 * Design: Player has 50 slots (8 inline + 42 overflow), NPC loot containers
 * have fewer slots and often fit entirely inline.
 */
struct InventoryData {
    static constexpr size_t INLINE_SLOT_COUNT = 8;

    // Flags for inventory state
    static constexpr uint8_t FLAG_VALID = 0x01;         // Slot is in use
    static constexpr uint8_t FLAG_WORLD_TRACKED = 0x02; // Registered with WorldResourceManager
    static constexpr uint8_t FLAG_DIRTY = 0x04;         // Needs cache rebuild

    InventorySlotData slots[INLINE_SLOT_COUNT];   // 96 bytes: Inline slot storage (8 * 12)
    uint32_t overflowId{0};                       // 4 bytes: ID into overflow map (0 = none)
    uint16_t maxSlots{INLINE_SLOT_COUNT};         // 2 bytes: Max slots for this inventory
    uint16_t usedSlots{0};                        // 2 bytes: Current used slot count
    uint8_t flags{0};                             // 1 byte: State flags
    uint8_t ownerKind{0};                         // 1 byte: EntityKind of owner (for debugging)
    uint8_t _padding[22]{};                       // 22 bytes: Pad to 128 bytes

    [[nodiscard]] bool isValid() const noexcept { return flags & FLAG_VALID; }
    [[nodiscard]] bool isWorldTracked() const noexcept { return flags & FLAG_WORLD_TRACKED; }
    [[nodiscard]] bool needsOverflow() const noexcept { return maxSlots > INLINE_SLOT_COUNT; }

    void setValid(bool v) noexcept {
        if (v) flags |= FLAG_VALID;
        else flags &= ~FLAG_VALID;
    }

    void setWorldTracked(bool v) noexcept {
        if (v) flags |= FLAG_WORLD_TRACKED;
        else flags &= ~FLAG_WORLD_TRACKED;
    }

    void clear() noexcept {
        for (auto& slot : slots) slot.clear();
        overflowId = 0;
        maxSlots = INLINE_SLOT_COUNT;
        usedSlots = 0;
        flags = 0;
        ownerKind = 0;
    }
};

// InventoryData target: ~128 bytes (may vary with compiler padding)

/**
 * @brief Overflow storage for large inventories
 *
 * When an inventory needs more than INLINE_SLOT_COUNT (12) slots,
 * additional slots are stored here. The overflowId in InventoryData
 * maps to an entry in EntityDataManager::m_inventoryOverflow.
 */
struct InventoryOverflow {
    std::vector<InventorySlotData> extraSlots;  // Slots beyond inline capacity

    void clear() noexcept { extraSlots.clear(); }
};

struct InventoryResourceChange {
    VoidLight::ResourceHandle resourceHandle{};
    int oldQuantity{0};
    int newQuantity{0};

    [[nodiscard]] bool isValid() const noexcept {
        return resourceHandle.isValid() && oldQuantity != newQuantity;
    }
};

struct InventoryTransferResult {
    InventoryResourceChange sourceChange{};
    InventoryResourceChange targetChange{};

    [[nodiscard]] bool isValid() const noexcept {
        return sourceChange.isValid() || targetChange.isValid();
    }
};

/**
 * @brief Area effect data for AoE zones (spell effects, traps)
 */
struct AreaEffectData {
    EntityHandle owner;         // Who created this effect
    float radius{50.0f};
    float damage{5.0f};         // Damage per tick
    float tickInterval{0.5f};   // Seconds between ticks
    float duration{5.0f};       // Total duration
    float elapsed{0.0f};        // Time since creation
    float lastTick{0.0f};       // Time since last damage tick
    uint8_t effectType{0};      // Poison, Fire, Heal, Slow
};

/**
 * @brief Render data for data-driven NPCs (velocity-based animation)
 *
 * Stores all rendering state for NPCs without needing the NPC class.
 * Animation is driven by velocity: Idle when stationary, Moving when velocity > threshold.
 * Indexed by typeLocalIndex (same as CharacterData for NPCs).
 */
struct NPCRenderData {
    uint16_t atlasX{0};                   // X offset in atlas (pixels)
    uint16_t atlasY{0};                   // Y offset in atlas (pixels)
    uint16_t frameWidth{32};              // Single frame width
    uint16_t frameHeight{32};             // Single frame height
    uint16_t idleSpeedMs{150};            // Milliseconds per frame for idle
    uint16_t moveSpeedMs{100};            // Milliseconds per frame for moving
    uint8_t currentFrame{0};              // Current animation frame index
    uint8_t numIdleFrames{1};             // Number of frames in idle animation (static)
    uint8_t numMoveFrames{2};             // Number of frames in move animation
    uint8_t idleRow{0};                   // Sprite sheet row for idle (0-based)
    uint8_t moveRow{0};                   // Sprite sheet row for moving (0-based, same as idle)
    uint8_t flipMode{0};                  // SDL_FLIP_NONE (0) or SDL_FLIP_HORIZONTAL (1)
    uint8_t currentRow{0};                // Active row (set by update from velocity)
    float animationAccumulator{0.0f};     // Time accumulator for frame advancement

    void clear() noexcept {
        atlasX = 0;
        atlasY = 0;
        frameWidth = 32;
        frameHeight = 32;
        idleSpeedMs = 150;
        moveSpeedMs = 100;
        currentFrame = 0;
        numIdleFrames = 1;
        numMoveFrames = 2;
        idleRow = 0;
        moveRow = 0;
        flipMode = 0;
        currentRow = 0;
        animationAccumulator = 0.0f;
    }
};

// ============================================================================
// CREATURE COMPOSITION SYSTEM (Race/Class, MonsterType/Variant, Species/Role)
// ============================================================================

/**
 * @brief Race definition for NPC composition
 *
 * Races define BASE stats and visual appearance. Combined with ClassInfo
 * at creation to produce final NPC stats (race.base * class.multiplier).
 */
struct RaceInfo {
    std::string name;

    // Base stats (before class modifiers)
    float baseHealth{100.0f};
    float baseStamina{100.0f};
    float baseMoveSpeed{100.0f};
    float baseAttackDamage{10.0f};
    float baseAttackRange{50.0f};

    // Visual (atlas region for this race's sprites)
    uint16_t atlasX{0};
    uint16_t atlasY{0};
    uint16_t atlasW{64};
    uint16_t atlasH{32};

    // Animations
    AnimationConfig idleAnim;
    AnimationConfig moveAnim;

    // Size (affects collision)
    float sizeMultiplier{1.0f};
};

/**
 * @brief Class definition for NPC composition
 *
 * Classes define stat MULTIPLIERS and behavior tendencies.
 * Applied to RaceInfo base stats at creation.
 */
struct ClassInfo {
    std::string name;

    // Stat multipliers (applied to race base)
    float healthMult{1.0f};
    float staminaMult{1.0f};
    float moveSpeedMult{1.0f};
    float attackDamageMult{1.0f};
    float attackRangeMult{1.0f};

    // Combat style ("melee" or "ranged") — determines attack mode
    std::string combatStyle{"melee"};
    float projectileSpeed{0.0f};         // Ranged projectile speed (px/s), 0 = melee

    // AI hints (not auto-applied, for reference)
    std::string suggestedBehavior;
    uint8_t basePriority{5};

    // Default faction (can be overridden at spawn)
    uint8_t defaultFaction{0};

    // Commerce flags
    bool isMerchant{false};  // If true, NPC can trade with player

    // Emotional resilience (0.0 = very emotional, 1.0 = stoic)
    // Affects how much emotions change when modified
    float emotionalResilience{0.5f};
    float personalityBraveryBias{0.5f};
    float personalityAggressionBias{0.5f};
    float personalityComposureBias{0.5f};
    float personalityLoyaltyBias{0.5f};

    // Starting inventory {resourceId, quantity}
    std::vector<std::pair<std::string, int>> startingItems;
};

/**
 * @brief Monster type definition for monster composition
 *
 * Monster types define BASE stats and visual appearance.
 * Combined with MonsterVariantInfo at creation.
 */
struct MonsterTypeInfo {
    std::string name;

    // Base stats
    float baseHealth{100.0f};
    float baseStamina{100.0f};
    float baseMoveSpeed{100.0f};
    float baseAttackDamage{10.0f};
    float baseAttackRange{50.0f};

    // Visual
    uint16_t atlasX{0};
    uint16_t atlasY{0};
    uint16_t atlasW{64};
    uint16_t atlasH{32};

    // Animations
    AnimationConfig idleAnim;
    AnimationConfig moveAnim;

    // Size
    float sizeMultiplier{1.0f};

    // Monsters are enemies by default
    uint8_t defaultFaction{1};
};

/**
 * @brief Monster variant definition for monster composition
 *
 * Variants define stat MULTIPLIERS for monster types.
 * E.g., "Scout" is fast/weak, "Boss" is strong/slow.
 */
struct MonsterVariantInfo {
    std::string name;

    // Stat multipliers
    float healthMult{1.0f};
    float staminaMult{1.0f};
    float moveSpeedMult{1.0f};
    float attackDamageMult{1.0f};
    float attackRangeMult{1.0f};

    // AI hints
    std::string suggestedBehavior;
    uint8_t basePriority{5};
};

/**
 * @brief Species definition for animal composition
 *
 * Species define BASE stats and visual appearance for animals.
 * Combined with AnimalRoleInfo at creation.
 */
struct SpeciesInfo {
    std::string name;

    // Base stats
    float baseHealth{50.0f};
    float baseStamina{100.0f};
    float baseMoveSpeed{80.0f};
    float baseAttackDamage{5.0f};
    float baseAttackRange{30.0f};

    // Visual
    uint16_t atlasX{0};
    uint16_t atlasY{0};
    uint16_t atlasW{64};
    uint16_t atlasH{32};

    // Animations
    AnimationConfig idleAnim;
    AnimationConfig moveAnim;

    // Size
    float sizeMultiplier{1.0f};

    // Behavior hint
    bool predator{false};
};

/**
 * @brief Animal role definition for animal composition
 *
 * Roles define stat MULTIPLIERS and behavior for animals.
 * E.g., "Pup" is weak, "Alpha" is strong/aggressive.
 */
struct AnimalRoleInfo {
    std::string name;

    // Stat multipliers
    float healthMult{1.0f};
    float staminaMult{1.0f};
    float moveSpeedMult{1.0f};
    float attackDamageMult{1.0f};

    // AI hints
    std::string suggestedBehavior;
    uint8_t basePriority{5};

    // Animals are neutral by default
    uint8_t defaultFaction{2};
};

// ============================================================================
// RESOURCE RENDER DATA STRUCTURES
// ============================================================================

/**
 * @brief Render data for dropped items (bobbing animation)
 *
 * Stores rendering state for DroppedItem entities.
 * Indexed by typeLocalIndex in EntityHotData.
 */
struct ItemRenderData {
    uint16_t atlasX{0};                   // X offset in atlas (pixels)
    uint16_t atlasY{0};                   // Y offset in atlas (pixels)
    uint16_t frameWidth{16};              // Single frame width
    uint16_t frameHeight{16};             // Single frame height
    uint16_t animSpeedMs{100};            // Milliseconds per frame
    uint8_t currentFrame{0};              // Current animation frame
    uint8_t numFrames{1};                 // Total animation frames
    float animTimer{0.0f};                // Animation accumulator
    float bobPhase{0.0f};                 // Sine-wave bob phase (0-2PI)
    float bobAmplitude{3.0f};             // Vertical bob amplitude in pixels

    void clear() noexcept {
        atlasX = 0;
        atlasY = 0;
        frameWidth = 16;
        frameHeight = 16;
        animSpeedMs = 100;
        currentFrame = 0;
        numFrames = 1;
        animTimer = 0.0f;
        bobPhase = 0.0f;
        bobAmplitude = 3.0f;
    }
};

/**
 * @brief Render data for containers (chests, barrels)
 *
 * Supports open/closed states with different textures.
 * Indexed by typeLocalIndex in EntityHotData.
 */
struct ContainerRenderData {
    uint16_t atlasX{0};                   // Atlas X offset (0 = unmapped, use default)
    uint16_t atlasY{0};                   // Atlas Y offset (0 = unmapped, use default)
    uint16_t openAtlasX{0};               // Atlas X offset for open state
    uint16_t openAtlasY{0};               // Atlas Y offset for open state
    uint16_t frameWidth{32};              // Sprite width
    uint16_t frameHeight{32};             // Sprite height
    uint16_t openFrameWidth{32};          // Open-state sprite width
    uint16_t openFrameHeight{32};         // Open-state sprite height
    uint8_t currentFrame{0};              // For animated open/close
    uint8_t numFrames{1};                 // Animation frames
    float animTimer{0.0f};                // Animation accumulator

    void clear() noexcept {
        atlasX = 0;
        atlasY = 0;
        openAtlasX = 0;
        openAtlasY = 0;
        frameWidth = 32;
        frameHeight = 32;
        openFrameWidth = 32;
        openFrameHeight = 32;
        currentFrame = 0;
        numFrames = 1;
        animTimer = 0.0f;
    }
};

/**
 * @brief Per-entity fixed-size waypoint storage slot (256 bytes, cache-aligned)
 *
 * Each entity owns one slot with space for MAX_WAYPOINTS_PER_ENTITY waypoints.
 * This eliminates contention from the old shared WaypointPool bump allocator.
 *
 * Benefits:
 * - Lock-free writes: Each entity writes to its own slot (no shared state)
 * - No fragmentation: Fixed memory per entity, overwrite in place
 * - Cache-friendly: 64-byte alignment, 4 cache lines per slot
 * - Simple: No allocation tracking, just overwrite the slot
 *
 * Threading: Safe for parallel writes when each thread writes to different entities.
 * pathRequestPending flag ensures single writer per entity at a time.
 */
struct alignas(64) FixedWaypointSlot {
    static constexpr size_t MAX_WAYPOINTS_PER_ENTITY = 32;
    Vector2D waypoints[MAX_WAYPOINTS_PER_ENTITY];

    [[nodiscard]] const Vector2D& operator[](size_t idx) const noexcept {
        assert(idx < MAX_WAYPOINTS_PER_ENTITY);
        return waypoints[idx];
    }

    Vector2D& operator[](size_t idx) noexcept {
        assert(idx < MAX_WAYPOINTS_PER_ENTITY);
        return waypoints[idx];
    }

    /** @brief Get read-only span of path waypoints */
    [[nodiscard]] std::span<const Vector2D> getPath(size_t length) const noexcept {
        return std::span<const Vector2D>(waypoints, std::min(length, MAX_WAYPOINTS_PER_ENTITY));
    }
};

static_assert(sizeof(FixedWaypointSlot) == 256, "FixedWaypointSlot must be 256 bytes (4 cache lines)");

// PathData and BehaviorData (indexed by edmIndex) are defined in ai/BehaviorCommonState.hpp
// (included above) — they're self-contained value types with no EDM-class dependency, so
// lightweight consumers (e.g. BehaviorExecutors.hpp) can use them without pulling in this
// whole header.

// ============================================================================
// NPC MEMORY SYSTEM
// ============================================================================

/**
 * @brief Memory types for NPC memory system
 *
 * NPCs can remember various events and interactions. Memory persists across
 * behavior changes (unlike BehaviorData) for the entity's session lifetime.
 */
enum class MemoryType : uint8_t {
    // Combat memories
    AttackedBy = 0,      // Who attacked this NPC
    Attacked = 1,        // Who this NPC attacked
    DamageDealt = 2,     // Damage dealt to a target
    DamageReceived = 3,  // Damage received from a source

    // Social memories
    Interaction = 4,     // Traded, talked, received item

    // Witnessed events
    WitnessedCombat = 5, // Saw combat between others
    WitnessedDeath = 6,  // Saw an entity die

    // Awareness memories
    ThreatSpotted = 7,   // Spotted a hostile entity
    AllySpotted = 8,     // Spotted a friendly entity
    LocationVisited = 9, // Visited a significant location

    COUNT = 10
};

/**
 * @brief Single memory entry - compact for inline storage (32 bytes)
 *
 * Stores who/what was involved, when it happened, and a numeric value.
 * The interpretation of 'value' depends on MemoryType:
 * - Damage memories: damage amount
 * - Interaction: interaction subtype (0=trade, 1=talk, 2=gift)
 * - Location: distance traveled to reach
 */
struct MemoryEntry {
    EntityHandle subject{};                 // Who/what is remembered
    Vector2D location{};                    // Where it happened
    float timestamp{0.0f};                  // Game time when it occurred
    float value{0.0f};                      // Context-dependent value (damage, etc.)
    MemoryType type{MemoryType::AttackedBy};// Type of memory
    uint8_t importance{0};                  // 0-255 importance score
    uint8_t flags{0};                       // Additional state (FLAG_VALID = live entry)
    uint8_t _pad{0};                        // Alignment padding

    static constexpr uint8_t FLAG_VALID = 0x01;

    [[nodiscard]] bool isValid() const noexcept { return flags & FLAG_VALID; }

    void clear() noexcept {
        subject = EntityHandle{};
        location = Vector2D{};
        timestamp = 0.0f;
        value = 0.0f;
        type = MemoryType::AttackedBy;
        importance = 0;
        flags = 0;
        _pad = 0;
    }
};

static_assert(sizeof(MemoryEntry) <= 40, "MemoryEntry exceeds 40 bytes");

/**
 * @brief NPC emotional state - affects behavior decisions (16 bytes)
 *
 * Emotions decay over time during AI processing.
 * Values are 0.0 to 1.0 representing intensity.
 */
struct EmotionalState {
    float aggression{0.0f};    // Combat readiness, attack likelihood
    float fear{0.0f};          // Flee threshold, caution level
    float curiosity{0.0f};     // Investigation tendency
    float suspicion{0.0f};     // Alertness to threats

    void clear() noexcept {
        aggression = 0.0f;
        fear = 0.0f;
        curiosity = 0.0f;
        suspicion = 0.0f;
    }

    /**
     * @brief Decay all emotions by the given rate
     * @param decayRate Rate per second (e.g., 0.1 = 10% decay per second)
     * @param deltaTime Frame time
     */
    void decay(float decayRate, float deltaTime) noexcept {
        float factor = 1.0f - (decayRate * deltaTime);
        factor = std::max(0.0f, factor);
        aggression *= factor;
        fear *= factor;
        curiosity *= factor;
        suspicion *= factor;
    }
};

static_assert(sizeof(EmotionalState) == 16, "EmotionalState should be 16 bytes");

/**
 * @brief NPC personality traits - affects emotional responses (16 bytes)
 *
 * Generated at spawn time with controlled randomness.
 * Combined with class resilience for final behavior modulation.
 * Foundation for future personality expansion (tendencies, quirks, etc.)
 */
struct PersonalityTraits {
    // Core traits (0.0 to 1.0, 0.5 = average)
    float bravery{0.5f};       // Resistance to fear (high = brave, low = cowardly)
    float aggression{0.5f};    // Combat eagerness (high = aggressive, low = passive)
    float composure{0.5f};     // Emotional stability (high = calm, low = reactive)
    float loyalty{0.5f};       // Faction commitment (affects flee vs fight for allies)

    /**
     * @brief Generate random personality with bell curve distribution
     * @param rng Thread-local random engine
     *
     * Uses normal distribution centered at 0.5 with std dev 0.15.
     * Most NPCs cluster around average, with outliers being rarer.
     */
    void randomize(std::mt19937& rng) {
        std::normal_distribution<float> dist(0.5f, 0.15f);  // Mean 0.5, most values 0.2-0.8
        bravery = std::clamp(dist(rng), 0.0f, 1.0f);
        aggression = std::clamp(dist(rng), 0.0f, 1.0f);
        composure = std::clamp(dist(rng), 0.0f, 1.0f);
        loyalty = std::clamp(dist(rng), 0.0f, 1.0f);

        // Guarantee a spawned NPC does not end up with an effectively all-neutral profile.
        constexpr float DEFAULT_TRAIT = 0.5f;
        constexpr float MIN_VARIATION = 0.01f;
        const bool allNeutral =
            std::abs(bravery - DEFAULT_TRAIT) < MIN_VARIATION &&
            std::abs(aggression - DEFAULT_TRAIT) < MIN_VARIATION &&
            std::abs(composure - DEFAULT_TRAIT) < MIN_VARIATION &&
            std::abs(loyalty - DEFAULT_TRAIT) < MIN_VARIATION;

        if (allNeutral) {
            std::uniform_int_distribution<int> directionDist(0, 1);
            const float offset = directionDist(rng) == 0 ? -0.05f : 0.05f;
            bravery = std::clamp(DEFAULT_TRAIT + offset, 0.0f, 1.0f);
        }
    }

    /**
     * @brief Effective resilience combines class + personality
     * @param classResilience Base resilience from class definition
     * @return Final resilience value (0.0-1.0) affecting emotion changes
     *
     * Bravery and composure both contribute to emotional resilience.
     * Class provides 60% of the factor, personality 40%.
     */
    [[nodiscard]] float getEffectiveResilience(float classResilience) const noexcept {
        float personalityFactor = (bravery + composure) * 0.5f;  // Average of two traits
        return classResilience * 0.6f + personalityFactor * 0.4f;
    }

    void clear() noexcept {
        bravery = 0.5f;
        aggression = 0.5f;
        composure = 0.5f;
        loyalty = 0.5f;
    }
};

static_assert(sizeof(PersonalityTraits) == 16, "PersonalityTraits should be 16 bytes");

// EXPANSION RULES (locked in by DOD discipline):
//   Every NPC, every frame  → add to the first cache line. 7 B padding available;
//                             beyond that, extend the hot block to a second line.
//   Some NPCs, sometimes    → new SparseSidecar<T> member of EDM. See m_knockback
//                             and m_memoryOverflow as the references.
//   Cross-session persistent → field stays here; SaveGameManager iterates m_memoryData
//                              when persistence lands. Personality is the first such field.

/**
 * @brief NPC memory data with inline storage + overflow (384 bytes, 6 cache lines)
 *
 * Fields are ordered by per-frame access cadence: the first cache line (64 B) holds
 * only the fields that every behavior reads every frame. The remaining cache lines
 * hold fields accessed only by specific behaviors or on specific events.
 *
 * Indexed by edmIndex (parallel to PathData, BehaviorData).
 * Persists across behavior changes - unlike BehaviorData.
 *
 * Overflow is stored in EDM's m_memoryOverflow SparseSidecar<MemoryOverflow>,
 * keyed by edmIdx — no ID field on this struct needed.
 */
struct alignas(64) NPCMemoryData {
    static constexpr size_t INLINE_MEMORY_COUNT = 6;
    static constexpr size_t INLINE_LOCATION_COUNT = 4;

    // Flags
    static constexpr uint8_t FLAG_VALID = 0x01;
    static constexpr uint8_t FLAG_HAS_OVERFLOW = 0x02;
    // Sentinel value — "no combat has ever occurred for this NPC".
    // Not a threshold: data convention only. The "recently in combat" policy
    // lives in Behaviors::COMBAT_TIMEOUT_SECONDS (BehaviorExecutors.hpp).
    static constexpr float NO_COMBAT_HISTORY = 999.0f;

    // First 64 B — read every frame by every behavior.
    EmotionalState    emotions;                         // 16 B  (read+write per frame in decay loop)
    PersonalityTraits personality;                      // 16 B  (read every frame, written once at spawn)
    EntityHandle      lastAttacker;                     // 16 B  (read by 5 behaviors)
    float             lastCombatTime{NO_COMBAT_HISTORY};// 4 B   (updated per frame in decay)
    float             lastDecayTime{0.0f};              // 4 B   (updated per frame)
    uint8_t           flags{0};                         // 1 B   (FLAG_VALID, FLAG_HAS_OVERFLOW)
    uint8_t           _pad1[7]{};                       // 7 B   → first 64 B exact

    // Next 64 B — read every frame by combat-tracking behaviors (Chase/Attack/Follow/Guard).
    // Kept adjacent so combat behaviors fault one extra cache line, not multiple.
    EntityHandle lastTarget;                            // 16 B  (read+write by 4 behaviors)
    uint8_t      _pad2[48]{};                           // 48 B  → next 64 B exact (room for future combat fields)

    // Remaining bytes — read on event or only by Guard's memory iteration.
    MemoryEntry  memories[INLINE_MEMORY_COUNT];         // 240 B (Guard iterates; findMemories on demand)
    Vector2D     locationHistory[INLINE_LOCATION_COUNT];// 32 B  (only addLocationToHistory writes)
    float        totalDamageReceived{0.0f};             // 4 B   (written on combat event)
    float        totalDamageDealt{0.0f};                // 4 B   (written on combat event)
    uint16_t     memoryCount{0};                        // 2 B   (total memories — inline + overflow)
    uint16_t     locationCount{0};                      // 2 B   (locations stored — 0..4)
    uint8_t      nextInlineSlot{0};                     // 1 B   (circular write position)
    uint8_t      combatEncounters{0};                   // 1 B   (combat encounter counter)
    uint8_t      _pad3[34]{};                           // 34 B  → struct totals 448 B (multiple of 64)

    [[nodiscard]] bool isValid() const noexcept { return flags & FLAG_VALID; }
    [[nodiscard]] bool hasOverflow() const noexcept { return flags & FLAG_HAS_OVERFLOW; }

    void setValid(bool v) noexcept {
        if (v) flags |= FLAG_VALID;
        else flags &= ~FLAG_VALID;
    }

    void clear() noexcept {
        emotions.clear();
        personality.clear();
        lastAttacker = EntityHandle{};
        lastCombatTime = NO_COMBAT_HISTORY;
        lastDecayTime = 0.0f;
        flags = 0;
        for (auto& m : memories) m.clear();
        std::fill(std::begin(locationHistory), std::end(locationHistory), Vector2D{});
        lastTarget = EntityHandle{};
        totalDamageReceived = 0.0f;
        totalDamageDealt = 0.0f;
        memoryCount = 0;
        locationCount = 0;
        nextInlineSlot = 0;
        combatEncounters = 0;
    }
};

static_assert(offsetof(NPCMemoryData, emotions) == 0,
              "First cache line must start at byte 0");
static_assert(offsetof(NPCMemoryData, lastTarget) == 64,
              "lastTarget must start at byte 64 — kept adjacent to first 64 B for combat behaviors");
static_assert(offsetof(NPCMemoryData, memories) == 128,
              "Event-only fields start at byte 128");
static_assert(sizeof(NPCMemoryData) == 448,
              "Struct size locked at 448 B — change deliberately if you adjust the layout");

/**
 * @brief Overflow storage for NPCs with extensive memory history
 *
 * Used when inline slots are full and full history is desired.
 * Capped at MAX_OVERFLOW_MEMORIES to prevent unbounded growth.
 */
struct MemoryOverflow {
    static constexpr size_t MAX_OVERFLOW_MEMORIES = 50;

    std::vector<MemoryEntry> extraMemories;

    void clear() noexcept { extraMemories.clear(); }

    void trimToMax() {
        if (extraMemories.size() > MAX_OVERFLOW_MEMORIES) {
            // Keep most important and most recent
            std::sort(extraMemories.begin(), extraMemories.end(),
                [](const MemoryEntry& a, const MemoryEntry& b) {
                    // Primary: importance, Secondary: timestamp (recent first)
                    if (a.importance != b.importance) return a.importance > b.importance;
                    return a.timestamp > b.timestamp;
                });
            extraMemories.resize(MAX_OVERFLOW_MEMORIES);
        }
    }
};

#endif // ENTITY_DATA_TYPES_HPP
