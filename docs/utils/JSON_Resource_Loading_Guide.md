# JSON Resource Loading Guide

## Overview

The project uses focused runtime resource catalogs under `res/data/`:
`items.json`, `weapons.json`, `equipment.json`, `materials.json`, and
`currency.json`. This keeps balance data grouped by gameplay role while
preserving one runtime loading path through `ResourceTemplateManager`.

Use `ResourceTemplateManager` for loading and fast handle lookup, then use handles at runtime.

## Recommended Flow

```cpp
auto& templates = ResourceTemplateManager::Instance();
templates.init();

auto wood = templates.getHandleById("wood");
auto ironOre = templates.getHandleById("iron_ore");
auto goldCoins = templates.getHandleById("gold_coins");
```

Cache the resulting handles in gameplay code instead of repeating string lookups.

## Important Schema Fields

Common fields still include:

- `id`
- `name`
- `category`
- `type`
- `value`
- `maxStackSize`
- `consumable`
- `properties`

`id` is the stable data identifier used by `ResourceTemplateManager::getHandleById(...)`.
`textureId` is the common texture identity for both icon and world rendering;
`iconTextureId` and `worldTextureId` override the common value for those specific
uses. Atlas source rectangles come from `res/data/atlas.json` when the texture
ID has an atlas entry.

Supported exact `type` strings are:

- `Equipment`
- `Consumable`
- `QuestItem`
- `Ammunition`
- `CraftingComponent`
- `RawResource`
- `Gold`
- `Gem`
- `FactionToken`
- `CraftingCurrency`

## Gameplay Implications

### `maxStackSize` matters during gameplay

Stack sizes are gameplay-significant rather than cosmetic:

- harvested wood/stone/ore/gems can fill inventory quickly
- trading and gift flows depend on current stack limits
- harvesting falls back to dropped items when inventory insertion fails
- tests now exercise inventory capacity behavior more directly

When adding new resources, choose `maxStackSize` deliberately and assume it affects both UI and interaction logic.

### Merchant / economy content

The split catalogs include merchant-facing equipment, consumables, raw
materials, and currencies used by the trading/social systems. Add weapons to
`weapons.json`, armor and other gear to `equipment.json`, consumables and ammo
to `items.json`, crafting inputs to `materials.json`, and spendable currencies
to `currency.json`.

## Related Data

Resource loading interacts closely with NPC class data in `res/data/classes.json`:

- `isMerchant`
- `startingItems`
- `emotionalResilience`

Merchant inventories and social/trade behavior are only coherent when both resource definitions and class definitions are kept in sync.

## Runtime Guidance

- load templates at startup, not mid-frame
- convert IDs to handles during initialization
- use handles in inventories, harvesting, crafting, and trade flows
- validate stack sizes and item value assumptions when changing gameplay balance
