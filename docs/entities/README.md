# Entity System

## Overview

The entity layer uses `EntityDataManager` as the single source of truth for runtime data, with lightweight entity classes and `EntityHandle` references sitting on top.

Core themes:

- SoA entity storage in EDM
- generation-safe `EntityHandle`
- tiered simulation (`Active`, `Background`, `Hibernated`)
- player inventory/currency helpers backed by EDM inventories

## Core Docs

- [EntityHandle](EntityHandle.md)
- [EntityStates](EntityStates.md)
- [Resource](Resource.md)
- [EntityDataManager](../managers/EntityDataManager.md)

## Player Notes

`Player` remains a lightweight wrapper over EDM-backed state plus local animation/input behavior.

Player resource details:

- default player inventory size is `20` slots
- gold convenience helpers exist on `Player`
  - `getGold()`
  - `addGold(int amount)`
  - `removeGold(int amount)`
  - `hasGold(int amount)`
- those helpers wrap the cached `gold_coins` resource handle instead of adding a separate currency subsystem

Other notable player capabilities:

- inventory add/remove/query through EDM
- equipment helpers by `ResourceHandle`
- crafting and consumption helpers
- combat stat access through EDM-backed character data
- optional GPU vertex recording/render support

## NPC and World Entities

NPCs use EDM-backed AI, memory, and behavior data. Harvestables, dropped items, and containers also rely on EDM-backed storage, with higher-level managers and controllers providing orchestration.

See:

- [AIManager](../ai/AIManager.md)
- [NPC Memory](../ai/NPCMemory.md)
- [WorldResourceManager](../managers/WorldResourceManager.md)
