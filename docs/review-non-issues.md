# Review Non-Issues — Adjudicated Findings (do not re-flag)

This file records review findings that were **investigated and deliberately not changed**,
with the rationale. Code reviewers — human or agent — should read this before re-raising
any item here. If you believe an entry is now wrong (the surrounding code changed, the
assumption no longer holds), re-verify by tracing the current code path and update this
file in the same change rather than silently "fixing" it.

References are by symbol/function (not line numbers) so they survive edits.

---

## A. Confirmed non-issues (looks like a bug, isn't)

- **`AIManager::update` — `enqueueBatch(std::move(m_allDamageEvents))`**
  Not a capacity-destroying move of a reusable buffer. `EventManager::enqueueBatch`
  takes `std::vector<DeferredEvent>&&` (an rvalue *reference*) and only pillages
  per-element `event.data`; the outer vector's storage is never transferred, so
  `m_allDamageEvents` keeps its capacity. The following `clear()` retains it. Correct as-is.

- **`Behaviors::clearPendingMessages` (BehaviorExecutors.cpp)**
  Not dead code. Live callers exist in `tests/BehaviorFunctionalityTest.cpp`. Keep it.

- **GamePlayState registry-access null-check asymmetry**
  Some `m_controllers.get<T>()` sites deref without a null check while others guard.
  All unchecked sites are inside `if (mp_Player)` and the controllers are unconditionally
  added in `enter()`, so null cannot occur. Stylistic only.

- **GamePlayState render-controller asymmetry** (`NPCRenderController`/`ProjectileRenderController`
  as direct members vs `ResourceRenderController` in the registry)
  Intentional: `ResourceRenderController::update` takes a `Camera&` so it can't ride the
  parameterless `updateAll()`. No double-update path. Documented inline.

## B. Thread/lifecycle items that are latent-only under current usage

- **ThreadSystem `getTaskStats` unlocked read** — diagnostics-only, benign data race on
  size_t counters read off the hot path. Not worth synchronizing.
- **ThreadSystem `isBusy`/accessors vs `clean()` TOCTOU** — `clean()` runs on the main
  thread at shutdown after states are torn down; no other thread enqueues/queries then.
  Cannot trigger.
- **ThreadSystem `Exists()` returns true before `init()`** — semantic quirk only; all
  callers (`WorkerBudget`) re-guard via `getThreadCount()<=1` / `m_threadPool` checks.
  "Fixing" it to read `m_threadPool` would introduce a real race on a non-atomic
  `unique_ptr` from a static method — worse than the quirk. Leave it.
- **WorkerBudgetManager non-atomic EMA/hill-climb grouping** — correct because the whole
  manager is driven from the main thread during sequential manager updates. The atomics /
  `alignas(64)` are defensive, not load-bearing for a multi-writer case that doesn't exist.
- **SoundManager asymmetric locking** (`loadSFX/loadMusic` take `m_loadMutex` on a worker;
  `playSFX/playMusic/cleanupStoppedTracks/clean` are unlocked) — init futures are joined
  before any main-thread play/clean call, so there is no concurrent map access. Do **not**
  add locks to the per-frame play path to defend a case that can't occur (audio-latency risk).
- **EventManager combat-commit cached EDM indices** (`prepareCombatBatch` caches index on a
  worker, `commitPreparedCombatEvent` uses it on the main thread) — safe because destruction
  is deferred and the SoA free-list bumps generation without compaction. Documented inline at
  the cache/use sites and at `EntityDataManager` `destroyEntity`/`freeSlot`. If destruction
  ever becomes synchronous/compacting, audit these callers.

## C. Behavior pinned / range decisions

- **SoundManager volume range `[0, 10]`** — the runtime clamp is intentionally 0.0–10.0
  (>1.0 amplifies) and is pinned by `tests/managers/ManagerRuntimeTests.cpp`
  (`TestInitialStateAndVolumeClamping`). Header docs were aligned to `[0,10]`; do not
  "fix" the clamp to `[0,1]` without also updating that test.
- **SoundManager per-call `volume` multiplier discarded by `setSFXVolume/setMusicVolume`** —
  preserving it across a global volume change needs per-track base-volume state (a new
  feature), intentionally not added.
- **WorldManager `WorldUnloaded` dispatch is `Immediate` (NOT Deferred)** — required: world
  replacement relies on unload handlers firing synchronously before the new world activates
  (asserted by `WorldManagerTests::TestWorldReplacementUnloadsBeforeActivatingNewWorld`, and
  per the documented "do not rely only on deferred WorldUnloaded" invariant). A prior review
  pass changed this to Deferred and it was reverted. Documented inline.

## D. Real but deliberately out-of-scope (deferred work, not a quick fix)

- **ParticleManager dead double-buffer machinery** (`particles[1]`, `swapBuffers` only bumps
  the epoch, `activeBuffer` never changes) — genuinely unused, but removal touches ~15 sites
  woven through the lock-free epoch coordination. Multi-site refactor with regression risk;
  not done as part of a hardening pass.
- **Camera shake computed but never applied** (`Camera::update` sets `m_shakeOffset`, but
  `getRenderOffset`/`getViewRect` never read it; no `Camera::shake()` callers) — wiring this
  into the render-offset pipeline is feature work, not a bug fix.
- **GPURenderer one-frame viewport/scene-texture mismatch on resize** — the scene records
  against the old viewport while the swapchain is acquired (and viewport synced) in
  `beginScenePass` after recording; self-corrects next frame. A real fix means restructuring
  the frame lifecycle (acquire before record), which is out of scope. Comment corrected to
  state the actual order.
- **WorldResourceManager stale spatial-index entries** — dead EDM indices are filtered from
  query *output* (`isAlive()`) but not erased from the index, and entries are fully cleared
  at state transition. Opportunistic erase during a query is impossible under the `shared_lock`
  the queries hold; upgrading to an exclusive lock would serialize the hot render/query path.
  Relies on EDM destruction always unregistering (it does today). Leave as-is.
- **ProjectileManager SIMD vs scalar-tail FP rounding** — full 4-wide batches use FMA
  (single rounding) while a `<4` trailing batch uses mul+add (two roundings), so positions can
  differ by ~1 ulp. Determinism micro-note, not a correctness bug.

## E. Intentional includes — do NOT strip on a clangd "unused" warning

clangd (macOS/libc++) flags these as unused because the using code is in an inactive
preprocessor branch or another TU on this platform. They are required cross-platform.
See the `cross-platform-include-caution` rule.

- **`SpriteBatch.hpp` `<vector>`** — used by `SpriteBatch.cpp` (`std::vector<uint32_t> indices`),
  which has no direct include; kept in the header for that TU and any consumers.
- **`EventManager.hpp` `<unordered_map>`** — central hub header; kept to protect consumers
  that may rely on it transitively on Windows/Linux.
- **`SaveManagerTests.cpp` `<thread>`** — used only by a `#ifdef _WIN32` `std::this_thread::sleep_for`,
  compiled out on macOS/Linux. Required on the Windows build.
