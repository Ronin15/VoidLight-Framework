/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "ai/internal/Crowd.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include <chrono>
#include <numeric>
#include <random>

namespace {

thread_local std::mt19937 s_rng{static_cast<unsigned>(
    std::chrono::steady_clock::now().time_since_epoch().count())};


void updateCooldowns(VoidLight::ChaseStateData& chase, float deltaTime) {
    if (chase.pathRequestCooldown > 0.0f) chase.pathRequestCooldown -= deltaTime;
    if (chase.stallRecoveryCooldown > 0.0f) chase.stallRecoveryCooldown -= deltaTime;
    if (chase.behaviorChangeCooldown > 0.0f) chase.behaviorChangeCooldown -= deltaTime;
}

bool canRequestPath(const VoidLight::ChaseStateData& chase) {
    return chase.pathRequestCooldown <= 0.0f && chase.stallRecoveryCooldown <= 0.0f;
}

void applyPathCooldown(VoidLight::ChaseStateData& chase, float cooldownSeconds) {
    chase.pathRequestCooldown = cooldownSeconds;
}

} // anonymous namespace

namespace Behaviors {

void initChase(size_t edmIndex, const VoidLight::ChaseBehaviorConfig&,
               VoidLight::ChaseStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Chase);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    state.isChasing = false;
    state.hasLineOfSight = false;
    state.timeWithoutSight = 0.0f;
    state.pathRequestCooldown = 0.0f;
    state.stallRecoveryCooldown = 0.0f;
    state.behaviorChangeCooldown = 0.0f;
    state.crowdCheckTimer = 0.0f;
    state.cachedChaserCount = 0;
    state.recalcCounter = 0;
    state.stallPositionVariance = 0.0f;
    state.unstickTimer = 0.0f;

    const auto& hotData = edm.getHotDataByIndex(edmIndex);
    state.lastKnownTargetPos = hotData.transform.position;
    state.currentDirection = Vector2D{0, 0};
    state.lastStallPosition = Vector2D{0, 0};
    state.hasExplicitTarget = false;
    state.explicitTarget = EntityHandle{};

    shared.setInitialized(true);
}

