# AGENTS.md - Tests

These instructions apply to tests under `tests/`. Follow the loaded parent
guidance first; this file adds test-specific guidance. Narrower test-subtree
`AGENTS.md` files add subsystem-specific rules.

## Test Focus

- Test the observable contract of the runtime path under change. For
  cross-subsystem tests, trace the participating managers, controllers, EDM
  storage, events, worker batches, and cleanup path before changing assertions.
- Root-level suites such as behavior functionality, UI manager/controller,
  collision/pathfinding integration, and thread-safe AI tests may exercise
  multiple subsystems. Apply every relevant owner contract from the source
  subtrees they cover.
- Keep tests durable. Avoid pinning helper names, temporary buffers, private
  branch structure, or layout details unless the test is explicitly guarding a
  data-layout or public contract.

## Fixtures

- Initialize only the managers needed by the runtime path, in the dependency
  order required by that path. Use existing focused fixtures as references, not
  as universal templates.
- When tests own singleton lifetime, keep cleanup explicit and reverse the
  initialization order where the managers depend on each other.
- Prefer production wiring over fakes when the behavior depends on event
  contracts, manager caches, EDM slot reuse, pathfinding, collision, AI command
  commits, or UI manager state.

## Execution

- Prefer direct Boost test executables over broad scripts.
- Use the built executable's `--list_content` output before adding or relying on
  focused `--run_test` filters.
- Never relax expectations to hide a production bug. If a fixture is missing
  production wiring, fix the fixture or state the limitation clearly.
