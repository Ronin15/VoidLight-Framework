# CombatController

**Code:** `include/controllers/combat/CombatController.hpp`, `src/controllers/combat/CombatController.cpp`

## Overview

`CombatController` is the player-facing combat helper used by gameplay and demo states. It is frame-updatable because it owns:

- attack cooldown timing
- stamina regeneration
- reusable query buffers for nearby AI targets

The controller performs ranged projectile attacks when the equipped weapon supports them, consumes compatible ammunition through EDM inventory, falls back to a melee weapon when one is available, performs melee hit detection by querying nearby AI/EDM entities directly, applies damage through the `DamageEvent` gameplay path, and updates the gameplay event log.

## Core API

```cpp
bool tryAttack();
void update(float deltaTime);
```

Constants:

```cpp
ATTACK_STAMINA_COST = 10.0f
STAMINA_REGEN_RATE = 15.0f
ATTACK_COOLDOWN = 0.5f
```

## Runtime Flow

1. `tryAttack()` checks cooldown and player stamina.
2. `performAttack()` verifies that a ranged attack can fire or that a melee fallback can be equipped.
3. successful ranged attacks consume ammunition and create a projectile.
4. melee attacks query nearby handles using a reused buffer.
5. valid melee targets are damaged through the current player/EDM combat path.
6. stamina, cooldown, and player attacking state are committed only after an attack can actually happen.
7. `update()` regenerates stamina.

## HUD Integration

`CombatController` does not own target-frame HUD state. It emits combat damage through the event path; `HudController` subscribes to combat events and owns the action HUD widgets (vitals, target frame). Production states must not rebuild those widgets through `UIManager` helpers.

## Notes

- The controller owns a reusable `m_nearbyHandlesBuffer` to avoid per-frame allocations.
- The controller updates the gameplay event log as part of player combat feedback.