void executeChase(BehaviorContext& ctx, const VoidLight::ChaseBehaviorConfig& config,
                  VoidLight::ChaseStateData& chase) {
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
            case BehaviorMessage::ATTACK_TARGET:
                // Redirect chase to new target from memory
                if (ctx.memoryData.lastAttacker.isValid())
                {
                    chase.hasExplicitTarget = true;
                    chase.explicitTarget = ctx.memoryData.lastAttacker;
                }
                break;
            case BehaviorMessage::RETREAT:
                shared.pendingMessageCount = 0;
                switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                return;
            default: break;
        }
    }
    shared.pendingMessageCount = 0;

    // Emotional modulation: fearful NPCs break off chase
    if (ctx.memoryData.isValid()) {
        float fear = ctx.memoryData.emotions.fear;
        float bravery = ctx.memoryData.personality.bravery;
        if (fear > 0.7f && bravery < 0.3f) {
            switchBehavior(ctx.edmIndex, BehaviorType::Flee);
            return;
        }
    }

    // Aggression speed modifier (up to +20%)
    float emotionalSpeedMod = 1.0f;
    if (ctx.memoryData.isValid()) {
        emotionalSpeedMod = 1.0f + ctx.memoryData.emotions.aggression * 0.2f;
    }
    float chaseSpeed = config.speedMultiplier * emotionalSpeedMod;

    // Crowd analysis cache
    shared.lastCrowdAnalysis += ctx.deltaTime;
    float crowdCacheInterval = 3.0f + (static_cast<float>(ctx.entityId % 200) * 0.01f);
    if (shared.lastCrowdAnalysis >= crowdCacheInterval) {
        constexpr float kCrowdQueryRadius = 80.0f;
        auto& nearbyPositions = AIInternal::GetNearbyPositionBuffer();
        nearbyPositions.clear();
        shared.cachedNearbyCount = AIInternal::GetNearbyEntitiesWithPositions(
            ctx.entityId, ctx.transform.position, kCrowdQueryRadius, nearbyPositions);

        if (!nearbyPositions.empty()) {
            Vector2D sum = std::accumulate(nearbyPositions.begin(), nearbyPositions.end(), Vector2D{0, 0});
            shared.cachedClusterCenter = sum * (1.0f / static_cast<float>(nearbyPositions.size()));
        }
        shared.lastCrowdAnalysis = 0.0f;
    }

    Vector2D entityPos = ctx.transform.position;
    Vector2D targetPos;
    EntityHandle targetHandle{};
    bool targetValid = false;
    auto& edm = EntityDataManager::Instance();

    if (chase.hasExplicitTarget && chase.explicitTarget.isValid()) {
        size_t targetIdx = edm.getIndex(chase.explicitTarget);
        if (targetIdx != SIZE_MAX) {
            const auto& targetHot = edm.getHotDataByIndex(targetIdx);
            if (targetHot.isAlive()) {
                targetPos = targetHot.transform.position;
                targetHandle = chase.explicitTarget;
                targetValid = true;
            }
        }
        if (!targetValid) {
            chase.hasExplicitTarget = false;
            chase.explicitTarget = EntityHandle{};
        }
    }

    if (!targetValid && ctx.memoryData.lastAttacker.isValid()) {
        size_t attackerIdx = edm.getIndex(ctx.memoryData.lastAttacker);
        if (attackerIdx != SIZE_MAX) {
            const auto& attackerHot = edm.getHotDataByIndex(attackerIdx);
            if (attackerHot.isAlive()) {
                targetPos = attackerHot.transform.position;
                targetHandle = ctx.memoryData.lastAttacker;
                targetValid = true;
            }
        }
    }

    if (!targetValid && ctx.memoryData.lastTarget.isValid()) {
        size_t targetIdx = edm.getIndex(ctx.memoryData.lastTarget);
        if (targetIdx != SIZE_MAX) {
            const auto& targetHot = edm.getHotDataByIndex(targetIdx);
            if (targetHot.isAlive()) {
                targetPos = targetHot.transform.position;
                targetHandle = ctx.memoryData.lastTarget;
                targetValid = true;
            }
        }
    }

    if (!targetValid) {
        if (chase.isChasing) {
            ctx.transform.velocity = Vector2D(0, 0);
            chase.isChasing = false;
            chase.hasLineOfSight = false;
            switchBehavior(ctx.edmIndex, BehaviorType::Idle);
        }
        return;
    }

    float distanceSquared = (targetPos - entityPos).lengthSquared();

    // Caught target — transition to Attack
    float catchRadiusSq = config.catchRadius * config.catchRadius;
    if (distanceSquared <= catchRadiusSq) {
        if (targetHandle.isValid()) {
            ctx.memoryData.lastTarget = targetHandle;
        }
        switchBehavior(ctx.edmIndex, BehaviorType::Attack);
        return;
    }
    float maxRangeSquared = config.maxChaseRange * config.maxChaseRange;
    float minRangeSquared = config.minChaseRange * config.minChaseRange;

    updateCooldowns(chase, ctx.deltaTime);

    if (distanceSquared <= maxRangeSquared) {
        chase.hasLineOfSight = true;
        chase.lastKnownTargetPos = targetPos;

        if (distanceSquared > minRangeSquared && ctx.pathData) {
            auto& pathData = *ctx.pathData;
            pathData.pathUpdateTimer += ctx.deltaTime;

            const bool skipRefresh = (pathData.pathRequestCooldown > 0.0f && pathData.isFollowingPath() &&
                                      pathData.progressTimer < 0.8f);
            bool needsNewPath = false;

            if (!skipRefresh) {
                if (!pathData.hasPath || pathData.navIndex >= pathData.pathLength) {
                    needsNewPath = true;
                } else if (pathData.pathUpdateTimer > config.pathRefreshInterval) {
                    needsNewPath = true;
                } else {
                    Vector2D pathGoal = edm.getPathGoal(ctx.edmIndex);
                    float targetMovementSquared = (targetPos - pathGoal).lengthSquared();
                    needsNewPath = (targetMovementSquared >
                                   config.pathInvalidationDistance * config.pathInvalidationDistance);
                }
            }

            bool stuckOnObstacle = (pathData.progressTimer > 3.0f);
            if (stuckOnObstacle) pathData.clear();

            if ((needsNewPath || stuckOnObstacle) && canRequestPath(chase)) {
                float minRangeCheckSquared = (config.minChaseRange * 1.5f) * (config.minChaseRange * 1.5f);
                if (distanceSquared < minRangeCheckSquared) {
                    ctx.transform.velocity = Vector2D(0, 0);
                    return;
                }

                PathfinderManager::Instance().requestPathToEDM(ctx.edmIndex, entityPos, targetPos,
                                                               PathfinderManager::Priority::High);
                applyPathCooldown(chase, config.pathRequestCooldown);
            }

            if (pathData.isFollowingPath()) {
                Vector2D waypoint = ctx.pathData->currentWaypoint;
                Vector2D toWaypoint = waypoint - entityPos;
                float dist = toWaypoint.length();

                if (dist < config.navRadius) {
                    edm.advanceWaypointWithCache(ctx.edmIndex);
                    if (pathData.isFollowingPath()) {
                        waypoint = ctx.pathData->currentWaypoint;
                        toWaypoint = waypoint - entityPos;
                        dist = toWaypoint.length();
                    }
                }

                bool following = (pathData.isFollowingPath() && dist > 0.001f);

                if (following) {
                    Vector2D direction = toWaypoint / dist;
                    ctx.transform.velocity = direction * shared.moveSpeed * chaseSpeed;
                    pathData.progressTimer = 0.0f;

                    chase.crowdCheckTimer += ctx.deltaTime;
                    if (chase.crowdCheckTimer >= config.crowdCheckInterval) {
                        chase.crowdCheckTimer = 0.0f;
                    }

                    if (chase.cachedChaserCount > 3) {
                        Vector2D lateral(-direction.getY(), direction.getX());
                        float lateralBias = ((float)(ctx.entityId % 3) - 1.0f) * 15.0f;
                        Vector2D adjustedTarget = targetPos + lateral * lateralBias;
                        Vector2D diff = adjustedTarget - entityPos;
                        float diffLenSq = diff.lengthSquared();
                        if (diffLenSq > 0.001f) {
                            ctx.transform.velocity = diff * ((shared.moveSpeed * chaseSpeed) / std::sqrt(diffLenSq));
                        }
                    }
                } else {
                    Vector2D direction = (targetPos - entityPos).normalized();
                    ctx.transform.velocity = direction * shared.moveSpeed * chaseSpeed;
                    pathData.progressTimer = 0.0f;
                }
            } else {
                Vector2D direction = (targetPos - entityPos).normalized();
                int nearbyCount = shared.cachedNearbyCount;

                if (nearbyCount > 1) {
                    Vector2D lateral(-direction.getY(), direction.getX());
                    float offset = ((float)(ctx.entityId % 3) - 1.0f) * 20.0f;
                    direction = direction + lateral * (offset / 400.0f);
                    direction.normalize();
                }

                ctx.transform.velocity = direction * shared.moveSpeed * chaseSpeed;
                pathData.progressTimer = 0.0f;
            }

            chase.isChasing = true;

            // Stall detection
            float currentSpeedSq = ctx.transform.velocity.lengthSquared();
            float stallThreshold = std::max(1.0f, shared.moveSpeed * chaseSpeed * config.stallSpeedMultiplier);
            float stallThresholdSq = stallThreshold * stallThreshold;
            float stallTimeLimit = config.stallTimeout;

            if (currentSpeedSq < stallThresholdSq) {
                pathData.stallTimer += ctx.deltaTime;
                if (pathData.stallTimer >= stallTimeLimit) {
                    pathData.clear();
                    pathData.stallTimer = 0.0f;

                    std::uniform_real_distribution<float> jitterDist(-0.1f, 0.1f);
                    float jitter = jitterDist(s_rng);
                    Vector2D dir = (targetPos - entityPos).normalized();
                    float c = std::cos(jitter), s = std::sin(jitter);
                    Vector2D rotated(dir.getX() * c - dir.getY() * s, dir.getX() * s + dir.getY() * c);
                    ctx.transform.velocity = rotated * shared.moveSpeed * chaseSpeed;
                }
            } else {
                pathData.stallTimer = 0.0f;
            }
        } else {
            if (chase.isChasing) {
                ctx.transform.velocity = Vector2D(0, 0);
                chase.isChasing = false;
            }
        }
    } else {
        if (chase.isChasing) {
            chase.isChasing = false;
            chase.hasLineOfSight = false;
            ctx.transform.velocity = Vector2D(0, 0);
            if (ctx.pathData) ctx.pathData->clear();
        }
    }
}

} // namespace Behaviors
