---
name: cpp-workflows
description: >-
  VoidLight-Framework C++20 specialist routing: design → implement → review.
  Use when the user asks for cpp-design-specialist, cpp-specialist, or
  cpp-review-specialist; when planning or implementing managers, EDM, AI,
  controllers, events, GPU/rendering, or threading; when reviewing a PR or
  branch diff; or when a numbered slice or non-trivial C++ feature needs the
  full specialist pipeline. Slash: /cpp-workflows.
---

# C++ Workflows

Canonical rules: root and nested `AGENTS.md`.  
Agents: `.grok/agents/cpp-*.md` (self-contained; they load `AGENTS.md`).  
Slice process and gates: `docs/framework-implementation-slices.md`.  
Models: `~/.grok/config.toml` → `[subagents.models]`.

Do not restate full guardrails in `spawn_subagent` prompts — agents load
`agents_md`. Only the parent session may spawn (depth 1).

## Inline vs delegate

Work **inline** when all of these are true: one or two files, no ownership /
lifecycle / EDM-AI contract change, user did not ask for a specialist or
review. Examples: typo, comment, single-test fix, rename.

**Delegate** when any of these are true:

| Signal | Route |
|--------|--------|
| Numbered slice, multi-system change, or non-trivial feature | Design → Implement → Review |
| Ownership / EDM / controller placement unclear | Design first |
| Contracts are clear; user wants code | Implement; Review if risky or multi-file |
| User names a specialist | That phase |
| PR, diff, or standards review | Review |
| Build / test failure after a change | Implement (fix); Review if risky |
| Hot path, threading, GPU frame, or transitions | Implement (Design first if unclear), then Review |

When unsure: **Implement** for coding, **Review** after substantive diffs,
**Design** when ownership or multi-manager flow is unclear.

Do not run Design and Implement in parallel on the same feature unless asked.
Prefer `background: true`; collect with `get_command_or_subagent_output`.

## Pipeline

1. **Design** (`cpp-design-specialist`) — ownership, data flow, threading,
   lifecycle, tests. Prompt: goal, in/out of scope, owning subsystem,
   files/areas, constraints.
2. **Implement** (`cpp-specialist`) — code + per-change tests. Feed the design
   summary. For a numbered slice, include the slice section path and require
   the slice-complete gate from `docs/framework-implementation-slices.md`.
3. **Review** (`cpp-review-specialist`) — severity-ordered findings. Scope:
   `branch changes` | `uncommitted changes` | file list. Do not fix unless
   asked.

Stop early when the user asked for only one phase, design says “inline /
single-file” and the user wants the parent to code, or implement is red —
fix before Review unless asked.

Numbered slice: Design if contracts unclear, else Implement; **Review before
commit**.

## Optional workflows (user-asked only)

Not part of design → implement → review, per-change, or slice-complete:

| Skill | When |
|-------|------|
| `voidlight-quality-gate` | Focused cppcheck + clang-tidy (branch/PR) |
| `voidlight-benchmark-regression` | Platform-local benches vs `test_results/baseline/` |

Agents under `.grok/agents/` win over skill prose when guidance conflicts.
