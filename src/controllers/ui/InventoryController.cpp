/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/ui/InventoryController.hpp"
#include "controllers/ui/HudController.hpp"
#include "core/Logger.hpp"
#include "entities/Player.hpp"
#include "entities/Resource.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/UIManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace {

constexpr int INVENTORY_GRID_COLUMNS = 5;
constexpr int INVENTORY_GRID_ROWS = 4;
constexpr int INVENTORY_SLOT_COUNT = INVENTORY_GRID_COLUMNS * INVENTORY_GRID_ROWS;
constexpr int INVENTORY_SLOT_SIZE = 40;
constexpr int INVENTORY_SLOT_GAP = 8;
constexpr int INVENTORY_ICON_INSET = 4;
constexpr int INVENTORY_ICON_SIZE = INVENTORY_SLOT_SIZE - (INVENTORY_ICON_INSET * 2);
constexpr int INVENTORY_GRID_WIDTH =
    (INVENTORY_GRID_COLUMNS * INVENTORY_SLOT_SIZE) +
    ((INVENTORY_GRID_COLUMNS - 1) * INVENTORY_SLOT_GAP);
constexpr int INVENTORY_GRID_HEIGHT =
    (INVENTORY_GRID_ROWS * INVENTORY_SLOT_SIZE) +
    ((INVENTORY_GRID_ROWS - 1) * INVENTORY_SLOT_GAP);
constexpr int INVENTORY_PANEL_WIDTH = 1040;
constexpr int INVENTORY_CHILD_INSET = 10;
constexpr int INVENTORY_HEADER_HEIGHT = 130;
constexpr int INVENTORY_BOTTOM_PADDING = 15;
constexpr int INVENTORY_SECTION_WIDTH = 280;
constexpr int INVENTORY_TITLE_Y = 20;
constexpr int INVENTORY_TITLE_WIDTH = 220;
constexpr int INVENTORY_TITLE_HEIGHT = 35;
constexpr int INVENTORY_STATUS_Y = 80;
constexpr int INVENTORY_STATUS_HEIGHT = 25;
constexpr int INVENTORY_SECTION_HEADER_Y = 55;
constexpr int INVENTORY_SECTION_HEADER_WIDTH = UIConstants::DEFAULT_BUTTON_WIDTH;
constexpr int INVENTORY_SECTION_HEADER_HEIGHT = 26;
constexpr int INVENTORY_CONTENT_Y = 110;
constexpr int INVENTORY_COUNT_INSET_X = 2;
constexpr int INVENTORY_COUNT_HEIGHT = 14;
constexpr int INVENTORY_COUNT_BOTTOM_INSET = 16;
constexpr int GEAR_SECTION_X = 320;
constexpr int GEAR_SLOT_WIDTH = 390;
constexpr int GEAR_SLOT_HEIGHT = 44;
constexpr int GEAR_SLOT_GAP = 8;
constexpr int GEAR_ICON_INSET = 6;
constexpr int GEAR_ICON_SIZE = 32;
constexpr int GEAR_LABEL_GAP = 5;
constexpr int GEAR_LABEL_INSET_Y = 4;
constexpr int GEAR_LABEL_VERTICAL_PADDING = GEAR_LABEL_INSET_Y * 2;
constexpr int GEAR_LABEL_WIDTH =
    GEAR_SLOT_WIDTH - GEAR_ICON_SIZE - (GEAR_ICON_INSET * 2) - GEAR_LABEL_GAP;
constexpr int GEAR_LABEL_HEIGHT = GEAR_SLOT_HEIGHT - GEAR_LABEL_VERTICAL_PADDING;
constexpr int GEAR_LIST_HEIGHT =
    (static_cast<int>(Equipment::EquipmentSlot::COUNT) * GEAR_SLOT_HEIGHT) +
    ((static_cast<int>(Equipment::EquipmentSlot::COUNT) - 1) * GEAR_SLOT_GAP);
constexpr int CONTAINER_SECTION_X = 750;
constexpr int CONTAINER_SECTION_WIDTH = 280;
constexpr int CONTAINER_GRID_COLUMNS = 4;
constexpr int CONTAINER_GRID_ROWS = 3;
constexpr int CONTAINER_SLOT_COUNT = CONTAINER_GRID_COLUMNS * CONTAINER_GRID_ROWS;
constexpr int CONTAINER_SLOT_SIZE = INVENTORY_SLOT_SIZE;
constexpr int CONTAINER_SLOT_GAP = INVENTORY_SLOT_GAP;
constexpr int CONTAINER_ICON_INSET = INVENTORY_ICON_INSET;
constexpr int CONTAINER_ICON_SIZE = INVENTORY_ICON_SIZE;
constexpr int CONTAINER_GRID_WIDTH =
    (CONTAINER_GRID_COLUMNS * CONTAINER_SLOT_SIZE) +
    ((CONTAINER_GRID_COLUMNS - 1) * CONTAINER_SLOT_GAP);
constexpr int CONTAINER_GRID_HEIGHT =
    (CONTAINER_GRID_ROWS * CONTAINER_SLOT_SIZE) +
    ((CONTAINER_GRID_ROWS - 1) * CONTAINER_SLOT_GAP);
constexpr int CONTAINER_STATUS_Y = INVENTORY_STATUS_Y;
constexpr int CONTAINER_STATUS_HEIGHT = INVENTORY_STATUS_HEIGHT;
constexpr int CONTAINER_LOOT_ALL_Y =
    INVENTORY_CONTENT_Y + CONTAINER_GRID_HEIGHT + 18;
constexpr int CONTAINER_LOOT_ALL_WIDTH = UIConstants::DEFAULT_BUTTON_WIDTH;
constexpr int CONTAINER_LOOT_ALL_HEIGHT = UIConstants::DEFAULT_BUTTON_HEIGHT;
const std::string HOTBAR_DRAG_GHOST_ID{"inventory_hotbar_drag_ghost"};
const std::string CONTAINER_HEADER_ID{"container_tab_items"};
const std::string CONTAINER_STATUS_ID{"container_status"};
const std::string CONTAINER_LOOT_ALL_ID{"container_loot_all"};
constexpr int HOTBAR_DRAG_GHOST_SIZE = 40;

constexpr int centerOffset(int position, int size, int containerHalfSize) {
    return position + (size / 2) - containerHalfSize;
}

const auto& gearSlots() {
    return Equipment::equipmentSlotDefinitions();
}

} // namespace

void InventoryController::subscribe() {
    if (checkAlreadySubscribed()) {
        return;
    }

    auto& eventMgr = EventManager::Instance();
    auto token = eventMgr.registerHandlerWithToken(
        EventTypeId::ResourceChange,
        [this](const EventData& data) { onResourceChange(data); });
    addHandlerToken(token);

    setSubscribed(true);
    INVENTORY_CONTROLLER_DEBUG("Subscribed to ResourceChangeEvent");
}

bool InventoryController::attemptPickup() {
    auto player = mp_player.lock();
    if (!player) {
        return false;
    }

    auto& wrm = WorldResourceManager::Instance();
    auto& edm = EntityDataManager::Instance();

    size_t itemIdx;
    if (!wrm.findClosestDroppedItem(player->getPosition(), PICKUP_RADIUS, itemIdx)) {
        return false;
    }

    EntityHandle itemHandle = edm.getStaticHandle(itemIdx);
    if (!itemHandle.isValid()) {
        return false;
    }

    const auto& hot = edm.getStaticHotDataByIndex(itemIdx);
    if (!hot.isAlive() || hot.kind != EntityKind::DroppedItem) {
        return false;
    }

    const auto& itemData = edm.getItemData(itemHandle);
    if (itemData.quantity <= 0) {
        return false;
    }

    const uint32_t playerInvIdx = player->getInventoryIndex();
    if (playerInvIdx == INVALID_INVENTORY_INDEX) {
        INVENTORY_CONTROLLER_WARN("Player has no inventory");
        return false;
    }

    const int oldQuantity = edm.getInventoryQuantity(playerInvIdx, itemData.resourceHandle);
    if (!edm.addToInventory(playerInvIdx, itemData.resourceHandle, itemData.quantity)) {
        INVENTORY_CONTROLLER_DEBUG("Inventory full, cannot pick up item");
        return false;
    }

    const int newQuantity = oldQuantity + itemData.quantity;
    EventManager::Instance().triggerResourceChange(
        player->getHandle(), itemData.resourceHandle, oldQuantity, newQuantity,
        "picked_up");

    edm.destroyEntity(itemHandle);

    INVENTORY_CONTROLLER_INFO(std::format("Picked up {} x{}", itemData.resourceHandle.toString(),
                                          itemData.quantity));
    return true;
}

