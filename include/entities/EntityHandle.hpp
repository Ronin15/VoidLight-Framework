/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef ENTITY_HANDLE_HPP
#define ENTITY_HANDLE_HPP

#include "utils/UniqueID.hpp"
#include <cstdint>
#include <format>
#include <functional>
#include <ostream>
#include <string>

/**
 * @brief Entity type enumeration for fast type checking without RTTI
 *
 * Use EntityHandle::getKind() for type filtering in hot paths.
 * Entity types are organized by category:
 * - Characters: Have health, AI behaviors, combat stats
 * - Interactables: World objects players interact with
 * - Combat: Physics-driven, typically short-lived
 * - Environment: Static or animated world objects
 */
enum class EntityKind : uint8_t {
    // Characters (have health, AI behaviors)
    Player = 0,
    NPC = 1,

    // Interactables (world objects)
    DroppedItem = 2,
    Container = 3,      // Chests, barrels, corpse loot
    Harvestable = 4,    // Trees, ore nodes, gathering spots

    // Combat (physics-driven, short-lived)
    Projectile = 5,
    AreaEffect = 6,     // AoE spell zones, traps

    // Environment (static or animated)
    Prop = 7,           // Decorations, animated objects
    Trigger = 8,        // Invisible trigger zones

    // World geometry (static collision bodies)
    StaticObstacle = 9, // Tiles, walls, terrain collision

    COUNT
};

/**
 * @brief Simulation tier for entity processing priority
 *
 * Determines how much processing an entity receives each frame.
 * Tier assignment is based on distance from camera/player.
 */
enum class SimulationTier : uint8_t {
    Active = 0,      // Full update: AI, collision, render (near camera)
    Background = 1,  // Simplified: position only, no collision (off-screen)
    Hibernated = 2   // Minimal: data stored, no updates (far away)
};

/**
 * @brief Type trait helpers for EntityKind
 */
namespace EntityTraits {

/// Returns true if this entity kind has health/combat stats
constexpr bool hasHealth(EntityKind kind) noexcept {
    return kind == EntityKind::Player || kind == EntityKind::NPC;
}

/// Returns true if this entity kind can have an inventory
constexpr bool hasInventory(EntityKind kind) noexcept {
    return kind == EntityKind::Player ||
           kind == EntityKind::NPC ||
           kind == EntityKind::Container;
}

/// Returns true if this entity kind participates in physics/collision
constexpr bool hasPhysics(EntityKind kind) noexcept {
    // Most types except Prop/Trigger, but StaticObstacle has collision
    return kind <= EntityKind::AreaEffect || kind == EntityKind::StaticObstacle;
}

/// Returns true if this entity kind has AI behaviors
constexpr bool hasAI(EntityKind kind) noexcept {
    return kind == EntityKind::NPC;
}

/// Returns true if this entity kind uses the static pool in EntityDataManager
/// Static pool entities: Resources that don't move and use immediate (not deferred) destruction
constexpr bool usesStaticPool(EntityKind kind) noexcept {
    return kind == EntityKind::DroppedItem ||
           kind == EntityKind::Container ||
           kind == EntityKind::Harvestable;
}

/// Returns true if this entity kind should be rendered
constexpr bool isRenderable(EntityKind kind) noexcept {
    return kind != EntityKind::Trigger;  // Only triggers are invisible
}

/// Returns string name for EntityKind (for debugging)
constexpr const char* kindToString(EntityKind kind) noexcept {
    switch (kind) {
        case EntityKind::Player:      return "Player";
        case EntityKind::NPC:         return "NPC";
        case EntityKind::DroppedItem: return "DroppedItem";
        case EntityKind::Container:   return "Container";
        case EntityKind::Harvestable: return "Harvestable";
        case EntityKind::Projectile:  return "Projectile";
        case EntityKind::AreaEffect:  return "AreaEffect";
        case EntityKind::Prop:           return "Prop";
        case EntityKind::Trigger:        return "Trigger";
        case EntityKind::StaticObstacle: return "StaticObstacle";
        default:                         return "Unknown";
    }
}

/// Returns string name for SimulationTier (for debugging)
constexpr const char* tierToString(SimulationTier tier) noexcept {
    switch (tier) {
        case SimulationTier::Active:     return "Active";
        case SimulationTier::Background: return "Background";
        case SimulationTier::Hibernated: return "Hibernated";
        default:                         return "Unknown";
    }
}

} // namespace EntityTraits

/**
 * @brief Lightweight handle for referencing entities in EntityDataManager
 *
 * EntityHandle is a 16-byte struct (8-byte aligned) that provides:
 * - Fast entity identification via EntityID
 * - Type information via EntityKind (no RTTI needed)
 * - Stale reference detection via generation counter
 *
 * Handles are the primary way to reference entities throughout the codebase.
 * They are cheap to copy and compare, making them suitable for containers
 * and passing by value.
 *
 * Usage:
 *   EntityHandle npc = EntityDataManager::Instance().createNPC(position);
 *   if (npc.isValid()) {
 *       auto& data = EntityDataManager::Instance().getNPCData(npc);
 *   }
 */
