/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef ENTITY_DATA_MANAGER_HPP
#define ENTITY_DATA_MANAGER_HPP

/**
 * @file EntityDataManager.hpp
 * @brief Central data authority for all entity data (Data-Oriented Design)
 *
 * EntityDataManager is a pure DATA STORE, not a processor. It owns:
 * - All entity transform data (position, velocity, acceleration)
 * - Type-specific data blocks (CharacterData, ItemData, ProjectileData, etc.)
 * - Simulation tier tracking (Active/Background/Hibernated)
 *
 * Processing systems read from and write to EntityDataManager:
 * - AIManager processes Active tier behaviors
 * - CollisionManager processes collision detection
 * - BackgroundSimulator processes Background tier entities
 * - Type-specific systems (ProjectileSystem, etc.)
 *
 * Benefits:
 * - Single source of truth (eliminates 4x position duplication)
 * - Cache-optimal SoA storage (~5MB contiguous vs ~30MB scattered)
 * - Supports 100K+ entities with tiered simulation
 *
 * THREADING CONTRACT:
 * - All structural operations (create/destroy/register/getIndex) MUST be called
 *   from the main thread only. These operations are NOT thread-safe.
 * - Index-based accessors (getHotDataByIndex, getTransformByIndex) are lock-free
 *   and safe for parallel batch processing with non-overlapping index ranges.
 * - Parallel batch processing uses pre-cached indices to avoid map lookups.
 * - GameEngine::update() sequential order guarantees no concurrent structural changes:
 *   EventManager → GameStateManager → AIManager → CollisionManager → BackgroundSimManager
 */

#include "ai/BehaviorCommonState.hpp"
#include "ai/BehaviorConfig.hpp"
#include "ai/BehaviorStateData.hpp"
#include "collisions/CollisionBody.hpp"
#include "collisions/TriggerTag.hpp"
#include "entities/Entity.hpp"
#include "entities/EntityHandle.hpp"
#include "managers/SparseSidecar.hpp"
#include "utils/ResourceHandle.hpp"
#include "utils/Vector2D.hpp"
#include "world/HarvestType.hpp"
#include <atomic>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <limits>
#include <mutex>
#include <random>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations - Entity and AnimationConfig now included via Entity.hpp

// Free-standing SoA component data types (TransformData, EntityHotData,
// CharacterData, CreatureCategory, ItemData, ProjectileData, ContainerData,
// HarvestableData, InventoryData, AreaEffectData, render-data structs,
// FixedWaypointSlot, NPC memory types, etc.) live in managers/EntityDataTypes.hpp
// — they have no dependency on this class, so lightweight consumers can use
// them without pulling in the full manager.
#include "managers/EntityDataTypes.hpp"

// ============================================================================
// ENTITY DATA MANAGER
// ============================================================================

/**
 * @brief Central data authority for all entity data
 *
 * This is a DATA STORE, not a processor. Systems read from and write to this
 * manager. It does not have an update() method - processing happens in
 * AIManager, CollisionManager, and type-specific systems.
 */
class EntityDataManager {
public:
    static EntityDataManager& Instance() {
        static EntityDataManager instance;
        return instance;
    }

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the entity data manager
     * @return true if initialization successful
     */
    [[nodiscard]] bool init();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept {
        return m_initialized.load(std::memory_order_acquire);
    }

    /**
     * @brief Clean up all entity data
     */
    void clean();

    /**
     * @brief Prepare for game state transition (clears all entities)
     */
    void prepareForStateTransition();

    // ========================================================================
    // ENTITY CREATION (Creature Composition System)
    // ========================================================================

    /**
     * @brief Create an NPC with race and class composition
     * @param position World position
     * @param race Race name (e.g., "Human", "Elf", "Orc")
     * @param charClass Class name (e.g., "Warrior", "Guard", "Merchant")
     * @param sex Character sex (defaults to Sex::Unknown for random assignment)
     * @param factionOverride Optional faction override (0xFF = use class default)
     * @return Handle to the created entity, or invalid handle if race/class not found
     *
     * Final stats computed as: raceBase * classMultiplier
     * Example: Human (100 HP) + Warrior (1.3x) = 130 HP
     */
    EntityHandle createNPCWithRaceClass(const Vector2D& position,
                                        const std::string& race,
                                        const std::string& charClass,
                                        Sex sex = Sex::Unknown,
                                        uint8_t factionOverride = 0xFF);

    /**
     * @brief Get all registered race IDs
     * @return Vector of race ID strings
     */
    [[nodiscard]] std::vector<std::string> getRaceIds() const;

    /**
     * @brief Get all registered class IDs
     * @return Vector of class ID strings
     */
    [[nodiscard]] std::vector<std::string> getClassIds() const;

    /**
     * @brief Create a monster with type and variant composition
     * @param position World position
     * @param monsterType Monster type name (e.g., "Goblin", "Skeleton", "Dragon")
     * @param variant Variant name (e.g., "Scout", "Brute", "Boss")
     * @param sex Optional sex (default Unknown)
     * @param factionOverride Optional faction override (0xFF = use type default, usually Enemy)
     * @return Handle to the created entity, or invalid handle if type/variant not found
     */
    EntityHandle createMonster(const Vector2D& position,
                               const std::string& monsterType,
                               const std::string& variant,
                               Sex sex = Sex::Unknown,
                               uint8_t factionOverride = 0xFF);

    /**
     * @brief Create an animal with species and role composition
     * @param position World position
     * @param species Species name (e.g., "Wolf", "Bear", "Deer")
     * @param role Role name (e.g., "Pup", "Adult", "Alpha")
     * @param sex Optional sex (default Unknown)
     * @param factionOverride Optional faction override (0xFF = use role default, usually Neutral)
     * @return Handle to the created entity, or invalid handle if species/role not found
     */
    EntityHandle createAnimal(const Vector2D& position,
                              const std::string& species,
                              const std::string& role,
                              Sex sex = Sex::Unknown,
                              uint8_t factionOverride = 0xFF);

    /**
     * @brief Get race info from registry
     * @param race Race name
     * @return Pointer to RaceInfo, or nullptr if not found
     */
    [[nodiscard]] const RaceInfo* getRaceInfo(const std::string& race) const;

    /**
     * @brief Get class info from registry
     * @param charClass Class name
     * @return Pointer to ClassInfo, or nullptr if not found
     */
    [[nodiscard]] const ClassInfo* getClassInfo(const std::string& charClass) const;

    /**
     * @brief Get monster type info from registry
     * @param monsterType Monster type name
     * @return Pointer to MonsterTypeInfo, or nullptr if not found
     */
    [[nodiscard]] const MonsterTypeInfo* getMonsterTypeInfo(const std::string& monsterType) const;

    /**
     * @brief Get monster variant info from registry
     * @param variant Variant name
     * @return Pointer to MonsterVariantInfo, or nullptr if not found
     */
    [[nodiscard]] const MonsterVariantInfo* getMonsterVariantInfo(const std::string& variant) const;

    /**
     * @brief Get species info from registry
     * @param species Species name
     * @return Pointer to SpeciesInfo, or nullptr if not found
     */
    [[nodiscard]] const SpeciesInfo* getSpeciesInfo(const std::string& species) const;

    /**
     * @brief Get animal role info from registry
     * @param role Role name
     * @return Pointer to AnimalRoleInfo, or nullptr if not found
     */
    [[nodiscard]] const AnimalRoleInfo* getAnimalRoleInfo(const std::string& role) const;

    /**
     * @brief Build a display label for a creature from its composition ids
     * @param handle Creature entity handle
     * @return Human-readable name such as "Human Warrior"
     */
    [[nodiscard]] std::string getCreatureDisplayName(EntityHandle handle) const;

    /**
     * @brief Return the display name for a race id (NPC typeId)
     * @param id Race id as stored in CharacterData::typeId
     * @return Race name, or empty string if id is out of range
     */
    [[nodiscard]] std::string_view getRaceName(uint32_t id) const;

    /**
     * @brief Return the display name for a class id (NPC subtypeId)
     * @param id Class id as stored in CharacterData::subtypeId
     * @return Class name, or empty string if id is out of range
     */
    [[nodiscard]] std::string_view getClassName(uint32_t id) const;

    /**
     * @brief Create a dropped item entity
     * @param position World position
     * @param resourceHandle Item template reference
     * @param quantity Stack size
     * @param worldId World to register with (empty = use active world from WRM)
     * @return Handle to the created entity
     *
     * Note: Auto-registers with WorldResourceManager for spatial queries.
     *       Dropped items use WRM spatial index, not collision system.
     */
    EntityHandle createDroppedItem(const Vector2D& position,
                                   VoidLight::ResourceHandle resourceHandle,
                                   int quantity = 1,
                                   const std::string& worldId = "");

