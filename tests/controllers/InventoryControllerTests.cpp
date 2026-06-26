/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file InventoryControllerTests.cpp
 * @brief Tests for InventoryController
 *
 * Tests inventory pickup and UI synchronization.
 */

#define BOOST_TEST_MODULE InventoryControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/ui/HudController.hpp"
#include "controllers/ui/InventoryController.hpp"
#include "entities/Player.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "../events/EventManagerTestAccess.hpp"
#include <atomic>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <string_view>

// ============================================================================
// Test Fixture
// ============================================================================

namespace {

VoidLight::ResourceHandle getResourceHandleById(const std::string& id) {
    return ResourceTemplateManager::Instance().getHandleById(id);
}

void moveMouseTo(const UIRect& bounds) {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = static_cast<float>(bounds.x + (bounds.width / 2));
    event.motion.y = static_cast<float>(bounds.y + (bounds.height / 2));
    InputManager::Instance().onMouseMove(event);
}

void setLeftMouseDownAt(const UIRect& bounds, bool down) {
    SDL_Event event;
    SDL_zero(event);
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = static_cast<float>(bounds.x + (bounds.width / 2));
    event.button.y = static_cast<float>(bounds.y + (bounds.height / 2));
    if (down) {
        InputManager::Instance().onMouseButtonDown(event);
    } else {
        InputManager::Instance().onMouseButtonUp(event);
    }
}

bool sameRect(const UIRect& lhs, const UIRect& rhs) {
    return lhs.x == rhs.x &&
        lhs.y == rhs.y &&
        lhs.width == rhs.width &&
        lhs.height == rhs.height;
}

size_t findInventorySlotFor(const std::shared_ptr<Player>& player,
                            const VoidLight::ResourceHandle& handle) {
    const uint32_t invIdx = player ? player->getInventoryIndex() : INVALID_INVENTORY_INDEX;
    if (invIdx == INVALID_INVENTORY_INDEX) {
        return SIZE_MAX;
    }

    const auto& inv = EntityDataManager::Instance().getInventoryData(invIdx);
    for (size_t i = 0; i < inv.maxSlots; ++i) {
        const InventorySlotData slot =
            EntityDataManager::Instance().getInventorySlot(invIdx, i);
        if (!slot.isEmpty() && slot.resourceHandle == handle) {
            return i;
        }
    }

    return SIZE_MAX;
}

EntityHandle createChestWithContents(
    const std::shared_ptr<Player>& player,
    std::initializer_list<std::string_view> resourceIds) {
    auto& edm = EntityDataManager::Instance();
    EntityHandle chest = edm.createContainer(
        player->getPosition(), ContainerType::Chest, 12, 0, "test_world");
    BOOST_REQUIRE(chest.isValid());

    const uint32_t inventoryIndex = edm.getContainerData(chest).inventoryIndex;
    BOOST_REQUIRE_NE(inventoryIndex, INVALID_INVENTORY_INDEX);
    for (std::string_view resourceId : resourceIds) {
        const auto handle =
            ResourceTemplateManager::Instance().getHandleById(std::string(resourceId));
        BOOST_REQUIRE_MESSAGE(handle.isValid(), "Missing test resource");
        BOOST_REQUIRE(edm.addToInventory(inventoryIndex, handle, 1));
    }

    return chest;
}

} // namespace

class InventoryControllerTestFixture {
public:
    InventoryControllerTestFixture() {
        // Reset EventManager to clean state
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());

        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(UIManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        if (!WorldResourceManager::Instance().hasWorld("test_world")) {
            BOOST_REQUIRE(WorldResourceManager::Instance().createWorld("test_world"));
        }
        WorldResourceManager::Instance().setActiveWorld("test_world");

        player = std::make_shared<Player>();
        player->initializeInventory();
        BOOST_REQUIRE(player->getHandle().isValid());

        goldHandle = getResourceHandleById("gold_coins");
        BOOST_REQUIRE(goldHandle.isValid());
    }

    ~InventoryControllerTestFixture() {
        WorldResourceManager::Instance().clean();
        UIManager::Instance().cleanupForStateTransition();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }

