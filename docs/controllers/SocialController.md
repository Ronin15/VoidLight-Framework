# SocialController

**Code:** `include/controllers/social/SocialController.hpp`, `src/controllers/social/SocialController.cpp`

## Overview

`SocialController` owns player-facing social interactions with NPCs. It has two major roles:

- merchant trading
- relationship, memory, and crime reporting flows

It is state-scoped and updateable because trade UI pricing can refresh while a session is open.

## Trading API

### Session Management

```cpp
bool openTrade(EntityHandle npcHandle);
void closeTrade();
void handleTradeInput(const InputManager& inputMgr);
bool isTrading() const;
EntityHandle getMerchantHandle() const;
```

### Selection and Transactions

```cpp
void selectMerchantItem(size_t index);
void selectPlayerItem(size_t index);
void setQuantity(int qty);
TradeResult executeBuy();
TradeResult executeSell();
```

### Price / Relationship Queries

```cpp
float getCurrentBuyPrice() const;
float getCurrentSellPrice() const;
std::string getCurrentTradeRelationshipDescription() const;
float getCurrentTradePriceModifier() const;
float getRelationshipLevel(EntityHandle npcHandle) const;
float getPriceModifier(EntityHandle npcHandle) const;
bool willRefuseTrade(EntityHandle npcHandle) const;
std::string getRelationshipDescription(EntityHandle npcHandle) const;
```

## Social / Crime API

```cpp
bool tryGift(EntityHandle npcHandle, ResourceHandle itemHandle, int quantity = 1);
void recordInteraction(EntityHandle npcHandle, InteractionType type, float value = 0.0f);
void reportTheft(EntityHandle thief, EntityHandle victim, ResourceHandle stolenItem, int quantity = 1);
void alertNearbyGuards(const Vector2D& location, EntityHandle criminal);
```

## Runtime Behavior

- `openTrade()` validates that the NPC exists, is a merchant, and is willing to trade.
- Merchant and player inventories are read from EDM-backed inventory indices.
- Trade UI is created through `UIManager`; price display and selection highlights are controller-managed.
- Buying and selling update inventory, gold, and relationship/memory state.
- Theft reporting records negative interaction state, fires event traffic, and alerts nearby guards.

## GamePlayState Integration

`GamePlayState` spawns its bootstrap merchant through `EventManager::spawnMerchant(...)` so merchant creation stays on the event path. During gameplay, `Interact` prioritizes merchant trading first, then inventory pickup, then harvesting.

While a trade session is open, trade input is modal:

- `handleTradeInput(inputMgr)` consumes menu-style commands for pane switching, item selection, quantity adjustment, confirm, and cancel.
- quantity step shortcuts still use raw `-` / `+` key handling; they are not
  separate rebindable commands yet.
- normal gameplay input such as attack, pickup, and pause-sensitive UI interaction should not run through the usual path until the trade session closes.
- pause/exit paths call `closeTrade()` so state-owned UI does not survive transition cleanup.

## Data Dependencies

- merchant capability comes from NPC character data loaded from `classes.json`
- trade inventory is an EDM inventory
- relationship and emotional changes are backed by NPC memory data
- nearby guard queries reuse an internal buffer and route through AI/EDM helpers

## NPC Class Fields

The data-driven NPC class schema matters for this controller:

- `isMerchant`
- `emotionalResilience`
- `startingItems`

These drive who can trade, how NPCs react emotionally, and what merchants start with.
