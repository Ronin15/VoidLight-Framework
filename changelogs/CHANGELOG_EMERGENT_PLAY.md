/* Copyright (c) 2025 Hammer Forged Games, All rights reserved. Licensed under the MIT License - see LICENSE file for details */

# Emergent Play Branch — Living World Foundations

**Branch:** `emergent_play`
**Date:** 2026-09-01 → 2026-09-30
**Roadmap:** [`docs/framework-implementation-slices.md`](../docs/framework-implementation-slices.md) (Slices 1–6, 6R, 6S)

---

## Executive Summary

`emergent_play` turns the generated world into a populated, reactive one. Worlds now load with settlements and their NPCs, factions hold stances toward each other and keep a separate standing with the player, NPC behavior scales with time of day and weather, and civilian NPCs have survival needs that drive them to forage harvestable resources. The work was delivered as numbered roadmap slices, each with runtime behavior, tests, docs, and a review before commit.

The second half of the branch is a cohesion pass. A branch-wide review produced Slice 6R (corrections across the earlier slices) and Slice 6S (pathfinding grid rebuilds moved back inside the pathfinder's own update), and codified the engine's threading contract: managers update sequentially on the main thread, and each manager's worker batches join before its update returns.

**Impact:**
- ✅ Worlds load populated: villages get a merchant, guards, and villagers; wilderness gets sparse hostiles; unload/reload neither leaks nor duplicates NPCs
- ✅ Load, spawn, and destroy contracts hardened (exclusive load window, structural mutex, harvestable unload destroy, `spawnNpc`, `WorldHarvestInit`)
- ✅ Environment-driven AI: time of day and weather scale detection, speed, and caution
- ✅ Faction stance table and territory; player standing per faction (assault, kill, theft, gift) separate from NPC factions
- ✅ NPC needs and the `Forage` behavior, with a local resource reserve, merchant leash near home, and `Scarcity` events
- ✅ Gameplay HUD ownership consolidated in `HudController` (vitals, target frame, hotbar, harvest progress bar)
- ✅ Weather particle variants follow `WeatherType` (heavy rain/snow, windy storm, fog)
- ✅ Pathfinding grid applies dirty rows in its own update slot (copy-then-publish), no detached rebuild work
- ✅ Collision query parity (`queryArea` / `queryAreaHasStaticOverlap`) and NPC-only crowd queries
- ✅ Repo-wide clang-format (K&R), streamlined ctest run (~40 s → ~10 s), static-analysis cleanup

---

## Changes Overview

### Scale

| Metric | Value |
|--------|-------|
| Commits | 48 |
| Files changed | ~505 |
| Lines changed (raw) | +62,290 / −51,580 |
| Lines changed (ignoring whitespace) | ~+22,300 / −11,300 |
| Date range | 2026-09-01 → 2026-09-30 |

The raw totals are dominated by the repo-wide clang-format pass; the whitespace-ignoring figures better reflect real code change.

---

## Roadmap Slices

### Slice 1 — Gameplay HUD ownership (Partial)
- `HudController` owns the persistent action HUD: player vitals, target frame, hotbar, and the new harvest progress bar.
- Combat HUD construction moved out of `UIManager`; pause/resume no longer enumerates HUD widget ids.
- Remaining: visual/GPU confirmation of the HUD in the running game.

### Slice 2 — World population
- `WorldManager` populates settlements from persisted settlement records on load (`docs/world/WorldPopulation.md`).
- Villages get a merchant, guards, and villagers; forest/haunted tiles outside settlements get sparse wilderness Warriors.
- Unload clears populated NPCs; reload does not duplicate them.

### Slice 3 — Load, spawn, and destroy contracts
- Exclusive load window and a structural mutex around entity creation during load.
- Harvestables are destroyed on world unload; `spawnNpc` and `WorldHarvestInit` give one spawn path.

### Slice 4 — Environment-driven AI
- `EnvironmentSnapshot` (detection, move speed, caution scales) combined from time period and weather, cached once per AI update and read by workers.

### Slice 5 — Faction stance and territory
- Faction stance table in `AIManager`, territory remapping in collision, stance-change events.
- Groundwork commit removed warriors attacking the player by default; hostility is now stance-based.

### Slice 6 — Survival and resource AI
- `NpcNeedData` sidecar for civilian NPCs and the `Forage` behavior (`BehaviorType::Forage`).
- Workers read a grid-bucketed harvestable snapshot; harvests commit on the main thread through `HarvestCommit`, shared with the player's `HarvestController`.
- NPCs keep a reserve of one node per local area; `EventTypeId::Scarcity` fires on local depletion.

### Slice 6R — Cohesion corrections
- **Player relations:** the player has no NPC faction; per-faction player standing is the source of truth, driven by `AIManager::recordPlayerIncident` (assault, kill, theft, gift).
- **Targeting:** `AIManager::scanHostileInRadius` replaces `scanFactionInRadius`; Attack acquisition range scales with detection; the unlimited-range player fallback is gone.
- **Forage:** merchants forage within a leash of home; inventory capacity is checked before commit; harvest arbitration is deterministic.
- **Weather contract:** pooled weather events carry type defaults; AI no longer multiplies visibility into detection.
- **Behavior messages:** `deferBehaviorMessage` is worker-only; the main-thread defer drain is removed.
- **Cleanup:** single harvestable container in `WorldResourceManager`; leftover `UIManager` animation/text-background APIs deleted; `ParticleManager` calls its threaded update directly.
- `WorldGenerationConfig::populate` lets AIDemo, EventDemo, and NPC-free fixtures skip population.

### Weather variants
- `ParticleManager::weatherEffectFor()` maps `WeatherType` to its particle variant (Rainy/Stormy → HeavyRain, Snowy → HeavySnow, Windy → WindyStorm, Foggy → Fog, Cloudy → Cloudy).
- `EventFactory::createWeatherEvent` takes an optional intensity and transition time; weather names parse to `WeatherType`.

### Slice 6S — Pathfinding grid rebuilds in-slot
- `PathfinderManager::update()` takes dirty rows from the published grid, rebuilds them in WorkerBudget-sized batches joined inside its own update, then publishes the new grid.
- The detached incremental rebuild, its dirty-percentage threshold, and the manager-level weight-field API are removed.
- `CollisionManager::queryAreaHasStaticOverlap` includes event-only triggers; the load-time static rebuild no longer floods `CollisionObstacleChanged`.

---

## Cross-Cutting Changes

- **Collision and crowd queries:** `CollisionManager::queryArea` does a true AABB overlap and includes event-only triggers; `setBodyEnabled` dirties the static hash; `PerfStats::staticHashRebuilds` added. AI crowd queries use `AIManager::scanActiveIndicesInRadius` (NPCs only) with a monotonic cache stamp that never survives a state transition.
- **Demo states:** Advanced AI, UI, and overlay demo states removed; AIDemo (load test) and EventDemo (power bench) kept as test-only states.
- **Static analysis:** const-correct read-only references, value-initialized `WorldGenerationConfig`, NUL-terminated window title, `std::array` in place of raw arrays, and removal of the unimplemented `ParticleManager::registerEffect()` and an unused BSM constant.
- **Formatting:** repo-wide clang-format to K&R braces.
- **Dependencies:** SDL3 bumped to 3.4.16.

---

## Testing

- New and expanded suites for world population, faction stance and player standing, environment snapshots, NPC needs and forage, harvestable snapshots, collision state transitions, and pathfinder in-slot rebuilds.
- ctest suite streamlined from ~40 s to ~10 s.
- `tests/TESTING.md` counts re-verified against `--list_content` for every documented suite.

---

## Documentation & Tooling

- `docs/framework-implementation-slices.md` holds the roadmap, slice records, and follow-ups for Slices 6.1–12.
- `docs/ARCHITECTURE.md` documents the sequential-manager threading contract and its listed exceptions.
- Docs audited against the current code: removed APIs, signatures, examples, and the `docs/README.md` index.
- Claude agent pipeline: design (`cpp-design-specialist`) → implement (`game-engine-specialist`) → review (`game-systems-architect`), with `systems-integrator` for cross-system analysis and `quality-engineer` for verification. Grok tooling archived.

---

## Known Open Items

- Slice 1 visual HUD confirmation.
- Branch/PR gate: core suite, ASan, and TSan runs (TSan also serves as Slice 6S acceptance).
- AIDemo has no hostile faction stances, so ranged NPCs no longer fire there; tuning deferred.
- Follow-ups routed to Slices 6.1 (harvestable respawn), 7, and 12 in the roadmap.
