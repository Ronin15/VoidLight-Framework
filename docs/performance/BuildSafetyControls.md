# Build Safety Controls

**Code:** `CMakeLists.txt`, `include/core/Logger.hpp`, `src/core/Logger.cpp`, `include/utils/SIMDMath.hpp`, `include/utils/BinarySerializer.hpp`

## Overview

VoidLight-Framework ships four CMake build types, each trading optimization aggressiveness against runtime safety checks differently: `Debug`, `ReleaseSafe`, `Release`, and `Profile`. This doc is the reference for which correctness guarantees actually hold in which build, and which performance flags carry hardware/portability constraints that aren't obvious from the flag name alone.

## Build Type Matrix

| Build Type | Optimization | `assert()` live? | STL bounds/precondition checks | Aggressive flags | Output dir | Use case |
|---|---|---|---|---|---|---|
| `Debug` | `-O0`, full `-g3` symbols | Yes | libstdc++/libc++ default debug-mode behavior | None | `bin/debug` | Day-to-day dev, stepping in a debugger |
| `ReleaseSafe` | `-O2` | Yes | `_GLIBCXX_ASSERTIONS` (libstdc++) / `_LIBCPP_HARDENING_MODE=EXTENSIVE` (libc++/Apple) — project code only | None (no `-ffast-math`, no wide SIMD) | `bin/releasesafe` | Soak testing / extended playtesting at near-Release speed with safety nets on |
| `Release` | `-O3` + LTO | **Yes** — see note below | Off | `-ffast-math`, `-march=x86-64-v3`/AVX2/FMA (non-Apple), `-mcpu=native` (Apple arm64) — project code only | `bin/release` | Shipping |
| `Profile` | `-O2`, `-g` | No (`-DNDEBUG`) | Off | SSE4.2 max, no AVX (Valgrind-compatible) | `bin/profile` | Profiling / Valgrind |

All aggressive/hardening flags are scoped to `VoidLightObjects` (project code) only, never to FetchContent dependencies (SDL3, SDL3_ttf, SDL3_mixer) — applying `-ffast-math` or STL hardening macros globally either breaks SDL3's audio DSP or fails to compile against a dependency's own bundled libc++ assumptions.

## `Release` does not strip `assert()`

`CMakeLists.txt` overrides `CMAKE_CXX_FLAGS_RELEASE` outright rather than appending to CMake's default (which normally includes `-DNDEBUG`). As a result, **only `Profile` defines `NDEBUG`** — `Debug`, `ReleaseSafe`, and `Release` all keep raw `assert()` calls live. This is a load-bearing but non-obvious fact: don't assume a shipped `Release` binary has asserts compiled out, and don't rely on that assumption when reasoning about what a `Release` build will actually do if an invariant is violated in the field.

## AVX2 is a hard minimum spec, not a soft optimization

Non-Apple `Release` compiles with `-march=x86-64-v3 -mavx -mavx2 -mfma`. `SIMDMath.hpp` selects its AVX2 code path via a **compile-time** check (`#if defined(__AVX2__)`) — there is no runtime CPU-feature dispatch (no `cpuid`, no `__builtin_cpu_supports`, no `target_clones`/`ifunc` multiversioning). This means:

- The compiler may emit AVX2 instructions anywhere it can auto-vectorize a loop in `VoidLightObjects`, not just inside the SIMDMath kernels.
- A shipped `Release` binary requires the end user's CPU to physically support AVX2. If it doesn't, the process crashes immediately with `SIGILL` — not a subtle bug, a hard launch failure.
- AVX2 landed with Intel Haswell (2013) and AMD Excavator/Zen (2015). Older, budget, embedded, or CPU-feature-restricted VM hardware cannot run a `Release` build at all.

`Profile` already demonstrates the safer fallback baseline (`-march=x86-64-v2 -msse4.2`, no AVX) if broader hardware compatibility is ever required for a shipped build — this is a deliberate tradeoff to revisit if the target audience isn't guaranteed modern hardware.

## Save data portability

`BinarySerializer.hpp` enforces `static_assert(std::endian::native == std::endian::little, ...)` — the save format is raw byte copies with no endianness normalization, by deliberate design (documented in the header). Every platform this project targets (Windows/macOS/Linux on x86-64, Apple Silicon ARM64) is little-endian, so this holds across the full target matrix without needing byte-swap logic.

Save files (`SaveGameData`) are **state snapshots**, not a replay/input log — `SaveGameManager` writes final computed values (position, health, etc.) directly and restores them as-is on load, without recomputing anything. This means `-ffast-math` in `Release` has no bearing on save compatibility: whatever float value the game computed gets serialized bit-for-bit, and loading never re-runs the math that produced it. The concern would only apply to a lockstep/replay architecture that reconstructs state by re-simulating from a recorded input sequence — this project has no such system.

## SIMD memory-safety pattern

The hot-path SIMD idiom used by `AIManager`, `ProjectileManager`, and `CollisionManager` is a fixed-width gather/scatter: data is copied element-by-element from EDM's `TransformData`/`EntityHotData` (via safe scalar accessors) into local `alignas(16) float x[4]` stack scratch arrays, SIMD-processed as exactly 4 lanes, then scattered back via safe setters. This is memory-safe by construction — the SIMD load/store always touches exactly 4 known-size stack slots regardless of container size, so there's no dynamically-sized buffer for a hardening flag to fail to protect.

`ParticleManager`'s own `LockFreeParticleStorage` is the one place that reads a wide SIMD load (`load_byte16`, 16 bytes) directly against a dynamically-sized container (`particles.flags`) rather than a fixed local array. The call site already computes a manual safety bound (`simdFlagSafeEnd`) to guarantee the read stays in range. `SIMDMath::load_byte16()` now takes an explicit `remaining` parameter and asserts `remaining >= 16` — this doesn't change behavior when the existing bound is correct, but turns a future regression (someone changing the loop math and breaking the invariant) into a loud, immediate assertion failure instead of a silent out-of-bounds read, in every build type except `Profile`.

## Recommendations

- **Day-to-day development / debugging**: `Debug`.
- **Soak testing, extended playtesting, QA passes**: `ReleaseSafe` — near-shipping speed, safety nets still on.
- **Shipping**: `Release` — confirm the AVX2 baseline (Haswell/Excavator-or-newer) matches your actual target audience before relying on it.
- **Valgrind / memory profiling**: `Profile` — the only build type without AVX and with `NDEBUG` defined.

## See Also

- [SIMDMath](../utils/SIMDMath.md)
- [Power Efficiency](PowerEfficiency.md)
