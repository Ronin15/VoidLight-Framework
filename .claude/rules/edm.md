---
paths:
  - "include/managers/EntityDataManager*"
  - "src/managers/EntityDataManager*"
  - "include/managers/EntityDataTypes*"
  - "include/managers/AIManager*"
  - "src/managers/AIManager*"
  - "include/ai/**"
  - "src/ai/**"
  - "**/Behavior*"
---

# EDM, AI, and Behavior Quick Reference

Canonical rules: root `CLAUDE.md` ("EDM, AI, and Controllers") plus the
nested `include/ai/CLAUDE.md`, `src/ai/CLAUDE.md`,
`include/managers/CLAUDE.md`, and `src/managers/CLAUDE.md`. Those win on
conflict. This file only adds concrete API anchors.

- **EDM is storage, not policy.** Fields describe what an entity *is*.
  Thresholds, weights, tuning, and decisions live in `Behaviors::`
  (`BehaviorExecutors.hpp/.cpp`, `src/ai/behaviors/`) or config.
- **Expansion rule** (`EntityDataTypes.hpp`): every-frame fields go on the
  hot/character line; "some NPCs, sometimes" data goes in a
  `SparseSidecar`, cleared on destroy and `prepareForStateTransition()`.
- **`BehaviorContext`** (batch contract, `include/ai/BehaviorExecutors.hpp`):
  `ctx.sharedState` (`BehaviorData`), `ctx.pathData` (optional
  `PathData*`), `ctx.memoryData`, `ctx.characterData`, and the by-value
  `ctx.envSnapshot` filled on the main thread. Executors never call
  `GameTimeManager` / `WeatherController`.
- **Behavior switching:** `Behaviors::switchBehavior()` enqueues.
  `AIManager::commitQueuedBehaviorTransitions()` runs
  `edm.clearBehaviorData()` → `edm.reassignBehaviorConfig()` →
  `Behaviors::init()`. Anything set before the commit is wiped — set new
  behavior state after it.
- **Behavior configs** live in per-variant dense pools on EDM:
  `getBehaviorConfigRef(idx)` then `get<Variant>Config(ref.index)`.
- **Messages:** main thread `Behaviors::queueBehaviorMessage()`; workers
  `Behaviors::deferBehaviorMessage()`. Controllers never write AI state in
  EDM directly.
- **Cross-frame state** (paths, timers) lives in EDM, never in locals.
