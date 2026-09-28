---
paths:
  - "tests/ai/**"
  - "tests/**/*Behavior*"
  - "tests/**/*AI*"
---

# AI Test Rules

Refines `.claude/rules/tests.md` for AI tests, including root-level
behavior and AI integration suites. Also honor `.claude/rules/ai.md`.

- Prove observable AI behavior and subsystem contracts: command-bus
  handoff, behavior transitions, state persistence, cache invalidation,
  and worker/main-thread boundaries.
- For a reported behavior bug, trace the runtime path first: behavior
  executor, `AIManager` assignment/commit path, EDM state/config, and any
  authored data that affects the behavior.
- Fixtures state the managers the behavior path needs, in that path's
  dependency order — do not copy a universal manager list.
- When execution crosses thread or entity boundaries, cover both the
  queued/deferred command path and the committed runtime state.
- For caches and reusable buffers, assert externally visible results and
  reset/invalidation behavior, not private storage details.
