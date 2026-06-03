/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/social/SocialController.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "core/Logger.hpp"
#include "entities/Player.hpp"

#include "events/EntityEvents.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"

#include "managers/InputManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/UIManager.hpp"
#include <algorithm>
#include <cmath>
#include <format>

namespace {
    constexpr const char* EVENT_LOG = "event_log";

    AIManager::SocialInteractionType toAISocialInteractionType(InteractionType type) {
        switch (type) {
            case InteractionType::Trade:
                return AIManager::SocialInteractionType::Trade;
            case InteractionType::Gift:
                return AIManager::SocialInteractionType::Gift;
            case InteractionType::Greeting:
                return AIManager::SocialInteractionType::Greeting;
            case InteractionType::Help:
                return AIManager::SocialInteractionType::Help;
            case InteractionType::Theft:
                return AIManager::SocialInteractionType::Theft;
            case InteractionType::Insult:
                return AIManager::SocialInteractionType::Insult;
        }

        return AIManager::SocialInteractionType::Greeting;
    }
}

void SocialController::subscribe() {
    if (checkAlreadySubscribed()) {
        return;
    }

    // Driven by player actions (tryBuy, trySell, tryGift, recordInteraction)
    // Future: subscribe to proximity/conversation events for social interactions

    setSubscribed(true);
    SOCIAL_INFO("SocialController subscribed");
}

void SocialController::update(float) {
    if (!m_isTrading) {
        return;
    }

    if (!m_merchantHandle.isValid() ||
        EntityDataManager::Instance().getIndex(m_merchantHandle) == SIZE_MAX) {
        closeTrade();
        return;
    }

    updatePriceDisplay();
}

// ============================================================================
// TRADING — Session Management
// ============================================================================

bool SocialController::openTrade(EntityHandle npcHandle) {
    if (m_isTrading) {
        SOCIAL_DEBUG("Already in trade session");
        return false;
    }

    auto player = mp_player.lock();
    if (!player) {
        SOCIAL_ERROR("Player reference lost");
        return false;
    }

    if (!isMerchant(npcHandle)) {
        SOCIAL_DEBUG("NPC is not a merchant");
        return false;
    }

    if (willRefuseTrade(npcHandle)) {
        SOCIAL_INFO("Merchant refused to trade (relationship too low)");
        return false;
    }

    m_merchantHandle = npcHandle;
    m_isTrading = true;
    m_selectedMerchantIndex = -1;
    m_selectedPlayerIndex = -1;
    m_quantity = 1;
    m_merchantPaneActive = true;
    m_priceDisplayDirty = true;

    refreshMerchantItems();
    refreshPlayerItems();
    createTradeUI();
    normalizeTradeSelections();
    ensurePaneSelection();
    updateSelectionHighlight();
    updatePriceDisplay();

    SOCIAL_INFO("Trade session opened");
    return true;
}

void SocialController::closeTrade() {
    if (!m_isTrading) {
        SOCIAL_DEBUG("closeTrade called but m_isTrading is false");
        return;
    }

    SOCIAL_INFO("closeTrade executing - destroying UI");
    destroyTradeUI();

    m_isTrading = false;
    m_merchantHandle = EntityHandle{};
    m_merchantItems.clear();
    m_playerItems.clear();
    m_selectedMerchantIndex = -1;
    m_selectedPlayerIndex = -1;
    m_quantity = 1;
    m_merchantPaneActive = true;
    m_priceDisplayDirty = true;

    SOCIAL_INFO("Trade session closed");
}

void SocialController::handleTradeInput(const InputManager& inputMgr) {
    if (!m_isTrading) {
        return;
    }

    if (inputMgr.isCommandPressed(InputManager::Command::MenuCancel) ||
        inputMgr.isCommandPressed(InputManager::Command::Interact)) {
        closeTrade();
        return;
    }

    if (inputMgr.isCommandPressed(InputManager::Command::MenuLeft)) {
        setActivePaneToMerchant();
    }

    if (inputMgr.isCommandPressed(InputManager::Command::MenuRight)) {
        setActivePaneToPlayer();
    }

    if (inputMgr.isCommandPressed(InputManager::Command::MenuUp)) {
        moveActiveSelection(-1);
    }

    if (inputMgr.isCommandPressed(InputManager::Command::MenuDown)) {
        moveActiveSelection(1);
    }

    if (inputMgr.wasKeyPressed(SDL_SCANCODE_MINUS) ||
        inputMgr.wasKeyPressed(SDL_SCANCODE_KP_MINUS)) {
        adjustQuantityBy(-1);
    }

    if (inputMgr.wasKeyPressed(SDL_SCANCODE_EQUALS) ||
        inputMgr.wasKeyPressed(SDL_SCANCODE_KP_PLUS)) {
        adjustQuantityBy(1);
    }

    if (inputMgr.isCommandPressed(InputManager::Command::MenuConfirm)) {
        executeActiveTrade();
    }
}

// ============================================================================
// TRADING — Item Selection & Transactions
// ============================================================================

void SocialController::selectMerchantItem(size_t index) {
    if (index < m_merchantItems.size()) {
        m_selectedMerchantIndex = static_cast<int>(index);
        m_selectedPlayerIndex = -1;
        m_merchantPaneActive = true;
        m_quantity = 1;
        m_priceDisplayDirty = true;
        updateSelectionHighlight();
        SOCIAL_DEBUG(std::format("Selected merchant item: {}", m_merchantItems[index].name));
    }
}

