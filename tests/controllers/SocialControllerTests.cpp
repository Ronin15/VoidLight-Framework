/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file SocialControllerTests.cpp
 * @brief Tests for SocialController
 *
 * Tests NPC social interactions: trading, gifts, and relationships.
 */

#define BOOST_TEST_MODULE SocialControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/social/SocialController.hpp"
#include "entities/Player.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "events/EntityEvents.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIManager.hpp"
#include "../events/EventManagerTestAccess.hpp"
#include <cmath>
#include <memory>
#include <vector>

// ============================================================================
// Test Fixture
// ============================================================================

class SocialControllerTestFixture {
public:
    SocialControllerTestFixture() {
        // Reset EventManager to clean state
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());

        // Initialize managers
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(AIManager::Instance().init());
        BOOST_REQUIRE(GameTimeManager::Instance().init());
        BOOST_REQUIRE(UIManager::Instance().init());

        player = std::make_shared<Player>();
        player->initializeInventory();
        BOOST_REQUIRE(player->getHandle().isValid());

        goldHandle = ResourceTemplateManager::Instance().getHandleById("gold_coins");
        breadHandle = ResourceTemplateManager::Instance().getHandleById("bread");
        BOOST_REQUIRE(goldHandle.isValid());
        BOOST_REQUIRE(breadHandle.isValid());
    }

    ~SocialControllerTestFixture() {
        UIManager::Instance().prepareForStateTransition();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }

    // Non-copyable
    SocialControllerTestFixture(const SocialControllerTestFixture&) = delete;
    SocialControllerTestFixture& operator=(const SocialControllerTestFixture&) = delete;

protected:
    struct ResourceEventRecord {
        EntityHandle owner;
        VoidLight::ResourceHandle handle;
        int delta{0};
    };

    EntityHandle spawnMerchant(const Vector2D& position = Vector2D(100.0f, 100.0f)) {
        EntityHandle merchant = EntityDataManager::Instance().createNPCWithRaceClass(
            position, "Human", "GeneralMerchant");
        BOOST_REQUIRE(merchant.isValid());
        return merchant;
    }

    EntityHandle spawnNPC(const std::string& charClass,
                          const Vector2D& position = Vector2D(100.0f, 100.0f)) {
        EntityHandle npc = EntityDataManager::Instance().createNPCWithRaceClass(
            position, "Human", charClass);
        BOOST_REQUIRE(npc.isValid());
        return npc;
    }

    static int resourceDeltaFor(const std::vector<ResourceEventRecord>& events,
                                EntityHandle owner,
                                VoidLight::ResourceHandle handle) {
        int total = 0;
        for (const auto& event : events) {
            if (event.owner == owner && event.handle == handle) {
                total += event.delta;
            }
        }
        return total;
    }

    std::shared_ptr<Player> player;
    VoidLight::ResourceHandle goldHandle;
    VoidLight::ResourceHandle breadHandle;
};

// ============================================================================
// TradeResult Enum Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(TradeResultTests)

