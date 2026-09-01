# AGENTS.md — AI Public Contracts

Subtree rules for `include/ai/`. Root `AGENTS.md` still applies; this file
adds API and data-contract rules for AI headers. On conflict, this file wins.

## Public API Shape

- Keep AI contracts data-oriented and explicit. Prefer compact config, state,
  command, and context structures over virtual hierarchies or generic
  extension hooks.
- Headers should define contracts, small data structures, and declarations.
  Put non-trivial behavior, dispatch, cache, and mutation logic in `.cpp`
  files.
- Headers may expose refs, indices, configs, and context needed by behavior
  execution. Do not make EDM a policy owner.
- Add a new API only when the caller's ownership and thread context are
  clear.

## State and Command Contracts

- Behavior config describes authored or assigned intent. Behavior state
  stores runtime progress. Shared behavior data is only for state that is
  actually shared across behaviors.
- `BehaviorContext` is the batch-time contract between `AIManager` and
  behavior executors. Keep it explicit about cached frame data, EDM refs,
  optional state, and thread-safety expectations.
- AI command-bus payloads must include enough identity to reject stale
  commands after entity reuse. Preserve deterministic arbitration fields
  where they exist.
- If a new command or behavior message is added, document who may enqueue
  it, who commits it, and whether it is safe from worker threads.

## Header Hygiene

- Keep includes narrow and prefer forward declarations where they do not
  weaken type safety.
- Do not introduce templates, type erasure, or general-purpose helper
  layers unless the existing AI data model requires them.
- Keep comments focused on ownership, threading, and data-flow contracts
  that future maintainers could otherwise violate.
