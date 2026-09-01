# AGENTS.md — AI Source

Subtree rules for `src/ai/`. Root `AGENTS.md` still applies; this file adds
ownership and runtime rules for AI implementation. On conflict, this file
wins.

## Development Stance

- Keep this subtree flexible for new behavior families and AI services.
  Prefer stable ownership and data-flow rules over behavior-by-behavior
  checklists.
- Match the existing data-oriented behavior execution model before adding
  new abstractions.
- Treat hot-path behavior code as performance-sensitive: no repeated
  singleton lookups and no avoidable map/string churn on the batch path.

## Ownership and Data Flow

- AI decision logic belongs in `Behaviors::` functions and behavior
  executor code. EDM stores behavior, path, character, and memory data.
- Use `BehaviorContext` data that `AIManager` cached for the frame. Do not
  bypass the context with worker-side world/player queries unless the
  current subsystem pattern explicitly supports it.
- Behavior transitions go through `Behaviors::switchBehavior()` and the
  command-bus commit path. Initialize new behavior state after transition
  commit, not before.
- Inter-entity behavior messages and worker-produced side effects must use
  the queued/deferred command paths. Do not mutate another entity's
  behavior state directly from behavior execution.

## Threading and Services

- Behavior execution may run inside worker batches. Code must remain safe
  for single-threaded and threaded `AIManager::update()` paths.
- Path requests and path state flow through `PathfinderManager` and EDM
  path data. Behavior code does not own path service lifetime or global
  caches.
- Crowd and spatial-query helpers are frame-cached services. Preserve
  their invalidation, read-only batch-window, and reusable-buffer
  assumptions.
- Damage, ranged attacks, equipment fallback, faction changes, and
  behavior messages should be emitted through the existing
  deferred/command mechanisms so `AIManager` can commit them on the main
  thread.

## Adding or Changing Behavior

- Add the smallest behavior-local code needed. Reuse shared behavior
  utilities only when they remove real duplication.
- Keep tuning constants close to the behavior or shared utility that owns
  the policy. Do not move tuning into EDM just to make it reachable.
- Preserve authored character and behavior config data. Runtime overrides
  must be explicit and stay in the owner that already combines those
  inputs.
- When adding a new behavior type, update config/state declarations,
  executor dispatch, initialization, registration, and focused tests
  together.