bool InventoryController::tryOpenNearbyContainer() {
    auto player = mp_player.lock();
    if (!player) {
        return false;
    }

    auto& wrm = WorldResourceManager::Instance();
    auto& edm = EntityDataManager::Instance();
    m_nearbyContainerIndices.clear();
    wrm.queryContainersInRadius(player->getPosition(), PICKUP_RADIUS,
                                m_nearbyContainerIndices);
    if (m_nearbyContainerIndices.empty()) {
        return false;
    }

    const Vector2D playerPos = player->getPosition();
    const float radiusSq = PICKUP_RADIUS * PICKUP_RADIUS;
    size_t closestIndex = SIZE_MAX;
    float closestDistanceSq = radiusSq;
    for (size_t staticIndex : m_nearbyContainerIndices) {
        if (staticIndex >= edm.getStaticHotDataArray().size()) {
            continue;
        }

        const auto& hot = edm.getStaticHotDataByIndex(staticIndex);
        if (!hot.isAlive() || hot.kind != EntityKind::Container) {
            continue;
        }

        const Vector2D delta = hot.transform.position - playerPos;
        const float distanceSq =
            (delta.getX() * delta.getX()) + (delta.getY() * delta.getY());
        if (distanceSq <= closestDistanceSq) {
            closestIndex = staticIndex;
            closestDistanceSq = distanceSq;
        }
    }

    if (closestIndex == SIZE_MAX) {
        return false;
    }

    const auto& hot = edm.getStaticHotDataByIndex(closestIndex);
    auto& container = edm.getContainerData(hot.typeLocalIndex);
    if (container.isLocked()) {
        UIManager::Instance().addEventLogEntry(EVENT_LOG_ID, "Chest is locked");
        return true;
    }

    closeOpenContainer();
    container.setOpen(true);
    m_openContainerStaticIndex = closestIndex;
    m_openContainerInventoryIndex = container.inventoryIndex;
    setInventoryVisible(true);
    refreshContainerUI();
    return true;
}

void InventoryController::update(float) {
    if (!hasOpenContainer()) {
        return;
    }

    auto player = mp_player.lock();
    if (!player) {
        closeOpenContainer();
        return;
    }

    auto& edm = EntityDataManager::Instance();
    if (m_openContainerStaticIndex >= edm.getStaticHotDataArray().size() ||
        !edm.isValidInventoryIndex(m_openContainerInventoryIndex)) {
        closeOpenContainer();
        return;
    }

    const auto& hot = edm.getStaticHotDataByIndex(m_openContainerStaticIndex);
    if (!hot.isAlive() || hot.kind != EntityKind::Container) {
        closeOpenContainer();
        return;
    }

    const Vector2D delta = hot.transform.position - player->getPosition();
    const float distanceSq =
        (delta.getX() * delta.getX()) + (delta.getY() * delta.getY());
    if (distanceSq > (PICKUP_RADIUS * PICKUP_RADIUS)) {
        closeOpenContainer();
    }
}