BOOST_AUTO_TEST_CASE(TestTradeResultValues) {
    // Ensure all enum values are distinct
    BOOST_CHECK(TradeResult::Success != TradeResult::InsufficientFunds);
    BOOST_CHECK(TradeResult::InsufficientFunds != TradeResult::InsufficientStock);
    BOOST_CHECK(TradeResult::InsufficientStock != TradeResult::InvalidNPC);
    BOOST_CHECK(TradeResult::InvalidNPC != TradeResult::InvalidItem);
    BOOST_CHECK(TradeResult::InvalidItem != TradeResult::InventoryFull);
    BOOST_CHECK(TradeResult::InventoryFull != TradeResult::NPCRefused);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// InteractionType Enum Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(InteractionTypeTests)

BOOST_AUTO_TEST_CASE(TestInteractionTypeValues) {
    // Ensure all enum values are distinct
    BOOST_CHECK(InteractionType::Trade != InteractionType::Gift);
    BOOST_CHECK(InteractionType::Gift != InteractionType::Greeting);
    BOOST_CHECK(InteractionType::Greeting != InteractionType::Help);
    BOOST_CHECK(InteractionType::Help != InteractionType::Theft);
    BOOST_CHECK(InteractionType::Theft != InteractionType::Insult);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Basic State Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SocialControllerStateTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestSocialControllerName) {
    SocialController controller(nullptr);

    BOOST_CHECK_EQUAL(controller.getName(), "SocialController");
}

BOOST_AUTO_TEST_CASE(TestSubscribe) {
    SocialController controller(nullptr);

    controller.subscribe();
    BOOST_CHECK(controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestDoubleSubscribe) {
    SocialController controller(nullptr);

    controller.subscribe();
    controller.subscribe();  // Should be no-op
    BOOST_CHECK(controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestConstants) {
    // Verify price multipliers are reasonable
    BOOST_CHECK_GT(SocialController::BUY_PRICE_MULTIPLIER, 1.0f);  // Markup
    BOOST_CHECK_LT(SocialController::SELL_PRICE_MULTIPLIER, 1.0f); // Markdown

    // Verify relationship thresholds are ordered
    BOOST_CHECK_LT(SocialController::RELATIONSHIP_HOSTILE, SocialController::RELATIONSHIP_UNFRIENDLY);
    BOOST_CHECK_LT(SocialController::RELATIONSHIP_UNFRIENDLY, SocialController::RELATIONSHIP_NEUTRAL);
    BOOST_CHECK_LT(SocialController::RELATIONSHIP_NEUTRAL, SocialController::RELATIONSHIP_FRIENDLY);
    BOOST_CHECK_LT(SocialController::RELATIONSHIP_FRIENDLY, SocialController::RELATIONSHIP_TRUSTED);

    // Verify guard alert range is positive
    BOOST_CHECK_GT(SocialController::GUARD_ALERT_RANGE, 0.0f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Trading Tests (No Player)
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SocialControllerTradingTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestTryBuyWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;
    VoidLight::ResourceHandle itemHandle(1, 1);

    TradeResult result = controller.tryBuy(invalidHandle, itemHandle, 1);
    BOOST_CHECK(result == TradeResult::InvalidNPC);
}

BOOST_AUTO_TEST_CASE(TestTrySellWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;
    VoidLight::ResourceHandle itemHandle(1, 1);

    TradeResult result = controller.trySell(invalidHandle, itemHandle, 1);
    BOOST_CHECK(result == TradeResult::InvalidNPC);
}

BOOST_AUTO_TEST_CASE(TestTryGiftWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;
    VoidLight::ResourceHandle itemHandle(1, 1);

    bool result = controller.tryGift(invalidHandle, itemHandle, 1);
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(TestCalculateBuyPriceWithInvalidItem) {
    SocialController controller(nullptr);

    EntityHandle npcHandle;
    VoidLight::ResourceHandle invalidItem;

    float price = controller.calculateBuyPrice(npcHandle, invalidItem, 1);
    BOOST_CHECK_EQUAL(price, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestCalculateSellPriceWithInvalidItem) {
    SocialController controller(nullptr);

    EntityHandle npcHandle;
    VoidLight::ResourceHandle invalidItem;

    float price = controller.calculateSellPrice(npcHandle, invalidItem, 1);
    BOOST_CHECK_EQUAL(price, 0.0f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Relationship Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SocialControllerRelationshipTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestGetRelationshipLevelWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;

    float level = controller.getRelationshipLevel(invalidHandle);
    BOOST_CHECK_EQUAL(level, SocialController::RELATIONSHIP_NEUTRAL);
}

BOOST_AUTO_TEST_CASE(TestGetPriceModifierWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;

    // Neutral relationship should give 1.0 price modifier
    float modifier = controller.getPriceModifier(invalidHandle);
    BOOST_CHECK_CLOSE(modifier, 1.0f, 0.01f);
}

BOOST_AUTO_TEST_CASE(TestWillRefuseTradeWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;

    // Invalid NPC with neutral relationship should not refuse
    bool willRefuse = controller.willRefuseTrade(invalidHandle);
    BOOST_CHECK(!willRefuse);
}

BOOST_AUTO_TEST_CASE(TestGetRelationshipDescriptionNeutral) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;

    std::string desc = controller.getRelationshipDescription(invalidHandle);
    BOOST_CHECK_EQUAL(desc, "Neutral");
}

BOOST_AUTO_TEST_CASE(TestIsMerchantWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;

    bool isMerchant = controller.isMerchant(invalidHandle);
    BOOST_CHECK(!isMerchant);
}

BOOST_AUTO_TEST_CASE(TestGetNPCInventoryIndexWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;

    uint32_t invIdx = controller.getNPCInventoryIndex(invalidHandle);
    BOOST_CHECK_EQUAL(invIdx, INVALID_INVENTORY_INDEX);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Interaction Recording Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(SocialControllerInteractionTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestRecordInteractionWithInvalidNPC) {
    SocialController controller(nullptr);

    EntityHandle invalidHandle;
    const float initialRelationship = controller.getRelationshipLevel(invalidHandle);

    controller.recordInteraction(invalidHandle, InteractionType::Trade, 1.0f);
    controller.recordInteraction(invalidHandle, InteractionType::Gift, 1.0f);
    controller.recordInteraction(invalidHandle, InteractionType::Greeting, 1.0f);
    controller.recordInteraction(invalidHandle, InteractionType::Help, 1.0f);
    controller.recordInteraction(invalidHandle, InteractionType::Theft, -1.0f);
    controller.recordInteraction(invalidHandle, InteractionType::Insult, -1.0f);

    BOOST_CHECK_EQUAL(controller.getRelationshipLevel(invalidHandle), initialRelationship);
    BOOST_CHECK_EQUAL(controller.getRelationshipDescription(invalidHandle), "Neutral");
}

BOOST_AUTO_TEST_CASE(TestRecordInteractionAppliesMemoryAndEmotionState) {
    auto& edm = EntityDataManager::Instance();
    EntityHandle npcHandle = edm.createNPCWithRaceClass(Vector2D(100.0f, 100.0f), "Human", "Guard");
    BOOST_REQUIRE(npcHandle.isValid());

    const size_t idx = edm.getIndex(npcHandle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    SocialController controller(nullptr);
    const auto initialMemory = edm.getMemoryData(idx).memoryCount;
    const auto initialEmotions = edm.getMemoryData(idx).emotions;

    controller.recordInteraction(npcHandle, InteractionType::Theft,
                                 SocialController::THEFT_RELATIONSHIP_LOSS);

    const auto& memoryData = edm.getMemoryData(idx);
    BOOST_CHECK_EQUAL(memoryData.memoryCount, initialMemory + 1);
    BOOST_CHECK(memoryData.memories[0].isValid());
    BOOST_CHECK(memoryData.memories[0].type == MemoryType::Interaction);
    BOOST_CHECK_GT(memoryData.emotions.suspicion, initialEmotions.suspicion);
    BOOST_CHECK_GT(memoryData.emotions.aggression, initialEmotions.aggression);
}

BOOST_AUTO_TEST_CASE(TestReportTheftWithInvalidVictim) {
    SocialController controller(nullptr);

    EntityHandle thief;
    EntityHandle victim;  // Invalid
    VoidLight::ResourceHandle stolenItem(1, 1);
    std::atomic<int> theftEvents{0};

    EventManager::Instance().registerHandler(EventTypeId::Entity,
        [&theftEvents](const EventData& data) {
            auto event = std::dynamic_pointer_cast<TheftEvent>(data.event);
            if (event) {
                theftEvents.fetch_add(1, std::memory_order_relaxed);
            }
        });

    controller.reportTheft(thief, victim, stolenItem, 1);

    BOOST_CHECK_EQUAL(theftEvents.load(std::memory_order_relaxed), 0);
    BOOST_CHECK_EQUAL(controller.getRelationshipLevel(victim), SocialController::RELATIONSHIP_NEUTRAL);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TradeItemInfo Struct Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(TradeItemInfoTests)

BOOST_AUTO_TEST_CASE(TestDefaultValues) {
    TradeItemInfo info;

    BOOST_CHECK(!info.handle.isValid());
    BOOST_CHECK(info.name.empty());
    BOOST_CHECK_EQUAL(info.quantity, 0);
    BOOST_CHECK_EQUAL(info.unitPrice, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestSetValues) {
    TradeItemInfo info;
    info.handle = VoidLight::ResourceHandle(1, 1);
    info.name = "Test Item";
    info.quantity = 10;
    info.unitPrice = 5.0f;

    BOOST_CHECK(info.handle.isValid());
    BOOST_CHECK_EQUAL(info.name, "Test Item");
    BOOST_CHECK_EQUAL(info.quantity, 10);
    BOOST_CHECK_EQUAL(info.unitPrice, 5.0f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Trade Session State Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TradeSessionStateTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestInitialTradeState) {
    SocialController controller(nullptr);

    BOOST_CHECK(!controller.isTrading());
    BOOST_CHECK(!controller.getMerchantHandle().isValid());
    BOOST_CHECK(controller.getMerchantItems().empty());
    BOOST_CHECK(controller.getPlayerItems().empty());
    BOOST_CHECK_EQUAL(controller.getQuantity(), 1);
    BOOST_CHECK_EQUAL(controller.getSelectedMerchantIndex(), -1);
    BOOST_CHECK_EQUAL(controller.getSelectedPlayerIndex(), -1);
}

BOOST_AUTO_TEST_CASE(TestOpenTradeWithInvalidNPC) {
    SocialController controller(player);

    EntityHandle invalidHandle;

    bool result = controller.openTrade(invalidHandle);
    BOOST_CHECK(!result);
    BOOST_CHECK(!controller.isTrading());
}

BOOST_AUTO_TEST_CASE(TestCloseTradeWhenNotTrading) {
    SocialController controller(nullptr);

    // Should not crash when closing without active trade
    controller.closeTrade();
    BOOST_CHECK(!controller.isTrading());
}

BOOST_AUTO_TEST_CASE(TestUpdateWhenNotTrading) {
    SocialController controller(nullptr);

    // Should not crash
    controller.update(0.016f);
    BOOST_CHECK(!controller.isTrading());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Trade Selection Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TradeSelectionTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestSelectMerchantItemOutOfBounds) {
    SocialController controller(nullptr);

    // No items in list, should not crash
    controller.selectMerchantItem(0);
    controller.selectMerchantItem(100);

    BOOST_CHECK_EQUAL(controller.getSelectedMerchantIndex(), -1);
}

BOOST_AUTO_TEST_CASE(TestSelectPlayerItemOutOfBounds) {
    SocialController controller(nullptr);

    // No items in list, should not crash
    controller.selectPlayerItem(0);
    controller.selectPlayerItem(100);

    BOOST_CHECK_EQUAL(controller.getSelectedPlayerIndex(), -1);
}

BOOST_AUTO_TEST_CASE(TestSetQuantityZero) {
    SocialController controller(nullptr);

    controller.setQuantity(0);
    BOOST_CHECK_EQUAL(controller.getQuantity(), 1);
}

BOOST_AUTO_TEST_CASE(TestSetQuantityNegative) {
    SocialController controller(nullptr);

    controller.setQuantity(-5);
    BOOST_CHECK_EQUAL(controller.getQuantity(), 1);
}

BOOST_AUTO_TEST_CASE(TestSetQuantityPositive) {
    SocialController controller(nullptr);

    controller.setQuantity(10);
    BOOST_CHECK_EQUAL(controller.getQuantity(), 10);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Trade Transaction Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TradeTransactionTests, SocialControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestOpenTradeWithMerchantPopulatesInventoryLists) {
    SocialController controller(player);
    EntityHandle merchant = spawnMerchant();

    BOOST_REQUIRE(controller.openTrade(merchant));
    BOOST_CHECK(controller.isTrading());
    BOOST_CHECK_EQUAL(controller.getMerchantHandle(), merchant);
    BOOST_CHECK(!controller.getMerchantItems().empty());
    BOOST_CHECK_EQUAL(controller.getSelectedMerchantIndex(), 0);
    BOOST_CHECK_EQUAL(controller.getSelectedPlayerIndex(), -1);
}

BOOST_AUTO_TEST_CASE(TestInventoryBearingNPCCanBePromotedToMerchant) {
    SocialController controller(player);
    auto& edm = EntityDataManager::Instance();
    EntityHandle villager = spawnNPC("Villager");

    BOOST_CHECK(!edm.isNPCMerchant(villager));
    BOOST_REQUIRE(edm.initNPCAsMerchant(villager));
    BOOST_CHECK(edm.isNPCMerchant(villager));

    BOOST_REQUIRE(controller.openTrade(villager));
    BOOST_CHECK(controller.isTrading());
    BOOST_CHECK(!controller.getMerchantItems().empty());
}

BOOST_AUTO_TEST_CASE(TestExecuteBuyUsesGoldCoinsCurrency) {
    SocialController controller(player);
    EntityHandle merchant = spawnMerchant();
    auto& edm = EntityDataManager::Instance();

    BOOST_REQUIRE(player->addGold(250));
    const int initialPlayerGold = player->getGold();
    const uint32_t merchantInvIdx = edm.getNPCInventoryIndex(merchant);
    BOOST_REQUIRE(merchantInvIdx != INVALID_INVENTORY_INDEX);
    const int initialMerchantGold = edm.getInventoryQuantity(merchantInvIdx, goldHandle);
    const int initialMerchantBread = edm.getInventoryQuantity(merchantInvIdx, breadHandle);
    const int initialPlayerBread = player->getInventoryQuantity(breadHandle);
    EventManager::Instance().drainAllDeferredEvents();
    std::vector<ResourceEventRecord> resourceEvents;

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&resourceEvents](const EventData& data) {
            const auto* event = dynamic_cast<const ResourceChangeEvent*>(data.event.get());
            if (!event) {
                return;
            }
            resourceEvents.push_back({event->getOwnerHandle(),
                                      event->getResourceHandle(),
                                      event->getQuantityChange()});
        });

    BOOST_REQUIRE(controller.openTrade(merchant));

    int breadIndex = -1;
    const auto& merchantItems = controller.getMerchantItems();
    for (size_t i = 0; i < merchantItems.size(); ++i) {
        if (merchantItems[i].handle == breadHandle) {
            breadIndex = static_cast<int>(i);
            break;
        }
    }

    BOOST_REQUIRE(breadIndex >= 0);
    controller.selectMerchantItem(static_cast<size_t>(breadIndex));
    controller.setQuantity(2);

    const int expectedCost = static_cast<int>(std::ceil(controller.getCurrentBuyPrice()));
    BOOST_REQUIRE_GT(expectedCost, 0);
    BOOST_CHECK(controller.executeBuy() == TradeResult::Success);
    EventManager::Instance().drainAllDeferredEvents();

    BOOST_CHECK_EQUAL(player->getGold(), initialPlayerGold - expectedCost);
    BOOST_CHECK_EQUAL(player->getInventoryQuantity(breadHandle), initialPlayerBread + 2);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(merchantInvIdx, breadHandle),
                      initialMerchantBread - 2);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(merchantInvIdx, goldHandle),
                      initialMerchantGold + expectedCost);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, player->getHandle(), breadHandle), 2);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, player->getHandle(), goldHandle), -expectedCost);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, merchant, breadHandle), -2);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, merchant, goldHandle), expectedCost);
}