    /**
     * @brief Create a container entity with auto-inventory
     * @param position World position
     * @param containerType Type of container (Chest, Barrel, Corpse, Crate)
     * @param maxSlots Maximum inventory slots (default 20)
     * @param lockLevel Lock difficulty (0 = unlocked, 1-10 = requires skill)
     * @param worldId World to register with (empty = use active world from WRM)
     * @return Handle to the created entity
     *
     * Validation:
     * - containerType must be valid
     * - maxSlots must be > 0 and <= 100
     * - lockLevel clamped to 0-10
     *
     * Auto-creates an inventory for the container via createInventory().
     * Auto-registers inventory with WorldResourceManager.
     */
    EntityHandle createContainer(const Vector2D& position,
                                 ContainerType containerType,
                                 uint16_t maxSlots = 20,
                                 uint8_t lockLevel = 0,
                                 const std::string& worldId = "");

    /**
     * @brief Create a harvestable resource node
     * @param position World position
     * @param yieldResource Resource to yield when harvested
     * @param yieldMin Minimum yield amount
     * @param yieldMax Maximum yield amount
     * @param respawnTime Seconds until respawn after depletion
     * @param worldId World to register with (empty = use active world from WRM)
     * @param harvestType Type of harvesting required (defaults to Gathering)
     * @return Handle to the created entity
     *
     * Validation:
     * - yieldResource must be valid
     * - yieldMin/yieldMax must be positive and yieldMax >= yieldMin
     * - respawnTime clamped to >= 0
     *
     * Auto-registers with WorldResourceManager for both registry and spatial queries.
     */
    EntityHandle createHarvestable(const Vector2D& position,
                                   VoidLight::ResourceHandle yieldResource,
                                   int yieldMin = 1,
                                   int yieldMax = 3,
                                   float respawnTime = 60.0f,
                                   const std::string& worldId = "",
                                   VoidLight::HarvestType harvestType = VoidLight::HarvestType::Gathering);

    // ========================================================================
    // PHASE 1: REGISTRATION OF EXISTING ENTITIES (Parallel Storage)
    // These methods register entities that were created via the old pattern
    // (Entity subclass constructors). They mirror data into EntityDataManager
    // until Phase 4 when Entity becomes a lightweight view.
    // ========================================================================

    /**
     * @brief Register an existing Player entity with EntityDataManager
     * @param entityId Existing entity ID from Entity::getID()
     * @param position Current position
     * @param halfWidth Collision half-width
     * @param halfHeight Collision half-height
     * @return Handle to the registered entity
     */
    EntityHandle registerPlayer(EntityHandle::IDType entityId,
                                const Vector2D& position,
                                float halfWidth = 32.0f,
                                float halfHeight = 32.0f);

    /**
     * @brief Register an existing DroppedItem entity with EntityDataManager
     * @param entityId Existing entity ID from Entity::getID()
     * @param position Current position
     * @param resourceHandle Item template reference
     * @param quantity Stack size
     * @return Handle to the registered entity
     */
    EntityHandle registerDroppedItem(EntityHandle::IDType entityId,
                                     const Vector2D& position,
                                     VoidLight::ResourceHandle resourceHandle,
                                     int quantity = 1);

    /**
     * @brief Unregister an entity (called when Entity is destroyed)
     * @param entityId Entity ID to unregister
     */
    void unregisterEntity(EntityHandle::IDType entityId);

    /**
     * @brief Create a projectile entity
     * @param position Initial position
     * @param velocity Initial velocity
     * @param owner Entity that fired this projectile
     * @param damage Damage on hit
     * @param lifetime Time until despawn
     * @return Handle to the created entity
     */
    EntityHandle createProjectile(const Vector2D& position,
                                  const Vector2D& velocity,
                                  EntityHandle owner,
                                  float damage,
                                  float lifetime = 5.0f);

    /**
     * @brief Create an area effect entity
     * @param position Center position
     * @param radius Effect radius
     * @param owner Entity that created this effect
     * @param damage Damage per tick
     * @param duration Total duration
     * @return Handle to the created entity
     */
    EntityHandle createAreaEffect(const Vector2D& position,
                                  float radius,
                                  EntityHandle owner,
                                  float damage,
                                  float duration);

    /**
     * @brief Create a static obstacle entity (world geometry)
     * @param position Center position
     * @param halfWidth Collision half-width
     * @param halfHeight Collision half-height
     * @return Handle to the created entity
     *
     * Static obstacles are used for world tiles, walls, and terrain collision.
     * They don't move, have no AI, and use Hibernated tier for minimal overhead.
     */
    EntityHandle createStaticBody(const Vector2D& position,
                                  float halfWidth,
                                  float halfHeight);

    /**
     * @brief Create a trigger entity for detecting entity overlap
     * @param position Center position
     * @param halfWidth Collision half-width
     * @param halfHeight Collision half-height
     * @param tag Semantic tag (Water, BossArea, etc.)
     * @param type EventOnly (skip broadphase) or Physical (full collision)
     * @return Handle to the created trigger entity
     *
     * Triggers are stored in static storage (don't move).
     * EventOnly triggers skip physics broadphase, only detect player overlap.
     * Physical triggers participate in full broadphase + resolution.
     */
    EntityHandle createTrigger(const Vector2D& position,
                               float halfWidth,
                               float halfHeight,
                               VoidLight::TriggerTag tag,
                               VoidLight::TriggerType type);

    /**
     * @brief Mark an entity for destruction (processed at end of frame)
     * @param handle Entity to destroy
     */
    void destroyEntity(EntityHandle handle);

    /**
     * @brief Process pending destructions (call at end of frame)
     */
    void processDestructionQueue();

    // ========================================================================
    // INVENTORY MANAGEMENT
    // ========================================================================

    /**
     * @brief Create a new inventory
     * @param maxSlots Maximum number of slots (uses overflow for > INLINE_SLOT_COUNT)
     * @param worldTracked Metadata flag for inventories that participate in world resource queries.
     * Callers with a world context must register/unregister explicitly with WorldResourceManager.
     * @return Inventory index, or INVALID_INVENTORY_INDEX on failure
     *
     * Validation:
     * - maxSlots must be > 0 and <= 1000
     * - Returns INVALID_INVENTORY_INDEX if allocation fails
     */
    uint32_t createInventory(uint16_t maxSlots, bool worldTracked = false);

    /**
     * @brief Initialize an NPC as a merchant with an inventory
     * @param handle NPC entity handle
     * @param maxSlots Maximum inventory slots (default 20)
     * @return true if successfully initialized, false on failure
     *
     * Ensures the NPC has an inventory and sets the FLAG_MERCHANT flag.
     * Use this to enable trading with the NPC via SocialController.
     */
    [[nodiscard]] bool initNPCAsMerchant(EntityHandle handle, uint16_t maxSlots = 20);

    /**
     * @brief Check if an NPC is a merchant
     * @param handle NPC entity handle
     * @return true if NPC has merchant capability
     */
    [[nodiscard]] bool isNPCMerchant(EntityHandle handle) const;

    /**
     * @brief Get an NPC's inventory index
     * @param handle NPC entity handle
     * @return Inventory index, or INVALID_INVENTORY_INDEX if the NPC has no inventory
     */
    [[nodiscard]] uint32_t getNPCInventoryIndex(EntityHandle handle) const;

    /**
     * @brief Equip one inventory item onto a character.
     *
     * Removes one item from the character inventory, stores it in the matching
     * equipment slot, returns any replaced item to the same inventory, and
     * refreshes cached effective combat stats.
     */
    bool equipCharacterItem(EntityHandle handle, VoidLight::ResourceHandle itemHandle);

    /**
     * @brief Unequip the item in a character slot and return it to inventory.
     */
    bool unequipCharacterItem(EntityHandle handle, const std::string& slotName);

    /**
     * @brief Get the currently equipped item for a character slot.
     */
    [[nodiscard]] VoidLight::ResourceHandle
    getEquippedCharacterItem(EntityHandle handle, const std::string& slotName) const;

    /**
     * @brief Consume one compatible ammunition item for a ranged equipped weapon.
     *
     * Ranged weapons with no ammo requirement return true. Weapons that require
     * ammo scan the owning inventory's physical slots in order and remove one
     * matching ammunition item.
     */
    [[nodiscard]] bool
    consumeRequiredAmmoForRangedAttack(EntityHandle handle,
                                       InventoryResourceChange* outChange);

    [[nodiscard]] float getEffectiveAttackDamage(EntityHandle handle) const;
    [[nodiscard]] float getEffectiveDefense(EntityHandle handle) const;
    [[nodiscard]] float getEffectiveMoveSpeed(EntityHandle handle) const;

    /**
     * @brief Destroy an inventory and release its resources
     * @param inventoryIndex Index from createInventory()
     *
     * Clears overflow data if present and adds slot to free-list.
     * WorldResourceManager registrations are owned by the caller that registered the inventory.
     */
    void destroyInventory(uint32_t inventoryIndex);

    /**
     * @brief Add resources to an inventory (with stacking)
     * @param inventoryIndex Target inventory
     * @param handle Resource type handle
     * @param quantity Amount to add (must be positive)
     * @return true if added successfully, false on validation failure or full
     *
     * Validation:
     * - inventoryIndex must be valid
     * - handle must be valid and registered with ResourceTemplateManager
     * - quantity must be positive
     *
     * Stacking: Tries to stack with existing slots of same type first,
     * then fills empty slots. Respects maxStackSize from ResourceTemplateManager.
     */
    bool addToInventory(uint32_t inventoryIndex,
                        VoidLight::ResourceHandle handle,
                        int quantity);

