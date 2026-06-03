# AGENTS.md - Manager Tests

These instructions apply to tests under `tests/managers/`. Follow the loaded
parent guidance first, including `tests/AGENTS.md`; this file adds manager and
EDM test guidance.

## Test Focus

- Prefer tests that prove externally visible manager contracts: lifecycle,
  event persistence, cache invalidation, slot reuse, generation safety, and
  cross-manager handoff.
- Keep tests flexible for future development. Avoid pinning incidental helper
  names, temporary buffer layout, or private storage details unless the test is
  specifically guarding a data-layout contract.
- For EDM changes, cover the ownership contract that matters to callers:
  creation, direct destruction, `processDestructionQueue()`,
  `prepareForStateTransition()`, reused slots, and invalid/stale handles.
- When manager tests exercise AI behavior, also honor the AI command-bus,
  behavior transition, EDM storage, and worker/main-thread contracts. Do not
  make EDM or a test fixture the behavior policy owner.

## Fixtures

- Initialize only the managers required by the runtime path under test, in the
  production order needed by those managers.
- Keep singleton cleanup explicit and reverse the initialization order when the
  test owns manager lifetime.
- When behavior, pathfinding, collision, or resources participate in the EDM
  path, include those managers in the fixture instead of faking hidden state.
- When a manager path uses worker batches, initialize `ThreadSystem` and any
  required worker-budget manager path explicitly in the fixture.

## Execution

- Prefer direct Boost test executables over broad scripts.
- Use the built executable's `--list_content` output before adding or relying on
  focused `--run_test` filters.
- Do not relax expectations to hide production defects. If a fixture lacks
  production wiring, fix the fixture or state the limitation in the test.
