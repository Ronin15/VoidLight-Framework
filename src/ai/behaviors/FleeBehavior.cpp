/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "ai/internal/Crowd.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

thread_local std::mt19937 s_rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> s_angleVariation{-0.5f, 0.5f};
thread_local std::uniform_real_distribution<float> s_panicVariation{0.8f, 1.2f};

constexpr size_t MAX_SAFE_ZONES = 4;
constexpr float CROWD_ANALYSIS_INTERVAL = 0.25f;

void processFleeMessages(BehaviorData& shared, VoidLight::FleeStateData& flee,
                         const VoidLight::FleeBehaviorConfig& config) {
    for (uint8_t i = 0; i < shared.pendingMessageCount; ++i) {
        uint8_t msgId = shared.pendingMessages[i].messageId;

        switch (msgId) {
            case BehaviorMessage::PANIC:
                flee.isInPanic = true;
                flee.panicTimer = config.panicDuration;
                break;

            case BehaviorMessage::CALM_DOWN:
                flee.isInPanic = false;
                flee.panicTimer = 0.0f;
                break;

            default:
                break;
        }
    }
    shared.pendingMessageCount = 0;
}

Vector2D normalizeVector(const Vector2D& direction) {
    float magnitude = direction.length();
    if (magnitude < 0.001f) return Vector2D(1, 0);
    return direction / magnitude;
}

Vector2D findNearestSafeZone(const BehaviorContext& ctx, const VoidLight::FleeStateData& flee) {
    if (flee.safeZoneCount == 0) return Vector2D{0, 0};

    Vector2D currentPos = ctx.transform.position;
    Vector2D nearest{0, 0};
    float minDistSq = std::numeric_limits<float>::max();

    for (uint8_t i = 0; i < flee.safeZoneCount && i < MAX_SAFE_ZONES; ++i) {
        Vector2D toZone = flee.safeZoneCenters[i] - currentPos;
        float distSq = toZone.lengthSquared();
        if (distSq < minDistSq) {
            minDistSq = distSq;
            nearest = flee.safeZoneCenters[i];
        }
    }

    return nearest;
}

Vector2D avoidBoundaries(const Vector2D& position, const Vector2D& direction, float padding,
                         float boundsMinX, float boundsMinY, float boundsMaxX, float boundsMaxY) {
    Vector2D adjustedDir = direction;
    float worldMinX = boundsMinX + padding;
    float worldMinY = boundsMinY + padding;
    float worldMaxX = boundsMaxX - padding;
    float worldMaxY = boundsMaxY - padding;

    if (position.getX() < worldMinX && direction.getX() < 0) {
        adjustedDir.setX(std::abs(direction.getX()));
    } else if (position.getX() > worldMaxX && direction.getX() > 0) {
        adjustedDir.setX(-std::abs(direction.getX()));
    }

    if (position.getY() < worldMinY && direction.getY() < 0) {
        adjustedDir.setY(std::abs(direction.getY()));
    } else if (position.getY() > worldMaxY && direction.getY() > 0) {
        adjustedDir.setY(-std::abs(direction.getY()));
    }

    return adjustedDir;
}

Vector2D calculateFleeDirection(const Vector2D& entityPos, const Vector2D& threatPos,
                                const VoidLight::FleeStateData& flee, float padding,
                                float boundsMinX, float boundsMinY,
                                float boundsMaxX, float boundsMaxY) {
    Vector2D fleeDir = entityPos - threatPos;

    if (fleeDir.length() < 0.001f) {
        if (flee.fleeDirection.length() > 0.001f) {
            fleeDir = flee.fleeDirection;
        } else {
            float angle = s_angleVariation(s_rng) * 2.0f * static_cast<float>(M_PI);
            fleeDir = Vector2D(std::cos(angle), std::sin(angle));
        }
    }

    fleeDir = avoidBoundaries(entityPos, fleeDir, padding, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY);
    return normalizeVector(fleeDir);
}