    /**
     * @brief Remove resources from an inventory
     * @param inventoryIndex Target inventory
     * @param handle Resource type handle
     * @param quantity Amount to remove (must be positive)
     * @return true if removed successfully, false if insufficient quantity
     *
     * Removes from slots in order until quantity is satisfied.
     * Clears empty slots for reuse.
     */
    bool removeFromInventory(uint32_t inventoryIndex,
                             VoidLight::ResourceHandle handle,
                             int quantity);

    /**
     * @brief Transfer resources between two inventories without partial mutation
     * @param sourceInventoryIndex Inventory to remove from
     * @param targetInventoryIndex Inventory to add to
     * @param handle Resource type handle
     * @param quantity Amount to transfer (must be positive)
     * @return source/target quantity deltas when the full quantity transferred,
     * std::nullopt if validation, capacity, or source quantity checks fail
     *
     * Capacity and source availability are checked before any slots mutate.
     */
    [[nodiscard]] std::optional<InventoryTransferResult>
    transferInventoryItem(uint32_t sourceInventoryIndex,
                          uint32_t targetInventoryIndex,
                          VoidLight::ResourceHandle handle,
                          int quantity);

    /**
     * @brief Get total quantity of a resource in an inventory
     * @param inventoryIndex Target inventory
     * @param handle Resource type handle
     * @return Total quantity across all slots, or 0 if not found/invalid
     */
    [[nodiscard]] int getInventoryQuantity(uint32_t inventoryIndex,
                                           VoidLight::ResourceHandle handle) const;

    /**
     * @brief Check if an inventory contains at least the specified quantity
     * @param inventoryIndex Target inventory
     * @param handle Resource type handle
     * @param quantity Required amount
     * @return true if inventory contains >= quantity
     */
    [[nodiscard]] bool hasInInventory(uint32_t inventoryIndex,
                                      VoidLight::ResourceHandle handle,
                                      int quantity) const;

    /**
     * @brief Get all resources in an inventory as a map
     * @param inventoryIndex Target inventory
     * @return Map of resource handle to total quantity
     *
     * Iterates through all slots (inline and overflow) and sums quantities
     * by resource type. Returns empty map for invalid inventory.
     */
    [[nodiscard]] std::unordered_map<VoidLight::ResourceHandle, int>
    getInventoryResources(uint32_t inventoryIndex) const;

    /**
     * @brief Get one ordered physical inventory slot
     * @param inventoryIndex Target inventory
     * @param slotIndex Physical slot index in the inventory
     * @return Slot contents by value, or an empty slot for invalid input/state
     */
    [[nodiscard]] InventorySlotData getInventorySlot(uint32_t inventoryIndex,
                                                     size_t slotIndex) const;

    /**
     * @brief Copy ordered physical inventory slots into caller-owned storage
     * @param inventoryIndex Target inventory
     * @param outSlots Destination span for slots starting at physical slot 0
     * @return Number of slots copied, or 0 for invalid input/state
     */
    [[nodiscard]] size_t getInventorySlots(uint32_t inventoryIndex,
                                           std::span<InventorySlotData> outSlots) const;

    /**
     * @brief Swap two ordered physical inventory slots
     * @param inventoryIndex Target inventory
     * @param sourceSlot Physical source slot index
     * @param targetSlot Physical target slot index
     * @return true when both slots are valid for this inventory, false otherwise
     *
     * Swaps storage only. Empty target slots naturally receive the source stack,
     * and usedSlots remains unchanged because occupancy count does not change.
     */
    bool swapInventorySlots(uint32_t inventoryIndex,
                            size_t sourceSlot,
                            size_t targetSlot);

    /**
     * @brief Get inventory data by index
     * @param inventoryIndex Target inventory
     * @return Reference to inventory data
     */
    [[nodiscard]] InventoryData& getInventoryData(uint32_t inventoryIndex);
    [[nodiscard]] const InventoryData& getInventoryData(uint32_t inventoryIndex) const;

    /**
     * @brief Get overflow data for large inventories
     * @param overflowId ID from InventoryData::overflowId
     * @return Pointer to overflow data, or nullptr if not found
     */
    [[nodiscard]] InventoryOverflow* getInventoryOverflow(uint32_t overflowId);
    [[nodiscard]] const InventoryOverflow* getInventoryOverflow(uint32_t overflowId) const;

    /**
     * @brief Check if inventory index is valid
     */
    [[nodiscard]] bool isValidInventoryIndex(uint32_t inventoryIndex) const noexcept {
        return inventoryIndex != INVALID_INVENTORY_INDEX &&
               inventoryIndex < m_inventoryData.size() &&
               m_inventoryData[inventoryIndex].isValid();
    }

    // ========================================================================
    // RESOURCE RENDER DATA ACCESS
    // ========================================================================

    /**
     * @brief Get item render data by type index
     * @param typeLocalIndex Index from EntityHotData::typeLocalIndex
     * @return Reference to item render data
     */
    [[nodiscard]] ItemRenderData& getItemRenderDataByTypeIndex(uint32_t typeLocalIndex);
    [[nodiscard]] const ItemRenderData& getItemRenderDataByTypeIndex(uint32_t typeLocalIndex) const;

    /**
     * @brief Get container render data by type index
     * @param typeLocalIndex Index from EntityHotData::typeLocalIndex
     * @return Reference to container render data
     */
    [[nodiscard]] ContainerRenderData& getContainerRenderDataByTypeIndex(uint32_t typeLocalIndex);
    [[nodiscard]] const ContainerRenderData& getContainerRenderDataByTypeIndex(uint32_t typeLocalIndex) const;

    // ========================================================================
    // HANDLE VALIDATION
    // ========================================================================

    /**
     * @brief Check if an entity handle is valid and refers to a live entity
     * @param handle Handle to validate
     * @return true if handle is valid and entity exists
     */
    [[nodiscard]] bool isValidHandle(EntityHandle handle) const;

    /**
     * @brief Get the storage index for a handle (internal use)
     * @param handle Entity handle
     * @return Storage index, or SIZE_MAX if invalid
     */
    [[nodiscard]] size_t getIndex(EntityHandle handle) const;

    /**
     * @brief Look up storage index by EntityID
     * @param entityId Entity's unique ID
     * @return Storage index, or SIZE_MAX if not registered
     */
    [[nodiscard]] size_t findIndexByEntityId(EntityHandle::IDType entityId) const {
        auto it = m_idToIndex.find(entityId);
        return (it != m_idToIndex.end()) ? it->second : SIZE_MAX;
    }

    // ========================================================================
    // TRANSFORM ACCESS (Single Source of Truth)
    // ========================================================================

    /**
     * @brief Get mutable transform data for an entity
     * @param handle Entity handle
     * @return Reference to transform data
     */
    [[nodiscard]] TransformData& getTransform(EntityHandle handle);

    /**
     * @brief Get const transform data for an entity
     * @param handle Entity handle
     * @return Const reference to transform data
     */
    [[nodiscard]] const TransformData& getTransform(EntityHandle handle) const;

    /**
     * @brief Get transform by storage index (for batch processing)
     * @param index Storage index
     * @return Reference to transform data
     */
    [[nodiscard]] TransformData& getTransformByIndex(size_t index);
    [[nodiscard]] const TransformData& getTransformByIndex(size_t index) const;

    /**
     * @brief Get static transform by index (for collision system)
     */
    [[nodiscard]] const TransformData& getStaticTransformByIndex(size_t index) const;

    // ========================================================================
    // HOT DATA ACCESS
    // ========================================================================

    /**
     * @brief Get hot data by handle
     */
    [[nodiscard]] EntityHotData& getHotData(EntityHandle handle);
    [[nodiscard]] const EntityHotData& getHotData(EntityHandle handle) const;

    /**
     * @brief Get hot data by index (for batch processing)
     */
    [[nodiscard]] EntityHotData& getHotDataByIndex(size_t index);
    [[nodiscard]] const EntityHotData& getHotDataByIndex(size_t index) const;

    /**
     * @brief Get read-only span of all hot data (for batch iteration)
     */
    [[nodiscard]] std::span<const EntityHotData> getHotDataArray() const;

    // ========================================================================
    // KNOCKBACK SIDECAR ACCESS (SparseSidecar<KnockbackData>)
    // ========================================================================

    /**
     * @brief Apply (or retrieve existing) knockback state for an entity.
     *        Returns a mutable ref so the caller can fill impulseX/Y/framesRemaining.
     *        Main-thread only.
     */
    [[nodiscard]] KnockbackData& applyKnockback(size_t edmIdx);

    /**
     * @brief Get mutable knockback state, nullptr if entity has no active knockback.
     */
    [[nodiscard]] KnockbackData* getKnockback(size_t edmIdx) noexcept;

    /**
     * @brief Get const knockback state, nullptr if entity has no active knockback.
     */
    [[nodiscard]] const KnockbackData* getKnockback(size_t edmIdx) const noexcept;

    /**
     * @brief True if the entity currently has active knockback.
     */
    [[nodiscard]] bool hasKnockback(size_t edmIdx) const noexcept;

    /**
     * @brief Clear knockback state for an entity (called when framesRemaining reaches 0
     *        or entity is destroyed).
     */
    void clearKnockback(size_t edmIdx) noexcept;

