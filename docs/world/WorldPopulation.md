# WorldPopulation and spawnNpc

**Code:** `include/world/WorldPopulation.hpp`, `src/world/WorldPopulation.cpp`,
`include/world/NpcSpawn.hpp`, `src/world/NpcSpawn.cpp`

## Ownership

`WorldManager` coordinates load/unload and keeps the `worldId` populate
registry plus current-world settlement queries. Spawn algorithm lives in
`WorldPopulation`. NPC factory+optional behavior override lives in
`VoidLight::spawnNpc`.

`loadNewWorld` populates only when `WorldGenerationConfig::populate` is
`true` (the default). `false` loads the world (tiles, settlements,
harvestables) with no populated NPCs and no registry entry; AIDemo, EventDemo,
and NPC-free test fixtures use it.

`WorldHarvestInit` is the sibling harvest spawn helper. Do not put
environment, stance, forage, decision, discovery, or background-tick policy
on WorldManager.

## spawnNpc

```cpp
EntityHandle spawnNpc(const Vector2D& position,
                      const std::string& race,
                      const std::string& charClass,
                      Sex sex = Sex::Unknown,
                      uint8_t factionOverride = 0xFF,
                      const std::string& behaviorOverride = {});
```

`createNPCWithRaceClass` currently auto-registers `classes.json`
suggestedBehavior via `AIManager::registerEntity`. Empty `behaviorOverride`
leaves that assignment. Non-empty override calls `assignBehavior` after create
(writes `homeRole` and current `behaviorType`). `.claude/rules/managers.md`
still says EDM is storage-only; spawn assignment has not been peeled out of
create yet.

Callers: `WorldPopulation` (wilderness Warriors pass faction 1 and empty
behavior override so `classes.json` Chase is home role; settlement merchants,
guards, and villagers pass `settlement.faction` — `0xFF` remains the
class-default sentinel on `spawnNpc`), GamePlayState debug `R` (faction 1, no
Attack override, then `AIManager::adjustPlayerStanding` drops player standing
with faction 1 to the minimum; the stance table is untouched; not in the
populate registry), and `NPCSpawnEvent::execute`.

`EventManager::spawnMerchant` stays event sugar; populate does not go through
deferred MerchantSpawn.

Settlement merchants (Idle, `FLAG_MERCHANT`) get a survival-need entry at
registration whose forage leash is anchored at their spawn position with
`Behaviors::MERCHANT_FORAGE_LEASH_RADIUS` (384 px), which is defined as
`VILLAGE_RADIUS * TILE_SIZE` (`VILLAGE_RADIUS` is shared from
`include/world/WorldData.hpp` with `WorldGenerator`), so merchants forage
within one settlement radius of home.