float calculateFleeSpeedModifier(const VoidLight::FleeStateData& flee,
                                 const VoidLight::FleeBehaviorConfig& config) {
    float modifier = 1.0f;

    if (flee.isInPanic) modifier *= config.panicSpeedMultiplier;
    modifier *= (1.0f + flee.fearBoost * 0.4f);

    if (config.useStamina) {
        float staminaRatio = flee.currentStamina / config.maxStamina;
        modifier *= (0.3f + 0.7f * staminaRatio);
    }

    return modifier;
}

void updateStamina(VoidLight::FleeStateData& flee, float deltaTime, bool fleeing,
                   const VoidLight::FleeBehaviorConfig& config) {
    if (fleeing) {
        flee.currentStamina -= config.staminaDrain * deltaTime;
        flee.currentStamina = std::max(0.0f, flee.currentStamina);
    } else {
        flee.currentStamina += config.staminaRecovery * deltaTime;
        flee.currentStamina = std::min(config.maxStamina, flee.currentStamina);
    }
}

bool tryFollowPathToGoal(BehaviorContext& ctx, const VoidLight::FleeStateData& flee,
                         const Vector2D& goal, float speed,
                         const VoidLight::FleeBehaviorConfig& config) {
    if (!ctx.pathData) return false;

    auto& pathData = *ctx.pathData;
    Vector2D currentPos = ctx.transform.position;

    const float pathTTL = config.pathTTL;
    const float noProgressWindow = config.noProgressWindow;
    const float GOAL_CHANGE_THRESH_SQUARED = config.goalChangeThreshold * config.goalChangeThreshold;

    const bool skipRefresh = (pathData.pathRequestCooldown > 0.0f && pathData.isFollowingPath() &&
                              pathData.progressTimer < noProgressWindow);
    auto& edm = EntityDataManager::Instance();
    bool needRefresh = !pathData.hasPath || pathData.navIndex >= pathData.pathLength;

    if (!skipRefresh && !needRefresh && pathData.isFollowingPath()) {
        float d = (edm.getWaypoint(ctx.edmIndex, pathData.navIndex) - currentPos).length();
        if (d + 1.0f < pathData.lastNodeDistance) {
            pathData.lastNodeDistance = d;
            pathData.progressTimer = 0.0f;
        } else if (pathData.progressTimer > noProgressWindow) {
            needRefresh = true;
        }
    }

    if (!skipRefresh && pathData.pathUpdateTimer > pathTTL) needRefresh = true;

    if (needRefresh && pathData.pathRequestCooldown <= 0.0f) {
        bool goalChanged = true;
        if (!skipRefresh && pathData.hasPath && pathData.pathLength > 0) {
            Vector2D lastGoal = edm.getPathGoal(ctx.edmIndex);
            goalChanged = ((goal - lastGoal).lengthSquared() > GOAL_CHANGE_THRESH_SQUARED);
        }

        if (goalChanged) {
            auto& pf = PathfinderManager::Instance();
            pf.requestPathToEDM(ctx.edmIndex, pf.clampToWorldBounds(currentPos, 100.0f), goal,
                               PathfinderManager::Priority::High);
            pathData.pathRequestCooldown = 0.8f;
        }
    }

    if (pathData.isFollowingPath()) {
        Vector2D node = ctx.pathData->currentWaypoint;
        Vector2D dir = node - currentPos;
        float len = dir.length();

        if (len > 0.01f) {
            dir = dir * (1.0f / len);
            ctx.transform.velocity = dir * speed;
        }

        if (len <= flee.navRadius) {
            edm.advanceWaypointWithCache(ctx.edmIndex);
        }
        return true;
    }

    return false;
}