    /**
     * @brief Count of entities currently under knockback (for tests and diagnostics).
     */
    [[nodiscard]] size_t knockbackActiveCount() const noexcept;

    /**
     * @brief Direct access to the knockback sidecar (for BehaviorContext pre-fetch).
     *        Worker threads hold a pointer to this and access via SparseSidecar::get().
     */
    [[nodiscard]] SparseSidecar<KnockbackData>& knockbackSidecar() noexcept;
    [[nodiscard]] const SparseSidecar<KnockbackData>& knockbackSidecar() const noexcept;

    /**
     * @brief Get read-only span of static hot data (for collision system)
     */
    [[nodiscard]] std::span<const EntityHotData> getStaticHotDataArray() const;

    /**
     * @brief Get static hot data by index
     */
    [[nodiscard]] const EntityHotData& getStaticHotDataByIndex(size_t index) const;

    /**
     * @brief Get static entity index from ID
     */
    [[nodiscard]] size_t getStaticIndex(EntityHandle handle) const;

    /**
     * @brief Get handle for static pool entity by index
     * @param staticIndex Index in m_staticHotData
     * @return EntityHandle with correct kind and generation
     * @note Used for resources (DroppedItem, Container, Harvestable) which are in static pool
     */
    [[nodiscard]] EntityHandle getStaticHandle(size_t staticIndex) const;

    // ========================================================================
    // TYPE-SPECIFIC DATA ACCESS
    // ========================================================================

    [[nodiscard]] CharacterData& getCharacterData(EntityHandle handle);
    [[nodiscard]] const CharacterData& getCharacterData(EntityHandle handle) const;

    void setCharacterBaseStats(EntityHandle handle,
                               float maxHealth,
                               float maxStamina,
                               float attackDamage,
                               float attackRange,
                               float moveSpeed);
    void setCharacterInventoryIndex(EntityHandle handle, uint32_t inventoryIndex);

    /**
     * @brief Set the faction of a character and update collision layers
     * @param handle Entity handle
     * @param newFaction New faction value (0=Friendly, 1=Enemy, 2=Neutral)
     */
    void setFaction(EntityHandle handle, uint8_t newFaction);

    [[nodiscard]] ItemData& getItemData(EntityHandle handle);
    [[nodiscard]] const ItemData& getItemData(EntityHandle handle) const;

    [[nodiscard]] ProjectileData& getProjectileData(EntityHandle handle);
    [[nodiscard]] const ProjectileData& getProjectileData(EntityHandle handle) const;
    [[nodiscard]] ProjectileData& getProjectileData(uint32_t typeLocalIndex);
    [[nodiscard]] const ProjectileData& getProjectileData(uint32_t typeLocalIndex) const;

    [[nodiscard]] ContainerData& getContainerData(EntityHandle handle);
    [[nodiscard]] const ContainerData& getContainerData(EntityHandle handle) const;
    [[nodiscard]] ContainerData& getContainerData(uint32_t typeLocalIndex);
    [[nodiscard]] const ContainerData& getContainerData(uint32_t typeLocalIndex) const;

    [[nodiscard]] HarvestableData& getHarvestableData(EntityHandle handle);
    [[nodiscard]] const HarvestableData& getHarvestableData(EntityHandle handle) const;
    [[nodiscard]] HarvestableData& getHarvestableData(uint32_t typeLocalIndex);
    [[nodiscard]] const HarvestableData& getHarvestableData(uint32_t typeLocalIndex) const;

    [[nodiscard]] AreaEffectData& getAreaEffectData(EntityHandle handle);
    [[nodiscard]] const AreaEffectData& getAreaEffectData(EntityHandle handle) const;

    // ========================================================================
    // NPC RENDER DATA ACCESS (for data-driven NPCs)
    // ========================================================================

    /**
     * @brief Get NPC render data by entity handle
     * @param handle Entity handle (must be NPC)
     * @return Reference to NPCRenderData
     */
    [[nodiscard]] NPCRenderData& getNPCRenderData(EntityHandle handle);
    [[nodiscard]] const NPCRenderData& getNPCRenderData(EntityHandle handle) const;

    /**
     * @brief Get NPC render data by type-local index (for batch processing)
     * @param typeLocalIndex Index from EntityHotData.typeLocalIndex
     * @return Reference to NPCRenderData
     */
    [[nodiscard]] NPCRenderData& getNPCRenderDataByTypeIndex(uint32_t typeLocalIndex);
    [[nodiscard]] const NPCRenderData& getNPCRenderDataByTypeIndex(uint32_t typeLocalIndex) const;

    // ========================================================================
    // BY-INDEX TYPE-SPECIFIC ACCESS (for batch processing)
    // ========================================================================

    /**
     * @brief Get character data by EDM index (for batch processing)
     * @param index EDM index from getActiveIndices()
     * @return CharacterData for the entity
     * @note Only valid for NPC/Player entities
     */
    [[nodiscard]] CharacterData& getCharacterDataByIndex(size_t index);
    [[nodiscard]] const CharacterData& getCharacterDataByIndex(size_t index) const;

    // ========================================================================
    // PATH DATA ACCESS (for AI pathfinding - indexed by edmIndex)
    // ========================================================================

    /**
     * @brief Get path data by EDM index
     * @param index EDM index from getActiveIndices()
     * @return PathData for the entity
     * @note Path data grows lazily - accessing an index will ensure storage exists
     */
    [[nodiscard]] PathData& getPathData(size_t index);
    [[nodiscard]] const PathData& getPathData(size_t index) const;

    /**
     * @brief Check if path data exists for an entity
     * @param index EDM index
     * @return true if path data storage exists and index is valid
     */
    [[nodiscard]] bool hasPathData(size_t index) const noexcept;

    /**
     * @brief Ensure path data storage exists for an entity
     * @param index EDM index
     * Called automatically when AI behavior is assigned
     */
    void ensurePathData(size_t index);

    /**
     * @brief Clear path data for an entity (called on destruction)
     * @param index EDM index
     */
    void clearPathData(size_t index);

    /**
     * @brief Get bounded waypoint slot view for direct write (zero-copy)
     */
    [[nodiscard]] std::span<Vector2D, FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY>
    getWaypointSlot(size_t index) noexcept;

    /**
     * @brief Finalize path after direct write
     */
    void finalizePath(size_t index, uint16_t length) noexcept;

    /**
     * @brief Advance waypoint and update cached currentWaypoint
     * @param index EDM index
     * Call this instead of PathData::advanceWaypoint() to keep cache in sync.
     */
    void advanceWaypointWithCache(size_t index);

    /**
     * @brief Get waypoint from entity's path
     * @param entityIdx EDM index
     * @param waypointIdx Index within the path
     * @return Waypoint position (inline for hot-path access)
     */
    [[nodiscard]] Vector2D getWaypoint(size_t entityIdx, size_t waypointIdx) const;

    /**
     * @brief Get current waypoint for entity's path
     * @param entityIdx EDM index
     * @return Current waypoint position (inline for hot-path access)
     */
    [[nodiscard]] Vector2D getCurrentWaypoint(size_t entityIdx) const;

    /**
     * @brief Get goal (last waypoint) of entity's path
     * @param entityIdx EDM index
     * @return Goal position (inline for hot-path access)
     */
    [[nodiscard]] Vector2D getPathGoal(size_t entityIdx) const;

    /**
     * @brief Clear all waypoint slots (call on state transitions)
     * With per-entity slots, this just clears the vector.
     */
    void clearWaypointSlots() noexcept { m_waypointSlots.clear(); }

    // ========================================================================
    // BEHAVIOR DATA ACCESS (for AI behaviors - indexed by edmIndex)
    // ========================================================================

    /**
     * @brief Get behavior data by EDM index
     * @param index EDM index from getActiveIndices()
     * @return BehaviorData for the entity
     */
    [[nodiscard]] BehaviorData& getBehaviorData(size_t index);
    [[nodiscard]] const BehaviorData& getBehaviorData(size_t index) const;

    /**
     * @brief Get the archetype reference for an entity's behavior config
     * @param edmIdx EDM index from getActiveIndices()
     * @return BehaviorConfigRef with type tag and pool index (read-only)
     */
    [[nodiscard]] BehaviorConfigRef getBehaviorConfigRef(size_t edmIdx) const;

    /**
     * @brief Reassign an entity's behavior config to a new variant pool slot
     *
     * Pops the old pool slot (swap-with-back + patch displaced owner ref),
     * pushes into the correct variant pool, and updates the entity's ref.
     * Main-thread only. O(1) after init() warmup (no allocation if pool reserved).
     *
     * @param edmIdx  EDM index of the entity being updated
     * @param newConfig  New config to store (type must not be None)
     */
    void reassignBehaviorConfig(size_t edmIdx, const VoidLight::BehaviorConfigData& newConfig);

    /**
     * @brief Clear an entity's behavior config pool slot and reset its ref
     *
     * Pops the old slot if present (swap-with-back + owner patch), resets
     * the entity's ref to {None, UINT32_MAX}. Used by unregistration paths.
     * Main-thread only.
     *
     * @param edmIdx EDM index of the entity being cleared
     */
    void clearBehaviorConfig(size_t edmIdx);