void InventoryController::initializeInventoryUI() {
    if (m_inventoryUICreated) {
        refreshInventoryUI();
        return;
    }

    auto player = mp_player.lock();
    if (!player) {
        return;
    }

    auto& ui = UIManager::Instance();
    constexpr int childWidth = INVENTORY_SECTION_WIDTH - (INVENTORY_CHILD_INSET * 2);
    constexpr int inventoryContentHeight = std::max(INVENTORY_GRID_HEIGHT, GEAR_LIST_HEIGHT);
    constexpr int inventoryHeight =
        INVENTORY_HEADER_HEIGHT + inventoryContentHeight + INVENTORY_BOTTOM_PADDING;
    constexpr int panelHalfWidth = INVENTORY_PANEL_WIDTH / 2;
    constexpr int panelHalfHeight = inventoryHeight / 2;
    constexpr int gridOffsetX = INVENTORY_CHILD_INSET +
        ((childWidth - INVENTORY_GRID_WIDTH) / 2);
    constexpr int gridOffsetY = INVENTORY_CONTENT_Y;
    constexpr int titleX = (INVENTORY_PANEL_WIDTH - INVENTORY_TITLE_WIDTH) / 2;

    UIStyle headerStyle;
    headerStyle.backgroundColor = {.r=20, .g=24, .b=30, .a=190};
    headerStyle.borderColor = {.r=95, .g=115, .b=135, .a=210};
    headerStyle.hoverColor = headerStyle.backgroundColor;
    headerStyle.pressedColor = headerStyle.backgroundColor;
    headerStyle.textColor = {.r=230, .g=235, .b=242, .a=255};
    headerStyle.borderWidth = 1;
    headerStyle.useTextBackground = false;
    headerStyle.fontID = UIConstants::FONT_UI;

    ui.createDialog(INVENTORY_PANEL_ID,
                    {0, 0, INVENTORY_PANEL_WIDTH, inventoryHeight});
    ui.setComponentPositioning(
        INVENTORY_PANEL_ID,
        {UIPositionMode::CENTERED_BOTH, 0, 0, INVENTORY_PANEL_WIDTH, inventoryHeight});

    ui.createTitle(INVENTORY_TITLE_ID,
                   {titleX, INVENTORY_TITLE_Y,
                    INVENTORY_TITLE_WIDTH, INVENTORY_TITLE_HEIGHT},
                   "Inventory", INVENTORY_PANEL_ID);
    ui.setStyle(INVENTORY_TITLE_ID, headerStyle);
    ui.setTitleAlignment(INVENTORY_TITLE_ID, UIAlignment::CENTER_CENTER);
    ui.enableAutoSizing(INVENTORY_TITLE_ID, false);
    ui.setComponentPositioning(
        INVENTORY_TITLE_ID,
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(titleX, INVENTORY_TITLE_WIDTH, panelHalfWidth),
         centerOffset(INVENTORY_TITLE_Y, INVENTORY_TITLE_HEIGHT, panelHalfHeight),
         INVENTORY_TITLE_WIDTH, INVENTORY_TITLE_HEIGHT});

    ui.createLabel(INVENTORY_STATUS_ID,
                   {INVENTORY_CHILD_INSET, INVENTORY_STATUS_Y,
                    childWidth, INVENTORY_STATUS_HEIGHT},
                   "Capacity: 0/20", INVENTORY_PANEL_ID);
    ui.enableAutoSizing(INVENTORY_STATUS_ID, false);
    ui.setComponentPositioning(
        INVENTORY_STATUS_ID,
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(INVENTORY_CHILD_INSET, childWidth, panelHalfWidth),
         centerOffset(INVENTORY_STATUS_Y, INVENTORY_STATUS_HEIGHT, panelHalfHeight),
         childWidth, INVENTORY_STATUS_HEIGHT});

    ui.createLabel("inventory_tab_items",
                   {gridOffsetX, INVENTORY_SECTION_HEADER_Y,
                    INVENTORY_SECTION_HEADER_WIDTH, INVENTORY_SECTION_HEADER_HEIGHT},
                   "Items", INVENTORY_PANEL_ID);
    ui.setStyle("inventory_tab_items", headerStyle);
    ui.setLabelAlignment("inventory_tab_items", UIAlignment::CENTER_LEFT);
    ui.enableAutoSizing("inventory_tab_items", false);
    ui.setComponentPositioning(
        "inventory_tab_items",
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(gridOffsetX, INVENTORY_SECTION_HEADER_WIDTH, panelHalfWidth),
         centerOffset(INVENTORY_SECTION_HEADER_Y, INVENTORY_SECTION_HEADER_HEIGHT, panelHalfHeight),
         INVENTORY_SECTION_HEADER_WIDTH, INVENTORY_SECTION_HEADER_HEIGHT});

    ui.createLabel("inventory_tab_gear",
                   {GEAR_SECTION_X, INVENTORY_SECTION_HEADER_Y,
                    INVENTORY_SECTION_HEADER_WIDTH, INVENTORY_SECTION_HEADER_HEIGHT},
                   "Gear", INVENTORY_PANEL_ID);
    ui.setStyle("inventory_tab_gear", headerStyle);
    ui.setLabelAlignment("inventory_tab_gear", UIAlignment::CENTER_LEFT);
    ui.enableAutoSizing("inventory_tab_gear", false);
    ui.setComponentPositioning(
        "inventory_tab_gear",
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(GEAR_SECTION_X, INVENTORY_SECTION_HEADER_WIDTH, panelHalfWidth),
         centerOffset(INVENTORY_SECTION_HEADER_Y, INVENTORY_SECTION_HEADER_HEIGHT, panelHalfHeight),
         INVENTORY_SECTION_HEADER_WIDTH, INVENTORY_SECTION_HEADER_HEIGHT});

    UIStyle slotStyle;
    slotStyle.backgroundColor = {.r=20, .g=24, .b=30, .a=190};
    slotStyle.borderColor = {.r=95, .g=115, .b=135, .a=210};
    slotStyle.hoverColor = slotStyle.backgroundColor;
    slotStyle.pressedColor = slotStyle.backgroundColor;
    slotStyle.borderWidth = 1;
    slotStyle.textAlign = UIAlignment::CENTER_CENTER;

    UIStyle countStyle;
    countStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};
    countStyle.textColor = {.r=255, .g=255, .b=255, .a=255};
    countStyle.textAlign = UIAlignment::CENTER_RIGHT;
    countStyle.fontID = UIConstants::FONT_UI;
    countStyle.useTextBackground = true;
    countStyle.textBackgroundColor = {.r=0, .g=0, .b=0, .a=150};
    countStyle.textBackgroundPadding = 2;

    for (int slot = 0; slot < INVENTORY_SLOT_COUNT; ++slot) {
        const int col = slot % INVENTORY_GRID_COLUMNS;
        const int row = slot / INVENTORY_GRID_COLUMNS;
        const int slotX = gridOffsetX +
            col * (INVENTORY_SLOT_SIZE + INVENTORY_SLOT_GAP);
        const int slotY = gridOffsetY +
            row * (INVENTORY_SLOT_SIZE + INVENTORY_SLOT_GAP);

        const std::string slotComponentId = slotId(static_cast<size_t>(slot));
        ui.createButton(slotComponentId,
                        {slotX, slotY, INVENTORY_SLOT_SIZE, INVENTORY_SLOT_SIZE},
                        "", INVENTORY_PANEL_ID);
        ui.enableAutoSizing(slotComponentId, false);
        ui.setStyle(slotComponentId, slotStyle);
        ui.setComponentPositioning(
            slotComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX, INVENTORY_SLOT_SIZE, panelHalfWidth),
             centerOffset(slotY, INVENTORY_SLOT_SIZE, panelHalfHeight),
             INVENTORY_SLOT_SIZE, INVENTORY_SLOT_SIZE});
        ui.setOnClick(slotComponentId, [this, slot]() {
            handleInventorySlotClicked(static_cast<size_t>(slot));
        });

        const std::string iconComponentId = iconId(static_cast<size_t>(slot));
        ui.createAtlasImage(
            iconComponentId,
            {slotX + INVENTORY_ICON_INSET, slotY + INVENTORY_ICON_INSET,
             INVENTORY_ICON_SIZE, INVENTORY_ICON_SIZE},
            "", UIRect{}, slotComponentId);
        ui.setComponentPositioning(
            iconComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX + INVENTORY_ICON_INSET, INVENTORY_ICON_SIZE, panelHalfWidth),
             centerOffset(slotY + INVENTORY_ICON_INSET, INVENTORY_ICON_SIZE, panelHalfHeight),
             INVENTORY_ICON_SIZE,
             INVENTORY_ICON_SIZE});

        const std::string countComponentId = countId(static_cast<size_t>(slot));
        ui.createLabel(countComponentId,
                       {slotX + INVENTORY_COUNT_INSET_X,
                        slotY + INVENTORY_SLOT_SIZE - INVENTORY_COUNT_BOTTOM_INSET,
                        INVENTORY_SLOT_SIZE - (INVENTORY_COUNT_INSET_X * 2),
                        INVENTORY_COUNT_HEIGHT},
                       "", slotComponentId);
        ui.enableAutoSizing(countComponentId, false);
        ui.setStyle(countComponentId, countStyle);
        ui.setComponentPositioning(
            countComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX + INVENTORY_COUNT_INSET_X,
                          INVENTORY_SLOT_SIZE - (INVENTORY_COUNT_INSET_X * 2),
                          panelHalfWidth),
             centerOffset(slotY + INVENTORY_SLOT_SIZE - INVENTORY_COUNT_BOTTOM_INSET,
                          INVENTORY_COUNT_HEIGHT,
                          panelHalfHeight),
             INVENTORY_SLOT_SIZE - (INVENTORY_COUNT_INSET_X * 2),
             INVENTORY_COUNT_HEIGHT});
    }

    UIStyle gearLabelStyle;
    gearLabelStyle.backgroundColor = {.r=0, .g=0, .b=0, .a=0};
    gearLabelStyle.textColor = {.r=230, .g=235, .b=242, .a=255};
    gearLabelStyle.textAlign = UIAlignment::CENTER_LEFT;
    gearLabelStyle.fontID = UIConstants::FONT_UI;

    constexpr int gearOffsetY = INVENTORY_CONTENT_Y;
    const auto& slots = gearSlots();
    for (size_t slot = 0; slot < slots.size(); ++slot) {
        const int row = static_cast<int>(slot);
        const int slotX = GEAR_SECTION_X;
        const int slotY = gearOffsetY +
            row * (GEAR_SLOT_HEIGHT + GEAR_SLOT_GAP);

        const std::string slotComponentId = gearSlotId(slot);
        ui.createButton(slotComponentId,
                        {slotX, slotY, GEAR_SLOT_WIDTH, GEAR_SLOT_HEIGHT},
                        "", INVENTORY_PANEL_ID);
        ui.enableAutoSizing(slotComponentId, false);
        ui.setStyle(slotComponentId, slotStyle);
        ui.setComponentPositioning(
            slotComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX, GEAR_SLOT_WIDTH, panelHalfWidth),
             centerOffset(slotY, GEAR_SLOT_HEIGHT, panelHalfHeight),
             GEAR_SLOT_WIDTH, GEAR_SLOT_HEIGHT});
        ui.setOnClick(slotComponentId, [this, slot]() {
            handleGearSlotClicked(slot);
        });

        const std::string iconComponentId = gearIconId(slot);
        ui.createAtlasImage(
            iconComponentId,
            {slotX + GEAR_ICON_INSET, slotY + GEAR_ICON_INSET,
             GEAR_ICON_SIZE, GEAR_ICON_SIZE},
            "", UIRect{}, slotComponentId);
        ui.setComponentPositioning(
            iconComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX + GEAR_ICON_INSET, GEAR_ICON_SIZE, panelHalfWidth),
             centerOffset(slotY + GEAR_ICON_INSET, GEAR_ICON_SIZE, panelHalfHeight),
             GEAR_ICON_SIZE, GEAR_ICON_SIZE});

        const std::string labelComponentId = gearLabelId(slot);
        ui.createLabel(labelComponentId,
                       {slotX + GEAR_ICON_INSET + GEAR_ICON_SIZE + GEAR_LABEL_GAP,
                        slotY + GEAR_LABEL_INSET_Y,
                        GEAR_LABEL_WIDTH, GEAR_LABEL_HEIGHT},
                       std::string(slots[slot].label), slotComponentId);
        ui.enableAutoSizing(labelComponentId, false);
        ui.setStyle(labelComponentId, gearLabelStyle);
        ui.setComponentPositioning(
            labelComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX + GEAR_ICON_INSET + GEAR_ICON_SIZE + GEAR_LABEL_GAP,
                          GEAR_LABEL_WIDTH, panelHalfWidth),
             centerOffset(slotY + GEAR_LABEL_INSET_Y, GEAR_LABEL_HEIGHT, panelHalfHeight),
             GEAR_LABEL_WIDTH, GEAR_LABEL_HEIGHT});
    }

    const int containerGridX = CONTAINER_SECTION_X +
        ((CONTAINER_SECTION_WIDTH - CONTAINER_GRID_WIDTH) / 2);
    ui.createLabel(CONTAINER_HEADER_ID,
                   {CONTAINER_SECTION_X, INVENTORY_SECTION_HEADER_Y,
                    INVENTORY_SECTION_HEADER_WIDTH, INVENTORY_SECTION_HEADER_HEIGHT},
                   "Container", INVENTORY_PANEL_ID);
    ui.setStyle(CONTAINER_HEADER_ID, headerStyle);
    ui.setLabelAlignment(CONTAINER_HEADER_ID, UIAlignment::CENTER_LEFT);
    ui.enableAutoSizing(CONTAINER_HEADER_ID, false);
    ui.setComponentPositioning(
        CONTAINER_HEADER_ID,
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(CONTAINER_SECTION_X, INVENTORY_SECTION_HEADER_WIDTH, panelHalfWidth),
         centerOffset(INVENTORY_SECTION_HEADER_Y, INVENTORY_SECTION_HEADER_HEIGHT, panelHalfHeight),
         INVENTORY_SECTION_HEADER_WIDTH, INVENTORY_SECTION_HEADER_HEIGHT});

    ui.createLabel(CONTAINER_STATUS_ID,
                   {CONTAINER_SECTION_X, CONTAINER_STATUS_Y,
                    CONTAINER_SECTION_WIDTH, CONTAINER_STATUS_HEIGHT},
                   "", INVENTORY_PANEL_ID);
    ui.enableAutoSizing(CONTAINER_STATUS_ID, false);
    ui.setComponentPositioning(
        CONTAINER_STATUS_ID,
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(CONTAINER_SECTION_X, CONTAINER_SECTION_WIDTH, panelHalfWidth),
         centerOffset(CONTAINER_STATUS_Y, CONTAINER_STATUS_HEIGHT, panelHalfHeight),
         CONTAINER_SECTION_WIDTH, CONTAINER_STATUS_HEIGHT});

    for (int slot = 0; slot < CONTAINER_SLOT_COUNT; ++slot) {
        const int col = slot % CONTAINER_GRID_COLUMNS;
        const int row = slot / CONTAINER_GRID_COLUMNS;
        const int slotX = containerGridX +
            col * (CONTAINER_SLOT_SIZE + CONTAINER_SLOT_GAP);
        const int slotY = INVENTORY_CONTENT_Y +
            row * (CONTAINER_SLOT_SIZE + CONTAINER_SLOT_GAP);

        const std::string slotComponentId =
            containerSlotId(static_cast<size_t>(slot));
        ui.createButton(slotComponentId,
                        {slotX, slotY, CONTAINER_SLOT_SIZE, CONTAINER_SLOT_SIZE},
                        "", INVENTORY_PANEL_ID);
        ui.enableAutoSizing(slotComponentId, false);
        ui.setStyle(slotComponentId, slotStyle);
        ui.setComponentPositioning(
            slotComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX, CONTAINER_SLOT_SIZE, panelHalfWidth),
             centerOffset(slotY, CONTAINER_SLOT_SIZE, panelHalfHeight),
             CONTAINER_SLOT_SIZE, CONTAINER_SLOT_SIZE});
        ui.setOnClick(slotComponentId, [this, slot]() {
            handleContainerSlotClicked(static_cast<size_t>(slot));
        });

        const std::string iconComponentId =
            containerIconId(static_cast<size_t>(slot));
        ui.createAtlasImage(
            iconComponentId,
            {slotX + CONTAINER_ICON_INSET, slotY + CONTAINER_ICON_INSET,
             CONTAINER_ICON_SIZE, CONTAINER_ICON_SIZE},
            "", UIRect{}, slotComponentId);
        ui.setComponentPositioning(
            iconComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX + CONTAINER_ICON_INSET, CONTAINER_ICON_SIZE, panelHalfWidth),
             centerOffset(slotY + CONTAINER_ICON_INSET, CONTAINER_ICON_SIZE, panelHalfHeight),
             CONTAINER_ICON_SIZE,
             CONTAINER_ICON_SIZE});

        const std::string countComponentId =
            containerCountId(static_cast<size_t>(slot));
        ui.createLabel(countComponentId,
                       {slotX + INVENTORY_COUNT_INSET_X,
                        slotY + CONTAINER_SLOT_SIZE - INVENTORY_COUNT_BOTTOM_INSET,
                        CONTAINER_SLOT_SIZE - (INVENTORY_COUNT_INSET_X * 2),
                        INVENTORY_COUNT_HEIGHT},
                       "", slotComponentId);
        ui.enableAutoSizing(countComponentId, false);
        ui.setStyle(countComponentId, countStyle);
        ui.setComponentPositioning(
            countComponentId,
            {UIPositionMode::CENTERED_BOTH,
             centerOffset(slotX + INVENTORY_COUNT_INSET_X,
                          CONTAINER_SLOT_SIZE - (INVENTORY_COUNT_INSET_X * 2),
                          panelHalfWidth),
             centerOffset(slotY + CONTAINER_SLOT_SIZE - INVENTORY_COUNT_BOTTOM_INSET,
                          INVENTORY_COUNT_HEIGHT,
                          panelHalfHeight),
             CONTAINER_SLOT_SIZE - (INVENTORY_COUNT_INSET_X * 2),
             INVENTORY_COUNT_HEIGHT});
    }

    const int lootAllX = CONTAINER_SECTION_X +
        ((CONTAINER_SECTION_WIDTH - CONTAINER_LOOT_ALL_WIDTH) / 2);
    ui.createButton(CONTAINER_LOOT_ALL_ID,
                    {lootAllX, CONTAINER_LOOT_ALL_Y,
                     CONTAINER_LOOT_ALL_WIDTH, CONTAINER_LOOT_ALL_HEIGHT},
                    "Loot All", INVENTORY_PANEL_ID);
    ui.enableAutoSizing(CONTAINER_LOOT_ALL_ID, false);
    ui.setComponentPositioning(
        CONTAINER_LOOT_ALL_ID,
        {UIPositionMode::CENTERED_BOTH,
         centerOffset(lootAllX, CONTAINER_LOOT_ALL_WIDTH, panelHalfWidth),
         centerOffset(CONTAINER_LOOT_ALL_Y, CONTAINER_LOOT_ALL_HEIGHT, panelHalfHeight),
         CONTAINER_LOOT_ALL_WIDTH, CONTAINER_LOOT_ALL_HEIGHT});
    ui.setOnClick(CONTAINER_LOOT_ALL_ID, [this]() { handleLootAllClicked(); });

    m_inventoryUICreated = true;
    setContainerComponentsVisible(false);
    refreshInventoryUI();
    ui.setComponentVisible(INVENTORY_PANEL_ID, false);
}

