/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"

namespace Behaviors {

void initFollow(size_t edmIndex, const VoidLight::FollowBehaviorConfig&,
                VoidLight::FollowStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Follow);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    auto& hotData = edm.getHotDataByIndex(edmIndex);

    state.lastTargetPosition = hotData.transform.position;
    state.currentVelocity = Vector2D(0, 0);
    state.desiredPosition = hotData.transform.position;
    state.formationOffset = Vector2D(0, 0);
    state.lastSepForce = Vector2D(0, 0);
    state.currentSpeed = 0.0f;
    state.currentHeading = 0.0f;
    state.backoffTimer = 0.0f;
    state.formationSlot = -1;
    state.isFollowing = false;
    state.targetMoving = false;
    state.inFormation = false;
    state.isStopped = true;

    shared.setInitialized(true);
}

void executeFollow(BehaviorContext& ctx, const VoidLight::FollowBehaviorConfig& config,
                   VoidLight::FollowStateData& follow) {
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

    // Throttle follow movement logic — responsive but not every frame
    follow.backoffTimer += ctx.deltaTime;
    if (follow.backoffTimer < config.updateInterval) return;
    float elapsed = follow.backoffTimer;
    follow.backoffTimer = 0.0f;

    // Get target (leader) position from memory
    Vector2D targetPos;
    bool targetValid = false;

    if (ctx.memoryData.lastTarget.isValid()) {
        auto& edm = EntityDataManager::Instance();
        size_t targetIdx = edm.getIndex(ctx.memoryData.lastTarget);
        if (targetIdx != SIZE_MAX) {
            const auto& targetHot = edm.getHotDataByIndex(targetIdx);
            if (targetHot.isAlive()) {
                targetPos = targetHot.transform.position;
                targetValid = true;
            }
        }
    }

    if (!targetValid) {
        follow.isFollowing = false;
        follow.isStopped = true;
        ctx.transform.velocity = Vector2D(0, 0);
        return;
    }

    Vector2D currentPos = ctx.transform.position;
    float distanceToTarget = (targetPos - currentPos).length();

    // Check if target is moving
    Vector2D targetMovement = targetPos - follow.lastTargetPosition;
    follow.targetMoving = targetMovement.lengthSquared() > 1.0f;
    follow.lastTargetPosition = targetPos;

    // Check if arrived
    if (distanceToTarget < config.arrivalRadius) {
        follow.isFollowing = false;
        follow.isStopped = true;
        ctx.transform.velocity = Vector2D(0, 0);
        return;
    }

    // Check if within follow distance
    if (distanceToTarget < config.followDistance) {
        if (!follow.targetMoving) {
            follow.isStopped = true;
            ctx.transform.velocity = Vector2D(0, 0);
            return;
        }
    }

    follow.isFollowing = true;
    follow.isStopped = false;

    // Calculate desired position (behind target)
    Vector2D toTarget = (targetPos - currentPos).normalized();
    Vector2D desiredPos = targetPos - toTarget * config.followDistance;
    follow.desiredPosition = desiredPos;

    // Use pathfinding for navigation
    if (ctx.pathData) {
        auto& pathData = *ctx.pathData;
        pathData.pathUpdateTimer += elapsed;
        if (pathData.pathRequestCooldown > 0.0f) {
            pathData.pathRequestCooldown -= elapsed;
        }

        bool needsPath = !pathData.hasPath || pathData.navIndex >= pathData.pathLength ||
                         pathData.pathUpdateTimer > config.pathTTL;

        // Check if target moved significantly
        if (!needsPath && pathData.hasPath) {
            auto& edm = EntityDataManager::Instance();
            Vector2D pathGoal = edm.getPathGoal(ctx.edmIndex);
            float goalChangeSq = (desiredPos - pathGoal).lengthSquared();
            needsPath = goalChangeSq > config.goalChangeThreshold * config.goalChangeThreshold;
        }

        if (needsPath && pathData.pathRequestCooldown <= 0.0f) {
            PathfinderManager::Instance().requestPathToEDM(ctx.edmIndex, currentPos, desiredPos,
                                                           PathfinderManager::Priority::Normal);
            pathData.pathRequestCooldown = config.pathCooldown;
        }

        if (pathData.isFollowingPath()) {
            Vector2D waypoint = ctx.pathData->currentWaypoint;
            Vector2D toWaypoint = waypoint - currentPos;
            float dist = toWaypoint.length();

            if (dist < config.nodeRadius) {
                auto& edm = EntityDataManager::Instance();
                edm.advanceWaypointWithCache(ctx.edmIndex);
                if (pathData.isFollowingPath()) {
                    waypoint = ctx.pathData->currentWaypoint;
                    toWaypoint = waypoint - currentPos;
                    dist = toWaypoint.length();
                }
            }

            if (dist > 0.001f) {
                Vector2D direction = toWaypoint / dist;
                float speed = shared.moveSpeed;

                // Speed adjustment based on distance
                if (distanceToTarget > config.catchupRange) {
                    speed *= config.catchupSpeedMultiplier;
                } else if (distanceToTarget < config.followDistance * 0.5f) {
                    speed *= config.slowdownSpeedMultiplier;
                }

                ctx.transform.velocity = direction * speed;
                follow.currentSpeed = speed;
            }
        } else {
            // Direct movement fallback
            Vector2D direction = (desiredPos - currentPos).normalized();
            ctx.transform.velocity = direction * shared.moveSpeed;
            follow.currentSpeed = shared.moveSpeed;
        }
    } else {
        // No pathData - direct movement
        Vector2D direction = (desiredPos - currentPos).normalized();
        ctx.transform.velocity = direction * shared.moveSpeed;
        follow.currentSpeed = shared.moveSpeed;
    }

    // Stall detection — uses shared separationTimer
    float speedSq = ctx.transform.velocity.lengthSquared();
    float stallThreshold = shared.moveSpeed * config.stallSpeedMultiplier;
    if (speedSq < stallThreshold * stallThreshold) {
        shared.separationTimer += config.updateInterval;
        if (shared.separationTimer > config.stallTimeout) {
            if (ctx.pathData) ctx.pathData->clear();
            shared.separationTimer = 0.0f;
        }
    } else {
        shared.separationTimer = 0.0f;
    }

    follow.currentVelocity = ctx.transform.velocity;
}

} // namespace Behaviors