    // Per-variant dense config accessors — const& (no copy)
    [[nodiscard]] const VoidLight::IdleBehaviorConfig&   getIdleConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::WanderBehaviorConfig& getWanderConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::ChaseBehaviorConfig&  getChaseConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::PatrolBehaviorConfig& getPatrolConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::FleeBehaviorConfig&   getFleeConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::FollowBehaviorConfig& getFollowConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::GuardBehaviorConfig&  getGuardConfig(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::AttackBehaviorConfig& getAttackConfig(uint32_t poolIndex) const;

    // Per-variant dense state accessors — mutable& for frame-by-frame writes in execute().
    // Indexed by the same pool index as the corresponding config pool.
    [[nodiscard]] VoidLight::IdleStateData&   getIdleState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::WanderStateData& getWanderState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::ChaseStateData&  getChaseState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::PatrolStateData& getPatrolState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::FleeStateData&   getFleeState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::FollowStateData& getFollowState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::GuardStateData&  getGuardState(uint32_t poolIndex);
    [[nodiscard]] VoidLight::AttackStateData& getAttackState(uint32_t poolIndex);

    // Const overloads for diagnostics / tests
    [[nodiscard]] const VoidLight::IdleStateData&   getIdleState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::WanderStateData& getWanderState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::ChaseStateData&  getChaseState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::PatrolStateData& getPatrolState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::FleeStateData&   getFleeState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::FollowStateData& getFollowState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::GuardStateData&  getGuardState(uint32_t poolIndex) const;
    [[nodiscard]] const VoidLight::AttackStateData& getAttackState(uint32_t poolIndex) const;

    /**
     * @brief Check if behavior data exists and is valid for an entity
     * @param index EDM index
     * @return true if behavior data exists and is valid
     */
    [[nodiscard]] bool hasBehaviorData(size_t index) const noexcept;

    /**
     * @brief Initialize behavior data for a specific behavior type
     * @param index EDM index
     * @param type The BehaviorType to initialize
     */
    void initBehaviorData(size_t index, BehaviorType type);

    /**
     * @brief Clear behavior data for an entity (called on behavior change/destruction)
     * @param index EDM index
     */
    void clearBehaviorData(size_t index);

    // ========================================================================
    // NPC MEMORY DATA ACCESS (indexed by edmIndex, parallel to PathData/BehaviorData)
    // ========================================================================

    /**
     * @brief Get memory data by EDM index
     * @param index EDM index from getActiveIndices()
     * @return NPCMemoryData for the entity
     */
    [[nodiscard]] NPCMemoryData& getMemoryData(size_t index);
    [[nodiscard]] const NPCMemoryData& getMemoryData(size_t index) const;

    /**
     * @brief Check if memory data exists and is valid for an entity
     * @param index EDM index
     * @return true if memory data storage exists and is initialized
     */
    [[nodiscard]] bool hasMemoryData(size_t index) const noexcept;

    /**
     * @brief Initialize memory data for an entity
     * @param index EDM index
     * Called when NPC is created or first needs memory
     */
    void initMemoryData(size_t index);

    /**
     * @brief Clear memory data for an entity (called on destruction)
     * @param index EDM index
     */
    void clearMemoryData(size_t index);

    /**
     * @brief Add a memory to an NPC
     * @param index EDM index
     * @param entry Memory entry to add
     * @param useOverflow If true, use overflow storage when inline is full
     */
    void addMemory(size_t index, const MemoryEntry& entry, bool useOverflow = false);

    /**
     * @brief Find memories of a specific type
     * @param index EDM index
     * @param type Memory type to find
     * @param outMemories Output vector of matching memories
     * @param maxResults Maximum results to return (0 = all)
     */
    void findMemoriesByType(size_t index, MemoryType type,
                            std::vector<const MemoryEntry*>& outMemories,
                            size_t maxResults = 0) const;

    /**
     * @brief Find memories involving a specific entity
     * @param index EDM index
     * @param subject Entity handle to search for
     * @param outMemories Output vector of matching memories
     */
    void findMemoriesOfEntity(size_t index, EntityHandle subject,
                              std::vector<const MemoryEntry*>& outMemories) const;

    /**
     * @brief Update emotional state with decay
     * @param index EDM index
     * @param deltaTime Frame time for decay calculation
     * @param decayRate Decay rate per second (default 0.05 = 5%/sec)
     */
    void updateEmotionalDecay(size_t index, float deltaTime, float decayRate = 0.05f);

    /**
     * @brief Modify emotional state
     * @param index EDM index
     * @param aggression Delta aggression (-1.0 to 1.0)
     * @param fear Delta fear (-1.0 to 1.0)
     * @param curiosity Delta curiosity (-1.0 to 1.0)
     * @param suspicion Delta suspicion (-1.0 to 1.0)
     */
    void modifyEmotions(size_t index, float aggression, float fear,
                        float curiosity, float suspicion);

    /**
     * @brief Record a combat event (updates aggregate stats + adds memory)
     * @param index EDM index of the NPC
     * @param attacker Who initiated the attack
     * @param target Who was attacked
     * @param damage Damage amount
     * @param wasAttacked true if this NPC was the target
     * @param gameTime Current game time for timestamp
     */
    void recordCombatEvent(size_t index, EntityHandle attacker,
                           EntityHandle target, float damage, bool wasAttacked,
                           float gameTime);

    /**
     * @brief Add a location to history
     * @param index EDM index
     * @param location Position to record
     */
    void addLocationToHistory(size_t index, const Vector2D& location);

    /**
     * @brief Get memory overflow data for an entity
     * @param edmIdx Entity data manager index
     * @return Pointer to overflow data, or nullptr if entity has no overflow
     */
    [[nodiscard]] MemoryOverflow* getMemoryOverflow(size_t edmIdx);
    [[nodiscard]] const MemoryOverflow* getMemoryOverflow(size_t edmIdx) const;

    // ========================================================================
    // SIMULATION TIER MANAGEMENT
    // ========================================================================

    /**
     * @brief Set the simulation tier for an entity
     * @param handle Entity handle
     * @param tier New simulation tier
     */
    void setSimulationTier(EntityHandle handle, SimulationTier tier);

    /**
     * @brief Update simulation tiers based on distance from reference point
     * @param referencePoint Typically player/camera position
     * @param activeRadius Entities within this are Active
     * @param backgroundRadius Entities within this (but outside activeRadius) are Background
     */
    void updateSimulationTiers(const Vector2D& referencePoint,
                               float activeRadius = 1500.0f,
                               float backgroundRadius = 10000.0f);

    /**
     * @brief Get indices of all Active tier entities
     */
    [[nodiscard]] std::span<const size_t> getActiveIndices() const;

    /**
     * @brief Get indices of Active tier entities with collision enabled
     * Cached and rebuilt when tiers change or collision is enabled/disabled.
     * Used by CollisionManager to avoid filtering in hot loop.
     */
    [[nodiscard]] std::span<const size_t> getActiveIndicesWithCollision() const;

    /**
     * @brief Get indices of Active tier entities that need trigger detection
     * Cached and rebuilt when trigger detection flag changes.
     * Used by CollisionManager for EventOnly trigger detection.
     */
    [[nodiscard]] std::span<const size_t> getTriggerDetectionIndices() const;

    /**
     * @brief Mark trigger detection indices as needing rebuild
     */
    void markTriggerDetectionDirty() noexcept { m_triggerDetectionDirty = true; }

    /**
     * @brief Get indices of all Background tier entities
     */
    [[nodiscard]] std::span<const size_t> getBackgroundIndices() const;

    /**
     * @brief Get indices of entities by kind
     */
    [[nodiscard]] std::span<const size_t> getIndicesByKind(EntityKind kind) const;

    // ========================================================================
    // QUERIES
    // ========================================================================

    /**
     * @brief Find entities within a radius
     * @param center Query center point
     * @param radius Search radius
     * @param outHandles Output vector for found handles
     * @param kindFilter Optional: only return entities of this kind (COUNT = all)
     */
    void queryEntitiesInRadius(const Vector2D& center,
                               float radius,
                               std::vector<EntityHandle>& outHandles,
                               EntityKind kindFilter = EntityKind::COUNT) const;

    /**
     * @brief Get total entity count
     */
    [[nodiscard]] size_t getEntityCount() const noexcept;

    /**
     * @brief Get count of entities by kind
     */
    [[nodiscard]] size_t getEntityCount(EntityKind kind) const noexcept;

    /**
     * @brief Get count of entities by tier
     */
    [[nodiscard]] size_t getEntityCount(SimulationTier tier) const noexcept;

    // ========================================================================
    // ENTITY ID LOOKUP
    // ========================================================================

    /**
     * @brief Get entity ID by index
     */
    [[nodiscard]] EntityHandle::IDType getEntityId(size_t index) const;

    /**
     * @brief Get handle by index
     */
    [[nodiscard]] EntityHandle getHandle(size_t index) const;

private:
    EntityDataManager() = default;
    ~EntityDataManager();

    EntityDataManager(const EntityDataManager&) = delete;
    EntityDataManager& operator=(const EntityDataManager&) = delete;

    // Internal allocation helpers
    size_t allocateSlot();
    void freeSlot(size_t index);
    uint32_t nextGeneration(size_t index);
    uint32_t nextStaticGeneration(size_t index);
    void clearAllEntityStorage();
    void rebuildTierIndicesFromHotData();

    /**
     * @brief Allocate a character slot (CharacterData + NPCRenderData in sync)
     * @return The allocated typeLocalIndex
     * @note Both arrays always grow together to keep indices valid
     */
    uint32_t allocateCharacterSlot();

    /**
     * @brief Internal: Create NPC entity with collision data
     * @note Use createNPCWithRaceClass() for the public API
     */
    EntityHandle createNPC(const Vector2D& position,
                          float halfWidth = 16.0f,
                          float halfHeight = 16.0f);

    // ========================================================================
    // STORAGE (Structure of Arrays)
    // ========================================================================

    // Shared data (indexed by global entity index)
    std::vector<EntityHotData> m_hotData;           // Dynamic entities only
    std::vector<EntityHotData> m_staticHotData;     // Static entities (separate, not tiered)
    std::vector<EntityHandle::IDType> m_entityIds;
    std::vector<EntityHandle::IDType> m_staticEntityIds;

    // ID to index mapping
    std::unordered_map<EntityHandle::IDType, size_t> m_idToIndex;
    std::unordered_map<EntityHandle::IDType, size_t> m_staticIdToIndex;

    // Type-specific data (indexed by typeLocalIndex in EntityHotData)
    std::vector<CharacterData> m_characterData;      // Player + NPC
    std::vector<ItemData> m_itemData;                // DroppedItem
    std::vector<ProjectileData> m_projectileData;    // Projectile
    std::vector<ContainerData> m_containerData;      // Container
    std::vector<HarvestableData> m_harvestableData;  // Harvestable
    std::vector<AreaEffectData> m_areaEffectData;    // AreaEffect
    std::vector<NPCRenderData> m_npcRenderData;      // NPC render data (same index as CharacterData for NPCs)
    std::vector<ItemRenderData> m_itemRenderData;    // DroppedItem render data (same index as ItemData)
    std::vector<ContainerRenderData> m_containerRenderData;  // Container render data

    // Inventory data (indexed by inventory index from createInventory())
    std::vector<InventoryData> m_inventoryData;
    std::unordered_map<uint32_t, InventoryOverflow> m_inventoryOverflow;  // overflowId -> overflow data
    std::vector<uint32_t> m_freeInventorySlots;                           // Free-list for inventory reuse
    uint32_t m_nextOverflowId{1};                                         // Next overflow ID (0 = none)
    mutable std::mutex m_inventoryMutex;                                  // Thread safety for inventory ops

    // Transient knockback state — sparse/dense; only entities under knockback occupy space.
    // resizeSparse() called at every m_hotData growth site (allocateSlot new-slot path).
    // removeAllFor() called from freeSlot() to clean up on entity destruction.
    SparseSidecar<KnockbackData> m_knockback;

    // Sidecar coordination hooks — registered once in init(). Each sidecar pushes its
    // three lambdas here so allocateSlot / freeSlot / clearAllEntityStorage dispatch
    // to all sidecars without per-sidecar edits at each call site.
    std::vector<std::function<void(size_t /*newCapacity*/)>> m_sidecarGrowHooks;
    std::vector<std::function<void(uint32_t /*edmIdx*/)>>    m_sidecarPerEntityHooks;
    std::vector<std::function<void()>>                       m_sidecarResetHooks;

    // Path data (indexed by edmIndex, sparse - grows lazily for AI entities)
    std::vector<PathData> m_pathData;

    // Per-entity waypoint slots (indexed parallel to m_pathData)
    // Each entity owns one 256-byte slot for lock-free writes
    std::vector<FixedWaypointSlot> m_waypointSlots;
    // Behavior data (indexed by edmIndex, pre-allocated alongside hotData)
    std::vector<BehaviorData> m_behaviorData;

    // Archetype behavior config storage.
    // m_behaviorConfigRef[edmIndex] holds a {type, poolIndex} pair pointing into
    // one of the per-variant dense pools below.  index == UINT32_MAX means no config.
    std::vector<BehaviorConfigRef> m_behaviorConfigRef;

    // Dense per-variant config pools.  Only entities that actually use a given
    // behavior type occupy space here — dramatically cheaper than the old 388-byte
    // union for every entity regardless of type.
    std::vector<VoidLight::IdleBehaviorConfig>   m_idleConfigs;
    std::vector<VoidLight::WanderBehaviorConfig> m_wanderConfigs;
    std::vector<VoidLight::ChaseBehaviorConfig>  m_chaseConfigs;
    std::vector<VoidLight::PatrolBehaviorConfig> m_patrolConfigs;
    std::vector<VoidLight::FleeBehaviorConfig>   m_fleeConfigs;
    std::vector<VoidLight::FollowBehaviorConfig> m_followConfigs;
    std::vector<VoidLight::GuardBehaviorConfig>  m_guardConfigs;
    std::vector<VoidLight::AttackBehaviorConfig> m_attackConfigs;

    // Parallel owner vectors — m_*Owners[i] == edmIndex that owns m_*Configs[i]
    // AND m_*States[i]. Config and state pools share the same owner vector by invariant.
    // Required so swap-with-back removal can patch the displaced entity's ref.
    std::vector<size_t> m_idleOwners;
    std::vector<size_t> m_wanderOwners;
    std::vector<size_t> m_chaseOwners;
    std::vector<size_t> m_patrolOwners;
    std::vector<size_t> m_fleeOwners;
    std::vector<size_t> m_followOwners;
    std::vector<size_t> m_guardOwners;
    std::vector<size_t> m_attackOwners;

    // Dense per-variant state pools — lockstep with config pools (same index, same owner).
    // Populated with a default-constructed slot in reassignBehaviorConfig; filled by
    // Behaviors::init*() afterward. Popped together with the config in clearBehaviorConfig.
    std::vector<VoidLight::IdleStateData>   m_idleStates;
    std::vector<VoidLight::WanderStateData> m_wanderStates;
    std::vector<VoidLight::ChaseStateData>  m_chaseStates;
    std::vector<VoidLight::PatrolStateData> m_patrolStates;
    std::vector<VoidLight::FleeStateData>   m_fleeStates;
    std::vector<VoidLight::FollowStateData> m_followStates;
    std::vector<VoidLight::GuardStateData>  m_guardStates;
    std::vector<VoidLight::AttackStateData> m_attackStates;

    // NPC Memory data (indexed by edmIndex, pre-allocated alongside hotData)
    // Persists across behavior changes unlike BehaviorData
    std::vector<NPCMemoryData> m_memoryData;

    // Sparse: only NPCs whose memory overflowed inline storage occupy a slot.
    // Follows the m_knockback reference implementation.
    SparseSidecar<MemoryOverflow> m_memoryOverflow;

    // Type-specific free-lists (reuse indices when entities are destroyed)
    std::vector<uint32_t> m_freeCharacterSlots;
    std::vector<uint32_t> m_freeItemSlots;
    std::vector<uint32_t> m_freeProjectileSlots;
    std::vector<uint32_t> m_freeContainerSlots;
    std::vector<uint32_t> m_freeHarvestableSlots;
    std::vector<uint32_t> m_freeAreaEffectSlots;

    // Tier indices (rebuilt when tiers change)
    std::vector<size_t> m_activeIndices;
    std::vector<size_t> m_backgroundIndices;
    std::vector<size_t> m_hibernatedIndices;
    bool m_tierIndicesDirty{true};

    // Collision-enabled active indices (cached for CollisionManager optimization)
    mutable std::vector<size_t> m_activeCollisionIndices;
    mutable bool m_activeCollisionDirty{true};

    // Trigger detection indices (cached for CollisionManager optimization)
    mutable std::vector<size_t> m_triggerDetectionIndices;
    mutable bool m_triggerDetectionDirty{true};

    // Kind indices (per-kind dirty flags to avoid full rebuild when querying single kind)
    // NOTE: Entity creation is protected by m_creationMutex (safe from worker threads).
    // Destruction uses m_destructionMutex. These dirty flags don't need atomics.
    std::array<std::vector<size_t>, static_cast<size_t>(EntityKind::COUNT)> m_kindIndices;
    mutable std::array<bool, static_cast<size_t>(EntityKind::COUNT)> m_kindIndicesDirty{};

    // Helper to mark specific kind dirty (called when entities are created/destroyed)
    void markKindDirty(EntityKind kind) {
        m_kindIndicesDirty[static_cast<size_t>(kind)] = true;
    }
    void markAllKindsDirty() {
        m_kindIndicesDirty.fill(true);
    }

    /**
     * @brief Internal: Get inventory quantity while already holding m_inventoryMutex
     * @param inventoryIndex Target inventory
     * @param handle Resource type handle
     * @return Total quantity across all slots
     * @note MUST be called while holding m_inventoryMutex lock
     */
    [[nodiscard]] int getInventoryQuantityLocked(uint32_t inventoryIndex,
                                                  VoidLight::ResourceHandle handle) const;

    /**
     * @brief Internal: Destroy a static resource entity (DroppedItem, Container, Harvestable)
     * @param handle Entity handle to destroy
     * @note Static resources are destroyed immediately (no deferred queue)
     */
    void destroyStaticResource(EntityHandle handle);

    // Destruction queue and processing buffer (avoid per-frame allocation)
    std::vector<EntityHandle> m_destructionQueue;
    std::vector<EntityHandle> m_destroyBuffer;  // Reused in processDestructionQueue

    // Free list for slot reuse
    std::vector<size_t> m_freeSlots;
    std::vector<size_t> m_freeStaticSlots;

    // Generation counters per slot (for stale handle detection)
    std::vector<uint32_t> m_generations;
    std::vector<uint32_t> m_staticGenerations;

    // Thread safety for entity operations
    std::mutex m_destructionMutex;  // Protects destruction queue
    std::mutex m_creationMutex;     // Protects entity creation (allocateSlot + vector growth)

    // ========================================================================
    // CREATURE COMPOSITION REGISTRIES
    // ========================================================================

    // NPC Race/Class registries
    std::unordered_map<std::string, RaceInfo> m_raceRegistry;
    std::unordered_map<std::string, ClassInfo> m_classRegistry;
    std::unordered_map<std::string, uint8_t> m_raceNameToId;
    std::unordered_map<std::string, uint8_t> m_classNameToId;
    std::vector<std::string> m_raceIdToName;
    std::vector<std::string> m_classIdToName;
    void initializeRaceRegistry();
    void initializeClassRegistry();

    // Monster Type/Variant registries
    std::unordered_map<std::string, MonsterTypeInfo> m_monsterTypeRegistry;
    std::unordered_map<std::string, MonsterVariantInfo> m_monsterVariantRegistry;
    std::unordered_map<std::string, uint8_t> m_monsterTypeNameToId;
    std::unordered_map<std::string, uint8_t> m_monsterVariantNameToId;
    std::vector<std::string> m_monsterTypeIdToName;
    std::vector<std::string> m_monsterVariantIdToName;
    void initializeMonsterTypeRegistry();
    void initializeMonsterVariantRegistry();

    // Animal Species/Role registries
    std::unordered_map<std::string, SpeciesInfo> m_speciesRegistry;
    std::unordered_map<std::string, AnimalRoleInfo> m_animalRoleRegistry;
    std::unordered_map<std::string, uint8_t> m_speciesNameToId;
    std::unordered_map<std::string, uint8_t> m_animalRoleNameToId;
    std::vector<std::string> m_speciesIdToName;
    std::vector<std::string> m_animalRoleIdToName;
    void initializeSpeciesRegistry();
    void initializeAnimalRoleRegistry();

    // Helper for faction-based collision layers
    void applyFactionCollision(size_t index, uint8_t faction);
    bool autoEquipCharacterEquipment(EntityHandle handle);
    void recalculateCharacterEquipmentStats(uint32_t characterIndex);
    [[nodiscard]] VoidLight::ResourceHandle
    findCompatibleAmmo(uint32_t inventoryIndex, std::string_view ammoType) const;

    // State
    std::atomic<bool> m_initialized{false};

    // Counters
    std::atomic<size_t> m_totalEntityCount{0};
    std::array<std::atomic<size_t>, static_cast<size_t>(EntityKind::COUNT)> m_countByKind{};
    std::array<std::atomic<size_t>, 3> m_countByTier{};  // Active, Background, Hibernated
};

// ============================================================================
// INLINE HOT-PATH ACCESSORS
// ============================================================================
// These accessors are inlined for zero-overhead access in hot loops.
// They are called thousands of times per frame in AIManager::processBatch(),
// CollisionManager, and PathfinderManager. Inlining eliminates ~5-20 cycles
// of function call overhead per access.

inline EntityHotData& EntityDataManager::getHotDataByIndex(size_t index) {
    assert(index < m_hotData.size() && "Index out of bounds");
    return m_hotData[index];
}

inline const EntityHotData& EntityDataManager::getHotDataByIndex(size_t index) const {
    assert(index < m_hotData.size() && "Index out of bounds");
    return m_hotData[index];
}

inline TransformData& EntityDataManager::getTransformByIndex(size_t index) {
    assert(index < m_hotData.size() && "Index out of bounds");
    return m_hotData[index].transform;
}

inline const TransformData& EntityDataManager::getTransformByIndex(size_t index) const {
    assert(index < m_hotData.size() && "Index out of bounds");
    return m_hotData[index].transform;
}

inline BehaviorData& EntityDataManager::getBehaviorData(size_t index) {
    assert(index < m_behaviorData.size() && "BehaviorData index out of bounds");
    return m_behaviorData[index];
}

inline const BehaviorData& EntityDataManager::getBehaviorData(size_t index) const {
    assert(index < m_behaviorData.size() && "BehaviorData index out of bounds");
    return m_behaviorData[index];
}

inline BehaviorConfigRef EntityDataManager::getBehaviorConfigRef(size_t edmIdx) const {
    assert(edmIdx < m_behaviorConfigRef.size() && "BehaviorConfigRef index out of bounds");
    return m_behaviorConfigRef[edmIdx];
}

inline const VoidLight::IdleBehaviorConfig& EntityDataManager::getIdleConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_idleConfigs.size() && "Idle config pool index out of bounds");
    return m_idleConfigs[poolIndex];
}