void SocialController::selectPlayerItem(size_t index) {
    if (index < m_playerItems.size()) {
        m_selectedPlayerIndex = static_cast<int>(index);
        m_selectedMerchantIndex = -1;
        m_merchantPaneActive = false;
        m_quantity = 1;
        m_priceDisplayDirty = true;
        updateSelectionHighlight();
        SOCIAL_DEBUG(std::format("Selected player item: {}", m_playerItems[index].name));
    }
}

void SocialController::setQuantity(int qty) {
    m_quantity = std::max(1, qty);

    if (m_selectedMerchantIndex >= 0 &&
        static_cast<size_t>(m_selectedMerchantIndex) < m_merchantItems.size()) {
        m_quantity = std::min(m_quantity, m_merchantItems[m_selectedMerchantIndex].quantity);
    } else if (m_selectedPlayerIndex >= 0 &&
               static_cast<size_t>(m_selectedPlayerIndex) < m_playerItems.size()) {
        m_quantity = std::min(m_quantity, m_playerItems[m_selectedPlayerIndex].quantity);
    }

    m_priceDisplayDirty = true;
    updatePriceDisplay();
}

TradeResult SocialController::executeBuy() {
    if (!m_isTrading || m_selectedMerchantIndex < 0) {
        return TradeResult::InvalidItem;
    }

    if (static_cast<size_t>(m_selectedMerchantIndex) >= m_merchantItems.size()) {
        return TradeResult::InvalidItem;
    }

    const auto& item = m_merchantItems[m_selectedMerchantIndex];
    const auto itemHandle = item.handle;
    const std::string itemName = item.name;
    TradeResult result = tryBuy(m_merchantHandle, itemHandle, m_quantity);

    if (result == TradeResult::Success) {
        float price = calculateBuyPrice(m_merchantHandle, itemHandle, m_quantity);
        int savedQty = m_quantity;

        refreshMerchantItems();
        refreshPlayerItems();
        normalizeTradeSelections();
        ensurePaneSelection();
        m_quantity = 1;
        m_priceDisplayDirty = true;
        rebuildTradeListsUI();
        updateSelectionHighlight();
        SOCIAL_INFO(std::format("Bought {} x{}", itemName, savedQty));

        UIManager::Instance().addEventLogEntry(
            EVENT_LOG,
            std::format("Bought {} x{} for {:.0f} gold", itemName, savedQty, price));
    }

    return result;
}

TradeResult SocialController::executeSell() {
    if (!m_isTrading || m_selectedPlayerIndex < 0) {
        return TradeResult::InvalidItem;
    }

    if (static_cast<size_t>(m_selectedPlayerIndex) >= m_playerItems.size()) {
        return TradeResult::InvalidItem;
    }

    const auto& item = m_playerItems[m_selectedPlayerIndex];
    const auto itemHandle = item.handle;
    const std::string itemName = item.name;
    TradeResult result = trySell(m_merchantHandle, itemHandle, m_quantity);

    if (result == TradeResult::Success) {
        float price = calculateSellPrice(m_merchantHandle, itemHandle, m_quantity);
        int savedQty = m_quantity;

        refreshMerchantItems();
        refreshPlayerItems();
        normalizeTradeSelections();
        ensurePaneSelection();
        m_quantity = 1;
        m_priceDisplayDirty = true;
        rebuildTradeListsUI();
        updateSelectionHighlight();
        SOCIAL_INFO(std::format("Sold {} x{}", itemName, savedQty));

        UIManager::Instance().addEventLogEntry(
            EVENT_LOG,
            std::format("Sold {} x{} for {:.0f} gold", itemName, savedQty, price));
    }

    return result;
}

// ============================================================================
// TRADING — Accessors (current session)
// ============================================================================

float SocialController::getCurrentBuyPrice() const {
    if (m_selectedMerchantIndex < 0 ||
        static_cast<size_t>(m_selectedMerchantIndex) >= m_merchantItems.size()) {
        return 0.0f;
    }

    const auto& item = m_merchantItems[m_selectedMerchantIndex];
    return calculateBuyPrice(m_merchantHandle, item.handle, m_quantity);
}

float SocialController::getCurrentSellPrice() const {
    if (m_selectedPlayerIndex < 0 ||
        static_cast<size_t>(m_selectedPlayerIndex) >= m_playerItems.size()) {
        return 0.0f;
    }

    const auto& item = m_playerItems[m_selectedPlayerIndex];
    return calculateSellPrice(m_merchantHandle, item.handle, m_quantity);
}

std::string SocialController::getCurrentTradeRelationshipDescription() const {
    if (!m_isTrading) {
        return "N/A";
    }
    return getRelationshipDescription(m_merchantHandle);
}

float SocialController::getCurrentTradePriceModifier() const {
    if (!m_isTrading) {
        return 1.0f;
    }
    return getPriceModifier(m_merchantHandle);
}

// ============================================================================
// TRADING — Backend (buy/sell/price calculation)
// ============================================================================

