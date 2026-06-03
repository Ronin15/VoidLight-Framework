/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HUD_CONTROLLER_HPP
#define HUD_CONTROLLER_HPP

/**
 * @file HudController.hpp
 * @brief Gameplay HUD controller for target-frame state and hotbar interaction
 *
 * HudController owns the hotbar UI subtree (nine slot buttons + key labels
 * created via UIManager) and the transient combat target-frame state.
 * It creates and styles its components in initializeHotbarUI(), applies
 * selection highlighting, and hides/shows slots in response to game events.
 * Combat event subscription populates the target display for GamePlayState's
 * updateCombatHUD() call each frame.
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

    explicit HudController(std::shared_ptr<Player> player);

    ~HudController() override = default;

    HudController(HudController&&) noexcept = default;
    HudController& operator=(HudController&&) noexcept = default;

    void subscribe() override;
    [[nodiscard]] std::string_view getName() const override { return "HudController"; }
    void update(float deltaTime) override;

    [[nodiscard]] bool hasActiveTarget() const;
    [[nodiscard]] float getTargetHealth() const;
    [[nodiscard]] const std::string& getTargetLabel() const { return m_targetLabel; }

    void initializeHotbarUI();
    void handleHotbarInput();
    void setHotbarSelectedIndex(size_t i);
    bool activateSelectedHotbarItem();
    void setHotbarVisible(bool visible);
    [[nodiscard]] size_t getHotbarSelectedIndex() const { return m_hotbarSelectedIndex; }

    bool assignHotbarItem(size_t slotIndex, VoidLight::ResourceHandle handle);
    bool moveHotbarItem(size_t sourceSlot, size_t targetSlot);
    void clearHotbarItem(size_t slotIndex);
    [[nodiscard]] VoidLight::ResourceHandle getHotbarItem(size_t slotIndex) const;
    void refreshHotbarUI();

    static constexpr size_t HOTBAR_SLOT_COUNT = 9;
    static constexpr const char* HOTBAR_PANEL_ID = "hotbar_panel";
    static std::string hotbarSlotId(size_t i);

private:
    void onCombatEvent(const EventData& data);
    void onResourceChange(const EventData& data);
    void clearTarget();
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
};

#endif // HUD_CONTROLLER_HPP
