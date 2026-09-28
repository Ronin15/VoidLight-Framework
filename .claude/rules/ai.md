---
paths:
  - "include/ai/**"
  - "src/ai/**"
  - "**/Behavior*"
  - "**/AIManager*"
---

# AI Contracts and Implementation

Applies to AI headers (`include/ai/`), AI source (`src/ai/`), behavior
code anywhere, and `AIManager`. Root `CLAUDE.md` still applies. Manager
and EDM storage rules are in `.claude/rules/managers.md`.

## Ownership and Data Flow

- AI decision logic belongs in `Behaviors::` functions and behavior
  executors (`BehaviorExecutors.hpp/.cpp`, `src/ai/behaviors/`). EDM
  stores behavior, path, character, and memory data; it is never the
  policy owner. Do not move tuning into EDM just to make it reachable.
- Behavior config describes authored or assigned intent. Behavior state
  stores runtime progress. Shared behavior data is only for state that is
  actually shared across behaviors. Configs live in per-variant dense EDM
  pools: `getBehaviorConfigRef(idx)` then `get<Variant>Config(ref.index)`.
- `BehaviorContext` (`include/ai/BehaviorExecutors.hpp`) is the batch-time
  contract between `AIManager` and executors: `ctx.sharedState`
  (`BehaviorData`), `ctx.pathData` (optional `PathData*`),
  `ctx.memoryData`, `ctx.characterData`, and the by-value
  `ctx.envSnapshot` filled on the main thread. Keep it explicit about
  cached frame data, EDM refs, optional state, and thread-safety.
  Executors use context data and never call `GameTimeManager` or
  `WeatherController`, and never bypass the context with worker-side
  world/player queries unless the subsystem pattern explicitly supports
  it.
- Transitions: `Behaviors::switchBehavior()` enqueues.
  `AIManager::commitQueuedBehaviorTransitions()` runs
  `edm.clearBehaviorData()` → `edm.reassignBehaviorConfig()` →
  `Behaviors::init()`. Anything set before the commit is wiped — set new
  behavior state after it.
- Inter-entity messages and worker side effects (damage, ranged attacks,
  equipment fallback, faction changes, behavior messages) go through the
  queued/deferred command paths so `AIManager` commits them on the main
  thread. Never mutate another entity's behavior state directly from
  behavior execution.
- AI command-bus payloads carry enough identity to reject stale commands
  after entity reuse. Preserve deterministic arbitration fields. A new
  command or behavior message documents who may enqueue it, who commits
  it, and whether it is worker-safe.

## Threading and Services

- Behavior execution may run inside worker batches; code must stay
  correct for both single-threaded and threaded `AIManager::update()`.
- Hot-path behavior code: no repeated singleton lookups, no avoidable
  map/string churn on the batch path.
- Path requests and path state flow through `PathfinderManager` and EDM
  path data. Behavior code does not own path-service lifetime or global
  caches.
- Crowd and spatial-query helpers are frame-cached services. Preserve
  their invalidation, read-only batch window, and reusable-buffer
  assumptions.

## API and Header Shape

- Keep contracts data-oriented and explicit: compact config, state,
  command, and context structures over virtual hierarchies or generic
  extension hooks. No templates, type erasure, or general-purpose helper
  layers unless the existing data model requires them.
- Headers hold contracts, small data structures, and declarations.
  Non-trivial behavior, dispatch, cache, and mutation logic goes in
  `.cpp`. Keep includes narrow; forward-declare where it does not weaken
  type safety.
- Add a new API only when the caller's ownership and thread context are
  clear.

## Adding or Changing Behavior

- Match the existing data-oriented execution model; add the smallest
  behavior-local code needed. Reuse shared behavior utilities only when
  they remove real duplication.
- Keep tuning constants next to the behavior or shared utility that owns
  the policy.
- Preserve authored character and behavior config data. Runtime
  overrides are explicit and stay in the owner that already combines
  those inputs.
- A new behavior type updates config/state declarations, executor
  dispatch, initialization, registration (`registerDefaultBehaviors()`),
  and focused tests together.