TradeResult SocialController::tryBuy(EntityHandle npcHandle,
                                     VoidLight::ResourceHandle itemHandle,
                                     int quantity) {
    auto player = mp_player.lock();
    if (!player) {
        return TradeResult::InvalidNPC;
    }

    if (!npcHandle.isValid()) {
        SOCIAL_DEBUG("tryBuy: Invalid NPC handle");
        return TradeResult::InvalidNPC;
    }

    auto& edm = EntityDataManager::Instance();
    size_t npcIdx = edm.getIndex(npcHandle);
    if (npcIdx == SIZE_MAX) {
        SOCIAL_DEBUG("tryBuy: NPC not found in EDM");
        return TradeResult::InvalidNPC;
    }

    if (!isMerchant(npcHandle)) {
        SOCIAL_DEBUG("tryBuy: NPC is not a merchant");
        return TradeResult::InvalidNPC;
    }

    if (willRefuseTrade(npcHandle)) {
        SOCIAL_INFO("tryBuy: NPC refused trade due to poor relationship");
        return TradeResult::NPCRefused;
    }

    if (!itemHandle.isValid()) {
        return TradeResult::InvalidItem;
    }

    uint32_t npcInvIdx = getNPCInventoryIndex(npcHandle);
    if (!edm.hasInInventory(npcInvIdx, itemHandle, quantity)) {
        SOCIAL_DEBUG(std::format("tryBuy: NPC doesn't have {} of item", quantity));
        return TradeResult::InsufficientStock;
    }

    float totalPrice = calculateBuyPrice(npcHandle, itemHandle, quantity);
    int priceInt = static_cast<int>(std::ceil(totalPrice));

    if (!player->hasGold(priceInt)) {
        SOCIAL_DEBUG(std::format("tryBuy: Player doesn't have {} gold", priceInt));
        return TradeResult::InsufficientFunds;
    }

    const auto goldHandle = getGoldHandle();
    const int npcOldGoldQuantity = edm.getInventoryQuantity(npcInvIdx, goldHandle);
    if (!edm.addToInventory(npcInvIdx, goldHandle, priceInt)) {
        SOCIAL_DEBUG("tryBuy: NPC inventory cannot accept payment");
        return TradeResult::InventoryFull;
    }

    const int npcOldItemQuantity = edm.getInventoryQuantity(npcInvIdx, itemHandle);
    if (!edm.removeFromInventory(npcInvIdx, itemHandle, quantity)) {
        edm.removeFromInventory(npcInvIdx, goldHandle, priceInt);
        SOCIAL_ERROR("tryBuy: Failed to remove item from NPC inventory");
        return TradeResult::InsufficientStock;
    }

    if (!player->addToInventory(itemHandle, quantity)) {
        edm.addToInventory(npcInvIdx, itemHandle, quantity);
        edm.removeFromInventory(npcInvIdx, goldHandle, priceInt);
        SOCIAL_DEBUG("tryBuy: Player inventory full");
        return TradeResult::InventoryFull;
    }

    if (!player->removeGold(priceInt)) {
        edm.addToInventory(npcInvIdx, itemHandle, quantity);
        player->removeFromInventory(itemHandle, quantity);
        edm.removeFromInventory(npcInvIdx, goldHandle, priceInt);
        SOCIAL_ERROR("tryBuy: Failed to remove payment from player");
        return TradeResult::InsufficientFunds;
    }

    dispatchResourceChange(npcHandle, itemHandle, npcOldItemQuantity,
                           edm.getInventoryQuantity(npcInvIdx, itemHandle),
                           "traded");
    dispatchResourceChange(npcHandle, goldHandle, npcOldGoldQuantity,
                           edm.getInventoryQuantity(npcInvIdx, goldHandle),
                           "traded");

    recordTrade(npcHandle, totalPrice, true);

    SOCIAL_INFO(std::format("Trade complete: Player bought {} items (value: {:.1f})",
                            quantity, totalPrice));

    return TradeResult::Success;
}

TradeResult SocialController::trySell(EntityHandle npcHandle,
                                      VoidLight::ResourceHandle itemHandle,
                                      int quantity) {
    auto player = mp_player.lock();
    if (!player) {
        return TradeResult::InvalidNPC;
    }

    if (!npcHandle.isValid()) {
        SOCIAL_DEBUG("trySell: Invalid NPC handle");
        return TradeResult::InvalidNPC;
    }

    auto& edm = EntityDataManager::Instance();
    size_t npcIdx = edm.getIndex(npcHandle);
    if (npcIdx == SIZE_MAX) {
        SOCIAL_DEBUG("trySell: NPC not found in EDM");
        return TradeResult::InvalidNPC;
    }

    if (!isMerchant(npcHandle)) {
        SOCIAL_DEBUG("trySell: NPC is not a merchant");
        return TradeResult::InvalidNPC;
    }

    if (willRefuseTrade(npcHandle)) {
        SOCIAL_INFO("trySell: NPC refused trade due to poor relationship");
        return TradeResult::NPCRefused;
    }

    if (!itemHandle.isValid()) {
        return TradeResult::InvalidItem;
    }

    if (!player->hasInInventory(itemHandle, quantity)) {
        SOCIAL_DEBUG(std::format("trySell: Player doesn't have {} of item", quantity));
        return TradeResult::InsufficientStock;
    }

    float totalPrice = calculateSellPrice(npcHandle, itemHandle, quantity);
    int priceInt = static_cast<int>(std::floor(totalPrice));

    uint32_t npcInvIdx = getNPCInventoryIndex(npcHandle);
    auto goldHandle = getGoldHandle();
    if (!edm.hasInInventory(npcInvIdx, goldHandle, priceInt)) {
        SOCIAL_DEBUG(std::format("trySell: NPC doesn't have {} gold to pay", priceInt));
        return TradeResult::InsufficientFunds;
    }

    const int npcOldItemQuantity = edm.getInventoryQuantity(npcInvIdx, itemHandle);
    if (!edm.addToInventory(npcInvIdx, itemHandle, quantity)) {
        SOCIAL_DEBUG("trySell: NPC inventory full");
        return TradeResult::InventoryFull;
    }

    if (!player->addGold(priceInt)) {
        edm.removeFromInventory(npcInvIdx, itemHandle, quantity);
        SOCIAL_DEBUG("trySell: Player inventory cannot accept payment");
        return TradeResult::InventoryFull;
    }

    if (!player->removeFromInventory(itemHandle, quantity)) {
        player->removeGold(priceInt);
        edm.removeFromInventory(npcInvIdx, itemHandle, quantity);
        SOCIAL_ERROR("trySell: Failed to remove item from player inventory");
        return TradeResult::InsufficientStock;
    }

    const int npcOldGoldQuantity = edm.getInventoryQuantity(npcInvIdx, goldHandle);
    if (!edm.removeFromInventory(npcInvIdx, goldHandle, priceInt)) {
        player->addToInventory(itemHandle, quantity);
        player->removeGold(priceInt);
        edm.removeFromInventory(npcInvIdx, itemHandle, quantity);
        SOCIAL_ERROR("trySell: Failed to remove payment from NPC inventory");
        return TradeResult::InsufficientFunds;
    }

    dispatchResourceChange(npcHandle, itemHandle, npcOldItemQuantity,
                           edm.getInventoryQuantity(npcInvIdx, itemHandle),
                           "traded");
    dispatchResourceChange(npcHandle, goldHandle, npcOldGoldQuantity,
                           edm.getInventoryQuantity(npcInvIdx, goldHandle),
                           "traded");

    recordTrade(npcHandle, totalPrice, true);

    SOCIAL_INFO(std::format("Trade complete: Player sold {} items (value: {:.1f})",
                            quantity, totalPrice));

    return TradeResult::Success;
}