BOOST_AUTO_TEST_CASE(TestExecuteSellUsesGoldCoinsCurrency) {
    SocialController controller(player);
    EntityHandle merchant = spawnMerchant();
    auto& edm = EntityDataManager::Instance();

    BOOST_REQUIRE(player->addGold(50));
    BOOST_REQUIRE(player->addToInventory(breadHandle, 3));
    const int initialPlayerGold = player->getGold();
    const int initialPlayerBread = player->getInventoryQuantity(breadHandle);
    const uint32_t merchantInvIdx = edm.getNPCInventoryIndex(merchant);
    const int initialMerchantGold = edm.getInventoryQuantity(merchantInvIdx, goldHandle);
    const int initialMerchantBread = edm.getInventoryQuantity(merchantInvIdx, breadHandle);
    EventManager::Instance().drainAllDeferredEvents();
    std::vector<ResourceEventRecord> resourceEvents;

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&resourceEvents](const EventData& data) {
            const auto* event = dynamic_cast<const ResourceChangeEvent*>(data.event.get());
            if (!event) {
                return;
            }
            resourceEvents.push_back({event->getOwnerHandle(),
                                      event->getResourceHandle(),
                                      event->getQuantityChange()});
        });

    BOOST_REQUIRE(controller.openTrade(merchant));

    int breadIndex = -1;
    const auto& playerItems = controller.getPlayerItems();
    for (size_t i = 0; i < playerItems.size(); ++i) {
        if (playerItems[i].handle == breadHandle) {
            breadIndex = static_cast<int>(i);
            break;
        }
    }

    BOOST_REQUIRE(breadIndex >= 0);
    controller.selectPlayerItem(static_cast<size_t>(breadIndex));
    controller.setQuantity(2);

    const int expectedPayout = static_cast<int>(std::floor(controller.getCurrentSellPrice()));
    BOOST_REQUIRE_GT(expectedPayout, 0);
    BOOST_CHECK(controller.executeSell() == TradeResult::Success);
    EventManager::Instance().drainAllDeferredEvents();

    BOOST_CHECK_EQUAL(player->getGold(), initialPlayerGold + expectedPayout);
    BOOST_CHECK_EQUAL(player->getInventoryQuantity(breadHandle), initialPlayerBread - 2);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(merchantInvIdx, breadHandle),
                      initialMerchantBread + 2);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(merchantInvIdx, goldHandle),
                      initialMerchantGold - expectedPayout);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, player->getHandle(), breadHandle), -2);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, player->getHandle(), goldHandle), expectedPayout);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, merchant, breadHandle), 2);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, merchant, goldHandle), -expectedPayout);
}

