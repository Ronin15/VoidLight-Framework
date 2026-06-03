/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef BEHAVIOR_CONFIG_HPP
#define BEHAVIOR_CONFIG_HPP

#include <cstdint>

enum class BehaviorType : uint8_t {
    Wander = 0,
    Guard = 1,
    Patrol = 2,
    Follow = 3,
    Chase = 4,
    Attack = 5,
    Flee = 6,
    Idle = 7,
    Custom = 8,
    COUNT = 9,
    None = 0xFF  // Invalid/uninitialized
};

namespace VoidLight
{

/**
 * Configuration for IdleBehavior
 *
 * Controls idle animation modes: stationary, subtle swaying, occasional turns,
 * or light fidgeting movements.
 */
struct IdleBehaviorConfig
{
    enum class IdleMode : uint8_t {
        STATIONARY = 0,      // Completely still
        SUBTLE_SWAY = 1,     // Small swaying motion
        OCCASIONAL_TURN = 2, // Turn around occasionally
        LIGHT_FIDGET = 3     // Small random movements
    };

    IdleMode mode{IdleMode::SUBTLE_SWAY};
    float idleRadius{20.0f};           // Movement radius for sway/fidget modes
    float movementFrequency{3.0f};     // Seconds between movements
    float turnFrequency{5.0f};         // Seconds between turns
    float swaySpeed{35.0f};            // Movement speed for SUBTLE_SWAY mode (px/s)
    float fidgetSpeed{40.0f};          // Movement speed for LIGHT_FIDGET mode (px/s)

    /**
     * Create stationary configuration (completely still)
     */
    static IdleBehaviorConfig createStationary()
    {
        IdleBehaviorConfig config;
        config.mode = IdleMode::STATIONARY;
        config.idleRadius = 0.0f;
        config.movementFrequency = 0.0f;
        config.turnFrequency = 0.0f;
        return config;
    }

    /**
     * Create subtle sway configuration
     */
    static IdleBehaviorConfig createSubtleSway()
    {
        IdleBehaviorConfig config;
        config.mode = IdleMode::SUBTLE_SWAY;
        config.idleRadius = 30.0f;
        config.movementFrequency = 2.0f;
        config.turnFrequency = 8.0f;
        return config;
    }

    /**
     * Create occasional turn configuration (rotation only)
     */
    static IdleBehaviorConfig createOccasionalTurn()
    {
        IdleBehaviorConfig config;
        config.mode = IdleMode::OCCASIONAL_TURN;
        config.idleRadius = 0.0f;
        config.movementFrequency = 0.0f;
        config.turnFrequency = 4.0f;
        return config;
    }

    /**
     * Create light fidget configuration
     */
    static IdleBehaviorConfig createLightFidget()
    {
        IdleBehaviorConfig config;
        config.mode = IdleMode::LIGHT_FIDGET;
        config.idleRadius = 50.0f;
        config.movementFrequency = 1.5f;
        config.turnFrequency = 3.0f;
        return config;
    }
};

/**
 * Configuration for WanderBehavior
 *
 * Controls how entities wander around the world with boundary avoidance
 * and crowd awareness.
 */
struct WanderBehaviorConfig
{
    // Movement parameters
    float speed = 35.0f;                          // Base wandering speed in px/s

    // Direction change parameters
    float changeDirectionIntervalMin = 3000.0f;   // Minimum ms between direction changes
    float changeDirectionIntervalMax = 8000.0f;   // Maximum ms between direction changes

    // Edge avoidance
    float edgeThreshold = 50.0f;                  // Distance from world edge (px) to start avoiding
    float worldPaddingMargin = 256.0f;            // Safety margin when selecting new goals near edges

    // Crowd escape
    int crowdEscapeThreshold = 8;                 // Number of nearby entities to trigger escape
    float crowdEscapeDistanceMultiplier = 3.0f;   // Multiply wander distance by this when escaping crowds

    // Pathfinding parameters
    float pathRequestCooldown = 30.0f;            // Seconds between pathfinding requests (reduces load)
    float minGoalChangeDistance = 200.0f;         // Minimum distance change to justify new path request
    float pathRefreshInterval = 25.0f;            // Seconds before path recalculation (TTL)