void updatePanicFlee(BehaviorContext& ctx, VoidLight::FleeStateData& flee,
                     const Vector2D& threatPos,
                     const VoidLight::FleeBehaviorConfig& config) {
    auto& shared = ctx.sharedState;
    Vector2D currentPos = ctx.transform.position;

    if (flee.directionChangeTimer > 0.2f || flee.fleeDirection.length() < 0.001f) {
        flee.fleeDirection = calculateFleeDirection(currentPos, threatPos, flee, config.worldPadding,
                                           ctx.worldMinX, ctx.worldMinY, ctx.worldMaxX, ctx.worldMaxY);
        float randomAngle = s_angleVariation(s_rng) * 0.8f;
        float cos_a = std::cos(randomAngle);
        float sin_a = std::sin(randomAngle);
        Vector2D rotated(flee.fleeDirection.getX() * cos_a - flee.fleeDirection.getY() * sin_a,
                        flee.fleeDirection.getX() * sin_a + flee.fleeDirection.getY() * cos_a);
        flee.fleeDirection = rotated;
        flee.directionChangeTimer = 0.0f;
    }

    float speedModifier = calculateFleeSpeedModifier(flee, config);
    ctx.transform.velocity = flee.fleeDirection * shared.moveSpeed * config.baseFleeSpeedMultiplier * speedModifier;
}

void updateStrategicRetreat(BehaviorContext& ctx, VoidLight::FleeStateData& flee,
                            const Vector2D& threatPos,
                            const VoidLight::FleeBehaviorConfig& config) {
    auto& shared = ctx.sharedState;
    Vector2D currentPos = ctx.transform.position;

    if (flee.directionChangeTimer > 1.0f || flee.fleeDirection.length() < 0.001f) {
        flee.fleeDirection = calculateFleeDirection(currentPos, threatPos, flee, config.worldPadding,
                                           ctx.worldMinX, ctx.worldMinY, ctx.worldMaxX, ctx.worldMaxY);
        flee.directionChangeTimer = 0.0f;
    }

    int nearbyCount = shared.cachedNearbyCount;

    float baseRetreatDistance = config.strategicRetreatDistance;
    float retreatDistance = baseRetreatDistance;
    if (nearbyCount > 2) {
        retreatDistance = baseRetreatDistance * 1.8f;
        Vector2D lateral(-flee.fleeDirection.getY(), flee.fleeDirection.getX());
        float lateralBias = ((float)(ctx.entityId % 5) - 2.0f) * 0.3f;
        flee.fleeDirection = (flee.fleeDirection + lateral * lateralBias).normalized();
    } else if (nearbyCount > 0) {
        retreatDistance = baseRetreatDistance * 1.3f;
    }

    Vector2D dest = PathfinderManager::Instance().clampToWorldBounds(
        currentPos + flee.fleeDirection * retreatDistance, 100.0f);

    float speedModifier = calculateFleeSpeedModifier(flee, config) * config.strategicSpeedMultiplier;
    if (!tryFollowPathToGoal(ctx, flee, dest, shared.moveSpeed * config.baseFleeSpeedMultiplier * speedModifier, config)) {
        ctx.transform.velocity = flee.fleeDirection * shared.moveSpeed * config.baseFleeSpeedMultiplier * speedModifier;
    }
}

void updateEvasiveManeuver(BehaviorContext& ctx, VoidLight::FleeStateData& flee,
                           const Vector2D& threatPos,
                           const VoidLight::FleeBehaviorConfig& config) {
    auto& shared = ctx.sharedState;
    Vector2D currentPos = ctx.transform.position;

    if (flee.zigzagTimer > config.zigzagInterval) {
        flee.zigzagDirection *= -1;
        flee.zigzagTimer = 0.0f;
    }

    Vector2D baseFleeDir = calculateFleeDirection(currentPos, threatPos, flee, config.worldPadding,
                                           ctx.worldMinX, ctx.worldMinY, ctx.worldMaxX, ctx.worldMaxY);

    float zigzagAngleRad = (config.zigzagAngle * static_cast<float>(M_PI) / 180.0f) * flee.zigzagDirection;
    float cos_z = std::cos(zigzagAngleRad);
    float sin_z = std::sin(zigzagAngleRad);

    Vector2D zigzagDir(baseFleeDir.getX() * cos_z - baseFleeDir.getY() * sin_z,
                      baseFleeDir.getX() * sin_z + baseFleeDir.getY() * cos_z);

    flee.fleeDirection = normalizeVector(zigzagDir);

    float speedModifier = calculateFleeSpeedModifier(flee, config);
    ctx.transform.velocity = flee.fleeDirection * shared.moveSpeed * config.baseFleeSpeedMultiplier * speedModifier;
}