    // Non-copyable
    InventoryControllerTestFixture(const InventoryControllerTestFixture&) = delete;
    InventoryControllerTestFixture& operator=(const InventoryControllerTestFixture&) = delete;

protected:
    std::shared_ptr<Player> player;
    VoidLight::ResourceHandle goldHandle;
};

// ============================================================================
// Basic State Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(InventoryControllerStateTests, InventoryControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestInventoryControllerName) {
    // Create with nullptr player
    InventoryController controller(nullptr);

    BOOST_CHECK_EQUAL(controller.getName(), "InventoryController");
}

BOOST_AUTO_TEST_CASE(TestAttemptPickupWithoutPlayer) {
    InventoryController controller(nullptr);

    // Should fail gracefully without player
    bool result = controller.attemptPickup();
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(TestSubscribeWithoutPlayer) {
    InventoryController controller(nullptr);

    // Subscribe should not crash
    controller.subscribe();

    // Should be marked as subscribed
    BOOST_CHECK(controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestConstants) {
    // Verify constants are reasonable
    BOOST_CHECK_GT(InventoryController::PICKUP_RADIUS, 0.0f);
    BOOST_CHECK_LT(InventoryController::PICKUP_RADIUS, 100.0f);

    // Check UI component IDs are not empty
    BOOST_CHECK(InventoryController::INVENTORY_PANEL_ID != nullptr);
    BOOST_CHECK(InventoryController::INVENTORY_STATUS_ID != nullptr);
    BOOST_CHECK(InventoryController::EVENT_LOG_ID != nullptr);
}

BOOST_AUTO_TEST_CASE(TestInitializeInventoryUICreatesReusableGrid) {
    InventoryController controller(player);

    controller.initializeInventoryUI();

    auto& ui = UIManager::Instance();
    BOOST_CHECK(ui.hasComponent(InventoryController::INVENTORY_PANEL_ID));
    BOOST_CHECK(ui.hasComponent(InventoryController::INVENTORY_TITLE_ID));
    BOOST_CHECK(ui.hasComponent(InventoryController::INVENTORY_STATUS_ID));
    BOOST_CHECK(ui.hasComponent("inventory_slot_0"));
    BOOST_CHECK(ui.hasComponent("inventory_icon_0"));
    BOOST_CHECK(ui.hasComponent("inventory_count_0"));
    BOOST_CHECK(ui.hasComponent("inventory_slot_19"));
    BOOST_CHECK(ui.hasComponent("inventory_icon_19"));
    BOOST_CHECK(ui.hasComponent("inventory_count_19"));
    BOOST_CHECK(ui.hasComponent("inventory_tab_items"));
    BOOST_CHECK(ui.hasComponent("inventory_tab_gear"));
    BOOST_CHECK(ui.hasComponent("gear_slot_0"));
    BOOST_CHECK(ui.hasComponent("gear_icon_0"));
    BOOST_CHECK(ui.hasComponent("gear_label_0"));
    BOOST_CHECK(ui.hasComponent("gear_slot_7"));
    BOOST_CHECK(ui.hasComponent("gear_icon_7"));
    BOOST_CHECK(ui.hasComponent("gear_label_7"));
    BOOST_CHECK(ui.hasComponent("gear_slot_8"));
    BOOST_CHECK(ui.hasComponent("gear_icon_8"));
    BOOST_CHECK(ui.hasComponent("gear_label_8"));
    BOOST_CHECK_EQUAL(ui.getText(InventoryController::INVENTORY_TITLE_ID), "Inventory");
    BOOST_CHECK_EQUAL(ui.getText("inventory_tab_items"), "Items");
    BOOST_CHECK_EQUAL(ui.getText("inventory_tab_gear"), "Gear");
    BOOST_CHECK_EQUAL(ui.getText("gear_label_0"), "Weapon: Empty");
    BOOST_CHECK_EQUAL(ui.getText("gear_label_1"), "Shield: Empty");

    controller.setInventoryVisible(true);
    BOOST_CHECK(controller.isInventoryVisible());

    controller.setInventoryVisible(false);
    BOOST_CHECK(!controller.isInventoryVisible());
}

BOOST_AUTO_TEST_CASE(TestInventoryGearLayoutHasReadableSpacingAt1280x720) {
    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    InventoryController controller(player);
    controller.initializeInventoryUI();

    const UIRect panelBounds = ui.getBounds(InventoryController::INVENTORY_PANEL_ID);
    const UIRect titleBounds = ui.getBounds(InventoryController::INVENTORY_TITLE_ID);
    const UIRect inventoryHeaderBounds = ui.getBounds("inventory_tab_items");
    const UIRect firstInventoryBounds = ui.getBounds("inventory_slot_0");
    const UIRect gearHeaderBounds = ui.getBounds("inventory_tab_gear");
    const UIRect firstGearBounds = ui.getBounds("gear_slot_0");
    const UIRect secondGearBounds = ui.getBounds("gear_slot_1");
    const UIRect firstGearLabelBounds = ui.getBounds("gear_label_0");
    const int panelCenterX = panelBounds.x + (panelBounds.width / 2);

    BOOST_CHECK_GE(panelBounds.width, 480);
    BOOST_CHECK_LT(titleBounds.width, panelBounds.width / 2);
    BOOST_CHECK_GE(firstGearBounds.width, 240);
    BOOST_CHECK_GE(firstGearLabelBounds.width, 200);
    BOOST_CHECK_GE(panelCenterX, 639);
    BOOST_CHECK_LE(panelCenterX, 641);
    BOOST_CHECK_LE(std::abs((titleBounds.x + (titleBounds.width / 2)) - panelCenterX), 1);
    BOOST_CHECK_EQUAL(inventoryHeaderBounds.x, firstInventoryBounds.x);
    BOOST_CHECK_EQUAL(gearHeaderBounds.x, firstGearBounds.x);
    BOOST_CHECK_GT(secondGearBounds.y, firstGearBounds.y + firstGearBounds.height);
    BOOST_CHECK_LE(panelBounds.x + panelBounds.width, 1280);
}

BOOST_AUTO_TEST_CASE(TestPlayerEquipUsesEquipmentSlotMetadata) {
    auto chestHandle = getResourceHandleById("dragon_scale_armor");
    BOOST_REQUIRE(chestHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(chestHandle, 1));

    BOOST_REQUIRE(player->equipItem(chestHandle));

    BOOST_CHECK(player->getEquippedItem("chest") == chestHandle);
    BOOST_CHECK(!player->getEquippedItem("weapon").isValid());
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), chestHandle),
        0);
}