    // Stuck detection
    float stallSpeed = 0.5f;                      // Speed threshold (px/s) to consider entity stalled
    float stallTimeout = 0.6f;                    // Seconds without progress before triggering unstuck

    // Goal/distance parameters
    float baseGoalDistance{600.0f};               // Base distance for wander goal selection (px)
    float jitterThresholdMultiplier{1.5f};        // Multiplier on moveSpeed for jitter detection
    float crowdSlowdownHeavy{0.5f};              // Speed multiplier when crowdCount > 10
    float crowdSlowdownLight{0.7f};              // Speed multiplier when crowdCount 5-10

    // Update throttling (peaceful behavior — heavy logic runs at this interval)
    float updateInterval{5.0f};                   // Seconds between full movement updates

    // Factory methods for common presets
    static WanderBehaviorConfig createSmallWander()
    {
        WanderBehaviorConfig config;
        config.changeDirectionIntervalMin = 1000.0f;  // Faster changes
        config.changeDirectionIntervalMax = 3000.0f;
        config.minGoalChangeDistance = 80.0f;         // Smaller goals
        config.pathRefreshInterval = 10.0f;
        return config;
    }

    static WanderBehaviorConfig createLargeWander()
    {
        WanderBehaviorConfig config;
        config.changeDirectionIntervalMin = 8000.0f;  // Slower changes
        config.changeDirectionIntervalMax = 15000.0f;
        config.minGoalChangeDistance = 500.0f;        // Larger goals
        config.pathRefreshInterval = 45.0f;
        return config;
    }

    static WanderBehaviorConfig createEventWander(float areaRadius = 300.0f)
    {
        WanderBehaviorConfig config;
        config.changeDirectionIntervalMin = 2000.0f;
        config.changeDirectionIntervalMax = 5000.0f;
        config.minGoalChangeDistance = areaRadius * 0.5f;
        config.worldPaddingMargin = areaRadius;       // Constrain to area
        config.edgeThreshold = areaRadius * 0.8f;
        return config;
    }
};

/**
 * Configuration for ChaseBehavior
 *
 * Controls how entities pursue and catch targets with pathfinding and
 * line-of-sight tracking.
 */
struct ChaseBehaviorConfig
{
    // Movement parameters
    float chaseSpeed = 60.0f;                     // Speed when actively chasing target

    // Pathfinding parameters
    float pathInvalidationDistance = 300.0f;      // Distance target must move to invalidate path
    float pathRefreshInterval = 12.0f;            // Seconds between path recalculations
    float pathRequestCooldown = 5.0f;             // Minimum seconds between path requests

    // Crowd awareness
    float crowdCheckInterval = 2.0f;              // Seconds between crowd density checks
    float lateralOffsetDistance = 48.0f;          // Perpendicular offset to reduce clumping

    // Stall recovery
    float stallSpeedMultiplier = 0.5f;            // Fraction of chase speed to trigger stall detection
    float stallTimeout = 2.0f;                    // Seconds stalled before recovery attempt
    float jitterAmount = 12.0f;                   // Random position offset when stuck (px)

    // Line-of-sight
    float losCheckInterval = 0.5f;                // Seconds between LOS checks
    float catchRadius = 20.0f;                    // Distance to consider target "caught"

    // Movement range/nav parameters
    float navRadius{64.0f};                       // Waypoint reached radius (px)
    float maxChaseRange{1000.0f};                 // Maximum range to chase target (px)
    float minChaseRange{50.0f};                   // Minimum range (stop chasing when within) (px)
    float speedMultiplier{1.1f};                  // Urgent movement multiplier applied to moveSpeed

    // Factory method for event targeting
    static ChaseBehaviorConfig createEventTarget()
    {
        ChaseBehaviorConfig config;
        config.pathRefreshInterval = 5.0f;        // Fast updates for event
        config.catchRadius = 50.0f;               // Larger arrival radius
        return config;
    }
};

/**
 * Configuration for PatrolBehavior
 *
 * Controls how entities patrol between waypoints with obstacle avoidance
 * and stuck recovery.
 */
struct PatrolBehaviorConfig
{
    // Movement parameters
    float moveSpeed = 40.0f;                      // Speed when patrolling between waypoints

