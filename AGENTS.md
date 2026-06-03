# AGENTS.md — VoidLight-Framework

Codex instructions for this repository. Match existing subsystem patterns, fix root causes, and keep changes aligned with production architecture.

## Instruction Order

Apply guidance in this order:

1. Explicit user instructions
2. Root `AGENTS.md`
3. Any nested `AGENTS.md` or `AGENTS.override.md` that applies to touched paths
4. Existing local subsystem patterns
5. General style preferences

- Codex is launched from the project root, so only this root file is guaranteed
  to load automatically. Before editing a path covered by nested guidance, read
  the matching nested file explicitly.
- This is repo-level guidance. Narrower nested `AGENTS.md` files add
  subtree-specific rules and override broader guidance for their subtree.
- Keep this file durable, concise, and repo-specific. Move subsystem-only detail into nested agent files when needed.
- If the user names a specific file, work in that file only unless they approve spillover.

## Subtree Guidance

Read these files before editing matching paths:

- `include/ai/AGENTS.md` for AI public contracts.
- `src/ai/AGENTS.md` for AI implementation code.
- `include/controllers/ui/AGENTS.md` for UI controller public contracts.
- `src/controllers/ui/AGENTS.md` for UI controller implementation code.
- `include/managers/AGENTS.md` for manager public contracts.
- `src/managers/AGENTS.md` for manager implementation code.
- `tests/AGENTS.md` for tests, plus narrower test guidance when present.
- `tests/ai/AGENTS.md` for tests under `tests/ai/`.
- `tests/managers/AGENTS.md` for tests under `tests/managers/`.

## Project Stance

- Performance-oriented, data-oriented, and minimal abstraction overhead.
- Prioritize memory safety, cache efficiency, low latency, minimal allocations, and deterministic behavior.

## Workflow

- Read the exact code path before editing.
- Search the same subsystem for the established pattern before inventing one.
- Do not assume system ownership, hot paths, or participating managers. Trace them in code first.
- Prefer targeted fixes over cleanup or opportunistic refactors.
- Do not add compatibility overloads, ad-hoc safety layers, or new abstractions unless the task requires them.
- Keep production and test updates in the same change when behavior changes.
- Before finishing, run the most targeted build or test feasible and state exactly what you verified.

## Fast Commands

Build:

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build
```

Sanitizers:

- ASan and TSan are mutually exclusive.
- Remove `build/CMakeCache.txt` when switching sanitizers or major build options.

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=address -fno-omit-frame-pointer -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" -DUSE_MOLD_LINKER=OFF && ninja -C build
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=thread -fno-omit-frame-pointer -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" -DUSE_MOLD_LINKER=OFF && ninja -C build
export TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"
```

Reconfigure:

```bash
rm build/CMakeCache.txt && cmake -B build/ ...
```

Run:

```bash
./bin/debug/VoidLight_Template
./bin/release/VoidLight_Template
```

Tests:

```bash
./bin/debug/<test_executable>
./bin/debug/<test_executable> --list_content
./bin/debug/<test_executable> --run_test="TestCase*"
./bin/debug/entity_data_manager_tests
./bin/debug/ai_manager_edm_integration_tests
./bin/debug/behavior_functionality_tests --run_test="FleeFromAttacker*"
```

Slow scripts:

```bash
./tests/test_scripts/run_all_tests.sh --core-only --errors-only
./tests/test_scripts/run_controller_tests.sh --verbose
```

Boost.Test notes:

- Test names use the `BOOST_AUTO_TEST_CASE` name directly.
- Suite prefixes are optional.
- Use `--list_content` to confirm the exact test name before filtering.

See `tests/TESTING.md` for broader test documentation.

## Repo Map

- Source: `src/{core,managers,controllers,gameStates,entities,events,ai,collisions,utils,world,gpu}`
- Headers mirror source under `include/`
- Other important dirs: `tests/`, `docs/`, `res/`, `res/shaders/`
- Dependency direction: `Core -> Managers -> GameStates -> Entities/Controllers`
- Common architectural anchors: `GameEngine`, `ThreadSystem`, `EntityDataManager`, `AIManager`, `EventManager`, `ControllerRegistry`, `GPURenderer`, `GPUSceneRecorder`, `SpriteBatch`

