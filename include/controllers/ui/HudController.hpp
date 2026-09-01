/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HUD_CONTROLLER_HPP
#define HUD_CONTROLLER_HPP

/**
 * @file HudController.hpp
 * @brief State-scoped owner of the gameplay action HUD
 *
 * HudController owns the action HUD subtree: player HP/SP bars, combat target
 * frame, optional hotbar, and harvest progress. It creates those widgets via
 * UIManager primitives, updates them with dirty-flag setValue/setText/visibility,
 * and exposes setVisible() for pause/resume of its subtree.
 *
 * Session chrome (event_log, time_label, fps) stays on GamePlayState.
 * Inventory and trade stay on their controllers. UIManager is the widget service.
 *
 * initializeActionHUD() is required for vitals/target/harvest widgets.
 * initializeHotbarUI() remains optional. Both are idempotent. There is no
 * destroyActionHUD(); UIManager state-transition cleanup removes widgets.
 *
 * Ownership: ControllerRegistry owns the controller instance.
 */

#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/ResourceHandle.hpp"
#include <array>
#include <memory>
#include <string>

class Player;

class HudController : public ControllerBase, public IUpdatable
{
public:
    static constexpr float TARGET_DISPLAY_DURATION{3.0f};

    explicit HudController(const std::shared_ptr<Player>& player);

    ~HudController() override = default;

    HudController(HudController&&) noexcept = default;
    HudController& operator=(HudController&&) noexcept = default;

    void subscribe() override;
    [[nodiscard]] std::string_view getName() const override { return "HudController"; }
    void update(float deltaTime) override;

    [[nodiscard]] bool hasActiveTarget() const;
    [[nodiscard]] float getTargetHealth() const;
    [[nodiscard]] const std::string& getTargetLabel() const { return m_targetLabel; }

    void initializeActionHUD();
    void setVisible(bool visible);
    void setHarvestProgress(bool active, float progress01);

    void initializeHotbarUI();
    void handleHotbarInput();
    void setHotbarSelectedIndex(size_t i);
    bool activateSelectedHotbarItem();
    [[nodiscard]] size_t getHotbarSelectedIndex() const { return m_hotbarSelectedIndex; }

    bool assignHotbarItem(size_t slotIndex, VoidLight::ResourceHandle handle);
    bool moveHotbarItem(size_t sourceSlot, size_t targetSlot);
    void clearHotbarItem(size_t slotIndex);
    [[nodiscard]] VoidLight::ResourceHandle getHotbarItem(size_t slotIndex) const;
    void refreshHotbarUI();

    static constexpr size_t HOTBAR_SLOT_COUNT = 9;
    static constexpr const char* HOTBAR_PANEL_ID = "hotbar_panel";
    static std::string hotbarSlotId(size_t i);

    static constexpr const char* HEALTH_LABEL_ID = "hud_health_label";
    static constexpr const char* HEALTH_BAR_ID = "hud_health_bar";
    static constexpr const char* STAMINA_LABEL_ID = "hud_stamina_label";
    static constexpr const char* STAMINA_BAR_ID = "hud_stamina_bar";
    static constexpr const char* TARGET_NAME_ID = "hud_target_name";
    static constexpr const char* TARGET_HP_LABEL_ID = "hud_target_hp_label";
    static constexpr const char* TARGET_HEALTH_BAR_ID = "hud_target_health";
    static constexpr const char* HARVEST_LABEL_ID = "hud_harvest_label";
    static constexpr const char* HARVEST_BAR_ID = "hud_harvest_bar";

private:
    void onCombatEvent(const EventData& data);
    void onResourceChange(const EventData& data);
    void clearTarget();
    void applyActionHUDWidgets();
    void applyPlayerVitalsWidgets();
    void applyTargetWidgets();
    void applyHarvestWidgets();
    void setHotbarVisible(bool visible);
    void applyHotbarSelectionStyling();
    bool activateHotbarItem(VoidLight::ResourceHandle handle);
    static std::string hotbarKeyLabelId(size_t i);
    static std::string hotbarIconId(size_t i);
    static std::string hotbarCountId(size_t i);

    std::weak_ptr<Player> mp_player;
    EntityHandle m_playerHandle{};
    EntityHandle m_targetedHandle{};
    EntityHandle m_lastLabeledHandle{};
    float m_targetDisplayTimer{0.0f};
    float m_cachedTargetHealth{0.0f};
    std::string m_targetLabel{"Target"};

    size_t m_hotbarSelectedIndex{0};
    std::array<VoidLight::ResourceHandle, HOTBAR_SLOT_COUNT> m_hotbarItems{};
    // Set strictly in lockstep with UIManager hotbar component existence; gates pollHotbarInput and visibility ops.
    bool m_hotbarUICreated{false};

    bool m_actionHUDCreated{false};
    bool m_hudVisible{true};
    float m_lastHealthPct{-1.0f};
    float m_lastStaminaPct{-1.0f};
    bool m_lastTargetVisible{false};
    float m_lastTargetHealthPct{-1.0f};
    std::string m_lastTargetLabel{};
    bool m_harvestActive{false};
    float m_harvestProgress{0.0f};
    bool m_lastHarvestVisible{false};
    float m_lastHarvestProgress{-1.0f};
};

#endif // HUD_CONTROLLER_HPP