    // Waypoint parameters
    float waypointReachedRadius = 32.0f;          // Distance to consider waypoint reached
    float waypointCooldown = 0.75f;               // Seconds to wait at waypoint before advancing
    int randomWaypointGenerationAttempts = 50;    // Max attempts to find valid random waypoint

    // Pathfinding parameters
    float pathRequestCooldown = 15.0f;            // Minimum seconds between path requests
    float pathRequestCooldownVariation = 3.0f;    // Random variation added to cooldown (0-3s)

    // Stall recovery
    float stallSpeedMultiplier = 0.3f;            // Fraction of move speed to trigger stall detection
    float sidestepDistance = 64.0f;               // Distance to sidestep when stalled
    float advanceWaypointDelay = 1.5f;            // Seconds stalled before skipping to next waypoint

    // Boundary padding
    float boundaryPadding = 80.0f;                // Keep patrol paths this far from world edges

    // Update throttling (peaceful behavior — heavy logic runs at this interval)
    float updateInterval{3.0f};                   // Seconds between full movement updates

    // Factory methods for common presets
    static PatrolBehaviorConfig createRandomPatrol()
    {
        PatrolBehaviorConfig config;
        config.randomWaypointGenerationAttempts = 100;  // More attempts for variety
        config.waypointCooldown = 1.5f;                 // Longer pause at points
        return config;
    }

    static PatrolBehaviorConfig createCirclePatrol(float radius = 200.0f)
    {
        PatrolBehaviorConfig config;
        config.waypointReachedRadius = radius * 0.15f;  // Scale with circle size
        config.waypointCooldown = 0.5f;                 // Quick transitions
        config.boundaryPadding = radius * 0.1f;
        return config;
    }
};

/**
 * Configuration for FleeBehavior
 *
 * Controls how entities flee from threats using pathfinding to escape.
 */
struct FleeBehaviorConfig
{
    // Movement parameters
    float fleeSpeed = 70.0f;                      // Speed when fleeing from threat

    // Flee distance parameters
    float safeDistance = 400.0f;                  // Distance to flee to be considered "safe"
    float worldPadding = 80.0f;                   // Keep flee goals this far from world edges

    // Pathfinding parameters
    float pathTTL = 2.5f;                         // Path time-to-live in seconds
    float noProgressWindow = 0.4f;                // Seconds without progress before repath
    float goalChangeThreshold = 180.0f;           // Distance threat must move to trigger repath

    // Stamina system
    float maxStamina = 100.0f;                    // Maximum stamina pool
    float staminaDrain = 10.0f;                   // Stamina drain per second while fleeing
    float staminaRecovery = 5.0f;                 // Stamina recovery per second when not fleeing
    bool useStamina = true;                       // Whether stamina affects flee speed

    // Panic settings
    float panicDuration = 5.0f;                   // Duration of panic mode in seconds
    float panicSpeedMultiplier = 1.3f;            // Speed multiplier during panic

    // Zigzag/evasive settings
    float zigzagInterval = 0.5f;                  // Seconds between zigzag direction changes
    float zigzagAngle = 30.0f;                    // Angle of zigzag deviation in degrees

    // Strategic retreat
    float strategicSpeedMultiplier = 0.8f;        // Speed multiplier for strategic retreat

    // Additional flee parameters
    float baseFleeSpeedMultiplier{1.3f};          // Urgent movement multiplier applied to moveSpeed
    float distressBroadcastInterval{2.0f};        // Seconds between distress calls while fleeing
    float navRadius{18.0f};                       // Waypoint reached radius (px)
    float strategicRetreatDistance{800.0f};       // Base retreat destination distance (px)
    float coverSeekDistance{720.0f};              // Base cover-seek destination distance (px)
};

/**
 * Configuration for FollowBehavior
 *
 * Controls how entities follow a leader with distance management.
 */
struct FollowBehaviorConfig
{
    // Movement parameters
    float followSpeed = 50.0f;                    // Speed when following leader
    float followDistance = 100.0f;                // Desired distance to maintain from leader

    // Catchup parameters
    float catchupRange = 200.0f;                  // Distance at which to slow down for stragglers

