/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef BEHAVIOR_COMMON_STATE_HPP
#define BEHAVIOR_COMMON_STATE_HPP

#include "utils/Vector2D.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility> // IWYU pragma: keep - std::move; only transitively provided on libstdc++/Linux

/**
 * @brief Path state for AI entities (indexed by edmIndex)
 *
 * Stores pathfinding state for AI entities. Waypoints are stored in per-entity
 * FixedWaypointSlot for lock-free parallel writes with no contention.
 *
 * Threading: Safe for parallel reads during AI batch processing.
 * Each entity has its own waypoint slot - no shared state to contend on.
 */
struct PathData {
    uint16_t pathLength{0};             // Number of waypoints (max 32)
    uint16_t navIndex{0};               // Current waypoint index
    float pathUpdateTimer{0.0f};        // Time since last path update
    float progressTimer{0.0f};          // Time since last progress
    float lastNodeDistance{std::numeric_limits<float>::max()};
    float stallTimer{0.0f};             // Stall detection
    float pathRequestCooldown{0.0f};    // Prevent request spam
    Vector2D currentWaypoint{0, 0};     // Cached current waypoint for fast access
    bool hasPath{false};                // Quick check if path is valid
    std::atomic<uint8_t> pathRequestPending{0}; // Path request in flight (release/acquire)
    std::atomic<uint32_t> latestPathRequestId{0}; // Monotonic request token for stale-result filtering

    PathData() = default;
    PathData(const PathData&) = delete;
    PathData& operator=(const PathData&) = delete;
    PathData(PathData&& other) noexcept { *this = std::move(other); }
    PathData& operator=(PathData&& other) noexcept {
        if (this != &other) {
            pathLength = other.pathLength;
            navIndex = other.navIndex;
            pathUpdateTimer = other.pathUpdateTimer;
            progressTimer = other.progressTimer;
            lastNodeDistance = other.lastNodeDistance;
            stallTimer = other.stallTimer;
            pathRequestCooldown = other.pathRequestCooldown;
            currentWaypoint = other.currentWaypoint;
            hasPath = other.hasPath;
            pathRequestPending.store(
                other.pathRequestPending.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            latestPathRequestId.store(
                other.latestPathRequestId.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            other.pathRequestPending.store(0, std::memory_order_relaxed);
            other.latestPathRequestId.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    void clear() noexcept {
        pathLength = 0;
        navIndex = 0;
        pathUpdateTimer = 0.0f;
        progressTimer = 0.0f;
        lastNodeDistance = std::numeric_limits<float>::max();
        stallTimer = 0.0f;
        pathRequestCooldown = 0.0f;
        currentWaypoint = Vector2D{0, 0};
        hasPath = false;
        pathRequestPending.store(0, std::memory_order_relaxed);
        latestPathRequestId.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] bool isFollowingPath() const noexcept {
        return hasPath && navIndex < pathLength;
    }

    void advanceWaypoint() noexcept {
        if (navIndex < pathLength) {
            ++navIndex;
            progressTimer = 0.0f;
            stallTimer = 0.0f;
        }
    }

    [[nodiscard]] size_t size() const noexcept { return pathLength; }
};

// BehaviorType defined in ai/BehaviorConfig.hpp

/**
 * @brief Compact behavior-specific state (indexed by edmIndex like PathData)
 *
 * Uses tagged union - only ONE behavior can be active per entity at a time.
 * All pathfinding state is in PathData - this stores behavior-specific state only.
 *
 * Threading: Safe for parallel reads during AI batch processing.
 * Each thread accesses distinct edmIndex ranges.
 */
struct BehaviorData {
    // Common header (all behaviors) — shared state that persists across frame ticks.
    // Variant-specific state has moved to per-type dense pools (see m_*States in EDM).
    uint8_t flags{0};
    uint8_t _pad[3]{};

    // Cached from CharacterData at init (avoids typeLocalIndex indirection every frame)
    float moveSpeed{0.0f};  // 0 = uninitialized, set from CharacterData in initXxx()

    // Common separation state (used by most behaviors)
    float separationTimer{0.0f};
    Vector2D lastSepVelocity;

    // Common crowd analysis cache
    float lastCrowdAnalysis{0.0f};
    int cachedNearbyCount{0};
    Vector2D cachedClusterCenter;

    // Pending message queue (8 bytes: 4 messages max)
    // Each message: messageId (1 byte) + param encoded as uint8 (1 byte) = 2 bytes
    struct PendingMessage {
        uint8_t messageId{0};   // BehaviorMessage::* constant
        uint8_t param{0};       // Optional parameter (behavior-specific)
    };
    PendingMessage pendingMessages[4];
    uint8_t pendingMessageCount{0};
    uint8_t _msgPad[3]{};       // Padding for alignment

    static constexpr uint8_t FLAG_VALID = 0x01;
    static constexpr uint8_t FLAG_INITIALIZED = 0x02;

    // Default constructor
    BehaviorData() = default;

    void clear() noexcept {
        flags = 0;
        moveSpeed = 0.0f;
        separationTimer = 0.0f;
        lastSepVelocity = Vector2D{};
        lastCrowdAnalysis = 0.0f;
        cachedNearbyCount = 0;
        cachedClusterCenter = Vector2D{};
        pendingMessageCount = 0;
    }

    [[nodiscard]] bool isValid() const noexcept { return (flags & FLAG_VALID) != 0; }

    void setValid(bool v) noexcept {
        if (v) flags |= FLAG_VALID;
        else flags &= ~FLAG_VALID;
    }

    [[nodiscard]] bool isInitialized() const noexcept { return (flags & FLAG_INITIALIZED) != 0; }

    void setInitialized(bool v) noexcept {
        if (v) flags |= FLAG_INITIALIZED;
        else flags &= ~FLAG_INITIALIZED;
    }
};

// Slimmed BehaviorData: shared header only (~48 bytes). Variant state lives in
// per-type dense pools (m_idleStates, m_wanderStates, etc.) indexed by the same
// pool index as the config pools.
static_assert(sizeof(BehaviorData) <= 64, "BehaviorData shared header exceeds 64 bytes");

#endif // BEHAVIOR_COMMON_STATE_HPP
