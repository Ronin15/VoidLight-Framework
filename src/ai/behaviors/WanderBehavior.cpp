/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

thread_local std::mt19937 s_rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> s_angleDistribution{0.0f, 2.0f * static_cast<float>(M_PI)};
thread_local std::uniform_int_distribution<uint64_t> s_delayDistribution{0, 5000};

void updateTimers(VoidLight::WanderStateData& wander, float deltaTime, PathData* pathData) {
    wander.directionChangeTimer += deltaTime;
    wander.lastDirectionFlip += deltaTime;
    wander.stallTimer += deltaTime;
    wander.unstickTimer += deltaTime;

    if (pathData) {
        pathData->pathUpdateTimer += deltaTime;
        pathData->progressTimer += deltaTime;
        if (pathData->pathRequestCooldown > 0.0f) {
            pathData->pathRequestCooldown -= deltaTime;
        }
    }
}

bool handleStartDelay(BehaviorContext& ctx, VoidLight::WanderStateData& wander) {
    if (wander.movementStarted) return true;

    if (wander.directionChangeTimer < wander.startDelay) return false;

    wander.movementStarted = true;
    ctx.transform.velocity = wander.currentDirection * ctx.sharedState.moveSpeed;
    return true;
}

float calculateMoveDistance(BehaviorData& shared, VoidLight::WanderStateData& wander,
                            const Vector2D& position, float baseDistance,
                            const VoidLight::WanderBehaviorConfig& config) {
    int nearbyCount = shared.cachedNearbyCount;
    float moveDistance = baseDistance;

    if (nearbyCount > config.crowdEscapeThreshold) {
        moveDistance = baseDistance * config.crowdEscapeDistanceMultiplier;
        if (nearbyCount > 0) {
            Vector2D escapeDirection = (position - shared.cachedClusterCenter).normalized();
            float randomOffset = (nearbyCount % 60 - 30) * 0.01f;
            escapeDirection.setX(escapeDirection.getX() + randomOffset);
            escapeDirection.setY(escapeDirection.getY() + randomOffset);
            wander.currentDirection = escapeDirection;
        }
    } else if (nearbyCount > 5) {
        moveDistance = baseDistance * 2.0f;
    } else if (nearbyCount > 2) {
        moveDistance = baseDistance * 1.3f;
    }

    return moveDistance;
}

void applyBoundaryAvoidance(VoidLight::WanderStateData& wander, const Vector2D& position,
                            const VoidLight::WanderBehaviorConfig& config,
                            const BehaviorContext& ctx) {
    if (!ctx.worldBoundsValid) {
        return;
    }

    const float EDGE_THRESHOLD = config.edgeThreshold;
    Vector2D boundaryForce(0, 0);

    if (position.getX() < ctx.worldMinX + EDGE_THRESHOLD) {
        float strength = 1.0f - ((position.getX() - ctx.worldMinX) / EDGE_THRESHOLD);
        boundaryForce = boundaryForce + Vector2D(strength, 0);
    } else if (position.getX() > ctx.worldMaxX - EDGE_THRESHOLD) {
        float strength = 1.0f - ((ctx.worldMaxX - position.getX()) / EDGE_THRESHOLD);
        boundaryForce = boundaryForce + Vector2D(-strength, 0);
    }

    if (position.getY() < ctx.worldMinY + EDGE_THRESHOLD) {
        float strength = 1.0f - ((position.getY() - ctx.worldMinY) / EDGE_THRESHOLD);
        boundaryForce = boundaryForce + Vector2D(0, strength);
    } else if (position.getY() > ctx.worldMaxY - EDGE_THRESHOLD) {
        float strength = 1.0f - ((ctx.worldMaxY - position.getY()) / EDGE_THRESHOLD);
        boundaryForce = boundaryForce + Vector2D(0, -strength);
    }

    if (boundaryForce.lengthSquared() > 0.01f) {
        Vector2D blended = wander.currentDirection * 0.4f + boundaryForce * 0.6f;
        float lenSq = blended.lengthSquared();
        if (lenSq > 0.0001f) {
            wander.currentDirection = blended * (1.0f / std::sqrt(lenSq));
        }
    }
}