BOOST_AUTO_TEST_CASE(TestUnknownEquipmentSlotDoesNotEquip) {
    auto unknownSlotHandle = ResourceTemplateManager::Instance().generateHandle();
    auto unknownSlotEquipment = std::make_shared<Equipment>(
        unknownSlotHandle,
        "unknown_slot_relic",
        "Unknown Slot Relic",
        Equipment::EquipmentSlot::COUNT);
    BOOST_REQUIRE(
        ResourceTemplateManager::Instance().registerResourceTemplate(unknownSlotEquipment));
    BOOST_REQUIRE(player->addToInventory(unknownSlotHandle, 1));

    BOOST_CHECK(!player->equipItem(unknownSlotHandle));
    BOOST_CHECK(!player->getEquippedItem("weapon").isValid());
    BOOST_CHECK(!player->getEquippedItem("unknown").isValid());
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), unknownSlotHandle),
        1);
}

BOOST_AUTO_TEST_CASE(TestInventorySlotClickEquipsAndGearSlotClickUnequips) {
    auto chestHandle = getResourceHandleById("dragon_scale_armor");
    BOOST_REQUIRE(chestHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(chestHandle, 1));

    InventoryController controller(player);
    controller.initializeInventoryUI();
    controller.setInventoryVisible(true);

    auto& ui = UIManager::Instance();
    const size_t chestSlot = findInventorySlotFor(player, chestHandle);
    BOOST_REQUIRE_NE(chestSlot, SIZE_MAX);
    ui.simulateClick("inventory_slot_" + std::to_string(chestSlot));
    ui.update(0.016f);

    BOOST_CHECK(player->getEquippedItem("chest") == chestHandle);
    BOOST_CHECK_EQUAL(ui.getText("gear_label_3").find("Dragon Scale Armor"), 7);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), chestHandle),
        0);

    ui.simulateClick("gear_slot_3");
    ui.update(0.016f);

    BOOST_CHECK(!player->getEquippedItem("chest").isValid());
    BOOST_CHECK(ui.getText("gear_label_3").find("Empty") != std::string::npos);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), chestHandle),
        1);
}

