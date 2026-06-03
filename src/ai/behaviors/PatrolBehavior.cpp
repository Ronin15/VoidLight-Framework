/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "world/WorldData.hpp"
#include <cmath>
#include <random>

namespace {

thread_local std::mt19937 s_rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> s_angleDist{0.0f, 2.0f * static_cast<float>(M_PI)};
thread_local std::uniform_real_distribution<float> s_cooldownVariation{0.0f, 1.0f};

Vector2D generateRandomWaypoint(const Vector2D& currentPos, float boundaryPadding) {
    float minX, minY, maxX, maxY;
    if (!Behaviors::getCachedWorldBounds(minX, minY, maxX, maxY)) {
        float angle = s_angleDist(s_rng);
        return currentPos + Vector2D(std::cos(angle), std::sin(angle)) * 200.0f;
    }

    constexpr float TILE = VoidLight::TILE_SIZE;
    float worldMinX = minX * TILE + boundaryPadding;
    float worldMinY = minY * TILE + boundaryPadding;
    float worldMaxX = maxX * TILE - boundaryPadding;
    float worldMaxY = maxY * TILE - boundaryPadding;
    if (worldMaxX < worldMinX) {
        worldMinX = (minX * TILE + maxX * TILE) * 0.5f;
        worldMaxX = worldMinX;
    }
    if (worldMaxY < worldMinY) {
        worldMinY = (minY * TILE + maxY * TILE) * 0.5f;
        worldMaxY = worldMinY;
    }

    std::uniform_real_distribution<float> xDist(worldMinX, worldMaxX);
    std::uniform_real_distribution<float> yDist(worldMinY, worldMaxY);

    return Vector2D(xDist(s_rng), yDist(s_rng));
}

bool isAtWaypoint(const Vector2D& position, const Vector2D& waypoint, float radius) {
    return (position - waypoint).lengthSquared() < radius * radius;
}

} // anonymous namespace

namespace Behaviors {

void initPatrol(size_t edmIndex, const VoidLight::PatrolBehaviorConfig& config,
                VoidLight::PatrolStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Patrol);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    auto& hotData = edm.getHotDataByIndex(edmIndex);

    state.currentPatrolIndex = 0;
    state.currentPatrolTarget = hotData.transform.position;
    state.patrolMoveTimer = 0.0f;
    state.assignedPosition = hotData.transform.position;
    state.patrolThrottleTimer = 0.0f;

    // Generate initial waypoints in the waypoint slot
    auto waypointSlot = edm.getWaypointSlot(edmIndex);
    for (size_t i = 0; i < 4; ++i) {
        waypointSlot[i] = generateRandomWaypoint(hotData.transform.position, config.boundaryPadding);
    }

    shared.setInitialized(true);
}