void updateSeekCover(BehaviorContext& ctx, VoidLight::FleeStateData& flee,
                     const Vector2D& threatPos,
                     const VoidLight::FleeBehaviorConfig& config) {
    auto& shared = ctx.sharedState;
    Vector2D currentPos = ctx.transform.position;

    int nearbyCount = shared.cachedNearbyCount;

    float baseCoverDistance = config.coverSeekDistance;
    float coverDistance = baseCoverDistance;
    if (nearbyCount > 2) {
        coverDistance = baseCoverDistance * 1.6f;
    } else if (nearbyCount > 0) {
        coverDistance = baseCoverDistance * 1.2f;
    }

    flee.fleeDirection = calculateFleeDirection(currentPos, threatPos, flee, config.worldPadding,
                                           ctx.worldMinX, ctx.worldMinY, ctx.worldMaxX, ctx.worldMaxY);

    Vector2D safeZoneTarget = findNearestSafeZone(ctx, flee);
    if (safeZoneTarget.lengthSquared() > 0.01f) {
        Vector2D toSafeZone = (safeZoneTarget - currentPos).normalized();
        flee.fleeDirection = (flee.fleeDirection * 0.4f + toSafeZone * 0.6f).normalized();
    }

    Vector2D dest = PathfinderManager::Instance().clampToWorldBounds(
        currentPos + flee.fleeDirection * coverDistance, 100.0f);

    float speedModifier = calculateFleeSpeedModifier(flee, config);
    if (!tryFollowPathToGoal(ctx, flee, dest, shared.moveSpeed * config.baseFleeSpeedMultiplier * speedModifier, config)) {
        ctx.transform.velocity = flee.fleeDirection * shared.moveSpeed * config.baseFleeSpeedMultiplier * speedModifier;
    }
}

} // anonymous namespace

namespace Behaviors {

void initFlee(size_t edmIndex, const VoidLight::FleeBehaviorConfig& config,
              VoidLight::FleeStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Flee);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    auto& hotData = edm.getHotDataByIndex(edmIndex);

    state.lastThreatPosition = hotData.transform.position;
    state.fleeDirection = Vector2D(0, 0);
    state.lastKnownSafeDirection = Vector2D(0, 0);
    state.fleeTimer = 0.0f;
    state.directionChangeTimer = 0.0f;
    state.panicTimer = 0.0f;
    state.currentStamina = config.maxStamina;
    state.zigzagTimer = 0.0f;
    state.navRadius = config.navRadius;
    state.backoffTimer = 0.0f;
    state.zigzagDirection = 1;
    state.isFleeing = false;
    state.isInPanic = false;
    state.hasValidThreat = false;
    state.fearBoost = 0.0f;
    state.safeZoneCount = 0;

    shared.setInitialized(true);
}