BOOST_AUTO_TEST_CASE(TestGiftTransfersItemToNPCInventoryAndDispatchesResourceChangeEvent) {
    SocialController controller(player);
    auto& edm = EntityDataManager::Instance();
    EntityHandle villager = spawnNPC("Villager");
    const uint32_t villagerInvIdx = edm.getNPCInventoryIndex(villager);
    BOOST_REQUIRE(villagerInvIdx != INVALID_INVENTORY_INDEX);

    BOOST_REQUIRE(player->addToInventory(breadHandle, 3));
    const int initialPlayerBread = player->getInventoryQuantity(breadHandle);
    const int initialVillagerBread = edm.getInventoryQuantity(villagerInvIdx, breadHandle);
    EventManager::Instance().drainAllDeferredEvents();
    std::vector<ResourceEventRecord> resourceEvents;

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&resourceEvents](const EventData& data) {
            const auto* event = dynamic_cast<const ResourceChangeEvent*>(data.event.get());
            if (!event) {
                return;
            }
            resourceEvents.push_back({event->getOwnerHandle(),
                                      event->getResourceHandle(),
                                      event->getQuantityChange()});
        });

    BOOST_CHECK(!edm.isNPCMerchant(villager));
    BOOST_REQUIRE(controller.tryGift(villager, breadHandle, 2));
    EventManager::Instance().drainAllDeferredEvents();

    BOOST_CHECK_EQUAL(player->getInventoryQuantity(breadHandle), initialPlayerBread - 2);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(villagerInvIdx, breadHandle),
                      initialVillagerBread + 2);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, player->getHandle(), breadHandle), -2);
    BOOST_CHECK_EQUAL(resourceDeltaFor(resourceEvents, villager, breadHandle), 2);
}