BOOST_AUTO_TEST_CASE(TestEquippingReplacementReturnsPreviousItemToInventory) {
    auto arcaneStaffHandle = getResourceHandleById("arcane_staff");
    auto daggerHandle = getResourceHandleById("dagger");
    BOOST_REQUIRE(arcaneStaffHandle.isValid());
    BOOST_REQUIRE(daggerHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(arcaneStaffHandle, 1));
    BOOST_REQUIRE(player->addToInventory(daggerHandle, 1));

    BOOST_REQUIRE(player->equipItem(arcaneStaffHandle));
    BOOST_REQUIRE(player->getEquippedItem("weapon") == arcaneStaffHandle);

    BOOST_REQUIRE(player->equipItem(daggerHandle));

    BOOST_CHECK(player->getEquippedItem("weapon") == daggerHandle);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), arcaneStaffHandle),
        1);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), daggerHandle),
        0);
}

BOOST_AUTO_TEST_CASE(TestWeaponInventoryClickStartsHotbarAssignmentInsteadOfEquip) {
    auto daggerHandle = getResourceHandleById("dagger");
    BOOST_REQUIRE(daggerHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(daggerHandle, 1));

    InventoryController controller(player);
    controller.initializeInventoryUI();
    controller.setInventoryVisible(true);

    auto& ui = UIManager::Instance();
    const size_t daggerSlot = findInventorySlotFor(player, daggerHandle);
    BOOST_REQUIRE_NE(daggerSlot, SIZE_MAX);
    ui.simulateClick("inventory_slot_" + std::to_string(daggerSlot));
    ui.update(0.016f);

    BOOST_CHECK(!player->getEquippedItem("weapon").isValid());
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), daggerHandle),
        1);
}

BOOST_AUTO_TEST_CASE(TestOpenNearbyContainerShowsFallbackIcon) {
    const auto oldShirt = getResourceHandleById("old_shirt");
    BOOST_REQUIRE(oldShirt.isValid());
    createChestWithContents(player, {"old_shirt"});

    InventoryController controller(player);
    controller.initializeInventoryUI();

    auto& ui = UIManager::Instance();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());

    BOOST_CHECK_EQUAL(ui.getText("container_status"), "Chest: 1/12");
    BOOST_CHECK_EQUAL(ui.getTexture("container_icon_0"), "default");
    BOOST_CHECK(sameRect(ui.getImageSourceRect("container_icon_0"), UIRect{}));
}

BOOST_AUTO_TEST_CASE(TestOpenNearbyContainerUsesExplicitNonAtlasIconTexture) {
    const auto bow = getResourceHandleById("bow");
    BOOST_REQUIRE(bow.isValid());
    createChestWithContents(player, {"bow"});

    InventoryController controller(player);
    controller.initializeInventoryUI();

    auto& ui = UIManager::Instance();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());

    BOOST_CHECK_EQUAL(ui.getText("container_status"), "Chest: 1/12");
    BOOST_CHECK_EQUAL(ui.getTexture("container_icon_0"), "bow_icon");
    BOOST_CHECK(sameRect(ui.getImageSourceRect("container_icon_0"), UIRect{}));
}

BOOST_AUTO_TEST_CASE(TestOpenContainerClosesWhenPlayerLeavesInteractionRange) {
    const auto oldShirt = getResourceHandleById("old_shirt");
    BOOST_REQUIRE(oldShirt.isValid());
    const EntityHandle chest = createChestWithContents(player, {"old_shirt"});
    auto& edm = EntityDataManager::Instance();
    const uint32_t chestInventory = edm.getContainerData(chest).inventoryIndex;

    InventoryController controller(player);
    controller.initializeInventoryUI();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());
    BOOST_REQUIRE(controller.isInventoryVisible());
    BOOST_REQUIRE(edm.getContainerData(chest).isOpen());

    player->setPosition(
        player->getPosition() + Vector2D(InventoryController::PICKUP_RADIUS + 8.0f, 0.0f));
    controller.update(0.016f);

    BOOST_CHECK(!edm.getContainerData(chest).isOpen());
    BOOST_CHECK(controller.isInventoryVisible());

    auto& ui = UIManager::Instance();
    ui.simulateClick("container_slot_0");
    ui.update(0.016f);

    BOOST_CHECK_EQUAL(player->getInventoryQuantity(oldShirt), 0);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(chestInventory, oldShirt), 1);
}

