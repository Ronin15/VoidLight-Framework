/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include <cmath>
#include <random>

namespace {

// ============================================================================
// THREAD-LOCAL STATE
// ============================================================================

thread_local std::mt19937 s_rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> s_angleDistribution{0.0f, 2.0f * static_cast<float>(M_PI)};
thread_local std::uniform_real_distribution<float> s_radiusDistribution{0.3f, 1.0f};

// ============================================================================
// CONSTANTS
// ============================================================================

constexpr float SUSPICIOUS_THRESHOLD = 2.0f;
constexpr float INVESTIGATING_THRESHOLD = 4.0f;
constexpr float DEFAULT_ROAM_INTERVAL = 6.0f;
constexpr float DEFAULT_THREAT_DETECTION_RANGE = 150.0f;
constexpr float DEFAULT_ATTACK_ENGAGE_RANGE = 80.0f;
constexpr float PATH_TTL = 5.0f;
constexpr float NAV_RADIUS = 18.0f;
constexpr float THREAT_DETECTION_INTERVAL = 0.25f;
constexpr float CALM_POLL_INTERVAL = 2.0f;

constexpr float GUARD_ALERT_SPEED_MULT = 1.2f;
constexpr float GUARD_PURSUIT_SPEED_MULT = 1.5f;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

float getModeSpeed(uint8_t currentMode, float baseSpeed, const VoidLight::GuardBehaviorConfig& config) {
    switch (currentMode) {
        case 0: return baseSpeed * config.staticSpeedMultiplier;
        case 1: return baseSpeed * config.patrolSpeedMultiplier;
        case 2: return baseSpeed * config.areaSpeedMultiplier;
        case 3: return baseSpeed * config.roamingSpeedMultiplier;
        case 4: return baseSpeed * config.alertSpeedMultiplier;
        default: return baseSpeed;
    }
}

float getModeAlertRadius(uint8_t currentMode, const VoidLight::GuardBehaviorConfig& config) {
    switch (currentMode) {
        case 0: return config.staticAlertRadiusMultiplier;
        case 1: return config.patrolAlertRadiusMultiplier;
        case 2: return config.areaAlertRadiusMultiplier;
        case 3: return config.roamingAlertRadiusMultiplier;
        case 4: return config.alertModeAlertRadiusMultiplier;
        default: return 1.0f;
    }
}

float getAttackEngageRange(const CharacterData* charData) {
    if (charData && charData->attackRange > 0.0f) {
        return charData->attackRange;
    }
    return DEFAULT_ATTACK_ENGAGE_RANGE;
}

void processGuardMessages(BehaviorData& shared, VoidLight::GuardStateData& guard,
                          const VoidLight::GuardBehaviorConfig&) {
    for (uint8_t i = 0; i < shared.pendingMessageCount; ++i) {
        uint8_t msgId = shared.pendingMessages[i].messageId;

        switch (msgId) {
            case BehaviorMessage::RAISE_ALERT:
                guard.currentAlertLevel = 3;
                guard.alertTimer = 0.0f;
                guard.alertDecayTimer = 0.0f;
                break;

            case BehaviorMessage::CALM_DOWN:
                if (guard.currentAlertLevel > 0)
                {
                    guard.currentAlertLevel--;
                    guard.alertDecayTimer = 0.0f;
                }
                break;

            case BehaviorMessage::PANIC:
                guard.currentAlertLevel = 3;
                guard.alertTimer = 0.0f;
                guard.alertDecayTimer = 0.0f;
                break;

            case BehaviorMessage::DISTRESS:
                if (guard.currentAlertLevel < 1)
                {
                    guard.currentAlertLevel = 1;
                    guard.alertTimer = 0.0f;
                    guard.alertDecayTimer = 0.0f;
                }
                break;

            default:
                break;
        }
    }
    shared.pendingMessageCount = 0;
}

float normalizeAngle(float angle) {
    while (angle > static_cast<float>(M_PI)) angle -= 2.0f * static_cast<float>(M_PI);
    while (angle < -static_cast<float>(M_PI)) angle += 2.0f * static_cast<float>(M_PI);
    return angle;
}

float calculateAngle(const Vector2D& from, const Vector2D& to) {
    Vector2D diff = to - from;
    return std::atan2(diff.getY(), diff.getX());
}

bool isAtPosition(const Vector2D& currentPos, const Vector2D& targetPos, float threshold = 20.0f) {
    return (currentPos - targetPos).lengthSquared() <= threshold * threshold;
}

Vector2D generateRoamTarget(const Vector2D& center, float radius) {
    float angle = s_angleDistribution(s_rng);
    float dist = s_radiusDistribution(s_rng) * radius;
    return center + Vector2D(dist * std::cos(angle), dist * std::sin(angle));
}

Vector2D getNextPatrolWaypoint(VoidLight::GuardStateData& guard) {
    if (guard.patrolWaypointCount == 0) {
        return guard.assignedPosition;
    }
    return guard.patrolWaypoints[guard.currentPatrolWaypointIndex];
}

void advancePatrolWaypoint(VoidLight::GuardStateData& guard) {
    if (guard.patrolWaypointCount == 0) return;

    if (guard.reversePatrol) {
        if (guard.patrolForward) {
            if (guard.currentPatrolWaypointIndex + 1 >= guard.patrolWaypointCount) {
                guard.patrolForward = false;
                if (guard.currentPatrolWaypointIndex > 0) {
                    guard.currentPatrolWaypointIndex--;
                }
            } else {
                guard.currentPatrolWaypointIndex++;
            }
        } else {
            if (guard.currentPatrolWaypointIndex == 0) {
                guard.patrolForward = true;
                if (guard.patrolWaypointCount > 1) {
                    guard.currentPatrolWaypointIndex++;
                }
            } else {
                guard.currentPatrolWaypointIndex--;
            }
        }
    } else {
        guard.currentPatrolWaypointIndex = (guard.currentPatrolWaypointIndex + 1) % guard.patrolWaypointCount;
    }
}

void moveToPosition(BehaviorContext& ctx, EntityDataManager& edm, const Vector2D& targetPos, float speed) {
    if (speed <= 0.0f || !ctx.pathData) return;

    auto& pathData = *ctx.pathData;
    Vector2D currentPos = ctx.transform.position;

    const bool skipRefresh = (pathData.pathRequestCooldown > 0.0f && pathData.isFollowingPath() &&
                              pathData.progressTimer < 0.8f);
    bool needsPath = false;
    if (!skipRefresh) {
        needsPath = !pathData.hasPath || pathData.navIndex >= pathData.pathLength ||
                    pathData.pathUpdateTimer > PATH_TTL;
    }

    if (!skipRefresh && !needsPath && pathData.hasPath && pathData.pathLength > 0) {
        constexpr float GOAL_CHANGE_THRESH_SQ = 100.0f * 100.0f;
        Vector2D lastGoal = edm.getPathGoal(ctx.edmIndex);
        if ((targetPos - lastGoal).lengthSquared() > GOAL_CHANGE_THRESH_SQ) {
            needsPath = true;
        }
    }

    if (needsPath && pathData.pathRequestCooldown <= 0.0f) {
        PathfinderManager::Instance().requestPathToEDM(ctx.edmIndex, currentPos, targetPos,
                                                        PathfinderManager::Priority::Normal);
        pathData.pathRequestCooldown = 0.3f + (ctx.entityId % 200) * 0.001f;
    }

    if (pathData.isFollowingPath()) {
        Vector2D waypoint = ctx.pathData->currentWaypoint;
        Vector2D toWaypoint = waypoint - currentPos;
        float dist = toWaypoint.length();

        if (dist < NAV_RADIUS) {
            edm.advanceWaypointWithCache(ctx.edmIndex);
            if (pathData.isFollowingPath()) {
                waypoint = ctx.pathData->currentWaypoint;
                toWaypoint = waypoint - currentPos;
                dist = toWaypoint.length();
            }
        }

        if (dist > 0.001f) {
            Vector2D direction = toWaypoint / dist;
            ctx.transform.velocity = direction * speed;
            pathData.progressTimer = 0.0f;
        }
    } else {
        Vector2D toTarget = targetPos - currentPos;
        float dist = toTarget.length();
        if (dist > NAV_RADIUS && dist > 0.001f) {
            Vector2D direction = toTarget / dist;
            ctx.transform.velocity = direction * speed;
        } else {
            ctx.transform.velocity = Vector2D(0, 0);
        }
    }
}

EntityHandle detectThreat(BehaviorContext& ctx, EntityDataManager& edm, bool& isEnemyFaction,
                          uint8_t& witnessAlertLevel, Vector2D& witnessLocation,
                          const VoidLight::GuardStateData& guard) {
    isEnemyFaction = false;
    witnessAlertLevel = 0;

    uint8_t myFaction = ctx.characterData.faction;

    if (ctx.memoryData.lastAttacker.isValid()) {
        size_t idx = edm.getIndex(ctx.memoryData.lastAttacker);
        if (idx != SIZE_MAX && edm.getHotDataByIndex(idx).isAlive()) {
            isEnemyFaction = (edm.getCharacterDataByIndex(idx).faction != myFaction);
            return ctx.memoryData.lastAttacker;
        }
    }

    if (ctx.memoryData.lastTarget.isValid()) {
        size_t idx = edm.getIndex(ctx.memoryData.lastTarget);
        if (idx != SIZE_MAX && edm.getHotDataByIndex(idx).isAlive()) {
            isEnemyFaction = (edm.getCharacterDataByIndex(idx).faction != myFaction);
            return ctx.memoryData.lastTarget;
        }
    }

    if (ctx.memoryData.isValid()) {
        EntityHandle recentThreat{};
        float recentTimestamp = -1.0f;

        for (size_t i = 0; i < NPCMemoryData::INLINE_MEMORY_COUNT; ++i) {
            const auto& mem = ctx.memoryData.memories[i];
            if (!mem.isValid()) continue;
            if (mem.type != MemoryType::WitnessedCombat &&
                mem.type != MemoryType::WitnessedDeath) continue;

            const float memAge = ctx.gameTime - mem.timestamp;
            if (memAge > 10.0f) continue;

            if (mem.type == MemoryType::WitnessedDeath && witnessAlertLevel < 2) {
                witnessAlertLevel = 2;
                witnessLocation = mem.location;
            } else if (witnessAlertLevel < 1) {
                witnessAlertLevel = 1;
                witnessLocation = mem.location;
            }

            if (!mem.subject.isValid()) continue;
            size_t idx = edm.getIndex(mem.subject);
            if (idx == SIZE_MAX || !edm.getHotDataByIndex(idx).isAlive()) continue;

            if (mem.timestamp >= recentTimestamp) {
                recentTimestamp = mem.timestamp;
                recentThreat = mem.subject;
            }
        }

        if (recentThreat.isValid()) {
            size_t idx = edm.getIndex(recentThreat);
            isEnemyFaction = (edm.getCharacterDataByIndex(idx).faction != myFaction);
            return recentThreat;
        }
    }

    if (ctx.playerValid && ctx.characterData.faction == 1) {
        float detectionRange = guard.cachedDetectionRange;
        float distSq = Vector2D::distanceSquared(ctx.transform.position, ctx.playerPosition);
        if (distSq <= detectionRange * detectionRange) {
            isEnemyFaction = true;
            return ctx.playerHandle;
        }
    }

    return EntityHandle{};
}

void updateAlertLevel(VoidLight::GuardStateData& guard, bool threatPresent,
                      bool isEnemyFaction, float escalationMultiplier) {
    if (threatPresent) {
        guard.threatSightingTimer = 0.0f;
        guard.hasActiveThreat = true;

        if (isEnemyFaction) {
            guard.currentAlertLevel = 3;
            guard.alertTimer = 0.0f;
        } else {
            float threatDuration = guard.alertTimer;
            if (guard.currentAlertLevel == 0) {
                guard.currentAlertLevel = 1;
                guard.alertTimer = 0.0f;
            } else if (guard.currentAlertLevel == 1 && threatDuration > SUSPICIOUS_THRESHOLD * escalationMultiplier) {
                guard.currentAlertLevel = 2;
            } else if (guard.currentAlertLevel == 2 && threatDuration > INVESTIGATING_THRESHOLD * escalationMultiplier) {
                guard.currentAlertLevel = 3;
            }
        }
    } else {
        guard.hasActiveThreat = false;
    }
}

} // anonymous namespace