void executeFlee(BehaviorContext& ctx, const VoidLight::FleeBehaviorConfig& config,
                 VoidLight::FleeStateData& flee) {
    if (!ctx.sharedState.isValid() || !ctx.pathData) return;

    auto& shared = ctx.sharedState;
    auto& pathData = *ctx.pathData;

    // Process any pending messages before main logic
    processFleeMessages(shared, flee, config);

    // Update timers
    flee.fleeTimer += ctx.deltaTime;
    flee.directionChangeTimer += ctx.deltaTime;
    if (flee.panicTimer > 0.0f) flee.panicTimer -= ctx.deltaTime;
    flee.zigzagTimer += ctx.deltaTime;
    if (flee.backoffTimer > 0.0f) flee.backoffTimer -= ctx.deltaTime;

    // Countdown timer for cached crowd analysis (decrements to trigger refresh)
    if (shared.lastCrowdAnalysis > 0.0f) {
        shared.lastCrowdAnalysis -= ctx.deltaTime;
    }

    // Cache fear from emotions
    if (ctx.memoryData.isValid()) {
        float braveryFactor = ctx.memoryData.personality.bravery;
        float baseFear = ctx.memoryData.emotions.fear;
        flee.fearBoost = baseFear * (1.5f - braveryFactor);
    } else {
        flee.fearBoost = 0.0f;
    }

    // Update path timers
    pathData.pathUpdateTimer += ctx.deltaTime;
    pathData.progressTimer += ctx.deltaTime;
    if (pathData.pathRequestCooldown > 0.0f) pathData.pathRequestCooldown -= ctx.deltaTime;

    // Determine threat
    Vector2D threatPos;
    bool threatValid = false;

    if (ctx.memoryData.lastAttacker.isValid()) {
        auto& edm = EntityDataManager::Instance();
        size_t attackerIdx = edm.getIndex(ctx.memoryData.lastAttacker);
        if (attackerIdx != SIZE_MAX) {
            const auto& attackerHot = edm.getHotDataByIndex(attackerIdx);
            if (attackerHot.isAlive()) {
                threatPos = attackerHot.transform.position;
                threatValid = true;
            }
        }
    }

    if (!threatValid) {
        if (flee.isFleeing) {
            flee.isFleeing = false;
            flee.isInPanic = false;
            flee.hasValidThreat = false;
            switchBehavior(ctx.edmIndex, BehaviorType::Idle);
        }
        return;
    }

    float distanceToThreatSquared = (ctx.transform.position - threatPos).lengthSquared();
    float detectionRangeSquared = config.safeDistance * config.safeDistance;
    bool threatInRange = (distanceToThreatSquared <= detectionRangeSquared);

    if (threatInRange) {
        if (!flee.isFleeing) {
            flee.isFleeing = true;
            flee.fleeTimer = 0.0f;
            flee.lastThreatPosition = threatPos;
            flee.isInPanic = true;
            flee.panicTimer = config.panicDuration * s_panicVariation(s_rng);
        }
        flee.hasValidThreat = true;
        flee.lastThreatPosition = threatPos;
    } else if (flee.isFleeing) {
        float exitDistance = config.safeDistance * 1.2f;
        float safeDistanceSquared = exitDistance * exitDistance;
        if (distanceToThreatSquared >= safeDistanceSquared) {
            flee.isFleeing = false;
            flee.isInPanic = false;
            flee.hasValidThreat = false;
        }
    }

    if (flee.isInPanic && flee.panicTimer <= 0.0f) {
        flee.isInPanic = false;
    }

    if (flee.isFleeing) {
        // Refresh cached nearby count using countdown timer pattern
        if (shared.lastCrowdAnalysis <= 0.0f) {
            shared.cachedNearbyCount = AIInternal::CountNearbyEntities(
                ctx.entityId, ctx.transform.position, 100.0f);
            shared.lastCrowdAnalysis = CROWD_ANALYSIS_INTERVAL;
        }

        // Broadcast distress to nearby same-faction guards while fleeing
        if (flee.fleeTimer > 0.5f) {
            float timeSinceLastDistress = std::fmod(flee.fleeTimer - 0.5f, config.distressBroadcastInterval);
            if (timeSinceLastDistress < ctx.deltaTime) {
                thread_local std::vector<size_t> s_distressBuffer;
                uint8_t myFaction = ctx.characterData.faction;
                AIManager::Instance().scanGuardsInRadius(
                    ctx.transform.position, 400.0f, s_distressBuffer, true);
                auto& edm = EntityDataManager::Instance();
                for (size_t guardIdx : s_distressBuffer) {
                    if (guardIdx == ctx.edmIndex) continue;
                    if (edm.getCharacterDataByIndex(guardIdx).faction != myFaction) continue;
                    Behaviors::deferBehaviorMessage(guardIdx, BehaviorMessage::DISTRESS);
                }
            }
        }

        if (flee.isInPanic) {
            updatePanicFlee(ctx, flee, threatPos, config);
        } else {
            int nearbyCount = shared.cachedNearbyCount;

            if (nearbyCount > 3) {
                updateEvasiveManeuver(ctx, flee, threatPos, config);
            } else if (nearbyCount > 1) {
                updateSeekCover(ctx, flee, threatPos, config);
            } else {
                updateStrategicRetreat(ctx, flee, threatPos, config);
            }
        }
        updateStamina(flee, ctx.deltaTime, true, config);
    }
}

} // namespace Behaviors
