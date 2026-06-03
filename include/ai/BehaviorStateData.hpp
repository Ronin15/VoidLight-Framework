/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef BEHAVIOR_STATE_DATA_HPP
#define BEHAVIOR_STATE_DATA_HPP

/**
 * @file BehaviorStateData.hpp
 * @brief Per-variant behavior state structs stored in dense archetype pools.
 *
 * Each struct mirrors the layout of the corresponding StateUnion member that
 * previously lived inside BehaviorData. Moving them to per-variant pools
 * eliminates the 192-byte union padding paid by every entity regardless of
 * which behavior it runs — ~75% waste per entity.
 *
 * Lifecycle invariant: a state pool slot is created together with its matching
 * config pool slot (in reassignBehaviorConfig) and destroyed together with it
 * (in clearBehaviorConfig). The existing m_*Owners vectors cover both pools.
 *
 * Threading: slots are written on worker threads only to the entity's own
 * index (no cross-entity writes). Pool structural changes (push/pop) are
 * main-thread only.
 */

#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include <cstdint>

namespace VoidLight
{

// ============================================================================
// IDLE STATE (~48 bytes)
// ============================================================================

struct IdleStateData
{
    Vector2D originalPosition;   // 8 bytes
    Vector2D currentOffset;      // 8 bytes
    float movementTimer{0.0f};
    float turnTimer{0.0f};
    float movementInterval{0.0f};
    float turnInterval{0.0f};
    float currentAngle{0.0f};
    bool initialized{false};
    uint8_t _pad[3]{};
};

// ============================================================================
// WANDER STATE (~68 bytes)
// ============================================================================

struct WanderStateData
{
    Vector2D currentDirection;   // 8 bytes
    Vector2D previousVelocity;   // 8 bytes
    Vector2D lastStallPosition;  // 8 bytes
    float directionChangeTimer{0.0f};
    float lastDirectionFlip{0.0f};
    float startDelay{0.0f};
    float stallTimer{0.0f};
    float stallPositionVariance{0.0f};
    float unstickTimer{0.0f};
    float movementUpdateTimer{0.0f};  // Throttle heavy logic to run every ~5s
    bool movementStarted{false};
    uint8_t _pad[3]{};
};

// ============================================================================
// CHASE STATE (~80 bytes)
// ============================================================================

struct ChaseStateData
{
    Vector2D lastKnownTargetPos;      // Last known target position
    Vector2D currentDirection;         // Current movement direction
    Vector2D lastStallPosition;        // Position when stall was detected
    float timeWithoutSight{0.0f};     // Time since last line of sight
    float stallPositionVariance{0.0f};// Variance for stall detection
    float unstickTimer{0.0f};         // Timer for unstick behavior
    float crowdCheckTimer{0.0f};      // Throttle crowd detection
    float pathRequestCooldown{0.0f};  // Cooldown between path requests
    float stallRecoveryCooldown{0.0f};// Cooldown after stall recovery
    float behaviorChangeCooldown{0.0f};// Cooldown for behavior state changes
    int recalcCounter{0};             // Path recalculation counter
    int cachedChaserCount{0};         // Cached number of chasers nearby
    bool isChasing{false};            // Currently in chase mode
    bool hasLineOfSight{false};       // Has line of sight to target
    bool hasExplicitTarget{false};    // NPC-vs-NPC chase: explicit target set
    uint8_t _pad[1]{};
    EntityHandle explicitTarget;      // NPC-vs-NPC chase: overrides player targeting
};

// ============================================================================
// PATROL STATE (subset of guard fields used by patrol)
// ============================================================================

struct PatrolStateData
{
    Vector2D currentPatrolTarget;     // Current target waypoint position
    Vector2D assignedPosition;        // Starting/home position
    float patrolMoveTimer{0.0f};      // Timer for dwell at waypoint
    float patrolThrottleTimer{0.0f};  // Throttle timer for patrol update interval
    uint32_t currentPatrolIndex{0};   // Current waypoint index (wraps mod 4)
    uint8_t _pad[4]{};
};

// ============================================================================
// FLEE STATE (~136 bytes with safe zones)
// ============================================================================

struct FleeStateData
{
    Vector2D lastThreatPosition;
    Vector2D fleeDirection;
    Vector2D lastKnownSafeDirection;
    float fleeTimer{0.0f};
    float directionChangeTimer{0.0f};
    float panicTimer{0.0f};
    float currentStamina{0.0f};
    float zigzagTimer{0.0f};
    float navRadius{0.0f};
    float backoffTimer{0.0f};
    float fearBoost{0.0f};        // Cached from emotions each frame for speed modifier
    int zigzagDirection{1};
    bool isFleeing{false};
    bool isInPanic{false};
    bool hasValidThreat{false};
    uint8_t _pad{};

