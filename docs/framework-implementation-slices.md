# Framework Implementation Slices

Agent **implementation workflow** for this repo. Work that is a complete feature
chunk is a **numbered slice** with a **Goal**, **Checklist**, and **Acceptance
checks**. This file owns the process — not a product roadmap. Future slices are
added as `## Slice N` sections when that work is scheduled.

A slice is not done until every Checklist and Acceptance item in **that
section** is `[x]`, owning docs and tests are integrated, the slice-complete
gate has passed, and the slice has been **reviewed**.

## Ground Rules

- Preserve runnable defaults: `ninja -C build app` and
  `./bin/<cfg>/VoidLight_Template` should keep working after every slice.
- Partial wiring stays `[ ]` with remaining notes in that slice section —
  never implied complete elsewhere.
- If a dependent system does not exist yet, label the work as foundation or
  preparation and leave the checklist incomplete.
- Implement only the open slice's scope. Do not expand into unrelated
  refactors.
- Read [ARCHITECTURE.md](ARCHITECTURE.md) and the owning live modules before
  editing. **Code wins over stale slice prose.** Durable contracts stay in
  `AGENTS.md`, `CLAUDE.md`, and subsystem docs.
- Do not implement from chat notes or a gap list. Add a `## Slice N` section
  first (Goal / Checklist / Acceptance), then implement from that section.

## Gating (do not mix these)

| Gate | When | What |
| --- | --- | --- |
| **Per-change** | Every edit while implementing | Targeted `ninja -C build` or `ninja -C build app` plus the named Boost.Test executable for the touched system. Architecture / threading self-check. **Not** cppcheck, clang-tidy, ASan, or TSan. |
| **Slice complete** | Before marking the slice done | Every Checklist and Acceptance item `[x]`; owning docs and tests updated; `ninja -C build`; then `./tests/test_scripts/run_all_tests.sh --core-only --errors-only` (`.bat` on Windows). No benches. |
| **Slice review** | **Before committing the slice** | `cpp-review-specialist` on the slice diff. Do not commit a completed slice unreviewed. |
| **Branch / PR** | When the branch is ready to merge — **not** each incremental commit | cppcheck, clang-tidy, ASan, TSan (sanitizers are mutually exclusive). Optional Valgrind. These are **branch gates**, not per-change or per-commit gates. |

Interactive `./bin/<cfg>/VoidLight_Template` is display-gated. Leave visual/GPU
residuals `[ ]` on the live slice until confirmed; do not block the
compile/core-test gate on a display.

## Agent Workflow: Implementing A Slice

1. **Open or add** the slice section (`## Slice N: …`). Read **Goal**,
   **Current foundation**, and **Architecture notes**. Cross-read
   [ARCHITECTURE.md](ARCHITECTURE.md) and any doc linked in the slice.
2. **Implement only that slice's scope** in the owning `include/` + `src/`
   modules.
3. **Check off Checklist items** as each integration lands (runtime behavior +
   tests for that item). Use the **per-change** gate while iterating.
4. **Satisfy Acceptance checks** — each must pass before the slice is done.
5. **Update durable docs** the slice touches when contracts change.
6. **Set Status** (if present) and run the **slice-complete** gate.
7. **Review before commit.** Route the finished diff to
   **cpp-review-specialist**. Do not commit until that review has run.
8. When fully complete, move the entire section to
   `docs/framework-implementation-slices-archive.md` (create that file when the
   first slice is archived). Leave residual follow-ups only in a later slice
   — do not delete acceptance history.

### Standard slice section shape

| Block | Agent use |
| --- | --- |
| **Goal** | What "done" means for this chunk |
| **Current foundation** | What already exists — do not rebuild |
| **Architecture notes** / **Problem** | Constraints and ownership boundaries |
| **Checklist** | `[ ]` / `[x]` implementation steps — check off as you land each |
| **Acceptance checks** | `[ ]` / `[x]` verification gates — all required before complete |
| **Status** | Open/partial note, or one-line completion record before archive move |

### Template (copy into a new `## Slice N` section)

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
- [ ] `./tests/test_scripts/run_all_tests.sh --core-only --errors-only` passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started
```

## Slice Records

Numbered `## Slice N` bodies go below this line when a future roadmap adds
them. Implement from those sections, not from this preamble.
