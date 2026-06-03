# Emergent Gameplay Analysis

**Code:** `include/ai/AIBehavior.hpp`, `include/managers/EventManager.hpp`, `include/managers/EntityDataManager.hpp`, `include/managers/WorldResourceManager.hpp`

Analysis of SDL3 VoidLight-Framework's current capabilities and recommendations for supporting emergent gameplay systems.

**Date**: January 2026
**Status**: Analysis Complete

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current Capabilities](#current-capabilities)
3. [Architectural Strengths](#architectural-strengths)
4. [Identified Gaps](#identified-gaps)
5. [Recommendations](#recommendations)
6. [Emergent Scenarios](#emergent-scenarios)
7. [Implementation Priorities](#implementation-priorities)
8. [Key Files Reference](#key-files-reference)

---

## Executive Summary

SDL3 VoidLight-Framework has **strong foundational support** for emergent gameplay through:

- Data-oriented entity architecture with decoupled systems
- Event-driven communication enabling system interplay
- Modular AI behaviors with configuration-driven thresholds
- Environmental systems (weather, time, resources) that can affect gameplay

**Primary gaps** preventing deeper emergence:
- No faction/relationship system for group dynamics
- Limited autonomous behavior switching
- No NPC memory/knowledge persistence
- Resources don't influence AI decisions

---

## Current Capabilities

### AI Behavior System

The engine implements 8 modular behaviors in `include/ai/behaviors/`:

| Behavior | Emergent Features |
|----------|-------------------|
| **GuardBehavior** | 5-level alert cascade (CALM → SUSPICIOUS → INVESTIGATING → HOSTILE → ALARM) with `callForHelp()` broadcasts to nearby guards |
| **AttackBehavior** | 7 attack modes (MELEE, RANGED, CHARGE, AMBUSH, COORDINATED, HIT_AND_RUN, BERSERKER) with combo system, flanking, circle strafing |
| **FleeBehavior** | 4 modes including SEEK_COVER that distributes fleeing NPCs to safe zones |
| **FollowBehavior** | 5 formation modes with atomic slot assignment for self-organization |
| **WanderBehavior** | Crowd escape triggers when `cachedNearbyCount > crowdEscapeThreshold` |
| **PatrolBehavior** | Dynamic waypoint generation with `expandPatrolAreaIfCrowded()` |
| **ChaseBehavior** | Line-of-sight tracking with last-known-position fallback |
| **IdleBehavior** | 4 visual variety modes for natural NPC appearance |

**Key Pattern**: `BehaviorContext` (`include/ai/AIBehavior.hpp`) provides lock-free EDM access:

```cpp
struct BehaviorContext {
    TransformData& transform;       // Direct R/W (no locks)
    EntityHotData& hotData;         // Metadata
    size_t edmIndex;                // Vector-based state access
    BehaviorData* behaviorData;     // Behavior-specific state
    PathData* pathData;             // Navigation state
    // Cached player info, world bounds...
};
```

### Event System

`EventManager` (`include/managers/EventManager.hpp`) supports 15+ event types with:

- Type-indexed dispatch (O(1) routing via `EventTypeId` enum)
- Priority levels: CRITICAL (1000) → HIGH (800) → NORMAL (500) → LOW (200) → DEFERRED (0)
- Event pools for hot-path event objects such as particle effects
- Token-based handler registration for loose coupling

**Existing Event Chains**:
```
Time Progression:
  GameTimeManager → HourChangedEvent → DayNightController
                  → SeasonChangedEvent → TileRenderer (texture swap)
                  → WeatherCheckEvent → WeatherController → ParticleManager

World Changes:
  WorldManager::updateTile() → TileChangedEvent → CollisionManager
                                                → PathfinderManager (grid invalidation)

Combat:
  DamageEvent → Handler reduces health → DeathEvent (if lethal)
             → Loot spawn → ResourceChangeEvent
```

### Environmental Systems

| System | Location | Emergent Capability |
|--------|----------|---------------------|
| **GameTimeManager** | `include/managers/GameTimeManager.hpp` | Season-specific weather probability tables |
| **WorldGenerator** | `include/world/WorldGenerator.hpp` | Biome-based procedural content |
| **BackgroundSimulationManager** | `include/managers/BackgroundSimulationManager.hpp` | 3-tier simulation (Active/Background/Hibernated) |
| **WorldResourceManager** | `include/managers/WorldResourceManager.hpp` | Spatial registry over EDM-backed world resources |
| **CollisionManager** | `include/managers/CollisionManager.hpp` | Trigger system for zone-based events |

---

## Architectural Strengths

### 1. Data-Oriented Design

`EntityDataManager` uses Structure of Arrays (SoA) layout:

```cpp
struct EntityHotData {  // 64 bytes - one cache line
    TransformData transform;      // 32 bytes
    float halfWidth, halfHeight;  // 8 bytes
    EntityKind kind;              // 1 byte
    SimulationTier tier;          // 1 byte
    uint8_t flags;                // 1 byte
    // ... collision data, padding
};
```

**Benefits**:
- Batch processing 10K+ entities at 60 FPS
- Cache-friendly iteration for emergent queries
- Lock-free parallel behavior execution

### 2. Deferred Event Pattern

Events dispatched then processed in batches:
- Cascading event chains without stack overflow
- Priority-based processing order
- Pooled allocations for frequent events

### 3. Trigger System

`CollisionManager` trigger capabilities:
- `TriggerTag` enum for named trigger types (Water, Hazard, etc.)
- `TriggerType`: EventOnly (no physics) or Physical
- Per-entity trigger cooldowns
- Automatic trigger creation for water tiles and obstacles

### 4. Simulation Tiers

```cpp
enum class SimulationTier {
    Active,      // Full AI/collision/render (near player)
    Background,  // Position only, reduced frequency
    Hibernated   // Data stored, no updates
};
```

Enables large world populations with graceful degradation.

---

## Identified Gaps

### 1. No Faction/Relationship System

**Current State**: NPCs have individual behaviors but no inter-entity relationships. Guards can alert nearby guards, but cannot distinguish friend from foe beyond hardcoded checks.

**Impact**:
- No emergent faction wars or territorial behavior
- No reputation-based NPC reactions
- No alliance formation or betrayal mechanics

### 2. Limited Behavior Transitions

**Current State**: Behavior switching requires external triggers - game logic must explicitly assign new behaviors.

**Impact**:
- NPCs cannot autonomously decide to switch behaviors based on world state
- No utility-based decision making
- Behavior selection is reactive, not proactive

### 3. No Memory/Knowledge System

**Current State**: NPCs have `lastKnownTargetPos` in chase state but no persistent memory across behavior changes.

**Impact**:
- Guards forget threats immediately after losing sight
- NPCs cannot learn patterns or remember past encounters
- No information sharing between NPCs

### 4. Resources Don't Affect Behavior

**Current State**: `WorldResourceManager` tracks resources, but AI behaviors don't query or react to resource state.

**Impact**:
- NPCs don't compete for resources
- No emergent economy or scarcity-driven behavior
- No survival mechanics

### 5. No Environmental Influence on AI

**Current State**: Weather and time events exist but don't modify behavior parameters.

**Impact**:
- Rain doesn't reduce visibility/detection range
- Night doesn't make stealth easier
- No weather-based behavioral changes

---

## Recommendations

### Priority 1: Faction System

Add `FactionData` to EntityDataManager:

```cpp
struct FactionData {
    uint8_t factionId;                        // Which faction (0-255)
    std::array<int8_t, 16> relationships;     // -100 (hostile) to +100 (allied)
    uint8_t flags;                            // Neutral to outsiders, etc.
};

// In EntityDataManager
std::vector<FactionData> m_factionData;       // Indexed by EDM index
```

**Enables**:
- Group dynamics and territorial behavior
- Reputation systems
- Emergent alliances and conflicts
- Faction-based alert propagation

**Integration Points**:
- `GuardBehavior::callForHelp()` - only alert same faction
- `AttackBehavior` - check faction before attacking
- `FleeBehavior` - flee toward allied faction territory

### Priority 2: Utility AI Layer

Add utility scoring on top of current behaviors:

```cpp
struct BehaviorUtility {
    BehaviorType type;
    float (*calculateUtility)(const BehaviorContext& ctx, const UtilityFactors& factors);
};

struct UtilityFactors {
    float healthPercent;
    float nearbyThreatLevel;
    float resourceNeed;
    float distanceToGoal;
    // ...
};

// In AIManager::update()
BehaviorType selectBehavior(const BehaviorContext& ctx) {
    float bestScore = 0;
    BehaviorType best = BehaviorType::Idle;
    for (const auto& utility : m_utilities) {
        float score = utility.calculateUtility(ctx, factors);
        if (score > bestScore) {
            bestScore = score;
            best = utility.type;
        }
    }
    return best;
}
```

**Enables**:
- Autonomous behavior switching based on world state
- Configurable NPC "personalities" via utility weights
- Natural priority handling (flee when health low, attack when advantaged)

### Priority 3: NPC Memory System

Add `MemoryData` to EntityDataManager:

```cpp
struct MemoryEntry {
    EntityHandle observed;        // Who was seen
    Vector2D lastPosition;        // Where they were
    float timestamp;              // Game time when seen
    uint8_t threatLevel;          // Perceived threat (0-255)
    uint8_t flags;                // Hostile, friendly, neutral
};

struct MemoryData {
    std::array<MemoryEntry, 16> memories;    // Ring buffer
    uint8_t writeIndex;
    uint8_t count;

    void remember(const MemoryEntry& entry);
    const MemoryEntry* recall(EntityHandle target) const;
    void forget(float olderThan);
};
```

**Enables**:
- Guards remember threat locations after losing sight
- NPCs can share information ("I saw the player heading north")
- Learning patterns over time
- Grudges and long-term relationships

### Priority 4: Resource-Driven AI

Connect resource events to behavior triggers:

```cpp
// In behavior executeLogic or utility calculation:
float resourceNeed = calculateResourceNeed(ctx);
if (resourceNeed > threshold) {
    // High utility for ForageBehavior or resource-seeking
}

// Resource scarcity affects aggression:
float localResources = worldResources.getAreaTotal(myPosition, radius);
if (localResources < survivalThreshold) {
    aggressionModifier += 0.3f;  // More likely to attack for resources
}
```

**Enables**:
- Survival mechanics
- Resource competition and territorial disputes
- Migration patterns based on resource availability
- Economic simulation

### Priority 5: Environmental Influence on AI

Connect weather/time events to behavior parameters:

```cpp
// In AIManager or behavior configs:
void onWeatherChanged(const WeatherEvent& event) {
    float visibilityMod = 1.0f - (event.params.intensity * 0.5f);  // Rain reduces visibility
    for (auto& config : m_guardConfigs) {
        config.detectionRange *= visibilityMod;
    }
}

void onTimePeriodChanged(TimePeriod period) {
    if (period == TimePeriod::Night) {
        m_globalDetectionModifier = 0.6f;  // Harder to see at night
        m_globalAggressionModifier = 0.8f; // More cautious
    }
}
```

**Enables**:
- Weather-based stealth opportunities
- Day/night behavioral changes
- Seasonal behavior patterns
- Environmental storytelling

---

## Emergent Scenarios

### Scenario 1: Guard Post Assault (Currently Achievable)

```
1. Player enters guard detection range (250px)
2. GuardBehavior: CALM → SUSPICIOUS → INVESTIGATING → HOSTILE
3. Guard triggers callForHelp() broadcast
4. Nearby guards receive AlertMessage via onMessage()
5. Guards switch to ALERT_GUARD mode, converge on threat
6. Separation steering prevents clumping during approach
7. Guards in attack range → AttackBehavior (COORDINATED_ATTACK)
8. Combat: combo system, circle strafing, flanking maneuvers
9. Guard health < 30% → RETREATING state → FleeBehavior
10. Safe distance reached → return to patrol post
```

### Scenario 2: Faction War (With Faction System)

```
1. Two factions with negative relationship (-80)
2. Patrol routes overlap at border region
3. Guard detects enemy faction NPC
4. callForHelp() broadcasts to same-faction guards only
5. Enemy guards also call for help
6. Both sides converge → emergent battle
7. Losing side retreats (FleeBehavior)
8. Winners may pursue or return to territory
9. Relationship worsens (-90) from casualties
```

### Scenario 3: Resource Scarcity (With Resource-Driven AI)

```
1. Forest biome resources depleted
2. NPCs in area calculate high resourceNeed
3. Utility AI scores ForageBehavior/MigrateBehavior high
4. NPCs begin moving toward resource-rich areas
5. Competition for remaining resources increases aggression
6. Some NPCs switch to raiding behavior (attack + steal)
7. Emergent migration patterns form
8. Previously peaceful NPCs become hostile
```

### Scenario 4: Stealth and Memory (With Memory System)

```
1. Player sneaks past guard at night (reduced detection)
2. Guard catches glimpse → stores MemoryEntry
3. Player hides → guard loses sight
4. Guard doesn't forget → investigates last known position
5. Guard shares memory with patrol partner
6. Both guards now aware, search pattern expands
7. Player waits → memory fades after timeout
8. Guards return to normal patrol
```

---

## Implementation Priorities

| Priority | System | Effort | Impact | Dependencies |
|----------|--------|--------|--------|--------------|
| **1** | Faction System | Medium | High | EDM extension |
| **2** | Utility AI Layer | Medium | High | None |
| **3** | NPC Memory | Low | Medium | EDM extension |
| **4** | Resource-Driven AI | Low | Medium | WorldResourceManager events |
| **5** | Environmental Influence | Low | Medium | Weather/Time event subscriptions |

### Suggested Implementation Order

1. **Faction System** - Foundation for all social emergence
2. **Utility AI** - Enables autonomous decision-making
3. **Memory System** - Adds persistence and learning
4. **Resource Integration** - Connects economy to behavior
5. **Environmental Influence** - Adds weather/time dynamics

---

## Key Files Reference

### Core Architecture
| File | Purpose |
|------|---------|
| `include/managers/EntityDataManager.hpp` | Single source of truth for entity data (extend for new systems) |
| `include/managers/AIManager.hpp` | Behavior lifecycle and batch processing |
| `include/ai/AIBehavior.hpp` | Base behavior class and BehaviorContext |
| `include/managers/EventManager.hpp` | Type-indexed event dispatch |

### Existing Behaviors
| File | Purpose |
|------|---------|
| `include/ai/behaviors/GuardBehavior.hpp` | Alert cascade, callForHelp() |
| `include/ai/behaviors/AttackBehavior.hpp` | 7 attack modes, combo system |
| `include/ai/behaviors/FleeBehavior.hpp` | Safe zone seeking |
| `include/ai/behaviors/WanderBehavior.hpp` | Crowd awareness |
| `include/ai/BehaviorConfig.hpp` | All behavior configurations |

### Environmental Systems
| File | Purpose |
|------|---------|
| `include/managers/GameTimeManager.hpp` | Time/season/weather progression |
| `include/managers/WorldResourceManager.hpp` | Resource tracking |
| `include/managers/BackgroundSimulationManager.hpp` | Simulation tiers |
| `include/controllers/world/WeatherController.hpp` | Weather event handling |

### Event Types
| File | Purpose |
|------|---------|
| `include/events/WeatherEvent.hpp` | Weather parameters and conditions |
| `include/events/TimeEvent.hpp` | Time period transitions |
| `include/events/EntityEvents.hpp` | Damage, death, spawn events |

---

## Conclusion

SDL3 VoidLight-Framework has a solid architectural foundation for emergent gameplay. The event-driven design, modular behavior system, and data-oriented entity management provide the infrastructure needed.

The primary gaps are in **social systems** (factions, relationships), **autonomous decision-making** (utility AI), and **system interconnection** (resources/environment affecting AI).

Implementing the recommended systems in priority order will transform the current reactive NPC behavior into a living world where complex behaviors emerge from simple rules and system interactions.

## Related Docs

- [AIManager](ai/AIManager.md)
- [Behavior Execution Pipeline](ai/BehaviorExecutionPipeline.md)
- [EventManager](events/EventManager.md)
- [WorldResourceManager](managers/WorldResourceManager.md)
