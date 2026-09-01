# AGENTS.md — AI Tests

Subtree rules for `tests/ai/`. Root `AGENTS.md` and `tests/AGENTS.md`
still apply; this file adds AI-specific test guidance. On conflict, this
file wins.

## Test Focus

- Prefer tests that prove observable AI behavior and subsystem contracts:
  command-bus handoff, behavior transitions, state persistence, cache
  invalidation, and worker/main-thread boundaries.
- When a behavior bug is reported, trace the runtime path first: behavior
  executor, `AIManager` assignment/commit path, EDM state/config, and any
  authored data that affects the behavior.
- AI behavior coverage may also live outside this subtree, especially
  root-level behavior functionality tests and manager integration tests.
  Apply these AI contracts when those tests exercise behavior execution
  or command commits.

## Fixtures and Coverage

- Keep fixtures explicit about the managers required by the behavior
  path under test. Preserve dependency order for that path instead of
  copying a universal manager list.
- Cover both the queued/deferred command path and the committed runtime
  state when behavior execution crosses thread or entity boundaries.
- For cache and reusable-buffer behavior, assert externally visible
  results and reset/invalidation behavior rather than private storage
  details.
