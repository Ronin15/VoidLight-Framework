# Framework Implementation Slices

Process for complete feature chunks in this repo. This file owns the process —
not a product roadmap. Add work as a `## Slice N` section when it is scheduled.

A **numbered slice** has a **Goal**, **Checklist**, and **Acceptance checks**.
It is not done until every item in **that section** is `[x]`, owning docs and
tests are updated, the slice-complete gate has passed, and
**cpp-review-specialist** has reviewed the diff.

Do not implement from chat notes. Add the section first, then implement from
it. **Code wins over stale slice prose.** Durable contracts live in `AGENTS.md`
and subsystem docs.

## Ground Rules

- Keep `ninja -C build app` and `./bin/<cfg>/VoidLight_Template` working after
  every slice.
- Partial wiring stays `[ ]` with remaining notes in that section — never
  implied complete elsewhere.
- If a dependent system does not exist yet, label the work as foundation and
  leave the checklist incomplete.
- Implement only the open slice's scope. No unrelated refactors.

## Gates (do not mix these)

Canonical gate names used by agents and `AGENTS.md`:

| Gate | When | What |
| --- | --- | --- |
| **Per-change** | Every edit while implementing | Targeted `ninja -C build` or `ninja -C build app` plus the named Boost.Test executable for the touched system. **Not** cppcheck, clang-tidy, ASan, TSan, or the core-only suite. |
| **Slice complete** | Before marking the slice done | Every Checklist and Acceptance item `[x]`; owning docs and tests updated; `ninja -C build`; then the Boost.Test executables covering the slice's changed code (`--run_test` when a case is enough). No `run_all_tests.sh --core-only`. No benches. |
| **Slice review** | Before committing the slice | `cpp-review-specialist` on the slice diff. Do not commit a completed slice unreviewed. |
| **Branch / PR** | When the branch is ready to merge — not each commit or slice completion | `./tests/test_scripts/run_all_tests.sh --core-only --errors-only` (`.bat` on Windows); cppcheck, clang-tidy, ASan, TSan (sanitizers are mutually exclusive). Optional Valgrind. |

Interactive `./bin/<cfg>/VoidLight_Template` is display-gated. Leave visual/GPU
residuals `[ ]` until confirmed; they do not block slice-complete.

## Implementing a slice

Route through `.grok/skills/cpp-workflows` when using specialists.

1. **Open or add** `## Slice N: …`. Read Goal, Current foundation, and
   Architecture notes. Cross-read [ARCHITECTURE.md](ARCHITECTURE.md) and any
   doc linked in the slice.
2. **Design first** (`cpp-design-specialist`) if ownership, lifecycle, or
   multi-manager flow is unclear. Do not mark the slice complete in the design.
3. **Implement only that slice's scope** (`cpp-specialist`) in the owning
   `include/` + `src/` modules. Check off Checklist items as each integration
   lands (runtime behavior + tests). Use the **per-change** gate while iterating.
4. **Satisfy Acceptance checks.** Update durable docs when contracts change.
5. **Slice-complete gate**, then **slice review**. Do not commit until review
   has run.
6. When fully complete, move the entire section to
   `docs/framework-implementation-slices-archive.md` (create that file when the
   first slice is archived). Leave residual follow-ups only in a later slice —
   do not delete acceptance history.

### Section shape

| Block | Use |
| --- | --- |
| **Goal** | What “done” means |
| **Current foundation** | What already exists — do not rebuild |
| **Architecture notes** / **Problem** | Constraints and ownership |
| **Checklist** | `[ ]` / `[x]` implementation steps |
| **Acceptance checks** | `[ ]` / `[x]` verification gates — all required |
| **Status** | Open/partial note, or one-line completion record before archive |

### Template

```markdown
## Slice N: Title

Goal: …

Current foundation:

- …

Architecture notes:

- …

Checklist:

- [ ] …
- [ ] Owning docs updated
- [ ] Tests updated in the same change

Acceptance checks:

- [ ] …
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test executables for the changed code pass
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started
```

## Slice Records

Numbered `## Slice N` bodies go below this line when a future roadmap adds
them. Implement from those sections, not from this preamble.