BOOST_AUTO_TEST_CASE(TestLootAllMovesContainerContentsToPlayerInventory) {
    const auto oldShirt = getResourceHandleById("old_shirt");
    const auto oldPants = getResourceHandleById("old_pants");
    BOOST_REQUIRE(oldShirt.isValid());
    BOOST_REQUIRE(oldPants.isValid());
    const EntityHandle chest = createChestWithContents(player, {"old_shirt", "old_pants"});
    auto& edm = EntityDataManager::Instance();
    const uint32_t chestInventory = edm.getContainerData(chest).inventoryIndex;

    InventoryController controller(player);
    controller.initializeInventoryUI();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());

    auto& ui = UIManager::Instance();
    ui.simulateClick("container_loot_all");
    ui.update(0.016f);

    BOOST_CHECK_EQUAL(player->getInventoryQuantity(oldShirt), 1);
    BOOST_CHECK_EQUAL(player->getInventoryQuantity(oldPants), 1);
    BOOST_CHECK_EQUAL(edm.getInventoryData(chestInventory).usedSlots, 0);
    BOOST_CHECK(edm.getContainerData(chest).isOpen());
    BOOST_CHECK(edm.getContainerData(chest).wasLooted());
}

BOOST_AUTO_TEST_CASE(TestContainerDragToInventoryTransfersStack) {
    const auto woodenShield = getResourceHandleById("wooden_shield");
    BOOST_REQUIRE(woodenShield.isValid());
    const EntityHandle chest = createChestWithContents(player, {"wooden_shield"});
    auto& edm = EntityDataManager::Instance();
    const uint32_t chestInventory = edm.getContainerData(chest).inventoryIndex;

    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();

    InventoryController controller(player);
    controller.initializeInventoryUI();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds("container_slot_0");
    const UIRect targetBounds = ui.getBounds("inventory_slot_4");

    setLeftMouseDownAt(sourceBounds, true);
    controller.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    controller.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    controller.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK_EQUAL(player->getInventoryQuantity(woodenShield), 1);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(chestInventory, woodenShield), 0);
}

BOOST_AUTO_TEST_CASE(TestContainerDragToHotbarTransfersThenAssigns) {
    const auto woodenSword = getResourceHandleById("wooden_sword");
    BOOST_REQUIRE(woodenSword.isValid());
    const EntityHandle chest = createChestWithContents(player, {"wooden_sword"});
    auto& edm = EntityDataManager::Instance();
    const uint32_t chestInventory = edm.getContainerData(chest).inventoryIndex;

    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();

    InventoryController controller(player);
    controller.initializeInventoryUI();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds("container_slot_0");
    const UIRect targetBounds = ui.getBounds(HudController::hotbarSlotId(0));

    setLeftMouseDownAt(sourceBounds, true);
    controller.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    controller.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    controller.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK(hudController.getHotbarItem(0) == woodenSword);
    BOOST_CHECK_EQUAL(player->getInventoryQuantity(woodenSword), 1);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(chestInventory, woodenSword), 0);
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_0"), "default");
}

BOOST_AUTO_TEST_CASE(TestInventoryDragToContainerStoresStack) {
    const auto oldShirt = getResourceHandleById("old_shirt");
    BOOST_REQUIRE(oldShirt.isValid());
    BOOST_REQUIRE(player->addToInventory(oldShirt, 1));
    const EntityHandle chest = createChestWithContents(player, {});
    auto& edm = EntityDataManager::Instance();
    const uint32_t chestInventory = edm.getContainerData(chest).inventoryIndex;

    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();

    InventoryController controller(player);
    controller.initializeInventoryUI();
    BOOST_REQUIRE(controller.tryOpenNearbyContainer());
    InputManager::Instance().reset();

    const size_t oldShirtSlot = findInventorySlotFor(player, oldShirt);
    BOOST_REQUIRE_NE(oldShirtSlot, SIZE_MAX);
    const UIRect sourceBounds =
        ui.getBounds("inventory_slot_" + std::to_string(oldShirtSlot));
    const UIRect targetBounds = ui.getBounds("container_slot_0");

    setLeftMouseDownAt(sourceBounds, true);
    controller.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    controller.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    controller.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK_EQUAL(player->getInventoryQuantity(oldShirt), 0);
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(chestInventory, oldShirt), 1);
}

