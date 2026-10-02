---
paths:
  - "tests/managers/**"
  - "tests/**/*Manager*"
  - "tests/**/*EDM*"
---

# Manager and EDM Test Rules

Refines `.claude/rules/tests.md` for manager and EDM tests. Also honor
`.claude/rules/managers.md`.

- Prove externally visible manager contracts: lifecycle, event
  persistence, cache invalidation, slot reuse, generation safety, and
  cross-manager handoff.
- EDM changes cover the caller-facing ownership contract: creation,
  direct destruction, `processDestructionQueue()`,
  `prepareForStateTransition()`, reused slots, and invalid/stale handles.
- When manager tests exercise AI behavior, honor the AI command-bus,
  behavior transition, EDM storage, and worker/main-thread contracts.
  Neither EDM nor a fixture becomes the behavior policy owner.
- When behavior, pathfinding, collision, or resources participate in the
  EDM path, include those managers in the fixture instead of faking
  hidden state.
- When a manager path uses worker batches, initialize `ThreadSystem` and
  the required worker-budget path explicitly in the fixture.