inline const VoidLight::WanderBehaviorConfig& EntityDataManager::getWanderConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_wanderConfigs.size() && "Wander config pool index out of bounds");
    return m_wanderConfigs[poolIndex];
}

inline const VoidLight::ChaseBehaviorConfig& EntityDataManager::getChaseConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_chaseConfigs.size() && "Chase config pool index out of bounds");
    return m_chaseConfigs[poolIndex];
}

inline const VoidLight::PatrolBehaviorConfig& EntityDataManager::getPatrolConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_patrolConfigs.size() && "Patrol config pool index out of bounds");
    return m_patrolConfigs[poolIndex];
}

inline const VoidLight::FleeBehaviorConfig& EntityDataManager::getFleeConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_fleeConfigs.size() && "Flee config pool index out of bounds");
    return m_fleeConfigs[poolIndex];
}

inline const VoidLight::FollowBehaviorConfig& EntityDataManager::getFollowConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_followConfigs.size() && "Follow config pool index out of bounds");
    return m_followConfigs[poolIndex];
}

inline const VoidLight::GuardBehaviorConfig& EntityDataManager::getGuardConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_guardConfigs.size() && "Guard config pool index out of bounds");
    return m_guardConfigs[poolIndex];
}

inline const VoidLight::AttackBehaviorConfig& EntityDataManager::getAttackConfig(uint32_t poolIndex) const {
    assert(poolIndex < m_attackConfigs.size() && "Attack config pool index out of bounds");
    return m_attackConfigs[poolIndex];
}

