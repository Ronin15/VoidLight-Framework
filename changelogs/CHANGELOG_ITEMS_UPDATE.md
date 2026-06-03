/* Copyright (c) 2025 Hammer Forged Games, All rights reserved. Licensed under the MIT License - see LICENSE file for details */

# Items Update - Inventory, Container, and UI Flow Consolidation

**Branch:** `items`
**Date:** 2026-04-13 → 2026-06-03

---

## Executive Summary

The `items` branch focuses on tightening the item, inventory, and container gameplay path while cleaning up the supporting UI, validation, and profiling workflow around it. The main gameplay-facing work consolidates icon and equipment flow across the inventory/item/hotbar path, adds starter and container-driven item presentation, and fixes the open-range behavior so containers close when the player moves away.

The branch also pushes the item data model farther into a real item/resource split. `currency.json`, `equipment.json`, `items.json`, `materials.json`, and `weapons.json` replace the older monolithic resource bundle, and the supporting docs, factory code, and atlas tooling were updated with that split. On the UI side, the newer `HudController` and `InventoryController` paths replace the older gameplay HUD/item controller layout, which keeps item presentation and controller ownership cleaner.

Alongside the gameplay work, the branch removes stale UI state ownership from `UIManager`, fixes a focused text update bug, updates display constants for the FPS counter and related UI, and folds in a broad set of test, benchmark, valgrind, cppcheck, and docs cleanups so the new path is exercised and easier to maintain.

**Impact:**
- ✅ Inventory/item/hotbar icon flow consolidated to a single path
- ✅ Inventory sorting, swapping, hotbar reordering, and drag/drop behavior tightened
- ✅ Starter gear, starter chest, and container equipment UI added
- ✅ Container interaction now closes cleanly when the player leaves open range
- ✅ Player gear and hotbar presentation are better integrated into the inventory UI
- ✅ `UIManager` GPU state ownership removed and text update behavior fixed
- ✅ Validation flow tightened with release, threading, and AI benchmark fixes
- ✅ Valgrind, callgrind, and cppcheck tooling cleaned up for better signal
- ✅ Agent guidance and docs updated to match the newer item/UI structure

---

## Changes Overview

### Scale

| Metric | Value |
|--------|-------|
| Commits | 157 |
| Files changed | 311 |
| Lines added | 24,937 |
| Lines removed | 16,823 |
| Net change | +8,114 |

### Systems Touched (High Level)

| System | Change Type | Notes |
|--------|-------------|------|
| Items / inventory / containers | Gameplay + UI consolidation | One path for icons and equipment display, starter gear/chest support, open-range close behavior |
| Item data / resources | Data split | Item, currency, equipment, material, and weapon definitions split into dedicated JSON resources |
| UI / presentation | Refactor + bug fix | `UIManager` cleanup, text update fix, DPI and FPS counter adjustments |
| Controllers | Ownership cleanup | `HudController` and `InventoryController` replace the older gameplay HUD/item controller split |
| Testing / benchmarks | Validation update | Release test fix, flaky test cleanup, threading contract updates, AI scaling benchmark refresh |
| Profiling / analysis | Tooling cleanup | Valgrind and callgrind workflow fixes, cache analysis cleanup |
| Static analysis / code health | Cleanup | cppcheck stdlib replacements, dead code cleanup, parameter cleanup |
| Docs / agent guidance | Documentation update | Repo guidance and subsystem docs updated to match the current structure |

---

## Detailed Changes

### 1. Item, Inventory, and Container Flow Consolidation

The largest visible gameplay change is a cleaner path for item presentation and ownership across the inventory, item, and hotbar systems.

Highlights:
- Consolidated icon handling so the inventory/item/hotbar path uses one consistent flow
- Added inventory sorting and swapping support so item organization is no longer a dead end
- Added hotbar reordering and drag/drop behavior so equipped items can move without special-case UI
- Added starter gear and starter chest support to round out the early item experience
- Added container equipment UI so container contents and item state are surfaced in a dedicated view
- Updated container opening behavior so it closes when the player moves out of the allowed open range
- Kept player gear visible inside the inventory path instead of forcing a separate mental model
- Enforced tighter hotbar slot rules so duplicate-equipment behavior is explicit rather than accidental

This is the part of the branch that most directly changes how the player interacts with items in the world and in UI.

### 2. UI and Presentation Cleanup

The UI layer was cleaned up around the new item flow:
- Removed GPU state ownership from `UIManager`
- Fixed a `setText` bug that surfaced during refocused UI test work
- Reworked `HudController` and `InventoryController` around the new item flow
- Removed the older `GameplayHUDController` and `ItemController` layout instead of keeping parallel controller paths alive
- Adjusted DPI calculations and UI constants so the FPS counter and related UI elements sit correctly
- Cleaned up hotbar visibility when paused so UI state follows gameplay state instead of drifting

This keeps presentation state in the right place and avoids letting `UIManager` accumulate unrelated ownership.

### 3. Validation and Test Tightening

The branch includes a broad set of test follow-ups tied to the item and UI work:
- Release test fix
- Flaky attack retreat test fix
- Threading test updated for the new contract
- Test cleanup and refocusing work to keep the suite aligned with the current behavior
- AI scaling benchmark updated to reflect the multithreaded path
- Additional inventory, controller, and resource tests were refreshed around the reorganized item flow
- Callgrind and valgrind helper scripts were adjusted so the profiling side of the branch stays trustworthy

This is mostly maintenance work, but it matters because the branch changes user-facing flow and also touches test-sensitive timing and ownership code.

### 4. Profiling and Static Analysis Cleanup

The profiling and analysis tooling was sharpened instead of being left to drift:
- Fixed cache performance analysis output
- Fixed callgrind parsing
- Overhauled the valgrind workflow to be shorter and more useful
- Cleaned up quick memory check behavior
- Replaced cppcheck-triggered stdlib patterns and removed dead code
- Updated benchmark and analysis naming so the new item-oriented paths are easier to filter and compare
- Kept the quality/check tooling aligned with the new agent and subsystem guidance

These changes make the branch easier to inspect and reduce the amount of manual cleanup needed when validating performance or correctness.

### 5. Docs and Repo Guidance Updates

The branch also folds in a wide doc and guidance refresh:
- Updated agent guidance files
- Updated docs for the item, UI, manager, and test paths
- Bumped SDL3 to `3.4.10`
- Kept the repo instructions aligned with the current subsystem split
- Added doc coverage for the new resource split, inventory controller, HUD controller, and menu navigation helpers

That keeps the branch’s behavior and the documented workflow closer together, which matters for a repo with a lot of subsystem-specific conventions.

---

## Testing & Validation

This changelog is assembled from the `main...HEAD` diff and does not claim a fresh CI run in this writeup.