BOOST_AUTO_TEST_CASE(TestAmmunitionIsNotConsumableOrManuallyConsumed) {
    auto arrowsHandle = getResourceHandleById("arrows");
    BOOST_REQUIRE(arrowsHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(arrowsHandle, 5));

    auto arrowsTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(arrowsHandle);
    BOOST_REQUIRE(arrowsTemplate);
    BOOST_CHECK_EQUAL(static_cast<int>(arrowsTemplate->getType()),
                      static_cast<int>(ResourceType::Ammunition));
    BOOST_CHECK(!arrowsTemplate->isConsumable());
    BOOST_CHECK(!player->consumeItem(arrowsHandle));
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), arrowsHandle),
        5);
}

BOOST_AUTO_TEST_CASE(TestFoodAndDrinkConsumableEffectsMatchCatalog) {
    auto breadHandle = getResourceHandleById("bread");
    auto aleHandle = getResourceHandleById("ale");
    auto wineHandle = getResourceHandleById("wine");
    BOOST_REQUIRE(breadHandle.isValid());
    BOOST_REQUIRE(aleHandle.isValid());
    BOOST_REQUIRE(wineHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(breadHandle, 1));
    BOOST_REQUIRE(player->addToInventory(aleHandle, 1));
    BOOST_REQUIRE(player->addToInventory(wineHandle, 1));

    player->takeDamage(30.0f);
    player->consumeStamina(30.0f);

    BOOST_REQUIRE(player->consumeItem(breadHandle));
    BOOST_CHECK_CLOSE(player->getHealth(), 80.0f, 0.001f);
    BOOST_CHECK_CLOSE(player->getStamina(), 70.0f, 0.001f);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), breadHandle),
        0);

    const float healthBeforeAle = player->getHealth();
    BOOST_REQUIRE(player->consumeItem(aleHandle));
    BOOST_CHECK_CLOSE(player->getHealth(), healthBeforeAle, 0.001f);
    BOOST_CHECK_CLOSE(player->getStamina(), 90.0f, 0.001f);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), aleHandle),
        0);

    const float healthBeforeWine = player->getHealth();
    BOOST_REQUIRE(player->consumeItem(wineHandle));
    BOOST_CHECK_CLOSE(player->getHealth(), healthBeforeWine, 0.001f);
    BOOST_CHECK_CLOSE(player->getStamina(), player->getMaxStamina(), 0.001f);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), wineHandle),
        0);
}

BOOST_AUTO_TEST_CASE(TestInventoryDragSwapsOccupiedSlots) {
    auto ironOreHandle = getResourceHandleById("iron_ore");
    auto breadHandle = getResourceHandleById("bread");
    auto manaHandle = getResourceHandleById("mana_elixir");
    BOOST_REQUIRE(ironOreHandle.isValid());
    BOOST_REQUIRE(breadHandle.isValid());
    BOOST_REQUIRE(manaHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(manaHandle, 2));
    BOOST_REQUIRE(player->addToInventory(breadHandle, 4));
    BOOST_REQUIRE(player->addToInventory(ironOreHandle, 6));

    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();

    InventoryController inventoryController(player);
    inventoryController.initializeInventoryUI();
    inventoryController.setInventoryVisible(true);
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds("inventory_slot_0");
    const UIRect targetBounds = ui.getBounds("inventory_slot_4");
    const UIRect sourceRectBefore = ui.getImageSourceRect("inventory_icon_0");
    const UIRect targetRectBefore = ui.getImageSourceRect("inventory_icon_4");
    const std::string sourceCountBefore = ui.getText("inventory_count_0");
    const std::string targetCountBefore = ui.getText("inventory_count_4");

    setLeftMouseDownAt(sourceBounds, true);
    inventoryController.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    inventoryController.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    inventoryController.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK(sameRect(ui.getImageSourceRect("inventory_icon_0"), targetRectBefore));
    BOOST_CHECK(sameRect(ui.getImageSourceRect("inventory_icon_4"), sourceRectBefore));
    BOOST_CHECK_EQUAL(ui.getText("inventory_count_0"), targetCountBefore);
    BOOST_CHECK_EQUAL(ui.getText("inventory_count_4"), sourceCountBefore);
}

