---
name: voidlight-memory-profiler
description: Simplified memory profiling and leak detection for SDL3 VoidLight-Framework using valgrind memcheck, AddressSanitizer, and massif. Identifies memory leaks, allocation hotspots, buffer reuse violations, and provides system-by-system memory breakdown with optimization suggestions. Use after performance-critical changes or when investigating memory issues.
allowed-tools: [Bash, Read, Write, Grep, Glob]
---

# VoidLight-Framework Memory Profiler

Lean playbook for memory profiling and leak detection. Identifies leaks,
per-frame allocation hotspots, buffer reuse violations, and provides
optimization recommendations following CLAUDE.md patterns.

This file is the at-a-glance workflow. Full per-mode commands, output parsing,
and the report template live in `references/` and are read **on demand** — do
not inline them here.

## Platform Support (critical gate — read first)

**valgrind (memcheck + massif) is Linux-only and is NOT available on
macOS/darwin.** Modes that rely on valgrind (**Mode 1** Quick Leak Check,
**Mode 3** Full Memory Profile) only run on Linux. On macOS:

- Use **AddressSanitizer** (Mode 2) for leak/overflow/use-after-free detection.
- Use **ThreadSanitizer** (Mode 2b) for data races and deadlocks.

The helper scripts check for `valgrind` and exit with the ASan build command if
it is missing. Mode 4 (Buffer Reuse Audit) is a static scan and runs on any
platform.

## Available Scripts

Utility scripts live in `.claude/skills/voidlight-memory-profiler/scripts/`:

- **`run_leak_check.sh`** — Quick memory leak detection with valgrind memcheck
- **`run_massif_all_tests.sh`** — Run valgrind massif on all test executables
- **`parse_massif.py`** — Parse massif reports and generate analysis

Use these directly or let the skill invoke them. They encapsulate the valgrind
availability guard and the heavy massif parsing.

## Purpose

Memory management is critical for VoidLight-Framework's targets (10K+ entities
@ 60 FPS). This skill automates leak detection, allocation profiling, buffer
reuse verification, per-manager system breakdown, baseline comparison, and
optimization suggestions based on project patterns.

## Profiling Modes at a Glance

| Mode | Tool | Time | Platform | Use when |
|------|------|------|----------|----------|
| **1. Quick Leak Check** | valgrind memcheck | 2-5 min | Linux | Daily dev, before commits |
| **2. Allocation Profiling** | AddressSanitizer | 5-10 min | Any | Frame spikes, perf issues |
| **2b. Thread Safety** | ThreadSanitizer | varies | Any | Data races, deadlocks (threading tests) |
| **3. Full Memory Profile** | valgrind massif | 15-30 min | Linux | Release prep, major optimizations |
| **4. Buffer Reuse Audit** | static code scan | 10-15 min | Any | After new managers, perf tuning |

ASan and TSan are **mutually exclusive** — never enable both. For frame-spike
investigation use ASan (Mode 2); for threading correctness use TSan (Mode 2b).

**For full mode-by-mode commands and output interpretation, read
`references/modes.md`.**

## Core Workflow

### Step 1: Gather scope with AskUserQuestion

- **Mode** (single-select): Quick Leak Check / Allocation Profiling / Full
  Memory Profile / Buffer Reuse Audit.
- **Scope** (single-select): Core Tests Only / AI System / Collision-Pathfinding
  / All Systems.
- **Baseline** (single-select): Compare against baseline / Just current analysis
  / Create new baseline.

### Step 2: Build (if needed) and select test executables

Ensure a Debug build exists before memcheck/massif:

```bash
if [ ! -f "./bin/debug/thread_system_tests" ]; then
    cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
fi
```

Mode 2 (ASan) and Mode 2b (TSan) require a dedicated sanitizer rebuild with the
flags in `references/modes.md` (matching CLAUDE.md).

Populate `TEST_EXECUTABLES` from the scope. For **All Systems**, discover
executables at runtime — never freeze a list:

```bash
mapfile -t TEST_EXECUTABLES < <(find ./bin/debug -maxdepth 1 -name "*tests" -type f -perm -u+x | sort)
```

Scoped lists (Core / AI / Collision-Pathfinding) and every per-mode command
block (valgrind/ASan/TSan/massif invocations, parsing, severity tables) are in
**`references/modes.md`**.

### Step 3: Run the chosen mode

Follow the matching section of `references/modes.md`:
- Mode 1 → memcheck run + parse + severity classification
- Mode 2 → ASan build + run + per-frame hotspot scan
- Mode 2b → TSan build + run + data-race/deadlock parse
- Mode 3 → massif run + ms_print + system-by-system breakdown
- Mode 4 → buffer reuse / anti-pattern / CLAUDE.md pattern scan

### Step 4: Baseline comparison (if requested)

Load and diff against `test_results/memory_profiles/baseline/`, or save current
results as a new baseline. Commands are in
**`references/modes.md` → Baseline Comparison**.

### Step 5: Generate report and console summary

Produce a markdown report under `test_results/memory_profiles/` and a short
console summary. **For the full report structure, console summary format, and
exit codes, read `references/reporting.md`.**

## Usage Examples

Activate this skill automatically when the user says:
- "profile memory usage" / "check for memory leaks"
- "analyze memory allocations" / "find allocation hotspots"
- "audit buffer reuse patterns" / "check per-frame allocations"
- "memory profile AI system"

## Integration with Development Workflow

- **Daily development:** Quick leak check before commits (catches leaks early).
- **Performance investigation:** Allocation profiling for frame spikes.
- **Major changes:** Full profile after adding new managers; verify budget.
- **Release prep:** Comprehensive profile; check for regressions vs baseline.
- **Periodic audits:** Monthly buffer reuse audit.

## Common Memory Issues in VoidLight-Framework

1. **Per-frame allocations (frame spikes):** heap allocs in update loops →
   member buffer + `clear()` pattern from CLAUDE.md.
2. **Missing `reserve()`:** incremental vector reallocations → pre-calculate
   size, `reserve()` before insertion loops.
3. **SDL resource leaks ("still reachable"):** missing `SDL_Destroy` in
   destructors → ensure cleanup in manager destructors.
4. **Thread-safe container allocations:** heap contention across threads →
   thread-local buffers.
5. **Smart pointer overhead:** unnecessary `shared_ptr` copies → raw pointers in
   hot paths (see CLAUDE.md).

## Important Notes

1. Always profile in **Debug** mode — Release optimizations hide issues.
2. Run on a quiet system — background processes affect results.
3. Compare against baseline — trends matter more than absolutes.
4. Fix critical issues immediately — don't accumulate memory debt.
5. Document good buffer reuse examples as references.

---

**Ready to profile. Ask the user for profiling mode and scope, then follow
`references/modes.md` for the chosen mode and `references/reporting.md` for the
write-up.**
