# CLAUDE.md — VoidLight-Framework

Guidance for Claude Code working in this repository.

## Project Stance

- Performance-oriented, data-oriented, minimal abstraction overhead.
- Prioritize memory safety, cache efficiency, low latency, minimal allocations, and deterministic behavior.

## Build

```bash
# Debug / Release
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build

# Reconfigure (required when switching sanitizers or build options)
rm build/CMakeCache.txt && cmake -B build/ ...

# Run
./bin/debug/VoidLight_Template
```

**Sanitizers** (mutually exclusive; remove `CMakeCache.txt` to switch):
```bash
# ASan
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" -DUSE_MOLD_LINKER=OFF && ninja -C build

# TSan
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" -DUSE_MOLD_LINKER=OFF && ninja -C build
export TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"
```

## Testing

Boost.Test executables. See `tests/TESTING.md`. Prefer direct executables over wrapper scripts.

```bash
./bin/debug/<test_executable>                         # all tests
./bin/debug/<test_executable> --list_content          # list cases
./bin/debug/<test_executable> --run_test="TestCase*"  # specific case

# Concrete examples
./bin/debug/entity_data_manager_tests
./bin/debug/ai_manager_edm_integration_tests
./bin/debug/behavior_functionality_tests --run_test="FleeFromAttacker*"

./tests/test_scripts/run_all_tests.sh --core-only --errors-only  # slow (7–20 min)
./tests/test_scripts/run_controller_tests.sh --verbose
```

Test names use the `BOOST_AUTO_TEST_CASE` name directly (`ThreadingModeComparison`, not `TestThreadingModeComparison`). Suite prefix optional; verify with `--list_content`.

## Architecture

C++20 SDL3 engine, CMake/Ninja, data-oriented, 10K+ entities at 60+ FPS.

**Dependency direction**: `Core → Managers → GameStates → Entities/Controllers`

**Layout**: `src/` and `include/` mirror each other: `{core, managers, controllers, gameStates, entities, events, ai, collisions, utils, world, gpu}`. Tests in `tests/`, assets in `res/`, docs in `docs/`.

### Key Systems
- **Core**: GameEngine (fixed timestep) | ThreadSystem (WorkerBudget) | Logger | TimestepManager
- **Managers**: EntityDataManager (SoA) | AIManager (SIMD, 10K+) | EventManager (16 types) | CollisionManager (HierarchicalSpatialHash) | ParticleManager (SoA, pooled) | PathfinderManager | WorldManager (chunk-based procedural) | WorldResourceManager (spatial registry) | BackgroundSimulationManager (tiered) | UIManager | GameTimeManager | InputManager | TextureManager | FontManager | SoundManager
- **Entities**: EntityKind (10 types) | SimulationTier (Active/Background/Hibernated) | EntityHandle (generation-safe)
- **AI**: AIBehavior base → 9 behaviors (Idle, Wander, Patrol, Chase, Flee, Follow, Guard, Attack, Custom). Lock-free EDM access via `BehaviorContext`.
- **Controllers**: State-scoped via ControllerRegistry. `controllers/{combat,social,world,render}/`
- **Utils**: Camera | Vector2D | SIMDMath (SSE2/NEON/AVX2) | JsonReader | BinarySerializer | UniqueID | FrameProfiler
- **GPU**: GPUDevice | GPURenderer | GPUSceneRecorder | GPUShaderManager (SPIR-V/Metal) | SpriteBatch (50K) | GPUVertexPool (triple-buffered). Shaders in `res/shaders/`.

### Rendering

Flow: `beginFrame()` → state `recordGPUVertices()` → scene pass → composite to swapchain → UI pass → `endFrame()` from `GameEngine::present()`. Platform-native shaders (Metal/macOS, DXIL/Windows, SPIR-V/Linux). Game states implement `recordGPUVertices()`, `renderGPUScene()`, and `renderGPUUI()`; the engine owns frame lifetime.

- Scene texture = viewport dimensions (1x). Zoom lives in the composite shader, not tile scaling. Composite uses LINEAR sampler: `uv = fragTexCoord / zoom + subPixelOffset`.
- **GPU atlas interpretation is authoritative** for atlas-backed EDM render data.
- **One present per frame**: `GameEngine::render()` handles scene+UI; `GameEngine::present()` ends the GPU frame. NEVER end the frame, submit command buffers, or present from a GameState.
- **Render ownership**: EDM render data stores atlas coordinates and frame metadata, not texture ownership. Resolve manager-owned GPU textures at render submission and call `.get()` only at the final GPU API boundary.
- **GPU UI text**: `TTF_GetGPUTextDrawData()` only — no UV flips, half-texel offsets, or shader hacks. Snap text to whole pixels for integer UI layouts.
- **DayNightController**: Needs `update(dt)` each frame (30s lighting transitions). `GPURenderer::setDayNightParams()` does this automatically.
- **Loading**: Use `LoadingState` with async ThreadSystem ops, not blocking manual rendering.
- **Deferred transitions**: Set flag in `enter()`, transition in `update()`.

