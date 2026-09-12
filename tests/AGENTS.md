# AGENTS.md — Tests

Subtree rules for `tests/`. Root `AGENTS.md` still applies; this file
adds fixture and test-design rules. Narrower files under `tests/ai/` and
`tests/managers/` add subsystem-specific guidance. On conflict, the
deeper file wins.

## Test Focus

- Test the observable contract of the runtime path under change. For
  cross-subsystem tests, trace the participating managers, controllers,
  EDM storage, events, worker batches, and cleanup path before changing
  assertions.
- Root-level suites such as behavior functionality, UI
  manager/controller, collision/pathfinding integration, and thread-safe
  AI tests may exercise multiple subsystems. Apply every relevant owner
  contract from the source subtrees they cover.
- Keep tests durable. Avoid pinning helper names, temporary buffers,
  private branch structure, or layout details unless the test is
  explicitly guarding a data-layout or public contract.
- Tests must match the current production contract. When a production
  change fails a test, trace whether the assertion is still the right
  contract. Do not override production state (collision layers/masks,
  stance, factions, pause flags, event wiring) just to keep a diagnostic
  green. That hides the real error. Example: Neutral NPCs are
  `Layer_Default` and do not NPC-NPC pair; do not set `collisionMask =
  0xFFFF` after `assignBehavior` to inflate `lastPairs`. Assert stance
  grouping and Environment pairing instead. Wander crowd steering is
  nearby-entity queries, not CollisionManager pair generation. AI wander
  clamps to PathfinderManager cached world extents (or 32000px with no
  world), not CollisionManager wall bodies.

## Fixtures

- Initialize only the managers needed by the runtime path, in the
  dependency order required by that path. Use existing focused fixtures
  as references, not as universal templates.
- When tests own singleton lifetime, keep cleanup explicit and reverse
  the initialization order where the managers depend on each other.
- Prefer production wiring over fakes when the behavior depends on event
  contracts, manager caches, EDM slot reuse, pathfinding, collision, AI
  command commits, or UI manager state.
- NPCs created by world populate are WorldManager-owned. After
  `loadNewWorld`, tests that need an empty NPC set must call
  `WorldManager::clearPopulatedNpcs` then
  `EntityDataManager::processDestructionQueue` on the test thread. Do
  not hand-roll `destroyEntity` and leave the populate registry stale.
- World unload (`unloadWorld` or `loadNewWorld` replacement via
  `unloadWorldLocked`) destroys static harvestables for that `worldId`
  before WRM `removeWorld`. Tests that unload without
  `prepareForStateTransition` should still observe WRM harvestable count
  0 and no remaining EDM harvestables for the old world. Public
  `unloadWorld` drains queued NPCs; locked unload does not.

## Design and Execution

- Reproduce before changing expectations. Run the most targeted
  executable first.
- Prefer deterministic data, fixed `dt`, explicit seeds, and small entity
  counts. Avoid sleeps, real wall-clock timing, and filesystem/network
  dependencies unless the subsystem requires them.
- For lifecycle tests, cover init, enter, update, transition, cleanup,
  and shutdown paths that matter to the contract.
- For threaded behavior, verify future completion, WorkerBudget
  reporting, and main-thread ownership boundaries rather than only
  checking final values.
- Distinguish missing test setup from a production defect, especially
  for `EventManager` and state-owned handler wiring.
- Classify a failing test as production bug, test-setup issue, stale
  expectation, environment/tooling issue, or unrelated pre-existing
  failure. A stale expectation is updated to the live contract, not
  papered over with a test-only mask or helper that undoes production
  policy.