// ============================================================================
// BEHAVIOR STATE POOL ACCESSORS (inline hot-path)
// ============================================================================

inline VoidLight::IdleStateData& EntityDataManager::getIdleState(uint32_t poolIndex) {
    assert(poolIndex < m_idleStates.size() && "Idle state pool index out of bounds");
    return m_idleStates[poolIndex];
}
inline const VoidLight::IdleStateData& EntityDataManager::getIdleState(uint32_t poolIndex) const {
    assert(poolIndex < m_idleStates.size() && "Idle state pool index out of bounds");
    return m_idleStates[poolIndex];
}

inline VoidLight::WanderStateData& EntityDataManager::getWanderState(uint32_t poolIndex) {
    assert(poolIndex < m_wanderStates.size() && "Wander state pool index out of bounds");
    return m_wanderStates[poolIndex];
}
inline const VoidLight::WanderStateData& EntityDataManager::getWanderState(uint32_t poolIndex) const {
    assert(poolIndex < m_wanderStates.size() && "Wander state pool index out of bounds");
    return m_wanderStates[poolIndex];
}

inline VoidLight::ChaseStateData& EntityDataManager::getChaseState(uint32_t poolIndex) {
    assert(poolIndex < m_chaseStates.size() && "Chase state pool index out of bounds");
    return m_chaseStates[poolIndex];
}
inline const VoidLight::ChaseStateData& EntityDataManager::getChaseState(uint32_t poolIndex) const {
    assert(poolIndex < m_chaseStates.size() && "Chase state pool index out of bounds");
    return m_chaseStates[poolIndex];
}

inline VoidLight::PatrolStateData& EntityDataManager::getPatrolState(uint32_t poolIndex) {
    assert(poolIndex < m_patrolStates.size() && "Patrol state pool index out of bounds");
    return m_patrolStates[poolIndex];
}
inline const VoidLight::PatrolStateData& EntityDataManager::getPatrolState(uint32_t poolIndex) const {
    assert(poolIndex < m_patrolStates.size() && "Patrol state pool index out of bounds");
    return m_patrolStates[poolIndex];
}

inline VoidLight::FleeStateData& EntityDataManager::getFleeState(uint32_t poolIndex) {
    assert(poolIndex < m_fleeStates.size() && "Flee state pool index out of bounds");
    return m_fleeStates[poolIndex];
}
inline const VoidLight::FleeStateData& EntityDataManager::getFleeState(uint32_t poolIndex) const {
    assert(poolIndex < m_fleeStates.size() && "Flee state pool index out of bounds");
    return m_fleeStates[poolIndex];
}

