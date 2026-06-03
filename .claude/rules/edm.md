---
paths:
  - "include/managers/EntityDataManager*"
  - "src/managers/EntityDataManager*"
  - "include/managers/AIManager*"
  - "src/managers/AIManager*"
  - "include/ai/**"
  - "src/ai/**"
  - "**/Behavior*"
  - "**/BehaviorExecutors*"
---

# EDM, AI, and Behavior Rules

## EDM (EntityDataManager) Patterns

**Pure data storage** — stores/retrieves/aggregates entity state. AI decision logic belongs in `Behaviors::` (`BehaviorExecutors.hpp/.cpp`).

- **State, not policy**: EDM fields describe what the entity *is* (position, health, timers, emotion state). Thresholds, weights, tuning, and decision policy belong in the behavior layer or config — never EDM.
- **BehaviorContext access**: `ctx.behaviorData` (state), `ctx.pathData` (navigation), `ctx.memoryData` (combat/emotions) — all pre-fetched in `processBatch()`.
- **Combat/emotion split**: `EDM::recordCombatEvent()` records stats/memory only. Emotion math belongs outside EDM in AI/behavior code. Witnessed combat/death memories are behavior-consumed state; EDM stores memory records only. `AIManager::update()` commits command-bus changes and caches world/player data on the main thread before worker batches; behavior execution and emotional decay run in the AI batch path.
- **Controller → AI boundary**: Controllers MUST NEVER directly mutate AI behavior state in EDM. Main thread: `Behaviors::queueBehaviorMessage(idx, BehaviorMessage::X)`. Worker threads: `Behaviors::deferBehaviorMessage()`.
- **Cross-frame state** (paths, timers): MUST live in EDM, never local variables — temporaries die each frame, causing infinite recomputation.

## AI Behavior Switching

`Behaviors::switchBehavior()` enqueues a transition. `AIManager::commitQueuedBehaviorTransitions()` performs `clearBehaviorData()` → `reassignBehaviorConfig()` → `init()`. State set before the transition commit is wiped — always set new behavior state after the switch. Behavior configs live in per-variant dense pools on EDM; access via `getBehaviorConfigRef(idx)` + `get<Variant>Config(ref.index)`.