float SocialController::calculateBuyPrice(EntityHandle npcHandle,
                                          VoidLight::ResourceHandle itemHandle,
                                          int quantity) const {
    float baseValue = getItemBaseValue(itemHandle);
    float modifier = getPriceModifier(npcHandle);

    return baseValue * modifier * BUY_PRICE_MULTIPLIER * quantity;
}

float SocialController::calculateSellPrice(EntityHandle npcHandle,
                                           VoidLight::ResourceHandle itemHandle,
                                           int quantity) const {
    float baseValue = getItemBaseValue(itemHandle);
    float modifier = getPriceModifier(npcHandle);

    // Better relationship = better sell price (inverse of buy modifier)
    float sellModifier = 2.0f - modifier;

    return baseValue * sellModifier * SELL_PRICE_MULTIPLIER * quantity;
}

// ============================================================================
// SOCIAL — Interactions & Memory
// ============================================================================

bool SocialController::tryGift(EntityHandle npcHandle,
                               VoidLight::ResourceHandle itemHandle,
                               int quantity) {
    auto player = mp_player.lock();
    if (!player) {
        return false;
    }

    if (!npcHandle.isValid()) {
        SOCIAL_DEBUG("tryGift: Invalid NPC handle");
        return false;
    }

    auto& edm = EntityDataManager::Instance();
    size_t npcIdx = edm.getIndex(npcHandle);
    if (npcIdx == SIZE_MAX) {
        SOCIAL_DEBUG("tryGift: NPC not found in EDM");
        return false;
    }

    if (!itemHandle.isValid()) {
        return false;
    }

    if (!player->hasInInventory(itemHandle, quantity)) {
        SOCIAL_DEBUG(std::format("tryGift: Player doesn't have {} of item", quantity));
        return false;
    }

    const uint32_t npcInvIdx = getNPCInventoryIndex(npcHandle);
    if (npcInvIdx == INVALID_INVENTORY_INDEX) {
        SOCIAL_DEBUG("tryGift: NPC does not have an inventory");
        return false;
    }

    const int npcOldItemQuantity = edm.getInventoryQuantity(npcInvIdx, itemHandle);
    if (!edm.addToInventory(npcInvIdx, itemHandle, quantity)) {
        SOCIAL_DEBUG("tryGift: NPC inventory full");
        return false;
    }

    if (!player->removeFromInventory(itemHandle, quantity)) {
        edm.removeFromInventory(npcInvIdx, itemHandle, quantity);
        SOCIAL_ERROR("tryGift: Failed to remove item from player inventory");
        return false;
    }

    dispatchResourceChange(npcHandle, itemHandle, npcOldItemQuantity,
                           edm.getInventoryQuantity(npcInvIdx, itemHandle),
                           "gifted");

    float giftValue = getItemBaseValue(itemHandle) * quantity;
    recordGift(npcHandle, giftValue);

    SOCIAL_INFO(std::format("Gift given: {} items worth {:.1f} gold",
                            quantity, giftValue));

    return true;
}

void SocialController::recordInteraction(EntityHandle npcHandle,
                                         InteractionType type,
                                         float value) {
    if (!npcHandle.isValid()) {
        return;
    }

    auto player = mp_player.lock();
    EntityHandle playerHandle = player ? player->getHandle() : EntityHandle{};
    AIManager::Instance().applySocialInteraction(
        npcHandle, playerHandle, toAISocialInteractionType(type), value);
}