## Core Rules

### C++ and APIs

- C++20, 4-space indent, Allman braces.
- Naming: UpperCamelCase types, lowerCamelCase functions/vars, `m_`/`mp_` members, ALL_CAPS constants.
- Prefer RAII, smart pointers, forward declarations, and non-trivial logic in `.cpp`. Use `.hpp` for C++ headers and `.h` for C headers.
- Use `const T&` for read-only non-trivial inputs, `T&` for mutation, and value for primitives.
- Prefer `std::span`, `std::string_view`, `std::optional`, and explicit read/mutate APIs.
- Use `const std::string&` for map lookups. Avoid `string_view -> string` churn.
- Do not introduce raw-pointer ownership, nullable raw-pointer parameters/returns, or raw-pointer optional out-parameters. Use references, values, `std::optional`, handles, and smart pointers instead.
- Avoid raw arrays, `const char*` constants, C-string APIs, and new legacy compatibility overloads in C++ code. Use `std::string_view` for non-owning text and `std::string` when storage or lifetime is needed.
- Keep new code idiomatic C++. Add C-style code only when required by SDL or another unavoidable C API, and isolate it at the final boundary.
- Use `std::format()` for logs. Never concatenate log strings with `+`. Use `AI_INFO_IF(cond, msg)` when only logging is conditional.
- Use `VOIDLIGHT_DEBUG_ONLY(...)` for debug-only code. Do not use raw `#ifdef DEBUG`.
- Remove unused parameter names entirely, for example `void foo(float)`. Do not use `(void)param` or commented names. Avoid `[[maybe_unused]]` in production except on empty virtual base defaults; in tests prefer real assertions over unused probes.
- Check important `[[nodiscard]]` bool returns such as `init()`, `load()`, and `create()`. Use `BOOST_REQUIRE()` in tests.
- Preserve the project copyright header:

```cpp
/* Copyright (c) 2025 Hammer Forged Games ... MIT License */
```

### Performance and Threading

- Avoid per-frame allocations. Reuse member buffers, call `reserve()` when size is known, preserve capacity with `clear()`, never `swap()` away reusable capacity, and prefer reusable ref-based buffer APIs over return-by-value patterns.
- Keep reusable scratch state thread-local when used from worker code.
- Main thread owns SDL events and rendering. Worker threads process batches only.
- Use `ThreadSystem`, not raw threads.
- Use `WorkerBudget` to decide threading and batch sizing, and report execution after work completes.
- Futures must complete before dependent operations.
- Avoid non-`thread_local` static state in threaded code.
- Align hot atomics with `alignas(64)` when contention matters.
- Use `include/utils/SIMDMath.hpp` for SIMD work. Process 4 elements per iteration plus a scalar tail.

### EDM, AI, and Controllers

- `EntityDataManager` is storage only. AI decision logic belongs in `Behaviors::` and `BehaviorExecutors`.
- `EDM::recordCombatEvent()` records stats and memory only; emotion math belongs outside EDM in AI/behavior code.
- Witnessed combat/death memories are behavior-consumed state; EDM stores memory records only.
- `AIManager::update()` commits command-bus changes and caches world/player data on the main thread before worker batches; behavior execution and emotional decay run in the AI batch path.
- Cross-frame state such as paths and timers belongs in EDM, not local temporaries.
- Controllers must never mutate AI behavior state directly in EDM.
- Use `Behaviors::queueBehaviorMessage()` from the main thread and `Behaviors::deferBehaviorMessage()` from worker threads.
- `Behaviors::switchBehavior()` enqueues behavior transitions. `AIManager::commitQueuedBehaviorTransitions()` clears behavior data before `init()`; set new behavior state after the transition commit, not before.
- EDM render data stores atlas coordinates and frame metadata, not texture ownership. Resolve manager-owned GPU textures at render submission and call `.get()` only at the final GPU API boundary.

### State Transitions and Events