void InventoryController::refreshInventoryUI() {
    if (!m_inventoryUICreated) {
        return;
    }

    auto player = mp_player.lock();
    if (!player) {
        return;
    }

    auto& ui = UIManager::Instance();
    const uint32_t invIdx = player->getInventoryIndex();
    if (invIdx == INVALID_INVENTORY_INDEX) {
        ui.setText(INVENTORY_STATUS_ID, "Capacity: 0/0");
        for (size_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
            refreshSlot(i, std::nullopt);
        }
        return;
    }

    auto& edm = EntityDataManager::Instance();
    const auto& inv = edm.getInventoryData(invIdx);
    ui.setText(INVENTORY_STATUS_ID,
               std::format("Capacity: {}/{}", inv.usedSlots, inv.maxSlots));

    std::array<InventorySlotData, INVENTORY_SLOT_COUNT> inventorySlots{};
    const size_t copiedSlotCount = edm.getInventorySlots(invIdx, inventorySlots);

    m_gridEntries.clear();
    m_gridEntries.reserve(INVENTORY_SLOT_COUNT);

    for (size_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (i >= copiedSlotCount) {
            m_gridEntries.push_back({});
            continue;
        }

        const InventorySlotData& slot = inventorySlots[i];
        if (slot.isEmpty()) {
            m_gridEntries.push_back({});
            continue;
        }

        auto resourceTemplate =
            ResourceTemplateManager::Instance().getResourceTemplate(slot.resourceHandle);
        m_gridEntries.push_back({
            .name = resourceTemplate ? resourceTemplate->getName() : slot.resourceHandle.toString(),
            .handle = slot.resourceHandle,
            .quantity = slot.quantity});
    }

    for (size_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        const auto& entry = m_gridEntries[i];
        if (entry.handle.isValid() && entry.quantity > 0) {
            refreshSlot(i, std::cref(entry));
        } else {
            refreshSlot(i, std::nullopt);
        }
    }
    refreshGearUI();
    refreshContainerUI();
}