namespace Behaviors {

void initGuard(size_t edmIndex, const VoidLight::GuardBehaviorConfig&,
               VoidLight::GuardStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Guard);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    auto& hotData = edm.getHotDataByIndex(edmIndex);

    state.assignedPosition = hotData.transform.position;
    state.currentMode = 0;
    state.currentAlertLevel = 0;
    state.onDuty = true;
    state.hasActiveThreat = false;
    state.isInvestigating = false;
    state.returningToPost = false;
    state.alertRaised = false;
    state.helpCalled = false;
    state.threatSightingTimer = 0.0f;
    state.alertTimer = 0.0f;
    state.investigationTimer = 0.0f;
    state.positionCheckTimer = 0.0f;
    state.patrolMoveTimer = 0.0f;
    state.alertDecayTimer = 0.0f;
    state.currentHeading = 0.0f;
    state.roamTimer = DEFAULT_ROAM_INTERVAL;
    state.currentPatrolIndex = 0;
    state.escalationMultiplier = 1.0f;
    state.roamTarget = hotData.transform.position;
    state.currentPatrolTarget = hotData.transform.position;
    state.lastKnownThreatPosition = Vector2D(0, 0);
    state.investigationTarget = Vector2D(0, 0);
    state.hostileTimer = 0.0f;