BOOST_AUTO_TEST_CASE(TestInventoryDragMovesOccupiedSlotToEmptySlot) {
    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();

    InventoryController inventoryController(player);
    inventoryController.initializeInventoryUI();
    inventoryController.setInventoryVisible(true);
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds("inventory_slot_0");
    const UIRect targetBounds = ui.getBounds("inventory_slot_4");
    const UIRect sourceRectBefore = ui.getImageSourceRect("inventory_icon_0");
    const std::string sourceCountBefore = ui.getText("inventory_count_0");

    BOOST_REQUIRE_EQUAL(ui.getTexture("inventory_icon_4"), "");
    BOOST_REQUIRE_EQUAL(ui.getText("inventory_count_4"), "");

    setLeftMouseDownAt(sourceBounds, true);
    inventoryController.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    inventoryController.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    inventoryController.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK_EQUAL(ui.getTexture("inventory_icon_0"), "");
    BOOST_CHECK_EQUAL(ui.getText("inventory_count_0"), "");
    BOOST_CHECK(sameRect(ui.getImageSourceRect("inventory_icon_4"), sourceRectBefore));
    BOOST_CHECK_EQUAL(ui.getText("inventory_count_4"), sourceCountBefore);
}

BOOST_AUTO_TEST_CASE(TestInventoryDragDropOutsideLeavesOrderUnchanged) {
    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();

    InventoryController inventoryController(player);
    inventoryController.initializeInventoryUI();
    inventoryController.setInventoryVisible(true);
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds("inventory_slot_0");
    const UIRect outsideBounds{0, 0, 2, 2};
    const UIRect sourceRectBefore = ui.getImageSourceRect("inventory_icon_0");
    const UIRect secondRectBefore = ui.getImageSourceRect("inventory_icon_1");
    const std::string sourceCountBefore = ui.getText("inventory_count_0");
    const std::string secondCountBefore = ui.getText("inventory_count_1");

    setLeftMouseDownAt(sourceBounds, true);
    inventoryController.handleHotbarAssignmentInput(hudController);

    moveMouseTo(outsideBounds);
    inventoryController.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(outsideBounds, false);
    inventoryController.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK(sameRect(ui.getImageSourceRect("inventory_icon_0"), sourceRectBefore));
    BOOST_CHECK(sameRect(ui.getImageSourceRect("inventory_icon_1"), secondRectBefore));
    BOOST_CHECK_EQUAL(ui.getText("inventory_count_0"), sourceCountBefore);
    BOOST_CHECK_EQUAL(ui.getText("inventory_count_1"), secondCountBefore);
}

BOOST_AUTO_TEST_CASE(TestHotbarItemCanBeDraggedToAnotherSlot) {
    auto potionHandle = getResourceHandleById("health_potion");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);

    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();
    BOOST_REQUIRE(hudController.assignHotbarItem(0, potionHandle));

    InventoryController inventoryController(player);
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds(HudController::hotbarSlotId(0));
    const UIRect targetBounds = ui.getBounds(HudController::hotbarSlotId(4));

    setLeftMouseDownAt(sourceBounds, true);
    inventoryController.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    inventoryController.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    inventoryController.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK(!hudController.getHotbarItem(0).isValid());
    BOOST_CHECK(hudController.getHotbarItem(4) == potionHandle);
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_0"), "");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_0"), "");
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_4"), "atlas");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_4"), "3");
}

