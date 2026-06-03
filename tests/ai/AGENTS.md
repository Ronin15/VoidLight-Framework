# AGENTS.md - AI Tests

These instructions apply to tests under `tests/ai/`. Follow the loaded parent
guidance first, including `tests/AGENTS.md`; this file adds AI-specific test
guidance.

## Test Focus

- Prefer tests that prove observable AI behavior and subsystem contracts:
  command-bus handoff, behavior transitions, state persistence, cache
  invalidation, and worker/main-thread boundaries.
- Keep tests flexible for future AI development. Avoid locking down incidental
  implementation details such as exact helper names, temporary buffer layouts,
  or behavior-internal branch structure.
- When a behavior bug is reported, trace the runtime path first: behavior
  executor, `AIManager` assignment/commit path, EDM state/config, and any
  authored data that affects the behavior.
- AI behavior coverage may also live outside this subtree, especially
  root-level behavior functionality tests and manager integration tests. Apply
  these AI contracts when those tests exercise behavior execution or command
  commits.

## Test Execution

- Prefer direct Boost test executables over broad scripts.
- Use `--list_content` before adding or relying on a focused `--run_test`
  filter.
- Keep fixtures explicit about the managers required by the behavior path under
  test. Preserve dependency order for that path instead of copying a universal
  manager list.

## Regression Coverage

- Cover both the queued/deferred command path and the committed runtime state
  when behavior execution crosses thread or entity boundaries.
- For cache and reusable-buffer behavior, assert externally visible results and
  reset/invalidation behavior rather than private storage details.
- Do not relax expectations to hide a production behavior issue. If a test
  setup is missing production wiring, fix the setup or state that limitation
  clearly.