struct EntityHandle {
    // Type aliases matching existing codebase patterns
    using IDType = VoidLight::UniqueID::IDType;  // uint64_t
    using Generation = uint32_t;

    // Special values
    static constexpr IDType INVALID_ID = 0;
    static constexpr Generation INVALID_GENERATION = 0;

    // Handle components (16 bytes total, naturally aligned)
    IDType id{INVALID_ID};                      // 8 bytes: Unique entity identifier
    EntityKind kind{EntityKind::NPC};           // 1 byte: Entity type
    uint8_t padding[3]{};                       // 3 bytes: Alignment padding
    Generation generation{INVALID_GENERATION};  // 4 bytes: Stale reference detection

    // Default constructor creates invalid handle
    constexpr EntityHandle() noexcept = default;

    // Construct with all components
    constexpr EntityHandle(IDType entityId, EntityKind entityKind,
                          Generation gen) noexcept
        : id(entityId), kind(entityKind), padding{}, generation(gen) {}

    // Validity check
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return id != INVALID_ID && generation != INVALID_GENERATION;
    }

    // Accessors
    [[nodiscard]] constexpr IDType getId() const noexcept { return id; }
    [[nodiscard]] constexpr EntityKind getKind() const noexcept { return kind; }
    [[nodiscard]] constexpr Generation getGeneration() const noexcept {
        return generation;
    }

    // Type checking helpers
    [[nodiscard]] constexpr bool isPlayer() const noexcept {
        return kind == EntityKind::Player;
    }
    [[nodiscard]] constexpr bool isNPC() const noexcept {
        return kind == EntityKind::NPC;
    }
    [[nodiscard]] constexpr bool isItem() const noexcept {
        return kind == EntityKind::DroppedItem;
    }
    [[nodiscard]] constexpr bool isProjectile() const noexcept {
        return kind == EntityKind::Projectile;
    }

    // Trait helpers (delegate to EntityTraits)
    [[nodiscard]] constexpr bool hasHealth() const noexcept {
        return EntityTraits::hasHealth(kind);
    }
    [[nodiscard]] constexpr bool hasInventory() const noexcept {
        return EntityTraits::hasInventory(kind);
    }
    [[nodiscard]] constexpr bool hasPhysics() const noexcept {
        return EntityTraits::hasPhysics(kind);
    }
    [[nodiscard]] constexpr bool hasAI() const noexcept {
        return EntityTraits::hasAI(kind);
    }

    // Comparison operators
    [[nodiscard]] constexpr bool
    operator==(const EntityHandle& other) const noexcept {
        return id == other.id && generation == other.generation && kind == other.kind;
    }

    [[nodiscard]] constexpr bool
    operator!=(const EntityHandle& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool
    operator<(const EntityHandle& other) const noexcept {
        if (id != other.id) return id < other.id;
        if (generation != other.generation) return generation < other.generation;
        return static_cast<uint8_t>(kind) < static_cast<uint8_t>(other.kind);
    }

    // Hash support for containers
    [[nodiscard]] std::size_t hash() const noexcept {
        // Combine id, kind, and generation into hash
        std::size_t h = static_cast<std::size_t>(id);
        h ^= static_cast<std::size_t>(kind) << 48;
        h ^= static_cast<std::size_t>(generation) << 32;
        return h;
    }

    // String conversion for debugging
    [[nodiscard]] std::string toString() const {
        if (!isValid()) {
            return "EntityHandle::INVALID";
        }
        return std::format("EntityHandle({}:{}:{})",
                          id, EntityTraits::kindToString(kind), generation);
    }
};

// Static assertion to verify handle size
// Layout: id (8) + kind (1) + padding (3) + generation (4) = 16 bytes
// generation widened to uint32_t for safer slot-reuse headroom (see commit c4a29432).
static_assert(sizeof(EntityHandle) == 16, "EntityHandle should be 16 bytes (8-byte aligned)");

/**
 * @brief Invalid handle constant
 */
inline constexpr EntityHandle INVALID_ENTITY_HANDLE{};

// Stream output operator for debugging and logging
inline std::ostream& operator<<(std::ostream& os, const EntityHandle& handle) {
    return os << handle.toString();
}

// Hash function for std::unordered_map support
namespace std {
template <>
struct hash<EntityHandle> {
    std::size_t operator()(const EntityHandle& handle) const noexcept {
        return handle.hash();
    }
};
} // namespace std

#endif // ENTITY_HANDLE_HPP
