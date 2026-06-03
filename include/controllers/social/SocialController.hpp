/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef SOCIAL_CONTROLLER_HPP
#define SOCIAL_CONTROLLER_HPP

/**
 * @file SocialController.hpp
 * @brief Controller for all player-NPC social systems
 *
 * Two domains:
 *
 * 1. TRADING — Merchant buy/sell with UI session management.
 *    Price modifiers, inventory transfer, trade UI lifecycle.
 *
 * 2. SOCIAL INTERACTIONS — Gifts, greetings, theft, relationship building.
 *    Detects and routes interaction outcomes through AI-owned social state
 *    application so NPC memory and emotions stay under AI ownership.
 *
 * Both domains share relationship queries over NPC memory state.
 *
 * Ownership: ControllerRegistry owns the controller instance.
 */

#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"
#include "entities/EntityHandle.hpp"
#include "managers/EntityDataManager.hpp"  // For INVALID_INVENTORY_INDEX
#include "utils/ResourceHandle.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class Player;
class InputManager;

/**
 * @brief Result of a trade operation
 */
enum class TradeResult {
    Success,              // Trade completed successfully
    InsufficientFunds,    // Buyer doesn't have enough gold/currency
    InsufficientStock,    // Seller doesn't have the item
    InvalidNPC,           // NPC handle invalid or not a merchant
    InvalidItem,          // Item handle invalid
    InventoryFull,        // Buyer's inventory is full
    NPCRefused            // NPC refused trade (relationship too low)
};

/**
 * @brief Type of social interaction for memory recording
 */
enum class InteractionType {
    Trade,        // Bought or sold items
    Gift,         // Gave item to NPC
    Greeting,     // Basic social interaction
    Help,         // Helped the NPC (quest, rescue)
    Theft,        // Stole from NPC (negative)
    Insult        // Negative social interaction
};

/**
 * @brief Item display info for trade UI lists
 */
struct TradeItemInfo {
    VoidLight::ResourceHandle handle;
    std::string name;
    int quantity{0};
    float unitPrice{0.0f};
};

class SocialController : public ControllerBase, public IUpdatable {
public:
    explicit SocialController(std::shared_ptr<Player> player)
        : mp_player(std::move(player)) {}

    ~SocialController() override = default;

    SocialController(SocialController&&) noexcept = default;
    SocialController& operator=(SocialController&&) noexcept = default;

    // --- ControllerBase interface ---

    void subscribe() override;
    [[nodiscard]] std::string_view getName() const override { return "SocialController"; }

    // --- IUpdatable interface ---

    void update(float deltaTime) override;

    // ========================================================================
    // TRADING — Session Management
    // ========================================================================

    bool openTrade(EntityHandle npcHandle);
    void closeTrade();
    void handleTradeInput(const InputManager& inputMgr);
    [[nodiscard]] bool isTrading() const { return m_isTrading; }
    [[nodiscard]] EntityHandle getMerchantHandle() const { return m_merchantHandle; }

    // ========================================================================
    // TRADING — Item Selection & Transactions
    // ========================================================================

    void selectMerchantItem(size_t index);
    void selectPlayerItem(size_t index);
    void setQuantity(int qty);
    TradeResult executeBuy();
    TradeResult executeSell();

    // ========================================================================
    // TRADING — Accessors (current session)
    // ========================================================================

    [[nodiscard]] const std::vector<TradeItemInfo>& getMerchantItems() const { return m_merchantItems; }
    [[nodiscard]] const std::vector<TradeItemInfo>& getPlayerItems() const { return m_playerItems; }
    [[nodiscard]] int getQuantity() const { return m_quantity; }
    [[nodiscard]] int getSelectedMerchantIndex() const { return m_selectedMerchantIndex; }
    [[nodiscard]] int getSelectedPlayerIndex() const { return m_selectedPlayerIndex; }
    [[nodiscard]] float getCurrentBuyPrice() const;
    [[nodiscard]] float getCurrentSellPrice() const;
    [[nodiscard]] std::string getCurrentTradeRelationshipDescription() const;
    [[nodiscard]] float getCurrentTradePriceModifier() const;

    // ========================================================================
    // TRADING — Backend (buy/sell/price calculation)
    // ========================================================================

    TradeResult tryBuy(EntityHandle npcHandle,
                       VoidLight::ResourceHandle itemHandle,
                       int quantity = 1);

    TradeResult trySell(EntityHandle npcHandle,
                        VoidLight::ResourceHandle itemHandle,
                        int quantity = 1);

    [[nodiscard]] float calculateBuyPrice(EntityHandle npcHandle,
                                          VoidLight::ResourceHandle itemHandle,
                                          int quantity = 1) const;

    [[nodiscard]] float calculateSellPrice(EntityHandle npcHandle,
                                           VoidLight::ResourceHandle itemHandle,
                                           int quantity = 1) const;

    // ========================================================================
    // SOCIAL — Interactions & Memory
    // ========================================================================

    /**
     * @brief Give an item to an NPC as a gift
     *
     * Gifts transfer the item into the NPC inventory and improve relationship
     * based on item value. NPCs remember gifts and become more friendly.
     */
    bool tryGift(EntityHandle npcHandle,
                 VoidLight::ResourceHandle itemHandle,
                 int quantity = 1);

    /**
     * @brief Record a social interaction for AI-owned NPC state application
     * @param value Interaction quality (-1.0 to +1.0)
     *
     * Routes the interaction to AI-owned logic that records
     * MemoryType::Interaction and updates NPC emotions.
     */
    void recordInteraction(EntityHandle npcHandle,
                           InteractionType type,
                           float value = 0.0f);