    // Pathfinding parameters
    float nodeRadius = 20.0f;                     // Waypoint reached radius
    float pathTTL = 10.0f;                        // Path validity duration
    float goalChangeThreshold = 200.0f;           // Distance leader must move to trigger repath

    // Stall recovery
    float stallSpeedMultiplier = 0.5f;            // Fraction of follow speed to trigger stall
    float stallTimeout = 0.6f;                    // Seconds stalled before recovery

    // Arrival
    float arrivalRadius = 25.0f;                  // Distance to consider arrived at leader
    float velocityThreshold = 10.0f;              // Speed threshold for arrival detection

    // Speed adjustment parameters
    float pathCooldown{1.0f};                     // Seconds between path requests
    float catchupSpeedMultiplier{1.3f};           // Speed multiplier when far behind leader
    float slowdownSpeedMultiplier{0.5f};          // Speed multiplier when too close to leader

    // Update throttling (more responsive than wander/patrol since tracking a moving target)
    float updateInterval{1.0f};                   // Seconds between full movement updates
};

/**
 * Configuration for GuardBehavior
 *
 * Controls how entities guard a position and return to it after threats.
 */
struct GuardBehaviorConfig
{
    // Movement parameters
    float guardSpeed = 45.0f;                     // Base speed when guarding

    // Guard parameters
    float guardRadius = 50.0f;                    // Radius around guard position to patrol
    float returnToGuardDelay = 3.0f;              // Seconds after threat before returning

    // Pathfinding parameters
    float pathTTL = 1.8f;                         // Path validity duration in seconds
    float goalChangeThreshold = 64.0f;            // Distance to trigger path recalculation

    // Stall recovery
    float stallSpeedMultiplier = 0.5f;            // Fraction of guard speed to trigger stall

    // Mode-specific speed multipliers (applied to guardSpeed)
    float staticSpeedMultiplier = 1.0f;           // Speed when static (usually 0 movement)
    float patrolSpeedMultiplier = 1.5f;           // Speed when patrolling waypoints
    float areaSpeedMultiplier = 1.2f;             // Speed when guarding an area
    float roamingSpeedMultiplier = 1.8f;          // Speed when roaming freely
    float alertSpeedMultiplier = 2.5f;            // Speed when in alert mode

    // Alert radius multipliers (affect detection range)
    float staticAlertRadiusMultiplier = 1.5f;     // Detection range when static
    float patrolAlertRadiusMultiplier = 1.8f;     // Detection range when patrolling
    float areaAlertRadiusMultiplier = 2.0f;       // Detection range when area guarding
    float roamingAlertRadiusMultiplier = 1.6f;    // Detection range when roaming
    float alertModeAlertRadiusMultiplier = 2.5f;  // Detection range in alert mode

    // Investigation and alert
    float investigationTime = 10.0f;              // Seconds to investigate before giving up
    float alertDecayTime = 15.0f;                 // Seconds for alert level to decay one level
    float returnToPostTime = 5.0f;                // Delay before returning to post after all-clear

    // Help call system
    bool canCallForHelp = true;                   // Whether guard can call nearby guards
    float helpCallRadius = 300.0f;                // Radius to call for help
    int guardGroupId = 0;                         // Group ID for coordinated response

    // ALARM escalation (alert level 4)
    float alarmEscalationTime{8.0f};              // Seconds at HOSTILE before escalating to ALARM
    float alarmHelpCallRadius{600.0f};            // Wider scan radius for ALARM state
};

/**
 * Configuration for AttackBehavior
 *
 * Defines all parameters for attack behavior modes. Each mode has preset
 * configurations that can be created via static factory methods.
 */
struct AttackBehaviorConfig
{
    // Range parameters (in pixels)
    float attackRange = 80.0f;                    // Maximum attack range
    float optimalRangeMultiplier = 0.8f;          // Optimal range as % of attack range
    float minimumRangeMultiplier = 0.3f;          // Minimum range as % of attack range

    // Combat parameters
    float attackSpeed = 1.0f;                     // Attacks per second
    float movementSpeed = 55.0f;                  // Movement speed during combat (px/s)
    float attackCooldown = 1.0f;                  // Seconds between attacks
    float recoveryTime = 0.5f;                    // Seconds to recover after attack

