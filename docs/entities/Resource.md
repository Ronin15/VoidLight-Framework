# Resource

**Code:** `include/entities/Resource.hpp`, `include/entities/resources/`

## Overview

`Resource` is pure template data for items, materials, and currencies. It is not
an `Entity`, has no world transform, and does not own runtime behavior. Runtime
instances reference resource templates through `VoidLight::ResourceHandle`.

Dropped items, containers, merchants, player inventory, harvesting, and trading
store handles and quantities in EDM-backed data structures.

## Categories and Types

Categories:

- `Item`
- `Material`
- `Currency`

Types:

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

## Resource Families

Resource subclasses are grouped by gameplay role:

- `ItemResources.hpp`: equipment, consumables, quest items, ammunition
- `MaterialResources.hpp`: crafting components and raw resources
- `CurrencyResources.hpp`: gold, gems, faction tokens, crafting currency
- `EquipmentResources.hpp`: equipment-specific helpers and slot metadata

## Texture Data

Templates can define:

- `textureId`: shared texture identity
- `iconTextureId`: inventory/UI override
- `worldTextureId`: dropped/world rendering override
- atlas coordinates and animation metadata

`ResourceTemplateManager` applies atlas coordinates from `res/data/atlas.json`
when the texture ID has an atlas entry.

## Ownership Rules

- use `ResourceTemplateManager` to load and look up templates
- use `ResourceFactory` to deserialize JSON into typed `Resource` objects
- cache `VoidLight::ResourceHandle` values for runtime gameplay
- keep inventory quantities and slot order in EDM
- keep UI/drag/drop policy in controllers

## Related Docs

- [ResourceTemplateManager](../managers/ResourceTemplateManager.md)
- [ResourceFactory](../managers/ResourceFactory.md)
- [ResourceHandle System](../utils/ResourceHandle_System.md)
- [JSON Resource Loading Guide](../utils/JSON_Resource_Loading_Guide.md)