void handlePathfinding(const BehaviorContext& ctx, const Vector2D& dest,
                       const VoidLight::WanderBehaviorConfig& config) {
    Vector2D position = ctx.transform.position;
    float distanceToGoalSq = (dest - position).lengthSquared();
    if (distanceToGoalSq < 64.0f * 64.0f || !ctx.pathData) return;

    auto& pathData = *ctx.pathData;
    const bool skipRefresh = (pathData.pathRequestCooldown > 0.0f && pathData.isFollowingPath() &&
                              pathData.progressTimer < 0.8f);
    bool needsNewPath = false;
    if (!skipRefresh) {
        needsNewPath = !pathData.hasPath || pathData.navIndex >= pathData.pathLength ||
                       pathData.pathUpdateTimer > config.pathRefreshInterval;
    }

    bool stuckOnObstacle = pathData.progressTimer > 0.8f;
    if (stuckOnObstacle) pathData.clear();

    if ((needsNewPath || stuckOnObstacle) && pathData.pathRequestCooldown <= 0.0f) {
        const float MIN_GOAL_CHANGE = config.minGoalChangeDistance;
        bool goalChanged = true;
        auto& edm = EntityDataManager::Instance();
        if (!skipRefresh && pathData.hasPath && pathData.pathLength > 0) {
            Vector2D lastGoal = edm.getPathGoal(ctx.edmIndex);
            float goalDistanceSquared = (dest - lastGoal).lengthSquared();
            goalChanged = (goalDistanceSquared >= MIN_GOAL_CHANGE * MIN_GOAL_CHANGE);
        }

        if (goalChanged) {
            PathfinderManager::Instance().requestPathToEDM(ctx.edmIndex, position, dest,
                                                           PathfinderManager::Priority::Normal);
            pathData.pathRequestCooldown = config.pathRequestCooldown;
        }
    }
}

void chooseNewDirection(BehaviorContext& ctx, VoidLight::WanderStateData& wander,
                        const VoidLight::WanderBehaviorConfig&) {
    float angle = s_angleDistribution(s_rng);
    wander.currentDirection = Vector2D(std::cos(angle), std::sin(angle));
    if (wander.movementStarted) {
        ctx.transform.velocity = wander.currentDirection * ctx.sharedState.moveSpeed;
    }
}

void handleMovement(BehaviorContext& ctx, VoidLight::WanderStateData& wander,
                    const VoidLight::WanderBehaviorConfig& config) {
    auto& shared = ctx.sharedState;
    float baseDistance = config.baseGoalDistance;
    Vector2D position = ctx.transform.position;

    float moveDistance = calculateMoveDistance(shared, wander, position, baseDistance, config);
    applyBoundaryAvoidance(wander, position, config, ctx);

    Vector2D dest = position + wander.currentDirection * moveDistance;

    if (ctx.worldBoundsValid) {
        const float MARGIN = config.worldPaddingMargin;
        float minX = ctx.worldMinX + MARGIN;
        float maxX = ctx.worldMaxX - MARGIN;
        float minY = ctx.worldMinY + MARGIN;
        float maxY = ctx.worldMaxY - MARGIN;

        if (maxX < minX) {
            minX = (ctx.worldMinX + ctx.worldMaxX) * 0.5f;
            maxX = minX;
        }
        if (maxY < minY) {
            minY = (ctx.worldMinY + ctx.worldMaxY) * 0.5f;
            maxY = minY;
        }

        dest.setX(std::clamp(dest.getX(), minX, maxX));
        dest.setY(std::clamp(dest.getY(), minY, maxY));
    }

    handlePathfinding(ctx, dest, config);

    if (!ctx.pathData) {
        ctx.transform.velocity = wander.currentDirection * shared.moveSpeed;
        return;
    }
    auto& pathData = *ctx.pathData;

    if (pathData.isFollowingPath()) {
        Vector2D waypoint = ctx.pathData->currentWaypoint;
        Vector2D toWaypoint = waypoint - position;
        float dist = toWaypoint.length();

        if (dist < 64.0f) {
            auto& edm = EntityDataManager::Instance();
            edm.advanceWaypointWithCache(ctx.edmIndex);
            if (pathData.isFollowingPath()) {
                waypoint = ctx.pathData->currentWaypoint;
                toWaypoint = waypoint - position;
                dist = toWaypoint.length();
            }
        }

        if (dist > 0.001f) {
            Vector2D direction = toWaypoint / dist;
            ctx.transform.velocity = direction * shared.moveSpeed;
        }
    } else {
        ctx.transform.velocity = wander.currentDirection * shared.moveSpeed;
    }

    float speedSq = ctx.transform.velocity.lengthSquared();
    const float stallSpeed = std::max(config.stallSpeed, shared.moveSpeed * 0.5f);
    const float stallSpeedSq = stallSpeed * stallSpeed;
    const float stallSeconds = config.stallTimeout;

    if (speedSq < stallSpeedSq) {
        if (wander.stallTimer >= stallSeconds) {
            pathData.clear();
            chooseNewDirection(ctx, wander, config);
            pathData.pathRequestCooldown = 0.6f;
            wander.stallTimer = 0.0f;
            return;
        }
    } else {
        wander.stallTimer = 0.0f;
    }

    float changeIntervalSeconds = config.changeDirectionIntervalMin / 1000.0f;
    if (wander.directionChangeTimer >= changeIntervalSeconds) {
        chooseNewDirection(ctx, wander, config);
        wander.directionChangeTimer = 0.0f;
    }

    const float jitterThresholdSq = (shared.moveSpeed * config.jitterThresholdMultiplier) *
                                    (shared.moveSpeed * config.jitterThresholdMultiplier);
    if (speedSq < jitterThresholdSq && speedSq >= stallSpeedSq) {
        float jitter = (s_angleDistribution(s_rng) - static_cast<float>(M_PI)) * 0.1f;
        Vector2D dir = wander.currentDirection;
        float c = std::cos(jitter), s = std::sin(jitter);
        Vector2D rotated(dir.getX() * c - dir.getY() * s, dir.getX() * s + dir.getY() * c);
        if (rotated.lengthSquared() > 0.000001f) {
            rotated.normalize();
            wander.currentDirection = rotated;
            ctx.transform.velocity = wander.currentDirection * shared.moveSpeed;
        }
    }

    int nearbyCount = shared.cachedNearbyCount;
    if (nearbyCount > 5) {
        float slowdownMultiplier = (nearbyCount > 10) ? config.crowdSlowdownHeavy : config.crowdSlowdownLight;
        ctx.transform.velocity = ctx.transform.velocity * slowdownMultiplier;
    }

    wander.previousVelocity = ctx.transform.velocity;
}

} // anonymous namespace