void InventoryController::toggleInventoryDisplay() {
    setInventoryVisible(!m_inventoryVisible);
}

void InventoryController::setInventoryVisible(bool visible) {
    m_inventoryVisible = visible;
    UIManager::Instance().setComponentVisible(INVENTORY_PANEL_ID, visible);
    if (!visible) {
        closeOpenContainer();
        cancelHotbarAssignment();
        m_draggedInventorySourceSlot = -1;
        m_draggedInventoryHandle = VoidLight::ResourceHandle{};
        m_draggedContainerSourceSlot = -1;
        m_draggedContainerHandle = VoidLight::ResourceHandle{};
    }
    if (visible) {
        setContainerComponentsVisible(hasOpenContainer());
        refreshInventoryUI();
    }
}

void InventoryController::handleHotbarAssignmentInput(HudController& hudController) {
    auto& input = InputManager::Instance();
    const bool leftMouseDown = input.getMouseButtonState(LEFT);
    const bool mouseJustPressed = leftMouseDown && !m_leftMouseWasDown;
    const bool mouseJustReleased = !leftMouseDown && m_leftMouseWasDown;

    if (mouseJustPressed) {
        bool mousePressHandled = false;
        if (m_inventoryVisible) {
            const int inventorySlot = findInventorySlotAtMouse();
            if (inventorySlot >= 0) {
                startInventorySlotDrag(static_cast<size_t>(inventorySlot));
                mousePressHandled = true;
            } else {
                const int containerSlot = findContainerSlotAtMouse();
                if (containerSlot >= 0) {
                    startContainerSlotDrag(static_cast<size_t>(containerSlot));
                    mousePressHandled = true;
                }
            }

            if (!mousePressHandled && m_pendingHotbarAssignment.isValid()) {
                const int hotbarSlot = findHotbarSlotAtMouse();
                if (hotbarSlot >= 0 &&
                    hudController.assignHotbarItem(static_cast<size_t>(hotbarSlot),
                                                   m_pendingHotbarAssignment)) {
                    UIManager::Instance().addEventLogEntry(
                        EVENT_LOG_ID,
                        std::format("Assigned {} to hotbar {}",
                                    displayNameFor(m_pendingHotbarAssignment),
                                    hotbarSlot + 1));
                    cancelHotbarAssignment();
                    mousePressHandled = true;
                }
            }
        }

        if (!mousePressHandled && !m_draggingHotbarAssignment &&
            !m_pendingHotbarAssignment.isValid()) {
            const int hotbarSlot = findHotbarSlotAtMouse();
            if (hotbarSlot >= 0) {
                const auto assigned = hudController.getHotbarItem(static_cast<size_t>(hotbarSlot));
                if (assigned.isValid()) {
                    m_draggingHotbarAssignment = true;
                    m_draggedHotbarAssignment = assigned;
                    m_draggedHotbarSourceSlot = hotbarSlot;
                }
            }
        }
    }

    if (m_draggingHotbarAssignment && m_draggedHotbarAssignment.isValid() && leftMouseDown) {
        updateDragGhost(m_draggedHotbarAssignment, true);
    } else if (m_draggedInventoryHandle.isValid() && leftMouseDown) {
        updateDragGhost(m_draggedInventoryHandle, true);
    } else if (m_draggedContainerHandle.isValid() && leftMouseDown) {
        updateDragGhost(m_draggedContainerHandle, true);
    }

    if (mouseJustReleased) {
        bool completedDrop = false;
        const int releasedInventorySlot = findInventorySlotAtMouse();
        const int releasedContainerSlot = findContainerSlotAtMouse();

        if (m_draggedContainerSourceSlot >= 0 && m_draggedContainerHandle.isValid()) {
            if (releasedInventorySlot >= 0) {
                completedDrop = finishContainerToInventoryTransfer();
                if (completedDrop) {
                    UIManager::Instance().addEventLogEntry(
                        EVENT_LOG_ID,
                        std::format("Looted {}", displayNameFor(m_draggedContainerHandle)));
                }
            } else {
                const int hotbarSlot = findHotbarSlotAtMouse();
                if (hotbarSlot >= 0) {
                    completedDrop = finishContainerToHotbarTransfer(
                        hudController, static_cast<size_t>(hotbarSlot));
                    if (completedDrop) {
                        UIManager::Instance().addEventLogEntry(
                            EVENT_LOG_ID,
                            std::format("Assigned {} to hotbar {}",
                                        displayNameFor(m_draggedContainerHandle),
                                        hotbarSlot + 1));
                    }
                }
            }
        }

        if (!completedDrop && m_draggedInventorySourceSlot >= 0 &&
            m_draggedInventoryHandle.isValid()) {
            if (releasedContainerSlot >= 0) {
                completedDrop = finishInventoryToContainerTransfer();
                if (completedDrop) {
                    UIManager::Instance().addEventLogEntry(
                        EVENT_LOG_ID,
                        std::format("Stored {}", displayNameFor(m_draggedInventoryHandle)));
                }
            } else if (releasedInventorySlot >= 0) {
                finishInventorySlotDrag();
                completedDrop = true;
            }
        }

        if (!completedDrop && m_draggingHotbarAssignment &&
            m_draggedHotbarAssignment.isValid()) {
            const int hotbarSlot = findHotbarSlotAtMouse();
            if (hotbarSlot >= 0) {
                const bool hotbarUpdated = m_draggedHotbarSourceSlot >= 0
                    ? hudController.moveHotbarItem(
                          static_cast<size_t>(m_draggedHotbarSourceSlot),
                          static_cast<size_t>(hotbarSlot))
                    : hudController.assignHotbarItem(static_cast<size_t>(hotbarSlot),
                                                     m_draggedHotbarAssignment);
                if (hotbarUpdated) {
                    if (m_draggedHotbarSourceSlot >= 0 &&
                        m_draggedHotbarSourceSlot != hotbarSlot) {
                        UIManager::Instance().addEventLogEntry(
                            EVENT_LOG_ID,
                            std::format("Moved {} to hotbar {}",
                                        displayNameFor(m_draggedHotbarAssignment),
                                        hotbarSlot + 1));
                    } else if (m_draggedHotbarSourceSlot < 0) {
                        UIManager::Instance().addEventLogEntry(
                            EVENT_LOG_ID,
                            std::format("Assigned {} to hotbar {}",
                                        displayNameFor(m_draggedHotbarAssignment),
                                        hotbarSlot + 1));
                    }
                    m_pendingHotbarAssignment = VoidLight::ResourceHandle{};
                    completedDrop = true;
                }
            }
        }

        if (completedDrop) {
            refreshInventoryUI();
        }

        if (m_draggedInventorySourceSlot >= 0 && releasedInventorySlot < 0 && !completedDrop) {
            m_pendingHotbarAssignment = VoidLight::ResourceHandle{};
        }

        m_draggingHotbarAssignment = false;
        m_draggedHotbarAssignment = VoidLight::ResourceHandle{};
        m_draggedHotbarSourceSlot = -1;
        m_draggedInventorySourceSlot = -1;
        m_draggedInventoryHandle = VoidLight::ResourceHandle{};
        m_draggedContainerSourceSlot = -1;
        m_draggedContainerHandle = VoidLight::ResourceHandle{};
        updateDragGhost(VoidLight::ResourceHandle{}, false);
    }

    m_leftMouseWasDown = leftMouseDown;
}

