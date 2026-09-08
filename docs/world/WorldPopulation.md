# WorldPopulation and spawnNpc

**Code:** `include/world/WorldPopulation.hpp`, `src/world/WorldPopulation.cpp`,
`include/world/NpcSpawn.hpp`, `src/world/NpcSpawn.cpp`

## Ownership

`WorldManager` coordinates load/unload and keeps the `worldId` populate
registry plus current-world settlement queries. Spawn algorithm lives in
`WorldPopulation`. NPC factory+optional behavior override lives in
`VoidLight::spawnNpc`.

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
(writes `homeRole` and current `behaviorType`). `include/managers/AGENTS.md`
still says EDM is storage-only; spawn assignment has not been peeled out of
create yet.

Callers: `WorldPopulation` (hostiles pass `"Attack"`; settlement NPCs pass
empty), GamePlayState debug `R` (`"Attack"`, faction 1, not in the populate
registry), `NPCSpawnEvent::execute`, and demo village setup that previously
created then assigned.

`EventManager::spawnMerchant` stays event sugar; populate does not go through
deferred MerchantSpawn.