BOOST_AUTO_TEST_CASE(TestHotbarItemDragSwapsOccupiedSlots) {
    auto potionHandle = getResourceHandleById("health_potion");
    auto manaHandle = getResourceHandleById("mana_elixir");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE(manaHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);
    BOOST_REQUIRE(player->addToInventory(manaHandle, 2));

    auto& ui = UIManager::Instance();
    ui.onWindowResize(1280, 720);

    HudController hudController(player);
    hudController.initializeHotbarUI();
    BOOST_REQUIRE(hudController.assignHotbarItem(0, potionHandle));
    BOOST_REQUIRE(hudController.assignHotbarItem(4, manaHandle));

    InventoryController inventoryController(player);
    InputManager::Instance().reset();

    const UIRect sourceBounds = ui.getBounds(HudController::hotbarSlotId(0));
    const UIRect targetBounds = ui.getBounds(HudController::hotbarSlotId(4));

    setLeftMouseDownAt(sourceBounds, true);
    inventoryController.handleHotbarAssignmentInput(hudController);

    moveMouseTo(targetBounds);
    inventoryController.handleHotbarAssignmentInput(hudController);

    setLeftMouseDownAt(targetBounds, false);
    inventoryController.handleHotbarAssignmentInput(hudController);

    BOOST_CHECK(hudController.getHotbarItem(0) == manaHandle);
    BOOST_CHECK(hudController.getHotbarItem(4) == potionHandle);
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_0"), "atlas");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_0"), "2");
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_4"), "atlas");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_4"), "3");
}

BOOST_AUTO_TEST_CASE(TestMoveConstructor) {
    InventoryController controller(nullptr);
    controller.subscribe();

    // Move construct
    InventoryController moved(std::move(controller));

    BOOST_CHECK_EQUAL(moved.getName(), "InventoryController");
}

BOOST_AUTO_TEST_CASE(TestMoveAssignment) {
    InventoryController controller1(nullptr);
    controller1.subscribe();

    InventoryController controller2(nullptr);

    // Move assign
    controller2 = std::move(controller1);

    BOOST_CHECK_EQUAL(controller2.getName(), "InventoryController");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Event Subscription Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(InventoryControllerEventTests, InventoryControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestDoubleSubscribe) {
    InventoryController controller(nullptr);

    // First subscribe
    controller.subscribe();
    BOOST_CHECK(controller.isSubscribed());

    // Second subscribe should be a no-op (checkAlreadySubscribed)
    controller.subscribe();
    BOOST_CHECK(controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestUnsubscribe) {
    InventoryController controller(nullptr);

    // Subscribe first
    controller.subscribe();
    BOOST_CHECK(controller.isSubscribed());

    // Unsubscribe
    controller.unsubscribe();
    BOOST_CHECK(!controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestAttemptPickupDispatchesResourceChangeEvent) {
    std::atomic<int> resourceChangeEvents{0};
    std::atomic<int> quantityDelta{0};

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [this, &resourceChangeEvents, &quantityDelta](const EventData& data) {
            const auto* event = dynamic_cast<const ResourceChangeEvent*>(data.event.get());
            if (!event || event->getOwnerHandle() != player->getHandle()) {
                return;
            }

            resourceChangeEvents.fetch_add(1, std::memory_order_relaxed);
            quantityDelta.store(event->getQuantityChange(), std::memory_order_relaxed);
        });

    const int initialQuantity =
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), goldHandle);
    EntityHandle droppedItem = EntityDataManager::Instance().createDroppedItem(
        player->getPosition(), goldHandle, 7, "test_world");
    BOOST_REQUIRE(droppedItem.isValid());

    InventoryController controller(player);
    controller.subscribe();

    BOOST_REQUIRE(controller.attemptPickup());
    EventManager::Instance().drainAllDeferredEvents();
    BOOST_CHECK_EQUAL(resourceChangeEvents.load(std::memory_order_relaxed), 1);
    BOOST_CHECK_EQUAL(quantityDelta.load(std::memory_order_relaxed), 7);
    BOOST_CHECK_EQUAL(
        EntityDataManager::Instance().getInventoryQuantity(player->getInventoryIndex(), goldHandle),
        initialQuantity + 7);
}

BOOST_AUTO_TEST_CASE(TestAttemptPickupFailureDoesNotDispatchResourceChangeEvent) {
    std::atomic<int> resourceChangeEvents{0};

    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&resourceChangeEvents](const EventData&) {
            resourceChangeEvents.fetch_add(1, std::memory_order_relaxed);
        });

    InventoryController controller(player);
    controller.subscribe();

    BOOST_CHECK(!controller.attemptPickup());
    EventManager::Instance().drainAllDeferredEvents();
    BOOST_CHECK_EQUAL(resourceChangeEvents.load(std::memory_order_relaxed), 0);
}

BOOST_AUTO_TEST_SUITE_END()