void InventoryController::onResourceChange(const EventData& data) {
    const auto* event = dynamic_cast<const ResourceChangeEvent*>(data.event.get());
    if (!event) {
        return;
    }

    auto player = mp_player.lock();
    if (!player || event->getOwnerHandle() != player->getHandle()) {
        return;
    }

    refreshInventoryUI();
    addInventoryEventLogEntry(event->getResourceHandle(), event->getQuantityChange());
}

void InventoryController::addInventoryEventLogEntry(const VoidLight::ResourceHandle& handle,
                                                    int delta) {
    if (delta == 0) {
        return;
    }

    const auto& rtm = ResourceTemplateManager::Instance();
    auto resourceTemplate = rtm.getResourceTemplate(handle);
    const std::string displayName =
        resourceTemplate ? resourceTemplate->getName() : handle.toString();

    m_resourceNameBuffer = delta > 0
        ? std::format("+{} {}", delta, displayName)
        : std::format("{} {}", delta, displayName);
    UIManager::Instance().addEventLogEntry(EVENT_LOG_ID, m_resourceNameBuffer);
}

void InventoryController::refreshSlot(size_t slotIndex,
                                      InventoryEntryView entry) {
    auto& ui = UIManager::Instance();
    const std::string iconComponentId = iconId(slotIndex);
    const std::string countComponentId = countId(slotIndex);

    if (!entry.has_value()) {
        ui.setImageSource(iconComponentId, TextureSource{});
        ui.setText(countComponentId, "");
        return;
    }

    const auto& gridEntry = entry->get();
    ui.setImageSource(
        iconComponentId,
        ResourceTemplateManager::Instance().getIconTextureSource(gridEntry.handle));
    ui.setText(countComponentId, std::format("{}", gridEntry.quantity));
}

void InventoryController::refreshContainerUI() {
    if (!m_inventoryUICreated) {
        return;
    }

    auto& ui = UIManager::Instance();
    if (!hasOpenContainer()) {
        setContainerComponentsVisible(false);
        m_containerEntries.clear();
        for (size_t i = 0; i < CONTAINER_SLOT_COUNT; ++i) {
            refreshContainerSlot(i, std::nullopt);
        }
        return;
    }

    auto& edm = EntityDataManager::Instance();
    if (!edm.isValidInventoryIndex(m_openContainerInventoryIndex)) {
        closeOpenContainer();
        return;
    }

    setContainerComponentsVisible(m_inventoryVisible);
    const auto& inv = edm.getInventoryData(m_openContainerInventoryIndex);
    ui.setText(CONTAINER_STATUS_ID,
               std::format("Chest: {}/{}", inv.usedSlots, inv.maxSlots));

    std::array<InventorySlotData, CONTAINER_SLOT_COUNT> containerSlots{};
    const size_t copiedSlotCount =
        edm.getInventorySlots(m_openContainerInventoryIndex, containerSlots);

    m_containerEntries.clear();
    m_containerEntries.reserve(CONTAINER_SLOT_COUNT);

    for (size_t i = 0; i < CONTAINER_SLOT_COUNT; ++i) {
        if (i >= copiedSlotCount) {
            m_containerEntries.push_back({});
            continue;
        }

        const InventorySlotData& slot = containerSlots[i];
        if (slot.isEmpty()) {
            m_containerEntries.push_back({});
            continue;
        }

        auto resourceTemplate =
            ResourceTemplateManager::Instance().getResourceTemplate(slot.resourceHandle);
        m_containerEntries.push_back({
            .name = resourceTemplate ? resourceTemplate->getName() : slot.resourceHandle.toString(),
            .handle = slot.resourceHandle,
            .quantity = slot.quantity});
    }

    for (size_t i = 0; i < CONTAINER_SLOT_COUNT; ++i) {
        const auto& entry = m_containerEntries[i];
        if (entry.handle.isValid() && entry.quantity > 0) {
            refreshContainerSlot(i, std::cref(entry));
        } else {
            refreshContainerSlot(i, std::nullopt);
        }
    }
}

void InventoryController::refreshContainerSlot(size_t slotIndex,
                                               InventoryEntryView entry) {
    auto& ui = UIManager::Instance();
    const std::string iconComponentId = containerIconId(slotIndex);
    const std::string countComponentId = containerCountId(slotIndex);

    if (!entry.has_value()) {
        ui.setImageSource(iconComponentId, TextureSource{});
        ui.setText(countComponentId, "");
        return;
    }

    const auto& gridEntry = entry->get();
    ui.setImageSource(
        iconComponentId,
        ResourceTemplateManager::Instance().getIconTextureSource(gridEntry.handle));
    ui.setText(countComponentId, std::format("{}", gridEntry.quantity));
}

void InventoryController::refreshGearUI() {
    const auto& slots = gearSlots();
    for (size_t i = 0; i < slots.size(); ++i) {
        refreshGearSlot(i);
    }
}

void InventoryController::refreshGearSlot(size_t slotIndex) {
    auto player = mp_player.lock();
    const auto& slots = gearSlots();
    if (!player || slotIndex >= slots.size()) {
        return;
    }

    auto& ui = UIManager::Instance();
    const std::string iconComponentId = gearIconId(slotIndex);
    const std::string labelComponentId = gearLabelId(slotIndex);
    const VoidLight::ResourceHandle equipped =
        player->getEquippedItem(std::string(slots[slotIndex].id));

    if (!equipped.isValid()) {
        ui.setImageSource(iconComponentId, TextureSource{});
        ui.setText(labelComponentId, std::format("{}: Empty", slots[slotIndex].label));
        return;
    }

    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(equipped);
    ui.setImageSource(
        iconComponentId,
        ResourceTemplateManager::Instance().getIconTextureSource(equipped));

    ui.setText(labelComponentId,
               std::format("{}: {}", slots[slotIndex].label,
                           resourceTemplate ? resourceTemplate->getName()
                                            : equipped.toString()));
}

void InventoryController::handleInventorySlotClicked(size_t slotIndex) {
    if (slotIndex >= m_gridEntries.size()) {
        return;
    }

    const VoidLight::ResourceHandle itemHandle = m_gridEntries[slotIndex].handle;
    if (!itemHandle.isValid()) {
        return;
    }

    if (isHotbarAssignable(itemHandle)) {
        m_pendingHotbarAssignment = itemHandle;
        UIManager::Instance().addEventLogEntry(
            EVENT_LOG_ID,
            std::format("Select a hotbar slot for {}", displayNameFor(itemHandle)));
        return;
    }

    if (!isEquipment(itemHandle)) {
        return;
    }

    auto player = mp_player.lock();
    if (!player || !player->equipItem(itemHandle)) {
        return;
    }

    refreshInventoryUI();
    UIManager::Instance().addEventLogEntry(
        EVENT_LOG_ID, std::format("Equipped {}", displayNameFor(itemHandle)));
}