void SocialController::reportTheft(EntityHandle thief,
                                   EntityHandle victim,
                                   VoidLight::ResourceHandle stolenItem,
                                   int quantity) {
    if (!victim.isValid()) {
        SOCIAL_DEBUG("reportTheft: Invalid victim handle");
        return;
    }

    auto& edm = EntityDataManager::Instance();
    size_t victimIdx = edm.getIndex(victim);
    if (victimIdx == SIZE_MAX) {
        SOCIAL_DEBUG("reportTheft: Victim not found in EDM");
        return;
    }

    recordInteraction(victim, InteractionType::Theft, THEFT_RELATIONSHIP_LOSS);

    Vector2D theftLocation = edm.getHotDataByIndex(victimIdx).transform.position;

    auto theftEvent = std::make_shared<TheftEvent>(
        thief, victim, stolenItem, quantity, theftLocation);
    EventManager::Instance().dispatchEvent(theftEvent, EventManager::DispatchMode::Immediate);

    const auto& rtm = ResourceTemplateManager::Instance();
    auto resTemplate = rtm.getResourceTemplate(stolenItem);
    std::string itemName = resTemplate ? resTemplate->getName() : "unknown item";

    SOCIAL_INFO(std::format("Theft reported: {} x{} stolen at ({:.0f}, {:.0f})",
                            itemName, quantity, theftLocation.getX(), theftLocation.getY()));

    alertNearbyGuards(theftLocation, thief);
}

void SocialController::alertNearbyGuards(const Vector2D& location, EntityHandle) {
    // Guards detect threats autonomously after alert
    auto& edm = EntityDataManager::Instance();

    // Scan guard index for nearby guards — O(G) not O(N)
    m_nearbyGuardBuffer.clear();
    AIManager::Instance().scanGuardsInRadius(location, GUARD_ALERT_RANGE,
                                             m_nearbyGuardBuffer, true);
    int guardsAlerted = 0;

    for (size_t idx : m_nearbyGuardBuffer) {
        Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);

        ++guardsAlerted;
        SOCIAL_DEBUG(std::format("Guard at ({:.0f}, {:.0f}) alerted to theft",
                                 edm.getHotDataByIndex(idx).transform.position.getX(),
                                 edm.getHotDataByIndex(idx).transform.position.getY()));
    }

    if (guardsAlerted > 0) {
        SOCIAL_INFO(std::format("Alerted {} guards to theft at ({:.0f}, {:.0f})",
                                guardsAlerted, location.getX(), location.getY()));

        UIManager::Instance().addEventLogEntry(
            EVENT_LOG,
            std::format("Guards alerted! {} guards responding to crime.", guardsAlerted));
    }
}

// ============================================================================
// SHARED — Relationship Queries
// ============================================================================

float SocialController::getRelationshipLevel(EntityHandle npcHandle) const {
    auto player = mp_player.lock();
    EntityHandle playerHandle = player ? player->getHandle() : EntityHandle{};
    return Behaviors::getRelationshipLevel(npcHandle, playerHandle);
}

float SocialController::getPriceModifier(EntityHandle npcHandle) const {
    float relationship = getRelationshipLevel(npcHandle);
    return 1.0f - (relationship * 0.3f);
}

bool SocialController::willRefuseTrade(EntityHandle npcHandle) const {
    float relationship = getRelationshipLevel(npcHandle);
    return relationship < RELATIONSHIP_HOSTILE;
}

std::string SocialController::getRelationshipDescription(EntityHandle npcHandle) const {
    float relationship = getRelationshipLevel(npcHandle);

    if (relationship >= RELATIONSHIP_TRUSTED) {
        return "Trusted Friend";
    } else if (relationship >= RELATIONSHIP_FRIENDLY) {
        return "Friendly";
    } else if (relationship >= RELATIONSHIP_NEUTRAL - 0.1f) {
        return "Neutral";
    } else if (relationship >= RELATIONSHIP_UNFRIENDLY) {
        return "Unfriendly";
    } else if (relationship >= RELATIONSHIP_HOSTILE) {
        return "Hostile";
    } else {
        return "Despised";
    }
}

// ============================================================================
// SHARED — NPC Inventory Helpers
// ============================================================================

bool SocialController::isMerchant(EntityHandle npcHandle) const {
    return EntityDataManager::Instance().isNPCMerchant(npcHandle);
}

uint32_t SocialController::getNPCInventoryIndex(EntityHandle npcHandle) const {
    return EntityDataManager::Instance().getNPCInventoryIndex(npcHandle);
}

// ============================================================================
// PRIVATE — Memory Recording
// ============================================================================

void SocialController::recordTrade(EntityHandle npcHandle, float tradeValue, bool wasGoodDeal) {
    float value = wasGoodDeal ? (tradeValue * 0.01f) : -(tradeValue * 0.005f);
    recordInteraction(npcHandle, InteractionType::Trade, value);
}

void SocialController::recordGift(EntityHandle npcHandle, float giftValue) {
    float value = GIFT_RELATIONSHIP_BASE + (giftValue * GIFT_VALUE_SCALE);
    recordInteraction(npcHandle, InteractionType::Gift, value);
}

void SocialController::dispatchResourceChange(EntityHandle ownerHandle,
                                              VoidLight::ResourceHandle resourceHandle,
                                              int oldQuantity,
                                              int newQuantity,
                                              const std::string& reason) const {
    if (oldQuantity == newQuantity) {
        return;
    }

    EventManager::Instance().triggerResourceChange(
        ownerHandle, resourceHandle, oldQuantity, newQuantity, reason,
        EventManager::DispatchMode::Deferred);
}

float SocialController::getItemBaseValue(VoidLight::ResourceHandle itemHandle) const {
    if (!itemHandle.isValid()) {
        return 0.0f;
    }

    return ResourceTemplateManager::Instance().getValue(itemHandle);
}

