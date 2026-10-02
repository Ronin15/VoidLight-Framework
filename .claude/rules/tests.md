---
paths:
  - "tests/**"
---

# Test Rules

Applies to everything under `tests/`. Root `CLAUDE.md` still applies.
AI and manager/EDM tests also follow `.claude/rules/tests-ai.md` and
`.claude/rules/tests-managers.md`, which refine (never contradict) this
file. Tests that exercise a subsystem also honor that subsystem's rule
file (`ai.md`, `managers.md`, `ui-controllers.md`).

## Test Focus

- Test the observable contract of the runtime path under change. For
  cross-subsystem tests, trace the participating managers, controllers,
  EDM storage, events, worker batches, and cleanup path before changing
  assertions. Root-level suites (behavior functionality, UI
  manager/controller, collision/pathfinding integration, thread-safe AI)
  span several subsystems; apply every relevant owner contract.
- Keep tests durable: do not pin helper names, temporary buffers, private
  branch structure, or layout unless the test guards a data-layout or
  public contract.
- Tests match the current production contract. Never override production
  state (collision layers/masks, stance, factions, pause flags, event
  wiring) to keep a diagnostic green. Example: Neutral NPCs are
  `Layer_Default` and do not NPC-NPC pair — do not set
  `collisionMask = 0xFFFF` after `assignBehavior` to inflate `lastPairs`;
  assert stance grouping and Environment pairing instead. Wander crowd
  steering is nearby-entity queries, not CollisionManager pair
  generation. AI wander clamps to PathfinderManager cached world extents
  (or 32000px with no world), not CollisionManager wall bodies.

## Fixtures

- Initialize only the managers the runtime path needs, in its dependency
  order. Existing fixtures are references, not universal templates.
- When tests own singleton lifetime, clean up explicitly in reverse
  dependency order. `BOOST_REQUIRE()` on `init()` / `load()` /
  `create()`.
- Prefer production wiring over fakes when behavior depends on event
  contracts, manager caches, EDM slot reuse, pathfinding, collision, AI
  command commits, or UI manager state.
- World-populated NPCs are WorldManager-owned. A fixture needing an
  empty NPC set loads with `WorldGenerationConfig::populate = false`. A
  test that must drop NPCs from an already-populated world calls
  `WorldManager::clearPopulatedNpcs` then
  `EntityDataManager::processDestructionQueue` on the test thread — never
  hand-rolled `destroyEntity` that leaves the registry stale.
- World unload (`unloadWorld`, or `loadNewWorld` replacement via
  `unloadWorldLocked`) destroys that `worldId`'s static harvestables
  before WRM `removeWorld`; tests unloading without
  `prepareForStateTransition` still see WRM harvestable count 0 and no
  old-world EDM harvestables. Public `unloadWorld` drains queued NPCs;
  locked unload does not.

## Design and Execution

- Reproduce before changing expectations; run the most targeted
  executable first (`--list_content` to confirm names).
- Deterministic data, fixed `dt`, explicit seeds, small entity counts. No
  sleeps, wall-clock timing, or filesystem/network dependencies unless
  the subsystem requires them.
- Lifecycle tests cover the init, enter, update, transition, cleanup, and
  shutdown paths that matter to the contract.
- Threaded tests verify future completion, WorkerBudget reporting, and
  main-thread ownership, not only final values.
- Distinguish missing test setup from a production defect, especially
  `EventManager` state-owned handler wiring.
- Classify failures: production bug, test-setup issue, stale
  expectation, environment/tooling, or pre-existing. A stale expectation
  is updated to the live contract, never papered over with a test-only
  mask or helper that undoes production policy. Never relax assertions to
  hide a production bug.
