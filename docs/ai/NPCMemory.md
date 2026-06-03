# NPC Memory

## Overview

NPC memory is stored in EDM and consumed by AI-layer logic. It tracks remembered entities, locations, emotional state, and combat/social history.

Primary data lives in `NPCMemoryData` and related memory entry structures inside `EntityDataManager`.

## What EDM Owns

EDM owns storage for:

- memory entries
- emotional state values
- location history
- combat totals such as damage dealt/received
- last attacker / last target bookkeeping

EDM should remain a storage and aggregation layer.

## What AI Owns

AI-layer behavior code owns interpretation:

- personality-scaled emotion changes
- witnessed-combat falloff and alert/flee decision policy
- alert/fear/aggression responses
- emotional contagion pre-pass in `AIManager::update()`

## Common Flows

### Direct combat

`EventManager` performs the built-in `DamageEvent` result and records factual
combat data through EDM. `EntityDataManager::recordCombatEvent()` updates combat
totals, last attacker/target bookkeeping, and memory entries only; it does not
own behavior policy.

Behavior executors read memory and character state during `AIManager::update()`
and decide how emotions or behavior messages should affect the NPC.

### Witnessed combat

Witnessed combat is represented as memory data in EDM and interpreted by the
behavior layer. Distance falloff, composure, panic, guard alerts, and other
responses are behavior policy, not EDM storage policy.

### Social interactions

`SocialController` writes gifts, trade, theft, and other interactions into memory-backed relationship flows.

## Testing Coverage

Dedicated memory coverage lives in `tests/managers/NPCMemoryTests.cpp`, including:

- structure/layout assumptions
- add/find memory behavior
- emotional state decay and clamping
- combat statistics
- cleanup and state transition handling