    shared.setInitialized(true);
}

void executeGuard(BehaviorContext& ctx, const VoidLight::GuardBehaviorConfig& config,
                  VoidLight::GuardStateData& guard) {
    if (!ctx.sharedState.isValid()) return;

    auto& shared = ctx.sharedState;
    auto& edm = EntityDataManager::Instance();

    float engageRange = getAttackEngageRange(&ctx.characterData);
    float engageRangeSq = engageRange * engageRange;

    processGuardMessages(shared, guard, config);

    if (!guard.onDuty) return;

    // Update timers
    guard.threatSightingTimer += ctx.deltaTime;
    guard.alertTimer += ctx.deltaTime;
    guard.investigationTimer += ctx.deltaTime;
    guard.positionCheckTimer += ctx.deltaTime;
    guard.patrolMoveTimer += ctx.deltaTime;
    guard.alertDecayTimer += ctx.deltaTime;
    guard.roamTimer -= ctx.deltaTime;

    // Update escalation multiplier from personality
    if (ctx.memoryData.isValid()) {
        float suspicion = ctx.memoryData.emotions.suspicion;
        float loyalty = ctx.memoryData.personality.loyalty;
        guard.escalationMultiplier = 1.0f / (1.0f + suspicion * 0.5f + loyalty * 0.25f);
    } else {
        guard.escalationMultiplier = 1.0f;
    }

    if (!ctx.pathData) return;
    auto& pathData = *ctx.pathData;
    pathData.pathUpdateTimer += ctx.deltaTime;
    pathData.progressTimer += ctx.deltaTime;
    if (pathData.pathRequestCooldown > 0.0f) {
        pathData.pathRequestCooldown -= ctx.deltaTime;
    }

    // Cache detection range — recompute only on mode change
    if (guard.currentMode != guard.lastCachedMode) {
        guard.cachedDetectionRange = DEFAULT_THREAT_DETECTION_RANGE * getModeAlertRadius(guard.currentMode, config);
        guard.lastCachedMode = guard.currentMode;
    }

    EntityHandle threat{};
    bool threatPresent = false;
    bool isEnemyFaction = false;
    bool shouldPoll = (guard.currentAlertLevel == 0)
        ? (guard.threatSightingTimer >= CALM_POLL_INTERVAL)
        : (guard.threatSightingTimer >= THREAT_DETECTION_INTERVAL);

    if (shouldPoll) {
        uint8_t witnessAlertLevel = 0;
        Vector2D witnessLocation{};
        threat = detectThreat(ctx, edm, isEnemyFaction, witnessAlertLevel, witnessLocation, guard);
        threatPresent = threat.isValid();
        guard.threatSightingTimer = 0.0f;

        if (threatPresent) {
            ctx.memoryData.lastTarget = threat;
        }

        if (!threatPresent && guard.currentAlertLevel == 0 && witnessAlertLevel > 0) {
            guard.currentAlertLevel = witnessAlertLevel;
            guard.lastKnownThreatPosition = witnessLocation;
            guard.alertTimer = 0.0f;
            guard.alertDecayTimer = 0.0f;
        }
    }

    updateAlertLevel(guard, threatPresent, isEnemyFaction, guard.escalationMultiplier);

    if (guard.currentAlertLevel >= 3 && ctx.memoryData.isValid())
    {
        float fear = ctx.memoryData.emotions.fear;
        float effectiveBravery = ctx.memoryData.personality.bravery + 0.1f;
        if (fear > 0.7f && effectiveBravery < 0.3f)
        {
            switchBehavior(ctx.edmIndex, BehaviorType::Flee);
            return;
        }
    }

    if (guard.currentAlertLevel == 3 && !guard.helpCalled && config.canCallForHelp) {
        guard.helpCalled = true;
        thread_local std::vector<size_t> s_helpBuffer;
        uint8_t myFaction = ctx.characterData.faction;
        AIManager::Instance().scanFactionInRadius(
            myFaction, ctx.transform.position, config.helpCallRadius, s_helpBuffer, true);
        for (size_t idx : s_helpBuffer) {
            if (idx == ctx.edmIndex) continue;
            Behaviors::deferBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);
        }
    }

    if (threatPresent) {
        size_t threatIdx = edm.getIndex(threat);
        if (threatIdx != SIZE_MAX) {
            Vector2D threatPos = edm.getHotDataByIndex(threatIdx).transform.position;
            guard.lastKnownThreatPosition = threatPos;

            switch (guard.currentAlertLevel) {
                case 1:
                    guard.currentHeading = calculateAngle(ctx.transform.position, threatPos);
                    break;

                case 2:
                    guard.isInvestigating = true;
                    guard.investigationTarget = threatPos;
                    guard.investigationTimer = 0.0f;
                    moveToPosition(ctx, edm, threatPos, shared.moveSpeed);
                    break;

                case 3: {
                    guard.hostileTimer += ctx.deltaTime;
                    if (guard.hostileTimer >= config.alarmEscalationTime) {
                        guard.currentAlertLevel = 4;
                        guard.hostileTimer = 0.0f;
                    }

                    float distSq3 = (ctx.transform.position - threatPos).lengthSquared();
                    if (distSq3 <= engageRangeSq) {
                        switchBehavior(ctx.edmIndex, BehaviorType::Attack);
                        return;
                    } else {
                        moveToPosition(ctx, edm, threatPos, shared.moveSpeed * GUARD_ALERT_SPEED_MULT);
                    }
                    break;
                }
                case 4: {
                    float distSq4 = (ctx.transform.position - threatPos).lengthSquared();
                    if (distSq4 <= engageRangeSq) {
                        switchBehavior(ctx.edmIndex, BehaviorType::Attack);
                        return;
                    } else {
                        moveToPosition(ctx, edm, threatPos, shared.moveSpeed * GUARD_PURSUIT_SPEED_MULT);
                    }

                    if (!guard.helpCalled && config.canCallForHelp) {
                        guard.helpCalled = true;
                        thread_local std::vector<size_t> s_alarmBuffer;
                        uint8_t myFaction = ctx.characterData.faction;
                        AIManager::Instance().scanFactionInRadius(
                            myFaction, ctx.transform.position, config.alarmHelpCallRadius, s_alarmBuffer, true);
                        for (size_t idx : s_alarmBuffer) {
                            if (idx == ctx.edmIndex) continue;
                            Behaviors::deferBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    } else if (guard.isInvestigating) {
        if (guard.investigationTimer > config.investigationTime && guard.currentAlertLevel < 3) {
            guard.isInvestigating = false;
            guard.returningToPost = true;
        } else {
            if (guard.currentAlertLevel >= 3) {
                float distToThreatSq = (ctx.transform.position - guard.lastKnownThreatPosition).lengthSquared();
                if (distToThreatSq <= engageRangeSq) {
                    switchBehavior(ctx.edmIndex, BehaviorType::Attack);
                    return;
                }
            }

            if (!isAtPosition(ctx.transform.position, guard.investigationTarget)) {
                float speed = (guard.currentAlertLevel >= 3) ? shared.moveSpeed * GUARD_PURSUIT_SPEED_MULT : shared.moveSpeed;
                moveToPosition(ctx, edm, guard.investigationTarget, speed);
            }
        }
    } else if (guard.returningToPost) {
        if (!isAtPosition(ctx.transform.position, guard.assignedPosition)) {
            moveToPosition(ctx, edm, guard.assignedPosition, shared.moveSpeed);
        } else {
            guard.returningToPost = false;
            guard.currentAlertLevel = 0;
        }
    } else {
        float modeSpeed = getModeSpeed(guard.currentMode, shared.moveSpeed, config);

        switch (guard.currentMode) {
            case 0:
                if (!isAtPosition(ctx.transform.position, guard.assignedPosition, 10.0f)) {
                    moveToPosition(ctx, edm, guard.assignedPosition, modeSpeed);
                }
                if (guard.positionCheckTimer > 2.0f) {
                    guard.currentHeading += 0.5f;
                    guard.currentHeading = normalizeAngle(guard.currentHeading);
                    guard.positionCheckTimer = 0.0f;
                }
                break;

            case 1:
                if (guard.patrolWaypointCount > 0) {
                    Vector2D waypointTarget = getNextPatrolWaypoint(guard);
                    if (isAtPosition(ctx.transform.position, waypointTarget)) {
                        advancePatrolWaypoint(guard);
                        waypointTarget = getNextPatrolWaypoint(guard);
                    }
                    moveToPosition(ctx, edm, waypointTarget, modeSpeed);
                } else {
                    if (guard.roamTimer <= 0.0f || isAtPosition(ctx.transform.position, guard.roamTarget)) {
                        guard.roamTarget = generateRoamTarget(guard.assignedPosition, config.guardRadius);
                        guard.roamTimer = DEFAULT_ROAM_INTERVAL;
                    }
                    moveToPosition(ctx, edm, guard.roamTarget, modeSpeed);
                }
                break;

            case 2:
            case 3:
                if (guard.roamTimer <= 0.0f || isAtPosition(ctx.transform.position, guard.roamTarget)) {
                    guard.roamTarget = generateRoamTarget(guard.assignedPosition, config.guardRadius);
                    guard.roamTimer = DEFAULT_ROAM_INTERVAL;
                }
                moveToPosition(ctx, edm, guard.roamTarget, modeSpeed);
                break;

            case 4:
                if (guard.currentAlertLevel >= 2) {
                    if (guard.lastKnownThreatPosition.lengthSquared() > 0.0f) {
                        moveToPosition(ctx, edm, guard.lastKnownThreatPosition, modeSpeed);
                    }
                } else if (guard.roamTimer <= 0.0f || isAtPosition(ctx.transform.position, guard.roamTarget)) {
                    guard.roamTarget = generateRoamTarget(guard.assignedPosition, config.guardRadius);
                    guard.roamTimer = DEFAULT_ROAM_INTERVAL;
                    moveToPosition(ctx, edm, guard.roamTarget, modeSpeed);
                }
                break;

            default:
                break;
        }
    }

    // Handle alert decay
    if (guard.currentAlertLevel > 0 && guard.alertDecayTimer > config.alertDecayTime) {
        uint8_t prevLevel = guard.currentAlertLevel;
        guard.currentAlertLevel--;
        guard.alertDecayTimer = 0.0f;

        if (prevLevel == 4) {
            guard.hostileTimer = 0.0f;
            guard.helpCalled = false;
        }

        if (guard.currentAlertLevel == 0) {
            guard.helpCalled = false;
            thread_local std::vector<size_t> s_calmBuffer;
            uint8_t myFaction = ctx.characterData.faction;
            AIManager::Instance().scanFactionInRadius(
                myFaction, ctx.transform.position, config.helpCallRadius, s_calmBuffer, true);
            for (size_t idx : s_calmBuffer) {
                if (idx == ctx.edmIndex) continue;
                Behaviors::deferBehaviorMessage(idx, BehaviorMessage::CALM_DOWN);
            }
        }
    }
}

} // namespace Behaviors
