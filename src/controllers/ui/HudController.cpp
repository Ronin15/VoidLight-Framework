/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/ui/HudController.hpp"
#include "entities/Player.hpp"
#include "entities/Resource.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "events/EntityEvents.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/UIManager.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>

namespace {

constexpr int HOTBAR_SLOT_SIZE = 68;
constexpr int HOTBAR_SLOT_GAP = 6;
constexpr int HOTBAR_BOTTOM_OFFSET = 24;
constexpr int HOTBAR_KEY_LABEL_SIZE = 17;
constexpr int HOTBAR_ICON_INSET = 8;
constexpr int HOTBAR_ICON_SIZE = HOTBAR_SLOT_SIZE - (HOTBAR_ICON_INSET * 2);
constexpr int HOTBAR_COUNT_HEIGHT = 16;
constexpr int HOTBAR_COUNT_BOTTOM_OFFSET = HOTBAR_BOTTOM_OFFSET + 7;

constexpr int HOTBAR_TOTAL_WIDTH =
    static_cast<int>(HudController::HOTBAR_SLOT_COUNT) * HOTBAR_SLOT_SIZE +
    (static_cast<int>(HudController::HOTBAR_SLOT_COUNT) - 1) * HOTBAR_SLOT_GAP;

// BOTTOM_CENTERED positions x = (windowWidth - bounds.width)/2 + offsetX.
// For each slot we want bounds.x = (windowWidth - HOTBAR_TOTAL_WIDTH)/2 + i*(SLOT_SIZE + GAP),
// so offsetX = i*(SLOT_SIZE + GAP) - (HOTBAR_TOTAL_WIDTH - SLOT_SIZE)/2.
constexpr int slotCenterOffsetX(size_t i)
{
    return static_cast<int>(i) * (HOTBAR_SLOT_SIZE + HOTBAR_SLOT_GAP) -
           (HOTBAR_TOTAL_WIDTH - HOTBAR_SLOT_SIZE) / 2;
}

int getHotbarDisplayQuantity(EntityDataManager& edm,
                             const std::shared_ptr<Player>& player,
                             uint32_t inventoryIndex,
                             VoidLight::ResourceHandle handle)
{
    int quantity = (inventoryIndex != INVALID_INVENTORY_INDEX)
        ? edm.getInventoryQuantity(inventoryIndex, handle)
        : 0;

    if (!player)
    {
        return quantity;
    }

    const size_t playerIndex = edm.getIndex(player->getHandle());
    if (playerIndex == SIZE_MAX)
    {
        return quantity;
    }

    const auto& charData = edm.getCharacterDataByIndex(playerIndex);
    quantity += static_cast<int>(std::count(charData.equippedItems.begin(),
                                            charData.equippedItems.end(),
                                            handle));

    return quantity;
}

} // namespace

HudController::HudController(std::shared_ptr<Player> player)
    : mp_player(player)
    , m_playerHandle(player ? player->getHandle() : EntityHandle{})
{
}

void HudController::subscribe()
{
    if (checkAlreadySubscribed())
    {
        return;
    }

    auto token = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::Combat,
        [this](const EventData& data) { onCombatEvent(data); });
    addHandlerToken(token);

    auto resourceToken = EventManager::Instance().registerHandlerWithToken(
        EventTypeId::ResourceChange,
        [this](const EventData& data) { onResourceChange(data); });
    addHandlerToken(resourceToken);

    setSubscribed(true);
}

void HudController::update(float deltaTime)
{
    if (m_targetDisplayTimer > 0.0f)
    {
        m_targetDisplayTimer -= deltaTime;
        if (m_targetDisplayTimer <= 0.0f)
        {
            clearTarget();
            return;
        }
    }

    if (!m_targetedHandle.isValid())
    {
        return;
    }

    auto& edm = EntityDataManager::Instance();
    const size_t targetIdx = edm.getIndex(m_targetedHandle);
    if (targetIdx == SIZE_MAX || !edm.getHotDataByIndex(targetIdx).isAlive())
    {
        clearTarget();
        return;
    }

    const auto& charData = edm.getCharacterDataByIndex(targetIdx);
    m_cachedTargetHealth = (charData.maxHealth > 0.0f)
        ? (charData.health / charData.maxHealth) * 100.0f
        : 0.0f;
}

bool HudController::hasActiveTarget() const
{
    if (m_targetDisplayTimer <= 0.0f || !m_targetedHandle.isValid())
    {
        return false;
    }

    auto& edm = EntityDataManager::Instance();
    const size_t targetIdx = edm.getIndex(m_targetedHandle);
    if (targetIdx == SIZE_MAX)
    {
        return false;
    }

    return edm.getHotDataByIndex(targetIdx).isAlive();
}