namespace Behaviors {

void initWander(size_t edmIndex, const VoidLight::WanderBehaviorConfig&,
                VoidLight::WanderStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Wander);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    state.directionChangeTimer = 0.0f;
    state.lastDirectionFlip = 0.0f;
    state.startDelay = s_delayDistribution(s_rng) / 1000.0f;
    state.movementStarted = false;
    state.stallTimer = 0.0f;
    state.lastStallPosition = Vector2D{0, 0};
    state.stallPositionVariance = 0.0f;
    state.unstickTimer = 0.0f;
    state.movementUpdateTimer = 0.0f;

    float angle = s_angleDistribution(s_rng);
    state.currentDirection = Vector2D(std::cos(angle), std::sin(angle));
    state.previousVelocity = Vector2D{0, 0};

    shared.setInitialized(true);
}

void executeWander(BehaviorContext& ctx, const VoidLight::WanderBehaviorConfig& config,
                   VoidLight::WanderStateData& state) {
    auto& shared = ctx.sharedState;
    if (!shared.isValid()) return;

    // Process pending behavior messages
    for (uint8_t i = 0; i < shared.pendingMessageCount; ++i) {
        switch (shared.pendingMessages[i].messageId) {
            case BehaviorMessage::PANIC:
                shared.pendingMessageCount = 0;
                switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                return;
            case BehaviorMessage::CALM_DOWN:
                if (ctx.memoryData.isValid()) {
                    ctx.memoryData.emotions.fear = std::max(0.0f, ctx.memoryData.emotions.fear - 0.5f);
                }
                break;
            case BehaviorMessage::RAISE_ALERT:
                if (ctx.memoryData.personality.bravery < 0.4f) {
                    shared.pendingMessageCount = 0;
                    switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                    return;
                }
                break;
            default: break;
        }
    }
    shared.pendingMessageCount = 0;

    // Combat reaction: recent combat memory drives self-preservation/retaliation.
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

    updateTimers(state, ctx.deltaTime, ctx.pathData);

    if (!handleStartDelay(ctx, state)) return;

    if (state.movementStarted) {
        // Throttle heavy movement logic — wanderers are peaceful, just coast
        // on current velocity between updates
        state.movementUpdateTimer += ctx.deltaTime;
        if (state.movementUpdateTimer >= config.updateInterval) {
            state.movementUpdateTimer = 0.0f;
            handleMovement(ctx, state, config);
        }

        // Cautious movement when suspicious
        if (isOnAlert(ctx)) {
            ctx.transform.velocity = ctx.transform.velocity * 0.7f;
        }
    }
}

} // namespace Behaviors
