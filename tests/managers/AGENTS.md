# AGENTS.md — Manager Tests

Subtree rules for `tests/managers/`. Root `AGENTS.md` and
`tests/AGENTS.md` still apply; this file adds manager and EDM test
guidance. On conflict, this file wins.

## Test Focus

- Prefer tests that prove externally visible manager contracts:
  lifecycle, event persistence, cache invalidation, slot reuse,
  generation safety, and cross-manager handoff.
- For EDM changes, cover the ownership contract that matters to callers:
  creation, direct destruction, `processDestructionQueue()`,
  `prepareForStateTransition()`, reused slots, and invalid/stale handles.
- When manager tests exercise AI behavior, also honor the AI command-bus,
  behavior transition, EDM storage, and worker/main-thread contracts. Do
  not make EDM or a test fixture the behavior policy owner.

## Fixtures

- When behavior, pathfinding, collision, or resources participate in the
  EDM path, include those managers in the fixture instead of faking
  hidden state.
- When a manager path uses worker batches, initialize `ThreadSystem` and
  any required worker-budget manager path explicitly in the fixture.
