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

- **ResourceTemplateManager `loadResourcesFromJson/*String/*File` unlocked map mutation** — the
  public loaders write `m_resourceTemplates` etc. without taking `m_resourceMutex`. Latent-only:
  the sole production caller is `init()`, which takes the exclusive lock and then calls the
  loaders while holding it (`shared_mutex` is non-recursive, so the loaders must stay lock-free);
  all other callers are single-threaded tests. Self-synchronizing the loaders would deadlock
  `init()`; a correct fix needs a public-locking-wrapper + lock-free-internal split (locking-
  architecture change), and making them private breaks the `[[nodiscard]]` public loader API the
  tests exercise. Reviewed 2026-07-13; left as-is.
- **CollisionManager `createStaticObstacleBodies` flood-fill neighbor bounds** — neighbor cells are
  bounds-checked against the current row's width, not `grid[ny]`'s width. Cannot trigger: worlds
  are always built rectangular (`WorldGenerator` `grid.resize(height, vector<Tile>(width))`), so
  every row has identical width. Adding a per-row bound would be blind hardening of an unreachable
  case. Reviewed 2026-07-13; left as-is.

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

- **`EntityDataManager::addToInventory` inline slots ignore `maxSlots`** — a review pass flagged this
  as inconsistent with `transferInventoryItem` (which clamps via `inlineSlotCountFor()`) and clamped
  the three inline loops to `min(INLINE_SLOT_COUNT, maxSlots)`. That change was reverted: it breaks
  `ResourceEdgeCaseTests::TestInventoryAddFailureDoesNotPartiallyMutate`, which creates
  `createInventory(1, true)` and requires `INLINE_SLOT_COUNT * maxStack - 2` items to fit in the inline
  fast-path regardless of `maxSlots`. The inline slots are intentionally a fixed-size fast path;
  `maxSlots` governs logical/overflow capacity, not inline occupancy. Pinned behavior — do not re-clamp
  without updating that test. Reviewed 2026-07-13.

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

- **SettingsManager `loadFromFile` collapses whole-valued floats to int** — a setting authored as a
  whole-number float (e.g. `masterVolume: 1.0`) reloads as an `int` variant, so a later typed
  `get<float>` returns the default instead of round-tripping. Real but blocked on a core-type change:
  `JsonValue` stores every number as a bare `double` with no integer-vs-decimal token flag, so the
  authored type cannot be recovered inside SettingsManager. Both self-contained workarounds break
  pinned `SettingsManagerTests` (`TestLoadFromFile` needs `get<int>("graphics","width")==1920`;
  `TestTypeMismatch` needs `get<float>` on an int-holding variant to return the default). A correct fix
  requires `JsonReader`/`JsonValue` to preserve and expose the integer-vs-decimal distinction (wide
  blast radius across SaveGame/resource-templates/world-config) plus forcing a decimal on float
  serialization — out of scope for a review-hardening pass. Reviewed 2026-07-13.
- **WorldManager `initializeWorldResources` ~13–18 full-grid scans at world load** — the three spawn
  lambdas each rescan the grid. Collapsing the ~12 obstacle passes into one bucketing scan needs new
  reusable member buckets and a lambda restructure; it is a one-time, off-render-hot-path load cost
  with no measured world-load-time concern. Per the finding's own conditional suggestion, deferred.
- **WorldGenerator `tryConnectBuildings` rescans the whole grid per connected building** — a correct
  fix needs a `buildingId → tiles/origin` map populated in `createBuilding` and threaded through the
  static placement signatures; connected buildings are known only by id at the relabel site. Bounded
  one-time async generation cost; structural change with world-geometry regression risk, deferred.

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
- **`BehaviorCommonState.hpp` `<utility>`** — used unconditionally for `PathData`'s
  `std::move` in the move ctor/assignment (both platforms). Only appears "unused" because
  libstdc++/Linux happens to pull it in transitively (via `<atomic>`) today; libc++/macOS
  does not, hence the missing-include build failure this was added to fix. Not a
  macOS-only need — do not gate behind `#ifdef __APPLE__` or drop on Linux.