    // Damage parameters
    float attackDamage = 10.0f;                   // Base damage per attack
    float damageVariation = 0.2f;                 // ±20% damage variation
    float criticalHitChance = 0.1f;               // 10% chance for critical hit
    float criticalHitMultiplier = 2.0f;           // Critical hit damage multiplier
    float knockbackForce = 50.0f;                 // Knockback force on hit

    // Positioning parameters
    bool circleStrafe = false;                    // Enable circle strafing around target
    float strafeRadius = 100.0f;                  // Radius for circle strafing (px)
    bool flankingEnabled = true;                  // Attempt to flank target
    float preferredAttackAngle = 0.0f;            // Preferred attack angle in radians

    // Tactical parameters
    float retreatThreshold = 0.3f;                // Retreat when health drops below 30%
    float aggression = 0.7f;                      // 70% aggression (affects decision making)
    bool teamwork = true;                         // Coordinate with allies
    bool avoidFriendlyFire = true;                // Avoid hitting allies

    // Special abilities
    bool comboAttacks = false;                    // Enable combo attack system
    int maxCombo = 3;                             // Maximum combo chain length
    float specialAttackChance = 0.15f;            // 15% chance for special attack
    float aoeRadius = 0.0f;                       // Area of effect radius (0 = disabled)

    // Mode-specific parameters
    uint8_t attackMode = 0;                       // 0=Melee, 1=Ranged (matches AttackMode enum in AttackBehavior.cpp)
    float projectileSpeed = 250.0f;               // Ranged projectile speed (px/s)
    float chargeDamageMultiplier = 1.5f;          // Damage multiplier for charge attacks

    /**
     * Create configuration for MELEE_ATTACK mode
     * Close-range combat with high mobility.
     */
    static AttackBehaviorConfig createMeleeConfig(float baseRange = 100.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange;
        config.optimalRangeMultiplier = 0.8f;
        config.minimumRangeMultiplier = 0.3f;
        config.attackSpeed = 1.2f;
        config.movementSpeed = 60.0f;
        config.attackDamage = 10.0f;
        return config;
    }

    /**
     * Create configuration for RANGED_ATTACK mode
     * Long-range combat with kiting behavior.
     */
    static AttackBehaviorConfig createRangedConfig(float baseRange = 200.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange;
        config.optimalRangeMultiplier = 0.7f;
        config.minimumRangeMultiplier = 0.4f;
        config.attackSpeed = 0.8f;
        config.movementSpeed = 50.0f;
        config.attackDamage = 8.0f;
        config.attackMode = 1;           // RANGED
        config.projectileSpeed = 250.0f;
        return config;
    }

    /**
     * Create configuration for CHARGE_ATTACK mode
     * High-speed charge with increased damage.
     */
    static AttackBehaviorConfig createChargeConfig(float baseRange = 150.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange * 1.5f;
        config.optimalRangeMultiplier = 1.0f;    // Optimal range is max range for charge
        config.minimumRangeMultiplier = 0.0f;    // No minimum for charge
        config.attackSpeed = 0.5f;
        config.movementSpeed = 80.0f;
        config.attackDamage = 15.0f;
        config.chargeDamageMultiplier = 2.0f;
        return config;
    }

    /**
     * Create configuration for AMBUSH_ATTACK mode
     * Stealth-based attacks with high critical hit chance.
     */
    static AttackBehaviorConfig createAmbushConfig(float baseRange = 80.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange;
        config.optimalRangeMultiplier = 0.6f;
        config.minimumRangeMultiplier = 0.3f;
        config.attackSpeed = 2.0f;
        config.movementSpeed = 40.0f;
        config.criticalHitChance = 0.3f;
        config.attackDamage = 12.0f;
        return config;
    }

    /**
     * Create configuration for COORDINATED_ATTACK mode
     * Team-based combat with flanking.
     */
    static AttackBehaviorConfig createCoordinatedConfig(float baseRange = 80.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange;
        config.optimalRangeMultiplier = 0.8f;
        config.minimumRangeMultiplier = 0.3f;
        config.attackSpeed = 1.0f;
        config.movementSpeed = 55.0f;
        config.teamwork = true;
        config.flankingEnabled = true;
        config.attackDamage = 10.0f;
        return config;
    }