// ============================================================================
// PRIVATE — Trade UI Management
// ============================================================================

void SocialController::createTradeUI() {
    auto& ui = UIManager::Instance();

    constexpr int panelW = 600;
    constexpr int panelH = 450;
    constexpr int halfW = panelW / 2;
    constexpr int halfH = panelH / 2;
    constexpr int lowerInfoRowY = 332;
    constexpr int quantityButtonRowY = 376;
    constexpr int actionButtonRowY = 408;

    auto disableAutoSizing = [&ui](const std::string& id) {
        ui.enableAutoSizing(id, false);
    };

    ui.createModal(UI_PANEL, UIRect{0, 0, panelW, panelH}, "",
                   UIConstants::BASELINE_WIDTH,
                   UIConstants::BASELINE_HEIGHT);
    ui.setComponentPositioning(UI_PANEL, {UIPositionMode::CENTERED_BOTH, 0, 0, panelW, panelH});

    ui.createTitle(UI_TITLE, UIRect{0, 0, 560, 30}, "Trading", UI_PANEL);
    disableAutoSizing(UI_TITLE);
    ui.setComponentPositioning(UI_TITLE, {UIPositionMode::CENTERED_BOTH, 0, 10 + 15 - halfH, 560, 30});

    std::string relStr = std::format("Relationship: {}  (Price: {:.0f}%)",
                                     getCurrentTradeRelationshipDescription(),
                                     getCurrentTradePriceModifier() * 100.0f);
    ui.createLabel(UI_RELATIONSHIP, UIRect{0, 0, 560, 20}, relStr, UI_PANEL);
    disableAutoSizing(UI_RELATIONSHIP);
    ui.setComponentPositioning(UI_RELATIONSHIP, {UIPositionMode::CENTERED_BOTH, 0, 45 + 10 - halfH, 560, 20});

    ui.createLabel("trade_merchant_label", UIRect{0, 0, 270, 20}, "Merchant Inventory", UI_PANEL);
    disableAutoSizing("trade_merchant_label");
    ui.setComponentPositioning("trade_merchant_label", {UIPositionMode::CENTERED_BOTH,
        20 + 135 - halfW, 75 + 10 - halfH, 270, 20});

    ui.createList(UI_MERCHANT_LIST, UIRect{0, 0, 270, 200}, UI_PANEL);
    disableAutoSizing(UI_MERCHANT_LIST);
    ui.setComponentPositioning(UI_MERCHANT_LIST, {UIPositionMode::CENTERED_BOTH,
        20 + 135 - halfW, 100 + 100 - halfH, 270, 200});

    ui.createLabel("trade_player_label", UIRect{0, 0, 270, 20}, "Your Inventory", UI_PANEL);
    disableAutoSizing("trade_player_label");
    ui.setComponentPositioning("trade_player_label", {UIPositionMode::CENTERED_BOTH,
        310 + 135 - halfW, 75 + 10 - halfH, 270, 20});

    ui.createList(UI_PLAYER_LIST, UIRect{0, 0, 270, 200}, UI_PANEL);
    disableAutoSizing(UI_PLAYER_LIST);
    ui.setComponentPositioning(UI_PLAYER_LIST, {UIPositionMode::CENTERED_BOTH,
        310 + 135 - halfW, 100 + 100 - halfH, 270, 200});

    for (const auto& item : m_merchantItems) {
        std::string itemStr = std::format("{} x{} ({:.0f}g)", item.name, item.quantity, item.unitPrice);
        ui.addListItem(UI_MERCHANT_LIST, itemStr);
    }

    for (const auto& item : m_playerItems) {
        std::string itemStr = std::format("{} x{}", item.name, item.quantity);
        ui.addListItem(UI_PLAYER_LIST, itemStr);
    }

    ui.createLabel(UI_QUANTITY_LABEL, UIRect{0, 0, 150, 25}, "Quantity: 1", UI_PANEL);
    disableAutoSizing(UI_QUANTITY_LABEL);
    ui.setComponentPositioning(UI_QUANTITY_LABEL, {UIPositionMode::CENTERED_BOTH,
        20 + 75 - halfW, lowerInfoRowY + 12 - halfH, 150, 25});

    ui.createButton(UI_QUANTITY_DEC_BTN, UIRect{0, 0, 35, 30}, "-", UI_PANEL);
    disableAutoSizing(UI_QUANTITY_DEC_BTN);
    ui.setComponentPositioning(UI_QUANTITY_DEC_BTN, {UIPositionMode::CENTERED_BOTH,
        -205, quantityButtonRowY - halfH, 35, 30});
    ui.setOnClick(UI_QUANTITY_DEC_BTN, [this]() {
        adjustQuantityBy(-1);
    });

    ui.createButton(UI_QUANTITY_INC_BTN, UIRect{0, 0, 35, 30}, "+", UI_PANEL);
    disableAutoSizing(UI_QUANTITY_INC_BTN);
    ui.setComponentPositioning(UI_QUANTITY_INC_BTN, {UIPositionMode::CENTERED_BOTH,
        -145, quantityButtonRowY - halfH, 35, 30});
    ui.setOnClick(UI_QUANTITY_INC_BTN, [this]() {
        adjustQuantityBy(1);
    });

    ui.createLabel(UI_PRICE_LABEL, UIRect{0, 0, 200, 25}, "Select an item", UI_PANEL);
    disableAutoSizing(UI_PRICE_LABEL);
    ui.setComponentPositioning(UI_PRICE_LABEL, {UIPositionMode::CENTERED_BOTH,
        180 + 100 - halfW, lowerInfoRowY + 12 - halfH, 200, 25});

    auto player = mp_player.lock();
    int gold = player ? player->getGold() : 0;
    ui.createLabel(UI_GOLD_LABEL, UIRect{0, 0, 180, 25}, std::format("Your Gold: {}", gold), UI_PANEL);
    disableAutoSizing(UI_GOLD_LABEL);
    ui.setComponentPositioning(UI_GOLD_LABEL, {UIPositionMode::CENTERED_BOTH,
        400 + 90 - halfW, lowerInfoRowY + 12 - halfH, 180, 25});

    constexpr int btnW = 100;
    constexpr int btnH = 35;

    ui.createButtonSuccess(UI_BUY_BTN, UIRect{0, 0, btnW, btnH}, "Buy", UI_PANEL);
    disableAutoSizing(UI_BUY_BTN);
    ui.setComponentPositioning(UI_BUY_BTN, {UIPositionMode::CENTERED_BOTH,
        -175, actionButtonRowY + btnH/2 - halfH, btnW, btnH});
    ui.setOnClick(UI_BUY_BTN, [this]() {
        executeBuy();
    });

    ui.createButtonSuccess(UI_SELL_BTN, UIRect{0, 0, btnW, btnH}, "Sell", UI_PANEL);
    disableAutoSizing(UI_SELL_BTN);
    ui.setComponentPositioning(UI_SELL_BTN, {UIPositionMode::CENTERED_BOTH,
        0, actionButtonRowY + btnH/2 - halfH, btnW, btnH});
    ui.setOnClick(UI_SELL_BTN, [this]() {
        executeSell();
    });

    ui.createButtonDanger(UI_CLOSE_BTN, UIRect{0, 0, btnW, btnH}, "Close", UI_PANEL);
    disableAutoSizing(UI_CLOSE_BTN);
    ui.setComponentPositioning(UI_CLOSE_BTN, {UIPositionMode::CENTERED_BOTH,
        175, actionButtonRowY + btnH/2 - halfH, btnW, btnH});
    ui.setOnClick(UI_CLOSE_BTN, [this]() {
        closeTrade();
    });

    ui.setOnClick(UI_MERCHANT_LIST, [this]() {
        const auto& uiMgr = UIManager::Instance();
        int idx = uiMgr.getSelectedListItem(UI_MERCHANT_LIST);
        if (idx >= 0) {
            selectMerchantItem(static_cast<size_t>(idx));
        }
    });

    ui.setOnClick(UI_PLAYER_LIST, [this]() {
        const auto& uiMgr = UIManager::Instance();
        int idx = uiMgr.getSelectedListItem(UI_PLAYER_LIST);
        if (idx >= 0) {
            selectPlayerItem(static_cast<size_t>(idx));
        }
    });

    rebuildTradeListsUI();
}