void InventoryController::handleContainerSlotClicked(size_t slotIndex) {
    if (slotIndex >= m_containerEntries.size()) {
        return;
    }

    const auto entry = m_containerEntries[slotIndex];
    if (!entry.handle.isValid() || entry.quantity <= 0 ||
        !transferContainerItemToPlayer(entry)) {
        return;
    }

    refreshInventoryUI();
    UIManager::Instance().addEventLogEntry(
        EVENT_LOG_ID, std::format("Looted {}", displayNameFor(entry.handle)));
}

void InventoryController::handleGearSlotClicked(size_t slotIndex) {
    auto player = mp_player.lock();
    const auto& slots = gearSlots();
    if (!player || slotIndex >= slots.size()) {
        return;
    }

    const std::string slotName(slots[slotIndex].id);
    const VoidLight::ResourceHandle equipped = player->getEquippedItem(slotName);
    if (!equipped.isValid() || !player->unequipItem(slotName)) {
        return;
    }

    refreshInventoryUI();
    UIManager::Instance().addEventLogEntry(
        EVENT_LOG_ID, std::format("Unequipped {}", displayNameFor(equipped)));
}

void InventoryController::handleLootAllClicked() {
    if (!lootAllFromOpenContainer()) {
        return;
    }

    refreshInventoryUI();
    UIManager::Instance().addEventLogEntry(EVENT_LOG_ID, "Looted chest");
}

bool InventoryController::startHotbarAssignment(size_t slotIndex) {
    if (slotIndex >= m_gridEntries.size()) {
        return false;
    }

    const VoidLight::ResourceHandle itemHandle = m_gridEntries[slotIndex].handle;
    if (!itemHandle.isValid()) {
        return false;
    }

    if (!isHotbarAssignable(itemHandle)) {
        return false;
    }

    m_pendingHotbarAssignment = itemHandle;
    return true;
}

void InventoryController::cancelHotbarAssignment() {
    m_pendingHotbarAssignment = VoidLight::ResourceHandle{};
    m_draggedHotbarAssignment = VoidLight::ResourceHandle{};
    m_draggingHotbarAssignment = false;
    m_draggedHotbarSourceSlot = -1;
    m_draggedInventorySourceSlot = -1;
    m_draggedInventoryHandle = VoidLight::ResourceHandle{};
    m_draggedContainerSourceSlot = -1;
    m_draggedContainerHandle = VoidLight::ResourceHandle{};
    updateDragGhost(VoidLight::ResourceHandle{}, false);
}

void InventoryController::cancelDragOperation() {
    cancelHotbarAssignment();
}

void InventoryController::updateDragGhost(const VoidLight::ResourceHandle& handle,
                                          bool visible) {
    auto& ui = UIManager::Instance();
    if (!m_dragGhostCreated) {
        ui.createAtlasImage(HOTBAR_DRAG_GHOST_ID,
                            {0, 0, HOTBAR_DRAG_GHOST_SIZE, HOTBAR_DRAG_GHOST_SIZE},
                            "", UIRect{});
        ui.setComponentZOrder(HOTBAR_DRAG_GHOST_ID, 1000);
        ui.setComponentVisible(HOTBAR_DRAG_GHOST_ID, false);
        m_dragGhostCreated = true;
    }

    if (!visible || !handle.isValid()) {
        ui.setComponentVisible(HOTBAR_DRAG_GHOST_ID, false);
        return;
    }

    const Vector2D& mousePos = InputManager::Instance().getMousePosition();
    const float uiScale = std::max(ui.getGlobalScale(), 0.01f);
    ui.setComponentBounds(
        HOTBAR_DRAG_GHOST_ID,
        UIRect{static_cast<int>((mousePos.getX() / uiScale) - (HOTBAR_DRAG_GHOST_SIZE * 0.5f)),
               static_cast<int>((mousePos.getY() / uiScale) - (HOTBAR_DRAG_GHOST_SIZE * 0.5f)),
               HOTBAR_DRAG_GHOST_SIZE,
               HOTBAR_DRAG_GHOST_SIZE});

    ui.setImageSource(
        HOTBAR_DRAG_GHOST_ID,
        ResourceTemplateManager::Instance().getIconTextureSource(handle));
    ui.setComponentVisible(HOTBAR_DRAG_GHOST_ID, true);
}

bool InventoryController::startInventorySlotDrag(size_t slotIndex) {
    if (slotIndex >= m_gridEntries.size()) {
        return false;
    }

    const VoidLight::ResourceHandle itemHandle = m_gridEntries[slotIndex].handle;
    if (!itemHandle.isValid()) {
        return false;
    }

    m_draggedInventorySourceSlot = static_cast<int>(slotIndex);
    m_draggedInventoryHandle = itemHandle;

    if (startHotbarAssignment(slotIndex)) {
        m_draggingHotbarAssignment = true;
        m_draggedHotbarAssignment = m_pendingHotbarAssignment;
        m_draggedHotbarSourceSlot = -1;
    }

    return true;
}

bool InventoryController::startContainerSlotDrag(size_t slotIndex) {
    if (slotIndex >= m_containerEntries.size()) {
        return false;
    }

    const VoidLight::ResourceHandle itemHandle = m_containerEntries[slotIndex].handle;
    if (!itemHandle.isValid()) {
        return false;
    }

    m_draggedContainerSourceSlot = static_cast<int>(slotIndex);
    m_draggedContainerHandle = itemHandle;
    return true;
}

void InventoryController::finishInventorySlotDrag() {
    auto player = mp_player.lock();
    if (!player || m_draggedInventorySourceSlot < 0) {
        return;
    }

    const uint32_t invIdx = player->getInventoryIndex();
    if (invIdx == INVALID_INVENTORY_INDEX) {
        return;
    }

    const int targetSlot = findInventorySlotAtMouse();
    if (targetSlot < 0) {
        return;
    }

    if (EntityDataManager::Instance().swapInventorySlots(
            invIdx,
            static_cast<size_t>(m_draggedInventorySourceSlot),
            static_cast<size_t>(targetSlot))) {
        if (targetSlot != m_draggedInventorySourceSlot) {
            m_pendingHotbarAssignment = VoidLight::ResourceHandle{};
        }
        refreshInventoryUI();
    }
}

bool InventoryController::finishContainerToInventoryTransfer() {
    if (m_draggedContainerSourceSlot < 0 ||
        static_cast<size_t>(m_draggedContainerSourceSlot) >= m_containerEntries.size()) {
        return false;
    }

    const auto entry = m_containerEntries[static_cast<size_t>(m_draggedContainerSourceSlot)];
    return transferContainerItemToPlayer(entry);
}

bool InventoryController::finishInventoryToContainerTransfer() {
    if (m_draggedInventorySourceSlot < 0 ||
        static_cast<size_t>(m_draggedInventorySourceSlot) >= m_gridEntries.size()) {
        return false;
    }

    const auto entry = m_gridEntries[static_cast<size_t>(m_draggedInventorySourceSlot)];
    return transferPlayerItemToContainer(entry);
}

bool InventoryController::finishContainerToHotbarTransfer(
    HudController& hudController,
    size_t hotbarSlot) {
    if (m_draggedContainerSourceSlot < 0 ||
        static_cast<size_t>(m_draggedContainerSourceSlot) >= m_containerEntries.size()) {
        return false;
    }

    const auto entry = m_containerEntries[static_cast<size_t>(m_draggedContainerSourceSlot)];
    if (!isHotbarAssignable(entry.handle) || !transferContainerItemToPlayer(entry)) {
        return false;
    }

    return hudController.assignHotbarItem(hotbarSlot, entry.handle);
}