    /**
     * Create configuration for HIT_AND_RUN mode
     * High mobility with frequent retreats.
     */
    static AttackBehaviorConfig createHitAndRunConfig(float baseRange = 80.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange;
        config.optimalRangeMultiplier = 0.8f;
        config.minimumRangeMultiplier = 0.3f;
        config.attackSpeed = 1.5f;
        config.movementSpeed = 70.0f;
        config.retreatThreshold = 0.8f;          // Retreat early
        config.attackDamage = 8.0f;
        return config;
    }

    /**
     * Create configuration for BERSERKER_ATTACK mode
     * Aggressive close-combat with combo attacks.
     */
    static AttackBehaviorConfig createBerserkerConfig(float baseRange = 100.0f)
    {
        AttackBehaviorConfig config;
        config.attackRange = baseRange;
        config.optimalRangeMultiplier = 0.8f;
        config.minimumRangeMultiplier = 0.3f;
        config.attackSpeed = 1.8f;
        config.movementSpeed = 65.0f;
        config.aggression = 1.0f;               // Maximum aggression
        config.retreatThreshold = 0.1f;         // Almost never retreat
        config.comboAttacks = true;
        config.maxCombo = 5;
        config.attackDamage = 12.0f;
        return config;
    }
};

// ============================================================================
// BEHAVIOR CONFIG DATA UNION
// ============================================================================

/**
 * @brief Unified behavior configuration storage (data-oriented design)
 *
 * Stores the configuration for any behavior type in a single union.
 * Config is read-only during behavior execution - set once when behavior assigned.
 * Paired with BehaviorData (state) in EDM for complete behavior storage.
 *
 * Usage:
 *   auto config = BehaviorConfigData::makeWander({.speed = 50.0f});
 *   edm.reassignBehaviorConfig(edmIndex, config);
 */
struct BehaviorConfigData {
    BehaviorType type{static_cast<BehaviorType>(0xFF)};  // BehaviorType::None
    uint8_t _pad[3]{};

    union ConfigUnion {
        ConfigUnion() : raw{} {}

        IdleBehaviorConfig idle;
        WanderBehaviorConfig wander;
        ChaseBehaviorConfig chase;
        PatrolBehaviorConfig patrol;
        GuardBehaviorConfig guard;
        AttackBehaviorConfig attack;
        FleeBehaviorConfig flee;
        FollowBehaviorConfig follow;

        uint8_t raw[384];  // Sized to accommodate largest config (AttackBehaviorConfig)
    } params;

    BehaviorConfigData() = default;

    // Factory methods for type-safe construction
    static BehaviorConfigData makeIdle(const IdleBehaviorConfig& cfg = {});
    static BehaviorConfigData makeWander(const WanderBehaviorConfig& cfg = {});
    static BehaviorConfigData makeChase(const ChaseBehaviorConfig& cfg = {});
    static BehaviorConfigData makePatrol(const PatrolBehaviorConfig& cfg = {});
    static BehaviorConfigData makeGuard(const GuardBehaviorConfig& cfg = {});
    static BehaviorConfigData makeAttack(const AttackBehaviorConfig& cfg = {});
    static BehaviorConfigData makeFlee(const FleeBehaviorConfig& cfg = {});
    static BehaviorConfigData makeFollow(const FollowBehaviorConfig& cfg = {});
};

// Inline factory implementations
inline BehaviorConfigData BehaviorConfigData::makeIdle(const IdleBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Idle;
    data.params.idle = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makeWander(const WanderBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Wander;
    data.params.wander = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makeChase(const ChaseBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Chase;
    data.params.chase = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makePatrol(const PatrolBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Patrol;
    data.params.patrol = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makeGuard(const GuardBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Guard;
    data.params.guard = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makeAttack(const AttackBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Attack;
    data.params.attack = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makeFlee(const FleeBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Flee;
    data.params.flee = cfg;
    return data;
}

inline BehaviorConfigData BehaviorConfigData::makeFollow(const FollowBehaviorConfig& cfg) {
    BehaviorConfigData data;
    data.type = BehaviorType::Follow;
    data.params.follow = cfg;
    return data;
}

} // namespace VoidLight

#endif // BEHAVIOR_CONFIG_HPP