    /**
     * @brief Report a theft — records memory, fires event, alerts guards
     */
    void reportTheft(EntityHandle thief,
                     EntityHandle victim,
                     VoidLight::ResourceHandle stolenItem,
                     int quantity = 1);

    /**
     * @brief Alert guards within range to a crime location
     */
    void alertNearbyGuards(const Vector2D& location, EntityHandle criminal);

    // ========================================================================
    // SHARED — Relationship Queries
    // ========================================================================

    /** @brief Relationship score: -1.0 (hostile) to +1.0 (best friend), 0.0 neutral */
    [[nodiscard]] float getRelationshipLevel(EntityHandle npcHandle) const;

    /** @brief Price multiplier: 0.7 (trusted) to 1.3 (hostile) */
    [[nodiscard]] float getPriceModifier(EntityHandle npcHandle) const;

    /** @brief True if relationship too low for trading */
    [[nodiscard]] bool willRefuseTrade(EntityHandle npcHandle) const;

    /** @brief Human-readable relationship: "Friendly", "Neutral", "Hostile", etc. */
    [[nodiscard]] std::string getRelationshipDescription(EntityHandle npcHandle) const;

    // ========================================================================
    // SHARED — NPC Inventory Helpers
    // ========================================================================

    [[nodiscard]] bool isMerchant(EntityHandle npcHandle) const;
    [[nodiscard]] uint32_t getNPCInventoryIndex(EntityHandle npcHandle) const;

    // ========================================================================
    // CONSTANTS
    // ========================================================================

    // Trade pricing
    static constexpr float BUY_PRICE_MULTIPLIER = 1.2f;
    static constexpr float SELL_PRICE_MULTIPLIER = 0.6f;

    // Relationship thresholds
    static constexpr float RELATIONSHIP_HOSTILE = -0.5f;
    static constexpr float RELATIONSHIP_UNFRIENDLY = -0.25f;
    static constexpr float RELATIONSHIP_NEUTRAL = 0.0f;
    static constexpr float RELATIONSHIP_FRIENDLY = 0.25f;
    static constexpr float RELATIONSHIP_TRUSTED = 0.5f;

    // Interaction relationship deltas
    static constexpr float TRADE_RELATIONSHIP_GAIN = 0.02f;
    static constexpr float GIFT_RELATIONSHIP_BASE = 0.05f;
    static constexpr float GIFT_VALUE_SCALE = 0.001f;
    static constexpr float THEFT_RELATIONSHIP_LOSS = -0.3f;
    static constexpr float GUARD_ALERT_RANGE = 500.0f;

private:
    // --- Trade UI ---
    void createTradeUI();
    void destroyTradeUI();
    void rebuildTradeListsUI();
    void normalizeTradeSelections();
    void ensurePaneSelection();
    void refreshMerchantItems();
    void refreshPlayerItems();
    void updatePriceDisplay();
    void updateSelectionHighlight();
    void setActivePaneToMerchant();
    void setActivePaneToPlayer();
    void moveActiveSelection(int delta);
    void adjustQuantityBy(int delta);
    void executeActiveTrade();
    [[nodiscard]] VoidLight::ResourceHandle getGoldHandle() const;

    // --- Memory recording ---
    void recordTrade(EntityHandle npcHandle, float tradeValue, bool wasGoodDeal);
    void recordGift(EntityHandle npcHandle, float giftValue);
    // --- Utility ---
    void dispatchResourceChange(EntityHandle ownerHandle,
                                VoidLight::ResourceHandle resourceHandle,
                                int oldQuantity,
                                int newQuantity,
                                const std::string& reason) const;
    [[nodiscard]] float getItemBaseValue(VoidLight::ResourceHandle itemHandle) const;

    // Player reference
    std::weak_ptr<Player> mp_player;

    // Reusable buffer for spatial queries (avoids per-call allocation)
    std::vector<size_t> m_nearbyGuardBuffer;

    // Trade session state
    EntityHandle m_merchantHandle;
    bool m_isTrading{false};
    bool m_priceDisplayDirty{true};

    // Trade item lists
    std::vector<TradeItemInfo> m_merchantItems;
    std::vector<TradeItemInfo> m_playerItems;

    // Trade selection state
    int m_selectedMerchantIndex{-1};
    int m_selectedPlayerIndex{-1};
    int m_quantity{1};
    bool m_merchantPaneActive{true};

    // Trade UI element IDs
    static constexpr const char* UI_PANEL = "trade_panel";
    static constexpr const char* UI_TITLE = "trade_title";
    static constexpr const char* UI_RELATIONSHIP = "trade_relationship";
    static constexpr const char* UI_MERCHANT_LIST = "trade_merchant_list";
    static constexpr const char* UI_PLAYER_LIST = "trade_player_list";
    static constexpr const char* UI_QUANTITY_LABEL = "trade_qty_label";
    static constexpr const char* UI_PRICE_LABEL = "trade_price_label";
    static constexpr const char* UI_BUY_BTN = "trade_buy_btn";
    static constexpr const char* UI_SELL_BTN = "trade_sell_btn";
    static constexpr const char* UI_CLOSE_BTN = "trade_close_btn";
    static constexpr const char* UI_GOLD_LABEL = "trade_gold_label";
    static constexpr const char* UI_QUANTITY_DEC_BTN = "trade_qty_dec_btn";
    static constexpr const char* UI_QUANTITY_INC_BTN = "trade_qty_inc_btn";
};

#endif // SOCIAL_CONTROLLER_HPP