    // Safe zone system (4 safe zones inline)
    Vector2D safeZoneCenters[4];  // 32 bytes: Center positions of safe zones
    float safeZoneRadii[4]{};     // 16 bytes: Radii of safe zones
    uint8_t safeZoneCount{0};     // Number of active safe zones (0-4)
    uint8_t _fleePad[7]{};        // Padding for alignment
};

// ============================================================================
// FOLLOW STATE (~72 bytes)
// ============================================================================

struct FollowStateData
{
    Vector2D lastTargetPosition;
    Vector2D currentVelocity;
    Vector2D desiredPosition;
    Vector2D formationOffset;
    Vector2D lastSepForce;
    float currentSpeed{0.0f};
    float currentHeading{0.0f};
    float backoffTimer{0.0f};
    int formationSlot{-1};
    bool isFollowing{false};
    bool targetMoving{false};
    bool inFormation{false};
    bool isStopped{true};
};

// ============================================================================
// GUARD STATE (~176 bytes with patrol waypoints)
// ============================================================================

struct GuardStateData
{
    Vector2D assignedPosition;
    Vector2D lastKnownThreatPosition;
    Vector2D investigationTarget;
    Vector2D currentPatrolTarget;
    Vector2D roamTarget;
    float threatSightingTimer{0.0f};
    float alertTimer{0.0f};
    float investigationTimer{0.0f};
    float positionCheckTimer{0.0f};
    float patrolMoveTimer{0.0f};
    float alertDecayTimer{0.0f};
    float currentHeading{0.0f};
    float roamTimer{0.0f};
    float escalationMultiplier{1.0f};  // Suspicion-based threshold multiplier
    float cachedDetectionRange{0.0f};  // Cached detection range (recomputed on mode change)
    float hostileTimer{0.0f};          // Time spent at alert level HOSTILE (3) for ALARM escalation
    float patrolThrottleTimer{0.0f};   // Throttle timer for PatrolBehavior update interval
    uint32_t currentPatrolIndex{0};
    uint8_t currentAlertLevel{0};  // 0=Calm, 1=Suspicious, 2=Alert, 3=Combat
    uint8_t currentMode{0};
    uint8_t lastCachedMode{255};       // Track mode for cache invalidation
    bool hasActiveThreat{false};
    bool isInvestigating{false};
    bool returningToPost{false};
    bool onDuty{true};
    bool alertRaised{false};
    bool helpCalled{false};

    // Patrol waypoint system (64 bytes for 8 waypoints)
    static constexpr uint8_t MAX_PATROL_WAYPOINTS = 8;
    Vector2D patrolWaypoints[MAX_PATROL_WAYPOINTS];
    uint8_t patrolWaypointCount{0};
    uint8_t currentPatrolWaypointIndex{0};
    bool reversePatrol{false};   // Ping-pong patrol
    bool patrolForward{true};    // Current direction in ping-pong mode
    uint8_t _guardPad[4]{};
};

// ============================================================================
// ATTACK STATE
// ============================================================================

struct AttackStateData
{
    Vector2D lastTargetPosition;
    Vector2D attackPosition;
    Vector2D retreatPosition;
    Vector2D strafeVector;
    Vector2D tacticalResetAnchor;
    float attackTimer{0.0f};
    float stateChangeTimer{0.0f};
    float damageTimer{0.0f};
    float comboTimer{0.0f};
    float strafeTimer{0.0f};
    float currentHealth{100.0f};
    float maxHealth{100.0f};
    float currentStamina{100.0f};
    float targetDistance{0.0f};
    float attackChargeTime{0.0f};
    float recoveryTimer{0.0f};
    float scanCooldown{0.0f};
    float preferredAttackAngle{0.0f};
    float pressureScore{0.0f};
    float desiredCombatRange{0.0f};
    float resetConfidence{0.0f};
    int currentCombo{0};
    int attacksInCombo{0};
    int strafeDirectionInt{1};
    uint8_t lastTacticalRetreatEncounter{0};
    uint8_t currentState{0};   // 0=Seeking, 1=Assessing, 2=Approaching, 3=Attacking, 4=Recovering, 5=TacticalReset, 6=Pressuring, 7=Disengaging
    uint8_t attackMode{0};     // 0=Melee, 1=Ranged, 2=Charge, 3=Ambush, 4=Coordinated, 5=HitAndRun, 6=Berserker
    uint8_t lastCombatDecision{0};
    bool inCombat{false};
    bool hasTarget{false};
    bool isCharging{false};
    bool isRetreating{false};
    bool canAttack{true};
    bool lastAttackHit{false};
    bool specialAttackReady{false};
    bool circleStrafing{false};
    bool flanking{false};
    bool hasExplicitTarget{false};
    bool comboEnabled{false};
    bool hasHandledTacticalRetreat{false};
    EntityHandle explicitTarget;  // NPC-vs-NPC combat: overrides player targeting
};

} // namespace VoidLight

#endif // BEHAVIOR_STATE_DATA_HPP