BOOST_AUTO_TEST_CASE(TestExecuteBuyWhenNotTrading) {
    SocialController controller(nullptr);

    TradeResult result = controller.executeBuy();
    BOOST_CHECK(result == TradeResult::InvalidItem);
}

BOOST_AUTO_TEST_CASE(TestExecuteSellWhenNotTrading) {
    SocialController controller(nullptr);

    TradeResult result = controller.executeSell();
    BOOST_CHECK(result == TradeResult::InvalidItem);
}

BOOST_AUTO_TEST_CASE(TestGetCurrentBuyPriceNoSelection) {
    SocialController controller(nullptr);

    float price = controller.getCurrentBuyPrice();
    BOOST_CHECK_EQUAL(price, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestGetCurrentSellPriceNoSelection) {
    SocialController controller(nullptr);

    float price = controller.getCurrentSellPrice();
    BOOST_CHECK_EQUAL(price, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestGetRelationshipDescriptionWhenNotTrading) {
    SocialController controller(nullptr);

    std::string desc = controller.getCurrentTradeRelationshipDescription();
    BOOST_CHECK_EQUAL(desc, "N/A");
}

BOOST_AUTO_TEST_CASE(TestGetPriceModifierWhenNotTrading) {
    SocialController controller(nullptr);

    float modifier = controller.getCurrentTradePriceModifier();
    BOOST_CHECK_EQUAL(modifier, 1.0f);
}

BOOST_AUTO_TEST_SUITE_END()