float HudController::getTargetHealth() const
{
    if (!hasActiveTarget())
    {
        return 0.0f;
    }

    return m_cachedTargetHealth;
}

void HudController::onCombatEvent(const EventData& data)
{
    if (!data.isActive() || !data.event || !m_playerHandle.isValid())
    {
        return;
    }

    auto damageEvent = std::dynamic_pointer_cast<DamageEvent>(data.event);
    if (!damageEvent || damageEvent->getSource() != m_playerHandle)
    {
        return;
    }

    const EntityHandle targetHandle = damageEvent->getTarget();
    if (!targetHandle.isValid() || !targetHandle.isNPC())
    {
        return;
    }

    if (damageEvent->wasLethal())
    {
        clearTarget();
        return;
    }

    auto& edm = EntityDataManager::Instance();
    const size_t targetIdx = edm.getIndex(targetHandle);
    if (targetIdx == SIZE_MAX || !edm.getHotDataByIndex(targetIdx).isAlive())
    {
        clearTarget();
        return;
    }

    m_targetedHandle = targetHandle;
    m_targetDisplayTimer = TARGET_DISPLAY_DURATION;
    const float maxHealth = edm.getCharacterDataByIndex(targetIdx).maxHealth;
    m_cachedTargetHealth = (maxHealth > 0.0f)
        ? (damageEvent->getRemainingHealth() / maxHealth) * 100.0f
        : 0.0f;
    if (targetHandle != m_lastLabeledHandle)
    {
        m_targetLabel = edm.getCreatureDisplayName(targetHandle);
        m_lastLabeledHandle = targetHandle;
    }
}

void HudController::onResourceChange(const EventData& data)
{
    const auto* event = dynamic_cast<const ResourceChangeEvent*>(data.event.get());
    if (!event || event->getOwnerHandle() != m_playerHandle)
    {
        return;
    }

    refreshHotbarUI();
}

void HudController::clearTarget()
{
    m_targetedHandle = EntityHandle{};
    m_lastLabeledHandle = EntityHandle{};
    m_targetDisplayTimer = 0.0f;
    m_cachedTargetHealth = 0.0f;
    if (m_targetLabel != "Target")
    {
        m_targetLabel = "Target";
    }
}

void HudController::initializeHotbarUI()
{
    if (m_hotbarUICreated)
    {
        applyHotbarSelectionStyling();
        return;
    }

    auto& ui = UIManager::Instance();

    // No parent panel: slots are positioned independently via BOTTOM_CENTERED.
    // A parent panel would also receive a HOVERED state from UIManager when
    // the mouse is over any slot, painting its theme hoverColor behind the
    // slots and leaking visual state we do not own.

    for (size_t i = 0; i < HOTBAR_SLOT_COUNT; ++i)
    {
        const std::string slotComponentId = hotbarSlotId(i);
        ui.createButton(slotComponentId,
                        {0, 0, HOTBAR_SLOT_SIZE, HOTBAR_SLOT_SIZE},
                        "");
        ui.setComponentPositioning(
            slotComponentId,
            {UIPositionMode::BOTTOM_CENTERED, slotCenterOffsetX(i),
             HOTBAR_BOTTOM_OFFSET, HOTBAR_SLOT_SIZE, HOTBAR_SLOT_SIZE});
        ui.setOnClick(slotComponentId, [this, i]() { setHotbarSelectedIndex(i); });

        const std::string iconComponentId = hotbarIconId(i);
        ui.createAtlasImage(iconComponentId,
                            {0, 0, HOTBAR_ICON_SIZE, HOTBAR_ICON_SIZE},
                            "", UIRect{});
        ui.setComponentPositioning(
            iconComponentId,
            {UIPositionMode::BOTTOM_CENTERED, slotCenterOffsetX(i),
             HOTBAR_BOTTOM_OFFSET + HOTBAR_ICON_INSET,
             HOTBAR_ICON_SIZE, HOTBAR_ICON_SIZE});

        UIStyle countStyle;
        countStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};
        countStyle.textColor = {.r=255, .g=255, .b=255, .a=255};
        countStyle.textAlign = UIAlignment::CENTER_RIGHT;
        countStyle.fontID = UIConstants::FONT_UI;
        countStyle.useTextBackground = true;
        countStyle.textBackgroundColor = {.r=0, .g=0, .b=0, .a=150};
        countStyle.textBackgroundPadding = 2;

        const std::string countComponentId = hotbarCountId(i);
        ui.createLabel(countComponentId,
                       {0, 0, HOTBAR_SLOT_SIZE - 4, HOTBAR_COUNT_HEIGHT},
                       "");
        ui.enableAutoSizing(countComponentId, false);
        ui.setStyle(countComponentId, countStyle);
        ui.setComponentPositioning(
            countComponentId,
            {UIPositionMode::BOTTOM_CENTERED, slotCenterOffsetX(i),
             HOTBAR_COUNT_BOTTOM_OFFSET, HOTBAR_SLOT_SIZE - 4, HOTBAR_COUNT_HEIGHT});

        UIStyle keyLabelStyle;
        keyLabelStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};
        keyLabelStyle.textColor = {.r=220, .g=225, .b=235, .a=230};
        keyLabelStyle.textAlign = UIAlignment::TOP_LEFT;
        keyLabelStyle.fontID = UIConstants::FONT_UI;

        const std::string keyLabelComponentId = hotbarKeyLabelId(i);
        ui.createLabel(keyLabelComponentId,
                       {0, 0, HOTBAR_KEY_LABEL_SIZE, HOTBAR_KEY_LABEL_SIZE},
                       std::to_string(i + 1));
        ui.setStyle(keyLabelComponentId, keyLabelStyle);
        ui.setComponentPositioning(
            keyLabelComponentId,
            {UIPositionMode::BOTTOM_CENTERED, slotCenterOffsetX(i) + 4,
             HOTBAR_BOTTOM_OFFSET + HOTBAR_SLOT_SIZE - HOTBAR_KEY_LABEL_SIZE - 2,
             HOTBAR_KEY_LABEL_SIZE, HOTBAR_KEY_LABEL_SIZE});
    }

    m_hotbarUICreated = true;
    refreshHotbarUI();
    applyHotbarSelectionStyling();
}

