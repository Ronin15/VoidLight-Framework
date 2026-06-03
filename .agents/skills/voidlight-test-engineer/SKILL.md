---
name: voidlight-test-engineer
description: Design and implement targeted VoidLight-Framework regression tests, repro harnesses, Boost.Test filters, and sanitizer/debug workflows for C++20 engine, gameplay, AI, rendering, event, lifecycle, and threading bugs. Use when a bug needs a minimal reproduction, a behavior change needs focused test coverage, or a failing test needs root-cause diagnosis without relaxing expectations.
---

# VoidLight Test Engineer

## Use This Skill For

- Turning a bug report into a minimal local reproduction.
- Adding focused regression coverage for engine, gameplay, AI, rendering, event, lifecycle, or threading behavior.
- Diagnosing failing Boost.Test cases without weakening production expectations.
- Choosing the narrowest relevant test executable, `--list_content`, `--run_test`, build, or sanitizer check.

## Workflow

1. Read governing instructions first.
   - Start with the nearest `AGENTS.md` or `AGENTS.override.md` for touched test and production paths.
   - Treat production/test alignment, Boost.Test naming, and subsystem ownership rules as binding.

2. Reproduce before changing expectations.
   - Inspect the failing test, bug path, or behavior contract.
   - Run the most targeted executable first.
   - Use `--list_content` when the exact Boost.Test case or suite name is uncertain.
   - Prefer `--run_test="ExactOrWildcardName*"` over broad wrapper scripts.

3. Identify the production contract.
   - Trace the relevant manager, controller, EDM, event, GameState, render, or worker-thread path.
   - Compare with nearby tests in the same subsystem before inventing a new harness style.
   - Distinguish missing test setup from a production defect, especially for `EventManager` and state-owned handler wiring.
   - Prefer production entrypoints over raw internal events unless the test is explicitly isolating the event layer.

4. Add the narrowest durable coverage.
   - Test the externally meaningful behavior or contract, not private implementation detail.
   - Keep fixtures minimal but representative of the runtime owner relationships.
   - Add production and test changes together when behavior changes.
   - Use `BOOST_REQUIRE()` for important `[[nodiscard]]` setup calls such as `init()`, `load()`, and `create()`.
   - Never relax assertions to hide a production bug unless the user explicitly asks.

5. Use the right verification depth.
   - Prefer direct test executables such as `./bin/debug/<test_executable>`.
   - Use sanitizer builds only when memory, lifetime, race, or iterator invalidation risk justifies the cost.
   - Use slower scripts only when the affected surface is broad or the user explicitly asks.

6. Report the result precisely.
   - State the repro command, failing or fixed behavior, files changed, and exact verification command.
   - Call out any skipped broader coverage or residual risk.

## Test Design Rules

- Keep test names specific enough to filter directly with Boost.Test.
- Reuse local fixtures, helpers, fake managers, and event setup patterns before adding new scaffolding.
- Prefer deterministic data, fixed `dt`, explicit seeds, and small entity counts.
- Avoid sleeps, real wall-clock timing, broad integration setup, or filesystem/network dependencies unless required by the subsystem.
- For lifecycle tests, cover init, enter, update, transition, cleanup, and shutdown paths that matter to the bug.
- For threaded behavior, verify future completion, worker-budget reporting, and main-thread ownership boundaries rather than only checking final values.
- For rendering or UI regressions, test contract-level data flow where possible and pair with a manual or screenshot check only when visual output is the contract.

## Common Commands

```bash
./bin/debug/<test_executable> --list_content
./bin/debug/<test_executable> --run_test="TestCaseOrWildcard*"
./bin/debug/entity_data_manager_tests
./bin/debug/ai_manager_edm_integration_tests
./bin/debug/behavior_functionality_tests --run_test="FleeFromAttacker*"
```

When build options or sanitizer flags change, follow the repo guidance for reconfiguration and cache cleanup before trusting failures.

## Output Expectations

- For new or changed tests, explain what behavior the test protects.
- For failing tests, classify the failure as production bug, test setup issue, stale expectation, environment/tooling issue, or unrelated pre-existing failure.
- For fixes, state the narrowest command that passed and any broader checks not run.