void SocialController::destroyTradeUI() {
    auto& ui = UIManager::Instance();
    ui.clearKeyboardSelection();
    ui.removeComponentsWithPrefix("trade_");
    ui.removeOverlay();
}

void SocialController::rebuildTradeListsUI() {
    auto& ui = UIManager::Instance();
    if (!ui.hasComponent(UI_MERCHANT_LIST) || !ui.hasComponent(UI_PLAYER_LIST)) {
        return;
    }

    ui.clearList(UI_MERCHANT_LIST);
    for (const auto& item : m_merchantItems) {
        ui.addListItem(UI_MERCHANT_LIST,
                       std::format("{} x{} ({:.0f}g)", item.name, item.quantity, item.unitPrice));
    }

    ui.clearList(UI_PLAYER_LIST);
    for (const auto& item : m_playerItems) {
        ui.addListItem(UI_PLAYER_LIST, std::format("{} x{}", item.name, item.quantity));
    }
}

void SocialController::normalizeTradeSelections() {
    if (m_selectedMerchantIndex >= static_cast<int>(m_merchantItems.size())) {
        m_selectedMerchantIndex = m_merchantItems.empty() ? -1
                                                          : static_cast<int>(m_merchantItems.size()) - 1;
    }

    if (m_selectedPlayerIndex >= static_cast<int>(m_playerItems.size())) {
        m_selectedPlayerIndex = m_playerItems.empty() ? -1
                                                      : static_cast<int>(m_playerItems.size()) - 1;
    }

    if (m_selectedMerchantIndex < 0 && m_selectedPlayerIndex < 0) {
        m_merchantPaneActive = !m_merchantItems.empty() || m_playerItems.empty();
    }
}

void SocialController::ensurePaneSelection() {
    if (m_merchantPaneActive) {
        if (m_selectedMerchantIndex < 0 && !m_merchantItems.empty()) {
            m_selectedMerchantIndex = 0;
        }
        if (m_selectedMerchantIndex >= 0) {
            m_selectedPlayerIndex = -1;
        }
    } else {
        if (m_selectedPlayerIndex < 0 && !m_playerItems.empty()) {
            m_selectedPlayerIndex = 0;
        }
        if (m_selectedPlayerIndex >= 0) {
            m_selectedMerchantIndex = -1;
        }
    }
}