void HudController::setHotbarVisible(bool visible)
{
    if (!m_hotbarUICreated)
    {
        return;
    }
    auto& ui = UIManager::Instance();
    for (size_t i = 0; i < HOTBAR_SLOT_COUNT; ++i)
    {
        ui.setComponentVisible(hotbarSlotId(i), visible);
        ui.setComponentVisible(hotbarIconId(i), visible);
        ui.setComponentVisible(hotbarCountId(i), visible);
        ui.setComponentVisible(hotbarKeyLabelId(i), visible);
    }
}

void HudController::handleHotbarInput()
{
    // Must run from a per-render-frame path (e.g. GamePlayState::handleInput),
    // not from update(). isCommandPressed is rising-edge; refreshCommandState
    // advances the edge each render frame, so polling from a fixed-step update
    // drops taps whenever render rate exceeds the update rate.
    if (!m_hotbarUICreated)
    {
        return;
    }
    using C = InputManager::Command;
    static constexpr std::array<C, HOTBAR_SLOT_COUNT> kHotbarCommands{
        C::HotbarSlot1, C::HotbarSlot2, C::HotbarSlot3,
        C::HotbarSlot4, C::HotbarSlot5, C::HotbarSlot6,
        C::HotbarSlot7, C::HotbarSlot8, C::HotbarSlot9,
    };
    auto& inputMgr = InputManager::Instance();
    for (size_t i = 0; i < kHotbarCommands.size(); ++i)
    {
        if (inputMgr.isCommandPressed(kHotbarCommands[i]))
        {
            setHotbarSelectedIndex(i);
            break;
        }
    }
}

void HudController::setHotbarSelectedIndex(size_t i)
{
    if (i >= HOTBAR_SLOT_COUNT)
    {
        return;
    }
    if (i == m_hotbarSelectedIndex && m_hotbarUICreated)
    {
        activateSelectedHotbarItem();
        return;
    }
    m_hotbarSelectedIndex = i;
    applyHotbarSelectionStyling();
    activateSelectedHotbarItem();
}

bool HudController::activateSelectedHotbarItem()
{
    if (m_hotbarSelectedIndex >= HOTBAR_SLOT_COUNT)
    {
        return false;
    }

    return activateHotbarItem(m_hotbarItems[m_hotbarSelectedIndex]);
}

bool HudController::activateHotbarItem(VoidLight::ResourceHandle handle)
{
    if (!handle.isValid())
    {
        return false;
    }

    auto player = mp_player.lock();
    if (!player)
    {
        return false;
    }

    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(handle);
    if (resourceTemplate && resourceTemplate->isConsumable())
    {
        const bool consumed = player->consumeItem(handle);
        if (consumed)
        {
            refreshHotbarUI();
        }
        return consumed;
    }

    auto equipment = std::dynamic_pointer_cast<Equipment>(resourceTemplate);
    if (!equipment ||
        equipment->getEquipmentSlot() != Equipment::EquipmentSlot::Weapon)
    {
        return false;
    }

    if (player->getEquippedItem("weapon") == handle)
    {
        refreshHotbarUI();
        return true;
    }

    const bool equipped = player->equipItem(handle);
    if (equipped)
    {
        refreshHotbarUI();
    }
    return equipped;
}