void executePatrol(BehaviorContext& ctx, const VoidLight::PatrolBehaviorConfig& config,
                   VoidLight::PatrolStateData& patrol) {
    if (!ctx.sharedState.isValid()) return;

    auto& shared = ctx.sharedState;

    // Process pending behavior messages
    for (uint8_t i = 0; i < shared.pendingMessageCount; ++i)
    {
        switch (shared.pendingMessages[i].messageId)
        {
            case BehaviorMessage::PANIC:
                shared.pendingMessageCount = 0;
                switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                return;
            case BehaviorMessage::CALM_DOWN:
                if (ctx.memoryData.isValid())
                {
                    ctx.memoryData.emotions.fear = std::max(0.0f, ctx.memoryData.emotions.fear - 0.5f);
                }
                break;
            case BehaviorMessage::RAISE_ALERT:
                if (ctx.memoryData.personality.bravery < 0.4f)
                {
                    shared.pendingMessageCount = 0;
                    switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                    return;
                }
                break;
            default: break;
        }
    }
    shared.pendingMessageCount = 0;

    // Combat reaction: brave+aggressive NPCs fight back, others flee
    if (isUnderRecentAttack(ctx, 2.0f)) {
        if (shouldRetaliate(ctx)) {
            switchBehavior(ctx.edmIndex, BehaviorType::Chase);
        } else {
            switchBehavior(ctx.edmIndex, BehaviorType::Flee);
        }
        return;
    }
    if (shouldFleeFromFear(ctx)) {
        switchBehavior(ctx.edmIndex, BehaviorType::Flee);
        return;
    }

    // Throttle patrol movement logic — peaceful walking between waypoints
    patrol.patrolThrottleTimer += ctx.deltaTime;
    if (patrol.patrolThrottleTimer < config.updateInterval) return;
    float elapsed = patrol.patrolThrottleTimer;
    patrol.patrolThrottleTimer = 0.0f;

    Vector2D currentPos = ctx.transform.position;
    auto& edm = EntityDataManager::Instance();

    // Get waypoints from slot
    auto waypointSlot = edm.getWaypointSlot(ctx.edmIndex);

    // Check if at current waypoint
    Vector2D currentWaypoint = waypointSlot[patrol.currentPatrolIndex % 4];
    if (isAtWaypoint(currentPos, currentWaypoint, config.waypointReachedRadius)) {
        patrol.patrolMoveTimer += elapsed;
        if (patrol.patrolMoveTimer >= config.waypointCooldown) {
            patrol.currentPatrolIndex = (patrol.currentPatrolIndex + 1) % 4;
            patrol.patrolMoveTimer = 0.0f;
            patrol.currentPatrolTarget = waypointSlot[patrol.currentPatrolIndex];
        }
        ctx.transform.velocity = Vector2D(0, 0);
        return;
    }

    // Move toward waypoint using pathfinding
    if (ctx.pathData) {
        auto& pathData = *ctx.pathData;
        pathData.pathUpdateTimer += elapsed;
        if (pathData.pathRequestCooldown > 0.0f) {
            pathData.pathRequestCooldown -= elapsed;
        }

        bool needsPath = !pathData.hasPath || pathData.navIndex >= pathData.pathLength ||
                         pathData.pathUpdateTimer > config.pathRequestCooldown;

        if (needsPath && pathData.pathRequestCooldown <= 0.0f) {
            PathfinderManager::Instance().requestPathToEDM(ctx.edmIndex, currentPos, currentWaypoint,
                                                           PathfinderManager::Priority::Normal);
            pathData.pathRequestCooldown = config.pathRequestCooldown +
                                           s_cooldownVariation(s_rng) * config.pathRequestCooldownVariation;
        }

        if (pathData.isFollowingPath()) {
            Vector2D waypoint = ctx.pathData->currentWaypoint;
            Vector2D toWaypoint = waypoint - currentPos;
            float dist = toWaypoint.length();

            if (dist < config.waypointReachedRadius) {
                edm.advanceWaypointWithCache(ctx.edmIndex);
                if (pathData.isFollowingPath()) {
                    waypoint = ctx.pathData->currentWaypoint;
                    toWaypoint = waypoint - currentPos;
                    dist = toWaypoint.length();
                }
            }

            if (dist > 0.001f) {
                Vector2D direction = toWaypoint / dist;
                ctx.transform.velocity = direction * shared.moveSpeed;
            }
        } else {
            // Direct movement fallback
            Vector2D direction = (currentWaypoint - currentPos).normalized();
            ctx.transform.velocity = direction * shared.moveSpeed;
        }
    } else {
        // No pathData - direct movement
        Vector2D direction = (currentWaypoint - currentPos).normalized();
        ctx.transform.velocity = direction * shared.moveSpeed;
    }

    // Cautious movement when suspicious
    if (isOnAlert(ctx)) {
        ctx.transform.velocity = ctx.transform.velocity * 0.7f;
    }

    // Stall detection — uses shared separationTimer (persists across frames)
    float speedSq = ctx.transform.velocity.lengthSquared();
    float stallThreshold = shared.moveSpeed * config.stallSpeedMultiplier;
    if (speedSq < stallThreshold * stallThreshold) {
        shared.separationTimer += config.updateInterval;
        if (shared.separationTimer > config.advanceWaypointDelay) {
            // Skip to next waypoint
            patrol.currentPatrolIndex = (patrol.currentPatrolIndex + 1) % 4;
            patrol.currentPatrolTarget = waypointSlot[patrol.currentPatrolIndex];
            shared.separationTimer = 0.0f;
            if (ctx.pathData) ctx.pathData->clear();
        }
    } else {
        shared.separationTimer = 0.0f;
    }
}

} // namespace Behaviors