void SocialController::refreshMerchantItems() {
    m_merchantItems.clear();

    const auto& edm = EntityDataManager::Instance();
    const auto& rtm = ResourceTemplateManager::Instance();

    uint32_t invIdx = getNPCInventoryIndex(m_merchantHandle);
    if (invIdx == INVALID_INVENTORY_INDEX) {
        return;
    }

    auto resources = edm.getInventoryResources(invIdx);
    for (const auto& [handle, qty] : resources) {
        if (qty <= 0) continue;

        TradeItemInfo info;
        info.handle = handle;
        info.quantity = qty;
        auto resTemplate = rtm.getResourceTemplate(handle);
        info.name = resTemplate ? resTemplate->getName() : "Unknown";
        info.unitPrice = calculateBuyPrice(m_merchantHandle, handle, 1);

        m_merchantItems.push_back(info);
    }

    std::sort(m_merchantItems.begin(), m_merchantItems.end(),
              [](const TradeItemInfo& lhs, const TradeItemInfo& rhs) {
                  return lhs.name < rhs.name;
              });

    m_priceDisplayDirty = true;
}

void SocialController::refreshPlayerItems() {
    m_playerItems.clear();

    auto player = mp_player.lock();
    if (!player) {
        return;
    }

    const auto& edm = EntityDataManager::Instance();
    const auto& rtm = ResourceTemplateManager::Instance();

    uint32_t invIdx = player->getInventoryIndex();
    if (invIdx == INVALID_INVENTORY_INDEX) {
        return;
    }

    auto goldHandle = getGoldHandle();
    auto resources = edm.getInventoryResources(invIdx);
    for (const auto& [handle, qty] : resources) {
        if (qty <= 0) continue;
        if (handle == goldHandle) continue;

        TradeItemInfo info;
        info.handle = handle;
        info.quantity = qty;
        auto resTemplate = rtm.getResourceTemplate(handle);
        info.name = resTemplate ? resTemplate->getName() : "Unknown";
        info.unitPrice = calculateSellPrice(m_merchantHandle, handle, 1);

        m_playerItems.push_back(info);
    }

    std::sort(m_playerItems.begin(), m_playerItems.end(),
              [](const TradeItemInfo& lhs, const TradeItemInfo& rhs) {
                  return lhs.name < rhs.name;
              });

    m_priceDisplayDirty = true;
}

void SocialController::updatePriceDisplay() {
    if (!m_isTrading) {
        return;
    }

    if (!m_priceDisplayDirty) {
        return;
    }

    m_priceDisplayDirty = false;

    auto& ui = UIManager::Instance();

    ui.setText(UI_QUANTITY_LABEL, std::format("Quantity: {}", m_quantity));
    ui.setText(UI_RELATIONSHIP,
               std::format("Relationship: {}  (Price: {:.0f}%)",
                           getCurrentTradeRelationshipDescription(),
                           getCurrentTradePriceModifier() * 100.0f));

    if (m_selectedMerchantIndex >= 0) {
        float price = getCurrentBuyPrice();
        ui.setText(UI_PRICE_LABEL, std::format("Buy Price: {:.0f} gold", price));
    } else if (m_selectedPlayerIndex >= 0) {
        float price = getCurrentSellPrice();
        ui.setText(UI_PRICE_LABEL, std::format("Sell Price: {:.0f} gold", price));
    } else {
        ui.setText(UI_PRICE_LABEL, "Select an item");
    }

    auto player = mp_player.lock();
    int gold = player ? player->getGold() : 0;
    ui.setText(UI_GOLD_LABEL, std::format("Your Gold: {}", gold));
}

void SocialController::updateSelectionHighlight() {
    auto& ui = UIManager::Instance();
    if (ui.hasComponent(UI_MERCHANT_LIST)) {
        ui.clearKeyboardSelection();
        ui.setSelectedListItem(UI_MERCHANT_LIST, m_selectedMerchantIndex);
        ui.setSelectedListItem(UI_PLAYER_LIST, m_selectedPlayerIndex);

        ui.setKeyboardSelection(m_merchantPaneActive ? UI_MERCHANT_LIST : UI_PLAYER_LIST);
    }
}

void SocialController::setActivePaneToMerchant() {
    m_merchantPaneActive = true;
    ensurePaneSelection();
    updateSelectionHighlight();
    m_priceDisplayDirty = true;
}

void SocialController::setActivePaneToPlayer() {
    m_merchantPaneActive = false;
    ensurePaneSelection();
    updateSelectionHighlight();
    m_priceDisplayDirty = true;
}

void SocialController::moveActiveSelection(int delta) {
    if (delta == 0) {
        return;
    }

    if (m_merchantPaneActive) {
        if (m_merchantItems.empty()) {
            return;
        }

        int nextIndex = m_selectedMerchantIndex < 0 ? 0 : m_selectedMerchantIndex + delta;
        nextIndex = std::clamp(nextIndex, 0, static_cast<int>(m_merchantItems.size()) - 1);
        selectMerchantItem(static_cast<size_t>(nextIndex));
        return;
    }

    if (m_playerItems.empty()) {
        return;
    }

    int nextIndex = m_selectedPlayerIndex < 0 ? 0 : m_selectedPlayerIndex + delta;
    nextIndex = std::clamp(nextIndex, 0, static_cast<int>(m_playerItems.size()) - 1);
    selectPlayerItem(static_cast<size_t>(nextIndex));
}

void SocialController::adjustQuantityBy(int delta) {
    setQuantity(m_quantity + delta);
}

void SocialController::executeActiveTrade() {
    if (m_merchantPaneActive) {
        executeBuy();
        return;
    }

    executeSell();
}

VoidLight::ResourceHandle SocialController::getGoldHandle() const {
    auto player = mp_player.lock();
    if (!player) {
        return VoidLight::ResourceHandle{};
    }
    return player->getGoldHandle();
}
