---
name: cpp-workflows
description: >-
  VoidLight-Framework C++20 specialist routing: design → implement → review.
  Use when the user asks for cpp-design-specialist, cpp-specialist, or
  cpp-review-specialist; when planning or implementing managers, EDM, AI,
  controllers, events, GPU/rendering, or threading; when reviewing a PR or
  branch diff; or when a non-trivial C++ feature needs the full specialist
  pipeline. Slash: /cpp-workflows.
---

# C++ Workflows (Grok-native)

Canonical guardrails: root `AGENTS.md` / `Claude.md` and nested path `AGENTS.md` files.

**Agents (source of truth):** `.grok/agents/cpp-*.md`  
**Models:** user `~/.grok/config.toml` → `[subagents.models]`  
(design/review → `grok-4.5`, implement → `grok-composer-2.5-fast`)

Do not restate full guardrails in `spawn_subagent` prompts — agents load `agents_md`.

Repo skills under `.agents/skills/` and `.claude/skills/` **supplement** agents; they do
not replace them. Prefer spawning the agent, then loading a skill only if that phase
needs its checklist or scripts.

## Inline vs delegate

Work **inline** (no subagent) when ALL are true:

- One or two files, localized change
- No architecture, ownership, lifecycle, or EDM/AI contract change
- User did not ask for a specialist or review pass
- Examples: typo, comment, single test fix, rename, fmt-only

**Delegate** when ANY are true:

| Signal | Workflow |
|--------|----------|
| Non-trivial feature or multi-system change | Design → Implement → Review |
| User names a specialist | Matching phase below |
| Build / test / sanitizer failure after a change | Implement (fix) then Review if risky |
| PR, diff, or standards review | Review |
| Ownership / EDM / controller placement unclear | Design first |
| Touches hot paths, threading, GPU frame, or transitions | Implement (or Design first if unclear), then Review |

When unsure: **Implement** for coding tasks, **Review** after substantive diffs,
**Design** when ownership or multi-manager flow is unclear.

## Shared spawn conventions

Use the `spawn_subagent` tool. Only the **parent** session may spawn (depth limit 1).

| Subagent | `capability_mode` | Typical next step |
|----------|-------------------|-------------------|
| `cpp-design-specialist` | `read-only` (or omit; agent is `permission_mode: plan`) | Implement |
| `cpp-specialist` | omit / `all` | Review if risky or multi-file |
| `cpp-review-specialist` | `read-only` (or omit; agent is `permission_mode: plan`) | Design or Implement |

- Prefer `background: true` for independent units; collect with `get_command_or_subagent_output`.
- One subagent per logical unit unless the user asks for parallel work.
- Do **not** run Design + Implement in parallel on the same feature unless the user asks.
- Model pins live in `~/.grok/config.toml` — do not invent model overrides here.

## Auto-route: Design → Implement → Review

Default pipeline for non-trivial work:

1. **Design** (`cpp-design-specialist`) — ownership, data flow, threading, lifecycle, tests.
2. **Implement** (`cpp-specialist`) — code + targeted build/tests; feed prior design summary.
3. **Review** (`cpp-review-specialist`) — severity-ordered findings; re-enforces specialist standards.

Stop early when:

- User asked only for design / only for implement / only for review.
- Design concludes “inline / single-file” and user wants parent to code.
- Implement fails validation — fix first; do not open Review on a red build unless asked.

## Invocation examples

User says → do:

- "Design the AI combat handoff before we code" → **Design**
- "Implement X" (contracts clear) → **Implement**
- "Implement X" (ownership unclear) → **Design** then **Implement**
- "Full pass on this feature" / "design then implement then review" → full pipeline
- "Review my branch" → **Review**, scope `branch changes`
- "Review only uncommitted" → **Review**, scope `uncommitted changes`
- "Fix this test failure" + error → **Implement** (targeted fix + re-run)

## Design

```
subagent_type: cpp-design-specialist
capability_mode: read-only
```

Prompt: goal, in/out of scope, owning subsystem, files/areas, constraints.

Optional supplements after design returns: `voidlight-architecture-guard`,
`voidlight-dependency-analyzer`.

## Implement

```
subagent_type: cpp-specialist
```

Prompt: owning module(s), success criteria, validation commands, prior design if any.

Optional supplements: `voidlight-cpp-engineer`, `voidlight-test-suite-generator`,
`voidlight-build-validate`. Per-change self-check catalog: `voidlight-quality-check`
(static analysis is pre-commit/PR, not every edit).

## Review

```
subagent_type: cpp-review-specialist
capability_mode: read-only
```

```text
Full Repository Path: <abs path>
Review scope: branch changes | uncommitted changes | <file list>
Base Branch: <only if non-default base>
Custom Instructions: <optional>
```

Summarize as a severity-sorted table; do not fix unless asked.

Optional supplements: `voidlight-systems-reviewer`, `voidlight-architecture-guard`,
`voidlight-quality-gate`, `voidlight-quality-check`.

## Claude / repo skill map (supplement only)

| Phase | Grok agent | Optional skills / Claude analogues |
|-------|------------|--------------------------------------|
| Design | `cpp-design-specialist` | architecture-guard, dependency-analyzer · `.claude/agents/systems-integrator` |
| Implement | `cpp-specialist` | cpp-engineer, test-suite-generator, build-validate · `.claude/agents/game-engine-specialist` |
| Review | `cpp-review-specialist` | systems-reviewer, architecture-guard, quality-gate/check · `.claude/agents/game-systems-architect` |
| Validate (scripts) | parent or implement | build-validate, quality-check, benchmark-regression, memory-profiler · `.claude/agents/quality-engineer` |

Agents under `.grok/agents/` always win over skill prose when guidance conflicts.