- Call `prepareForStateTransition()` on active managers before cleanup.
- In AI-heavy states, clean up in this order when initialized: `AIManager`, `ProjectileManager`, `BackgroundSimulationManager`, `WorldManager`, `WorldResourceManager`, `EventManager`, `CollisionManager`, `PathfinderManager`, `EntityDataManager`, `WorkerBudgetManager`, `ParticleManager`.
- Demo states may skip managers they never initialized.
- `ControllerRegistry::clear()` must be called in `GamePlayState::exit()`, not just `unsubscribeAll()`.
- `EventManager` supports persistent and transient handlers. Persistent manager-level handlers register in `init()` with `registerPersistentHandler[WithToken]()` and survive transitions. State-level handlers register in `enter()` with `registerHandler[WithToken]()` and are cleared by `clearTransientHandlers()`. `clearAllHandlers()` is for shutdown only.
- Collision callbacks are manager-owned infrastructure; projectile collisions use the persistent projectile hit sink rather than state-owned callbacks.
- Do not manually unsubscribe and resubscribe persistent manager handlers across transitions.
- World-geometry caches, spatial indices, and reverse lookups must be cleared by transition cleanup or unload handling. Do not rely only on deferred `WorldUnloaded` after transition cleanup has begun.
- No game state should register collision callbacks directly.
- Use deferred transitions: set intent in `enter()`, then transition in `update()`.

### Rendering, UI, and GameState

- Exactly one present per frame. `GameEngine::render()` performs scene and UI rendering; `GameEngine::present()` performs the actual present. Never clear, end, submit command buffers, or present inside a game state.
- GPU flow is `beginFrame()`, state `recordGPUVertices()`, scene pass, composite to swapchain, UI pass, then `endFrame()` from `GameEngine::present()`. States implement `recordGPUVertices()`, `renderGPUScene()`, and `renderGPUUI()`.
- GPU scene textures stay at viewport dimensions. Zoom and sub-pixel offset belong in the composite shader, not tile scaling.
- GPU atlas interpretation is authoritative for atlas-backed EDM render data.
- For SDL3 GPU UI text, use `TTF_GetGPUTextDrawData()` only. Do not add UV flips, half-texel offsets, or shader hacks. Snap integer UI text placement to whole pixels before emitting vertices.
- Trace camera updates, interpolation, rounding, sub-pixel offsets, and draw submission before proposing jitter or flicker fixes. Do not apply speculative fixes for jitter, shimmer, or flicker.
- `DayNightController` requires `update(dt)` every frame. The GPU path already feeds it through `GPURenderer::setDayNightParams()`.
- Use `LoadingState` plus async `ThreadSystem` work for loading instead of blocking manual rendering.
- Call `setComponentPositioning()` after creating UI components. Prefer existing UI helpers such as `createTitleAtTop()`, `createButtonAtBottom()`, `createCenteredButton()`, and `createCenteredDialog()`.
- For UI/controller layout work, prefer `UIManager` public sizing, positioning, and relayout APIs. Do not reach back into `GameEngine` from controllers just to query window size or force UI relayout.
- Use `mp_stateManager->changeState()` for transitions. `GameEngine::Instance()` remains valid for non-transition engine access.
- Prefer local references over cached manager or controller members. Add controllers with `m_controllers.add<T>()` in `enter()` and do not keep cached `mp_*Ctrl` members.

### Tests, Debugging, and Traps

- Prefer direct test executables over slow wrapper scripts.
- Never relax test expectations to hide a production bug unless the user explicitly asks.
- For `EventManager` regressions, first distinguish missing state-owned handler wiring in tests from a production defect.
- Delete dead code and unused parameters. Do not comment them out.
- `FrameProfiler` uses `F3`. Prefer RAII timers: `ScopedPhaseTimer`, `ScopedManagerTimer`, `ScopedRenderTimer`, `ScopedRenderTimerGPU`. Profiling is a no-op in release builds; hitch detection starts above 20 ms.
- Demo states are test and showcase code. `GamePlayState` stays production-clean.
- Use `SettingsMenuState` and `MainMenuState` as menu references.
- `WorldResourceManager` is a spatial index over EDM, not a quantity store.
- `CollisionManager::subscribeWorldEvents()` is persistent manager infrastructure registered from `init()`; do not rewire it during state transitions.