### State Transitions

AI-heavy cleanup order:
`AIManager → ProjectileManager → BackgroundSimulationManager → WorldManager → WorldResourceManager → EventManager → CollisionManager → PathfinderManager → EntityDataManager → WorkerBudgetManager → ParticleManager`

- Call `prepareForStateTransition()` before cleanup (pauses work, drains queues, waits for batches).
- Demo states may skip managers they never initialized.
- `ControllerRegistry::clear()` (not just `unsubscribeAll()`) must be called in `GamePlayState::exit()`.
- **World lifecycle**: Manager-local caches, spatial indices, and reverse lookups tied to world geometry must be cleared by transition cleanup or unload handling. Do not rely only on deferred `WorldUnloaded` after transition cleanup has begun.

### Event Handler Persistence

EventManager has **persistent** and **transient** handlers. `prepareForStateTransition()` calls `clearTransientHandlers()` — persistents survive. `clearAllHandlers()` is shutdown-only.

- **`init()` → `registerPersistentHandler[WithToken]()`**: manager infrastructure (CollisionManager world events, ProjectileManager collision handler, PathfinderManager world/obstacle events, WorldManager season events). Registered once.
- **`enter()` → `registerHandler[WithToken]()`**: state-level handlers (GamePlayState time/weather/harvest, ControllerRegistry subscriptions). Auto-cleared on transition.

Never manually unsubscribe/resubscribe manager handlers across transitions.

## Standards

**C++20** | 4-space indent, Allman braces | RAII + smart pointers | ThreadSystem (not raw threads) | STL algorithms > loops

**Naming**: UpperCamelCase (classes) | lowerCamelCase (functions/vars) | `m_`/`mp_` members | ALL_CAPS (constants)

**Headers**: `.hpp` C++, `.h` C | forward declarations | non-trivial logic in `.cpp`

**Params**: `const T&` read-only, `T&` mutation, value for primitives. `const std::string&` for map lookups (never `string_view`→`string` conversion). Prefer `std::span`, `std::string_view`, `std::optional`. Avoid raw arrays and nullable pointer-return accessors.

**Stored raw pointers**: Not for ownership or long-lived cached state. Materialize only at the final C API submission boundary.

**Logging**: `std::format()`, never `+` concatenation. Use `AI_INFO_IF(cond, msg)` when the condition only gates logging. Use `VOIDLIGHT_DEBUG_ONLY(...)` for debug-only blocks — never raw `#ifdef DEBUG`. Defined in `Logger.hpp`.

**Unused parameters**: Drop the name, keep the type: `void foo(float)`. Never `(void)param;` or commented names to suppress warnings. Avoid `[[maybe_unused]]` in production except on empty virtual base defaults; in tests prefer real assertions over unused probes.

**`[[nodiscard]]`**: Required on critical bool-returning functions (`init()`, `load()`, `create()`). Check with `BOOST_REQUIRE()` in tests, `if (!init())` in production.

**Copyright header**: `/* Copyright (c) 2025 Hammer Forged Games ... MIT License */`

### Threading

Sequential execution with parallel batching. Main thread owns SDL (events, render); workers process batches.

- **ThreadSystem**: Pool of `hardware_concurrency - 1` workers, 5 priorities (Critical→Idle). `enqueueTaskWithResult()` for futures, `batchEnqueueTasks()` for bulk.
- **WorkerBudget**: Adaptive batch sizing. `shouldUseThreading()` / `getBatchStrategy()` / `reportExecution()` for unified throughput tracking.
- **Thread-local**: RNG (`thread_local std::mt19937`), reusable buffers, spatial caches. Collect from thread_local vectors via ref-based API with `clear()` — never `swap()` or return-by-value (destroys capacity → per-frame allocations per worker).
- **Sync**: `shared_mutex` (reader-writer) | `mutex` (exclusive) | `atomic<bool>` (flags) | `condition_variable` (worker wake).
- **Rules**: NEVER static vars in threaded code | Main thread only for SDL | Futures complete before dependent ops | Cache-line align hot atomics (`alignas(64)`).