bool HudController::assignHotbarItem(size_t slotIndex, VoidLight::ResourceHandle handle)
{
    if (slotIndex >= HOTBAR_SLOT_COUNT || !handle.isValid())
    {
        return false;
    }

    for (size_t i = 0; i < HOTBAR_SLOT_COUNT; ++i)
    {
        if (i != slotIndex && m_hotbarItems[i] == handle)
        {
            m_hotbarItems[i] = VoidLight::ResourceHandle{};
        }
    }
    m_hotbarItems[slotIndex] = handle;
    refreshHotbarUI();
    return true;
}

bool HudController::moveHotbarItem(size_t sourceSlot, size_t targetSlot)
{
    if (sourceSlot >= HOTBAR_SLOT_COUNT || targetSlot >= HOTBAR_SLOT_COUNT ||
        !m_hotbarItems[sourceSlot].isValid())
    {
        return false;
    }

    if (sourceSlot == targetSlot)
    {
        return true;
    }

    std::swap(m_hotbarItems[sourceSlot], m_hotbarItems[targetSlot]);
    refreshHotbarUI();
    return true;
}

void HudController::clearHotbarItem(size_t slotIndex)
{
    if (slotIndex >= HOTBAR_SLOT_COUNT)
    {
        return;
    }

    m_hotbarItems[slotIndex] = VoidLight::ResourceHandle{};
    refreshHotbarUI();
}

VoidLight::ResourceHandle HudController::getHotbarItem(size_t slotIndex) const
{
    if (slotIndex >= HOTBAR_SLOT_COUNT)
    {
        return VoidLight::ResourceHandle{};
    }

    return m_hotbarItems[slotIndex];
}

void HudController::refreshHotbarUI()
{
    if (!m_hotbarUICreated)
    {
        return;
    }

    auto& ui = UIManager::Instance();
    auto player = mp_player.lock();
    const uint32_t inventoryIndex = player ? player->getInventoryIndex() : INVALID_INVENTORY_INDEX;
    auto& edm = EntityDataManager::Instance();
    auto& rtm = ResourceTemplateManager::Instance();

    for (size_t i = 0; i < HOTBAR_SLOT_COUNT; ++i)
    {
        const std::string iconComponentId = hotbarIconId(i);
        const std::string countComponentId = hotbarCountId(i);
        const VoidLight::ResourceHandle handle = m_hotbarItems[i];

        if (!handle.isValid())
        {
            ui.setImageSource(iconComponentId, TextureSource{});
            ui.setText(countComponentId, "");
            continue;
        }

        ui.setImageSource(iconComponentId, rtm.getIconTextureSource(handle));

        const int quantity =
            getHotbarDisplayQuantity(edm, player, inventoryIndex, handle);
        ui.setText(countComponentId, std::format("{}", quantity));
    }
}

void HudController::applyHotbarSelectionStyling()
{
    if (!m_hotbarUICreated)
    {
        return;
    }

    UIStyle defaultStyle;
    defaultStyle.backgroundColor = {.r=20, .g=24, .b=30, .a=190};
    defaultStyle.borderColor = {.r=95, .g=115, .b=135, .a=210};
    // Hover/press match background — hotbar selection is owned by us, not by
    // UIManager's interactive state, so the slot must not visually flinch on hover.
    defaultStyle.hoverColor = defaultStyle.backgroundColor;
    defaultStyle.pressedColor = defaultStyle.backgroundColor;
    defaultStyle.borderWidth = 1;
    defaultStyle.textAlign = UIAlignment::CENTER_CENTER;

    UIStyle selectedStyle = defaultStyle;
    selectedStyle.borderColor = {.r=240, .g=200, .b=80, .a=255};
    selectedStyle.borderWidth = 3;
    selectedStyle.backgroundColor = {.r=40, .g=44, .b=52, .a=210};
    selectedStyle.hoverColor = selectedStyle.backgroundColor;
    selectedStyle.pressedColor = selectedStyle.backgroundColor;

    auto& ui = UIManager::Instance();
    for (size_t i = 0; i < HOTBAR_SLOT_COUNT; ++i)
    {
        ui.setStyle(hotbarSlotId(i),
                    (i == m_hotbarSelectedIndex) ? selectedStyle : defaultStyle);
    }
}

std::string HudController::hotbarSlotId(size_t i)
{
    return std::format("hotbar_slot_{}", i);
}

std::string HudController::hotbarKeyLabelId(size_t i)
{
    return std::format("hotbar_key_{}", i);
}

std::string HudController::hotbarIconId(size_t i)
{
    return std::format("hotbar_icon_{}", i);
}

std::string HudController::hotbarCountId(size_t i)
{
    return std::format("hotbar_count_{}", i);
}