bool InventoryController::lootAllFromOpenContainer() {
    if (!hasOpenContainer()) {
        return false;
    }

    const auto entries = m_containerEntries;
    bool transferredAny = false;
    for (const auto& entry : entries) {
        if (!entry.handle.isValid() || entry.quantity <= 0) {
            continue;
        }

        transferredAny = transferContainerItemToPlayer(entry) || transferredAny;
    }

    return transferredAny;
}

bool InventoryController::transferContainerItemToPlayer(
    const InventoryGridEntry& entry) {
    auto player = mp_player.lock();
    if (!player || !hasOpenContainer() || !entry.handle.isValid() ||
        entry.quantity <= 0) {
        return false;
    }

    auto& edm = EntityDataManager::Instance();
    const auto transfer = edm.transferInventoryItem(
        m_openContainerInventoryIndex,
        player->getInventoryIndex(),
        entry.handle,
        entry.quantity);
    if (!transfer) {
        return false;
    }

    dispatchPlayerResourceChange(transfer->targetChange, "container_loot");
    if (edm.getInventoryData(m_openContainerInventoryIndex).usedSlots == 0) {
        const auto& hot = edm.getStaticHotDataByIndex(m_openContainerStaticIndex);
        edm.getContainerData(hot.typeLocalIndex).setLooted(true);
    }
    return true;
}

bool InventoryController::transferPlayerItemToContainer(
    const InventoryGridEntry& entry) {
    auto player = mp_player.lock();
    if (!player || !hasOpenContainer() || !entry.handle.isValid() ||
        entry.quantity <= 0) {
        return false;
    }

    auto& edm = EntityDataManager::Instance();
    const auto transfer = edm.transferInventoryItem(
        player->getInventoryIndex(),
        m_openContainerInventoryIndex,
        entry.handle,
        entry.quantity);
    if (!transfer) {
        return false;
    }

    dispatchPlayerResourceChange(transfer->sourceChange, "container_store");
    return true;
}

void InventoryController::closeOpenContainer() {
    if (hasOpenContainer()) {
        auto& edm = EntityDataManager::Instance();
        if (m_openContainerStaticIndex < edm.getStaticHotDataArray().size()) {
            const auto& hot = edm.getStaticHotDataByIndex(m_openContainerStaticIndex);
            if (hot.isAlive() && hot.kind == EntityKind::Container) {
                edm.getContainerData(hot.typeLocalIndex).setOpen(false);
            }
        }
    }

    m_openContainerInventoryIndex = NO_OPEN_CONTAINER_INVENTORY;
    m_openContainerStaticIndex = SIZE_MAX;
    if (m_draggedContainerHandle.isValid()) {
        updateDragGhost(VoidLight::ResourceHandle{}, false);
    }
    m_draggedContainerSourceSlot = -1;
    m_draggedContainerHandle = VoidLight::ResourceHandle{};
    m_containerEntries.clear();
    setContainerComponentsVisible(false);
}

void InventoryController::setContainerComponentsVisible(bool visible) {
    auto& ui = UIManager::Instance();
    ui.setComponentVisible(CONTAINER_HEADER_ID, visible);
    ui.setComponentVisible(CONTAINER_STATUS_ID, visible);
    ui.setComponentVisible(CONTAINER_LOOT_ALL_ID, visible);
    for (size_t i = 0; i < CONTAINER_SLOT_COUNT; ++i) {
        ui.setComponentVisible(containerSlotId(i), visible);
    }
}

bool InventoryController::hasOpenContainer() const {
    return m_openContainerInventoryIndex != NO_OPEN_CONTAINER_INVENTORY &&
        m_openContainerStaticIndex != SIZE_MAX;
}

void InventoryController::dispatchPlayerResourceChange(
    const InventoryResourceChange& change,
    const std::string& reason) const {
    if (!change.isValid()) {
        return;
    }

    auto player = mp_player.lock();
    if (!player) {
        return;
    }

    EventManager::Instance().triggerResourceChange(
        player->getHandle(),
        change.resourceHandle,
        change.oldQuantity,
        change.newQuantity,
        reason,
        EventManager::DispatchMode::Deferred);
}

int InventoryController::findInventorySlotAtMouse() const {
    if (!m_inventoryUICreated || !m_inventoryVisible) {
        return -1;
    }

    const Vector2D& mousePos = InputManager::Instance().getMousePosition();
    const int mouseX = static_cast<int>(mousePos.getX());
    const int mouseY = static_cast<int>(mousePos.getY());
    auto& ui = UIManager::Instance();

    for (size_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        const UIRect bounds = ui.getBounds(slotId(i));
        if (bounds.contains(mouseX, mouseY)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int InventoryController::findContainerSlotAtMouse() const {
    if (!m_inventoryUICreated || !m_inventoryVisible || !hasOpenContainer()) {
        return -1;
    }

    const Vector2D& mousePos = InputManager::Instance().getMousePosition();
    const int mouseX = static_cast<int>(mousePos.getX());
    const int mouseY = static_cast<int>(mousePos.getY());
    auto& ui = UIManager::Instance();

    for (size_t i = 0; i < CONTAINER_SLOT_COUNT; ++i) {
        const UIRect bounds = ui.getBounds(containerSlotId(i));
        if (bounds.contains(mouseX, mouseY)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int InventoryController::findHotbarSlotAtMouse() const {
    const Vector2D& mousePos = InputManager::Instance().getMousePosition();
    const int mouseX = static_cast<int>(mousePos.getX());
    const int mouseY = static_cast<int>(mousePos.getY());
    auto& ui = UIManager::Instance();

    for (size_t i = 0; i < HudController::HOTBAR_SLOT_COUNT; ++i) {
        const UIRect bounds = ui.getBounds(HudController::hotbarSlotId(i));
        if (bounds.contains(mouseX, mouseY)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool InventoryController::isEquipment(const VoidLight::ResourceHandle& handle) {
    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(handle);
    return resourceTemplate && resourceTemplate->getType() == ResourceType::Equipment;
}

bool InventoryController::isWeapon(const VoidLight::ResourceHandle& handle) {
    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(handle);
    auto equipment = std::dynamic_pointer_cast<Equipment>(resourceTemplate);
    return equipment &&
        equipment->getEquipmentSlot() == Equipment::EquipmentSlot::Weapon;
}

bool InventoryController::isHotbarAssignable(const VoidLight::ResourceHandle& handle) {
    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(handle);
    if (!resourceTemplate) {
        return false;
    }

    if (resourceTemplate->getType() == ResourceType::Ammunition) {
        return false;
    }

    return resourceTemplate->isConsumable() ||
        resourceTemplate->getType() == ResourceType::Consumable ||
        isWeapon(handle);
}

std::string InventoryController::displayNameFor(const VoidLight::ResourceHandle& handle) {
    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(handle);
    return resourceTemplate ? resourceTemplate->getName() : handle.toString();
}

std::string InventoryController::slotId(size_t slotIndex) {
    return std::format("inventory_slot_{}", slotIndex);
}

std::string InventoryController::iconId(size_t slotIndex) {
    return std::format("inventory_icon_{}", slotIndex);
}

std::string InventoryController::countId(size_t slotIndex) {
    return std::format("inventory_count_{}", slotIndex);
}

std::string InventoryController::containerSlotId(size_t slotIndex) {
    return std::format("container_slot_{}", slotIndex);
}

std::string InventoryController::containerIconId(size_t slotIndex) {
    return std::format("container_icon_{}", slotIndex);
}

std::string InventoryController::containerCountId(size_t slotIndex) {
    return std::format("container_count_{}", slotIndex);
}

std::string InventoryController::gearSlotId(size_t slotIndex) {
    return std::format("gear_slot_{}", slotIndex);
}

std::string InventoryController::gearIconId(size_t slotIndex) {
    return std::format("gear_icon_{}", slotIndex);
}

std::string InventoryController::gearLabelId(size_t slotIndex) {
    return std::format("gear_label_{}", slotIndex);
}