**Manager pattern** (AIManager, ParticleManager):
```cpp
auto decision = budgetMgr.shouldUseThreading(SystemType::AI, count);
if (decision.shouldThread) {
    for (size_t i = 0; i < decision.batchCount; ++i) {
        m_futures.push_back(threadSystem.enqueueTaskWithResult([...] { processBatch(...); }));
    }
    for (auto& f : m_futures) { f.get(); }  // wait before frame ends
}
budgetMgr.reportExecution(SystemType::AI, count, decision.shouldThread, decision.batchCount, elapsedMs);
```

## Memory

No per-frame allocations. Reuse member buffers; `reserve()` when size is known:
```cpp
class Manager {
    std::vector<Data> m_buffer;
    void update() { m_buffer.clear(); /* clear() keeps capacity */ }
};
```

## UI Positioning

**Always** call `setComponentPositioning()` after creating components for resize/fullscreen support.

- Helpers: `createTitleAtTop()`, `createButtonAtBottom()`, `createCenteredButton()`, `createCenteredDialog()`
- Manual: `ui.createButton("id", rect, "text"); ui.setComponentPositioning("id", {UIPositionMode::TOP_ALIGNED, ...});`
- Modes: `ABSOLUTE`, `TOP_ALIGNED`, `BOTTOM_ALIGNED`, `LEFT/RIGHT_ALIGNED`, `TOP_RIGHT`, `BOTTOM_RIGHT`, `BOTTOM_CENTERED`, `CENTERED_H`, `CENTERED_V`, `CENTERED_BOTH`

## GameState Architecture

- **Transitions**: `mp_stateManager->changeState()`, never `GameEngine::Instance()` for transitions. `GameEngine::Instance()` remains valid for non-transition access (pause, window sizing, shutdown).
- **Manager/Controller access**: Local references, not cached `mp_*` members. Cache at function top when used multiple times; single use → call directly. No cached `mp_*Ctrl` pointers. Add controllers with `m_controllers.add<T>()` in `enter()`.
- **UI from controllers**: Prefer `UIManager` public sizing/positioning/relayout APIs. Do not reach back into `GameEngine` from controllers just to query window size or force UI relayout.

```cpp
void SomeState::update(float dt) {
    const auto& inputMgr = InputManager::Instance();            // multi-use → cache
    auto& combatCtrl = *m_controllers.get<CombatController>();
    // ... use throughout
}
m_controllers.get<WeatherController>()->getCurrentWeather();    // single use → direct
```

- **Lazy string caching**: Recompute enum→string only on change: `if (m_phase != m_lastPhase) { m_str = getPhaseString(); m_lastPhase = m_phase; }`
- **Layout caching**: Compute static positions in `enter()`, reuse in `render()`.

## Debug Tools

**FrameProfiler** (F3): Three-tier timing (Frame → Manager → Render). RAII timers: `ScopedPhaseTimer`, `ScopedManagerTimer`, `ScopedRenderTimer`, `ScopedRenderTimerGPU`. Hitch detection >20 ms. No-op in Release.

## Repo Traps

- `GamePlayState` is the pristine official gameplay state — keep it clean and production-ready. Demo-suffixed states (`EventDemoState`, `UIDemoState`, `AIDemoState`, `AdvancedAIDemoState`, `OverlayDemoState`) are for testing/showcasing.
- Reference menu patterns: `SettingsMenuState`, `MainMenuState`.
- `WorldResourceManager` is a spatial index over EDM, not a quantity store.
- `CollisionManager::subscribeWorldEvents()` is persistent manager infrastructure registered from `init()`; do not rewire it during state transitions.

## Working Principles

- Read the exact code path and search for matching patterns in the same subsystem before editing.
- Do not assume system ownership, hot paths, or participating managers. Trace them in code first.
- Use established systems and patterns (UIManager helpers, state architecture, existing constants). NEVER create ad-hoc alternatives when a pattern exists.
- Prefer minimal, direct fixes over new abstractions. No unnecessary safety checks, statistical analysis, or helper layers beyond what was asked.
- Do not add compatibility overloads, ad-hoc safety layers, or new abstractions unless the task requires them.
- When the user names a file, work on exactly that file — don't substitute similar-sounding ones.
- Keep production code and tests aligned in the same change. Run the most targeted test executable when feasible.
- Before finishing, run the most targeted build or test feasible and state exactly what was verified.
- Fix root causes in production code. NEVER bypass failing tests by changing expectations unless explicitly told to.
- For `EventManager` regressions, distinguish missing state-owned handler wiring in tests from a production bug before editing prod code.
- For rendering issues (jitter/shimmer/flicker), trace the full pipeline first (camera → interpolation → floor/round → sub-pixel offset → draw). No speculative fixes.
- Delete dead code and unused parameters entirely — never comment out.