inline VoidLight::FollowStateData& EntityDataManager::getFollowState(uint32_t poolIndex) {
    assert(poolIndex < m_followStates.size() && "Follow state pool index out of bounds");
    return m_followStates[poolIndex];
}
inline const VoidLight::FollowStateData& EntityDataManager::getFollowState(uint32_t poolIndex) const {
    assert(poolIndex < m_followStates.size() && "Follow state pool index out of bounds");
    return m_followStates[poolIndex];
}

inline VoidLight::GuardStateData& EntityDataManager::getGuardState(uint32_t poolIndex) {
    assert(poolIndex < m_guardStates.size() && "Guard state pool index out of bounds");
    return m_guardStates[poolIndex];
}
inline const VoidLight::GuardStateData& EntityDataManager::getGuardState(uint32_t poolIndex) const {
    assert(poolIndex < m_guardStates.size() && "Guard state pool index out of bounds");
    return m_guardStates[poolIndex];
}

inline VoidLight::AttackStateData& EntityDataManager::getAttackState(uint32_t poolIndex) {
    assert(poolIndex < m_attackStates.size() && "Attack state pool index out of bounds");
    return m_attackStates[poolIndex];
}
inline const VoidLight::AttackStateData& EntityDataManager::getAttackState(uint32_t poolIndex) const {
    assert(poolIndex < m_attackStates.size() && "Attack state pool index out of bounds");
    return m_attackStates[poolIndex];
}

inline PathData& EntityDataManager::getPathData(size_t index) {
    // PathData is pre-allocated in allocateSlot(), no lazy resize needed
    assert(index < m_pathData.size() && "PathData not pre-allocated for index");
    return m_pathData[index];
}

inline const PathData& EntityDataManager::getPathData(size_t index) const {
    assert(index < m_pathData.size() && "Path data index out of bounds");
    return m_pathData[index];
}

// Per-entity waypoint slot accessors - O(1) access with no shared state
inline std::span<Vector2D, FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY>
EntityDataManager::getWaypointSlot(size_t index) noexcept {
    assert(index < m_waypointSlots.size() && "Entity waypoint slot out of bounds");
    return std::span<Vector2D, FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY>(
        m_waypointSlots[index].waypoints, FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY);
}

inline Vector2D EntityDataManager::getWaypoint(size_t entityIdx, size_t waypointIdx) const {
    assert(entityIdx < m_waypointSlots.size() && "Entity waypoint slot out of bounds");
    const auto& pd = m_pathData[entityIdx];
    assert(waypointIdx < pd.pathLength && "Waypoint index out of bounds");
    return m_waypointSlots[entityIdx][waypointIdx];
}

// NPC Memory data inline accessors
inline NPCMemoryData& EntityDataManager::getMemoryData(size_t index) {
    assert(index < m_memoryData.size() && "MemoryData index out of bounds");
    return m_memoryData[index];
}

inline const NPCMemoryData& EntityDataManager::getMemoryData(size_t index) const {
    assert(index < m_memoryData.size() && "MemoryData index out of bounds");
    return m_memoryData[index];
}

inline bool EntityDataManager::hasMemoryData(size_t index) const noexcept {
    return index < m_memoryData.size() && m_memoryData[index].isValid();
}

inline bool EntityDataManager::hasBehaviorData(size_t index) const noexcept {
    return index < m_behaviorData.size() && m_behaviorData[index].isValid();
}

inline void EntityDataManager::updateEmotionalDecay(size_t index, float deltaTime, float decayRate) {
    if (index >= m_memoryData.size()) {
        return;
    }
    auto& data = m_memoryData[index];
    if (!data.isValid()) {
        return;
    }
    data.emotions.decay(decayRate, deltaTime);
    data.lastDecayTime += deltaTime;
    data.lastCombatTime += deltaTime;
}

inline MemoryOverflow* EntityDataManager::getMemoryOverflow(size_t edmIdx) {
    return m_memoryOverflow.get(static_cast<uint32_t>(edmIdx));
}

inline const MemoryOverflow* EntityDataManager::getMemoryOverflow(size_t edmIdx) const {
    return m_memoryOverflow.get(static_cast<uint32_t>(edmIdx));
}

inline Vector2D EntityDataManager::getCurrentWaypoint(size_t entityIdx) const {
    assert(entityIdx < m_waypointSlots.size() && "Entity waypoint slot out of bounds");
    const auto& pd = m_pathData[entityIdx];
    assert(pd.navIndex < pd.pathLength && "Current waypoint out of bounds");
    return m_waypointSlots[entityIdx][pd.navIndex];
}

inline Vector2D EntityDataManager::getPathGoal(size_t entityIdx) const {
    assert(entityIdx < m_waypointSlots.size() && "Entity waypoint slot out of bounds");
    const auto& pd = m_pathData[entityIdx];
    assert(pd.pathLength > 0 && "Cannot get goal of empty path");
    return m_waypointSlots[entityIdx][pd.pathLength - 1];
}

inline CharacterData& EntityDataManager::getCharacterDataByIndex(size_t index) {
    assert(index < m_hotData.size() && "Index out of bounds");
    uint32_t typeIndex = m_hotData[index].typeLocalIndex;
    assert(typeIndex < m_characterData.size() && "Type index out of bounds");
    return m_characterData[typeIndex];
}

inline const CharacterData& EntityDataManager::getCharacterDataByIndex(size_t index) const {
    assert(index < m_hotData.size() && "Index out of bounds");
    uint32_t typeIndex = m_hotData[index].typeLocalIndex;
    assert(typeIndex < m_characterData.size() && "Type index out of bounds");
    return m_characterData[typeIndex];
}

// NPC render data accessors - O(1) access by type index
inline NPCRenderData& EntityDataManager::getNPCRenderDataByTypeIndex(uint32_t typeLocalIndex) {
    assert(typeLocalIndex < m_npcRenderData.size() && "NPC render data type index out of bounds");
    return m_npcRenderData[typeLocalIndex];
}

inline const NPCRenderData& EntityDataManager::getNPCRenderDataByTypeIndex(uint32_t typeLocalIndex) const {
    assert(typeLocalIndex < m_npcRenderData.size() && "NPC render data type index out of bounds");
    return m_npcRenderData[typeLocalIndex];
}

// Resource render data accessors - O(1) access by type index
inline ItemRenderData& EntityDataManager::getItemRenderDataByTypeIndex(uint32_t typeLocalIndex) {
    assert(typeLocalIndex < m_itemRenderData.size() && "Item render data type index out of bounds");
    return m_itemRenderData[typeLocalIndex];
}

inline const ItemRenderData& EntityDataManager::getItemRenderDataByTypeIndex(uint32_t typeLocalIndex) const {
    assert(typeLocalIndex < m_itemRenderData.size() && "Item render data type index out of bounds");
    return m_itemRenderData[typeLocalIndex];
}

inline ContainerRenderData& EntityDataManager::getContainerRenderDataByTypeIndex(uint32_t typeLocalIndex) {
    assert(typeLocalIndex < m_containerRenderData.size() && "Container render data type index out of bounds");
    return m_containerRenderData[typeLocalIndex];
}

inline const ContainerRenderData& EntityDataManager::getContainerRenderDataByTypeIndex(uint32_t typeLocalIndex) const {
    assert(typeLocalIndex < m_containerRenderData.size() && "Container render data type index out of bounds");
    return m_containerRenderData[typeLocalIndex];
}

// Projectile data accessors by type index - O(1) access for batch processing
inline ProjectileData& EntityDataManager::getProjectileData(uint32_t typeLocalIndex) {
    assert(typeLocalIndex < m_projectileData.size() && "Projectile type index out of bounds");
    return m_projectileData[typeLocalIndex];
}

inline const ProjectileData& EntityDataManager::getProjectileData(uint32_t typeLocalIndex) const {
    assert(typeLocalIndex < m_projectileData.size() && "Projectile type index out of bounds");
    return m_projectileData[typeLocalIndex];
}

// Container/Harvestable data accessors by type index - O(1) access for batch processing
inline ContainerData& EntityDataManager::getContainerData(uint32_t typeLocalIndex) {
    assert(typeLocalIndex < m_containerData.size() && "Container type index out of bounds");
    return m_containerData[typeLocalIndex];
}

inline const ContainerData& EntityDataManager::getContainerData(uint32_t typeLocalIndex) const {
    assert(typeLocalIndex < m_containerData.size() && "Container type index out of bounds");
    return m_containerData[typeLocalIndex];
}

inline HarvestableData& EntityDataManager::getHarvestableData(uint32_t typeLocalIndex) {
    assert(typeLocalIndex < m_harvestableData.size() && "Harvestable type index out of bounds");
    return m_harvestableData[typeLocalIndex];
}

inline const HarvestableData& EntityDataManager::getHarvestableData(uint32_t typeLocalIndex) const {
    assert(typeLocalIndex < m_harvestableData.size() && "Harvestable type index out of bounds");
    return m_harvestableData[typeLocalIndex];
}

#endif // ENTITY_DATA_MANAGER_HPP
