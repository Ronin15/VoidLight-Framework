/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE HudControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/ui/HudController.hpp"
#include "collisions/CollisionInfo.hpp"
#include "entities/Player.hpp"
#include "events/EntityEvents.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/ProjectileManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIManager.hpp"
#include "../events/EventManagerTestAccess.hpp"
#include <format>

class HudControllerFixture
{
public:
    HudControllerFixture()
    {
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(GameTimeManager::Instance().init(12.0f, 1.0f));
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(UIManager::Instance().init());
        BOOST_REQUIRE(ProjectileManager::Instance().init());
    }

    ~HudControllerFixture()
    {
        ProjectileManager::Instance().clean();
        UIManager::Instance().cleanupForStateTransition();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }

protected:
    std::shared_ptr<Player> createPlayer(const Vector2D& pos = Vector2D(100.0f, 100.0f))
    {
        auto player = std::make_shared<Player>();
        player->setPosition(pos);
        return player;
    }

    EntityHandle createNPCTarget(const Vector2D& pos = Vector2D(200.0f, 100.0f))
    {
        return EntityDataManager::Instance().createNPCWithRaceClass(pos, "Human", "Guard");
    }

    void dispatchDamage(EntityHandle source, EntityHandle target, float damage)
    {
        auto& eventMgr = EventManager::Instance();
        auto damageEvent = eventMgr.acquireDamageEvent();
        damageEvent->configure(source, target, damage, Vector2D(10.0f, 0.0f));
        eventMgr.dispatchEvent(damageEvent, EventManager::DispatchMode::Immediate);
    }

    void handleProjectileCollision(const VoidLight::CollisionInfo& info)
    {
        ProjectileManager::Instance().handleProjectileCollision(info);
    }
};

bool sameRect(const UIRect& lhs, const UIRect& rhs)
{
    return lhs.x == rhs.x &&
        lhs.y == rhs.y &&
        lhs.width == rhs.width &&
        lhs.height == rhs.height;
}

BOOST_FIXTURE_TEST_SUITE(HudControllerTests, HudControllerFixture)

BOOST_AUTO_TEST_CASE(TestControllerNameAndInitialState)
{
    HudController controller(nullptr);

    BOOST_CHECK_EQUAL(controller.getName(), "HudController");
    BOOST_CHECK(!controller.hasActiveTarget());
    BOOST_CHECK_EQUAL(controller.getTargetHealth(), 0.0f);
    BOOST_CHECK_EQUAL(controller.getTargetLabel(), "Target");
}

BOOST_AUTO_TEST_CASE(TestPlayerMeleeHitSetsTargetState)
{
    auto player = createPlayer();
    EntityHandle playerHandle = player->getHandle();
    EntityHandle target = createNPCTarget();
    HudController controller(player);
    controller.subscribe();

    dispatchDamage(playerHandle, target, 25.0f);

    BOOST_REQUIRE(controller.hasActiveTarget());
    const auto& charData = EntityDataManager::Instance().getCharacterData(target);
    const float expectedPercent = (charData.health / charData.maxHealth) * 100.0f;
    BOOST_CHECK_CLOSE(controller.getTargetHealth(), expectedPercent, 0.01f);
    BOOST_CHECK_EQUAL(controller.getTargetLabel(), "Human Guard");
}

BOOST_AUTO_TEST_CASE(TestPlayerProjectileHitSetsTargetState)
{
    auto& edm = EntityDataManager::Instance();

    auto player = createPlayer();
    EntityHandle playerHandle = player->getHandle();
    EntityHandle target = createNPCTarget();
    HudController controller(player);
    controller.subscribe();

    EntityHandle projectile = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(220.0f, 0.0f), playerHandle, 30.0f, 5.0f);
    const size_t projectileIdx = edm.getIndex(projectile);
    const size_t targetIdx = edm.getIndex(target);
    BOOST_REQUIRE_NE(projectileIdx, SIZE_MAX);
    BOOST_REQUIRE_NE(targetIdx, SIZE_MAX);

    VoidLight::CollisionInfo info{};
    info.indexA = projectileIdx;
    info.indexB = targetIdx;
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    handleProjectileCollision(info);
    EventManager::Instance().update();

    BOOST_REQUIRE(controller.hasActiveTarget());
    const auto& targetCharData = EntityDataManager::Instance().getCharacterData(target);
    const float expectedPct = (targetCharData.health / targetCharData.maxHealth) * 100.0f;
    BOOST_CHECK_CLOSE(controller.getTargetHealth(), expectedPct, 0.01f);
    BOOST_CHECK_EQUAL(controller.getTargetLabel(), "Human Guard");
}

BOOST_AUTO_TEST_CASE(TestNPCDamageToPlayerDoesNotSetTarget)
{
    auto player = createPlayer();
    EntityHandle playerHandle = player->getHandle();
    EntityHandle npc = createNPCTarget();
    HudController controller(player);
    controller.subscribe();

    dispatchDamage(npc, playerHandle, 12.0f);

    BOOST_CHECK(!controller.hasActiveTarget());
}

BOOST_AUTO_TEST_CASE(TestLethalHitClearsTargetState)
{
    auto player = createPlayer();
    EntityHandle playerHandle = player->getHandle();
    EntityHandle target = createNPCTarget();
    HudController controller(player);
    controller.subscribe();

    dispatchDamage(playerHandle, target, 500.0f);

    BOOST_CHECK(!controller.hasActiveTarget());
    BOOST_CHECK_EQUAL(controller.getTargetHealth(), 0.0f);
}

BOOST_AUTO_TEST_CASE(TestTimerExpiryClearsTargetState)
{
    auto player = createPlayer();
    EntityHandle playerHandle = player->getHandle();
    EntityHandle target = createNPCTarget();
    HudController controller(player);
    controller.subscribe();

    dispatchDamage(playerHandle, target, 10.0f);
    BOOST_REQUIRE(controller.hasActiveTarget());

    controller.update(HudController::TARGET_DISPLAY_DURATION + 0.01f);

    BOOST_CHECK(!controller.hasActiveTarget());
}

BOOST_AUTO_TEST_CASE(TestHotbarAssignmentUpdatesSlotIconAndCount)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto potionHandle = ResourceTemplateManager::Instance().getHandleById("health_potion");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);

    HudController controller(player);
    controller.initializeHotbarUI();

    BOOST_REQUIRE(controller.assignHotbarItem(0, potionHandle));

    auto& ui = UIManager::Instance();
    BOOST_CHECK(controller.getHotbarItem(0) == potionHandle);
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_0"), "atlas");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_0"), "3");
}

BOOST_AUTO_TEST_CASE(TestHotbarAssignmentUsesExplicitNonAtlasIconTexture)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto bowHandle = ResourceTemplateManager::Instance().getHandleById("bow");
    BOOST_REQUIRE(bowHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(bowHandle, 1));

    HudController controller(player);
    controller.initializeHotbarUI();

    BOOST_REQUIRE(controller.assignHotbarItem(0, bowHandle));

    auto& ui = UIManager::Instance();
    BOOST_CHECK(controller.getHotbarItem(0) == bowHandle);
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_0"), "bow_icon");
    BOOST_CHECK(sameRect(ui.getImageSourceRect("hotbar_icon_0"), UIRect{}));
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_0"), "1");
}

BOOST_AUTO_TEST_CASE(TestHotbarSelectingWeaponEquipsIt)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto daggerHandle = ResourceTemplateManager::Instance().getHandleById("dagger");
    BOOST_REQUIRE(daggerHandle.isValid());
    BOOST_REQUIRE(player->addToInventory(daggerHandle, 1));

    HudController controller(player);
    controller.initializeHotbarUI();

    BOOST_REQUIRE(controller.assignHotbarItem(0, daggerHandle));
    BOOST_REQUIRE(controller.activateSelectedHotbarItem());

    BOOST_CHECK(player->getEquippedItem("weapon") == daggerHandle);
    BOOST_CHECK_EQUAL(player->getInventoryQuantity(daggerHandle), 0);
    BOOST_CHECK_EQUAL(UIManager::Instance().getText("hotbar_count_0"), "1");
    BOOST_CHECK(controller.activateSelectedHotbarItem());
    BOOST_CHECK(player->getEquippedItem("weapon") == daggerHandle);
    BOOST_CHECK_EQUAL(UIManager::Instance().getText("hotbar_count_0"), "1");
}

BOOST_AUTO_TEST_CASE(TestHotbarSelectingConsumableConsumesIt)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto potionHandle = ResourceTemplateManager::Instance().getHandleById("health_potion");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);
    player->takeDamage(30.0f);
    const float healthBefore = player->getHealth();

    bool resourceEventSeen = false;
    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&](const EventData& data) {
            const auto* event =
                dynamic_cast<const ResourceChangeEvent*>(data.event.get());
            if (!event || event->getResourceHandle() != potionHandle) {
                return;
            }
            resourceEventSeen = true;
            BOOST_CHECK_EQUAL(event->getOldQuantity(), 3);
            BOOST_CHECK_EQUAL(event->getNewQuantity(), 2);
            BOOST_CHECK_EQUAL(event->getChangeReason(), "player_action");
        });

    HudController controller(player);
    controller.initializeHotbarUI();

    BOOST_REQUIRE(controller.assignHotbarItem(0, potionHandle));
    BOOST_REQUIRE(controller.activateSelectedHotbarItem());
    EventManager::Instance().update();

    BOOST_CHECK_GT(player->getHealth(), healthBefore);
    BOOST_CHECK_EQUAL(player->getInventoryQuantity(potionHandle), 2);
    BOOST_CHECK_EQUAL(UIManager::Instance().getText("hotbar_count_0"), "2");
    BOOST_CHECK(resourceEventSeen);
}

BOOST_AUTO_TEST_CASE(TestHotbarReassigningSameItemMovesAssignment)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto potionHandle = ResourceTemplateManager::Instance().getHandleById("health_potion");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);

    HudController controller(player);
    controller.initializeHotbarUI();

    BOOST_REQUIRE(controller.assignHotbarItem(0, potionHandle));
    BOOST_REQUIRE(controller.assignHotbarItem(4, potionHandle));

    auto& ui = UIManager::Instance();
    BOOST_CHECK(!controller.getHotbarItem(0).isValid());
    BOOST_CHECK(controller.getHotbarItem(4) == potionHandle);
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_0"), "");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_0"), "");
    BOOST_CHECK_EQUAL(ui.getTexture("hotbar_icon_4"), "atlas");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_4"), "3");
}

BOOST_AUTO_TEST_CASE(TestHotbarMoveSwapsOccupiedSlotsAndMovesToEmptySlot)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto potionHandle = ResourceTemplateManager::Instance().getHandleById("health_potion");
    auto manaHandle = ResourceTemplateManager::Instance().getHandleById("mana_elixir");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE(manaHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);
    BOOST_REQUIRE(player->addToInventory(manaHandle, 2));

    HudController controller(player);
    controller.initializeHotbarUI();

    BOOST_REQUIRE(controller.assignHotbarItem(0, potionHandle));
    BOOST_REQUIRE(controller.assignHotbarItem(4, manaHandle));
    BOOST_REQUIRE(controller.moveHotbarItem(0, 4));

    auto& ui = UIManager::Instance();
    BOOST_CHECK(controller.getHotbarItem(0) == manaHandle);
    BOOST_CHECK(controller.getHotbarItem(4) == potionHandle);
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_0"), "2");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_4"), "3");

    BOOST_REQUIRE(controller.moveHotbarItem(4, 1));
    BOOST_CHECK(!controller.getHotbarItem(4).isValid());
    BOOST_CHECK(controller.getHotbarItem(1) == potionHandle);
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_4"), "");
    BOOST_CHECK_EQUAL(ui.getText("hotbar_count_1"), "3");
}

BOOST_AUTO_TEST_CASE(TestHotbarMoveRejectsInvalidSlotsAndEmptySource)
{
    auto player = createPlayer();
    player->initializeInventory();
    auto potionHandle = ResourceTemplateManager::Instance().getHandleById("health_potion");
    BOOST_REQUIRE(potionHandle.isValid());
    BOOST_REQUIRE_EQUAL(player->getInventoryQuantity(potionHandle), 3);

    HudController controller(player);
    controller.initializeHotbarUI();
    BOOST_REQUIRE(controller.assignHotbarItem(0, potionHandle));

    BOOST_CHECK(!controller.moveHotbarItem(8, 1));
    BOOST_CHECK(!controller.moveHotbarItem(0, HudController::HOTBAR_SLOT_COUNT));
    BOOST_CHECK(controller.getHotbarItem(0) == potionHandle);
    BOOST_CHECK(!controller.getHotbarItem(1).isValid());
}

BOOST_AUTO_TEST_CASE(TestActionHUDCreatesExpectedWidgets)
{
    auto player = createPlayer();
    HudController controller(player);
    controller.initializeActionHUD();

    auto& ui = UIManager::Instance();
    BOOST_CHECK(ui.hasComponent(HudController::HEALTH_LABEL_ID));
    BOOST_CHECK(ui.hasComponent(HudController::HEALTH_BAR_ID));
    BOOST_CHECK(ui.hasComponent(HudController::STAMINA_LABEL_ID));
    BOOST_CHECK(ui.hasComponent(HudController::STAMINA_BAR_ID));
    BOOST_CHECK(ui.hasComponent(HudController::TARGET_NAME_ID));
    BOOST_CHECK(ui.hasComponent(HudController::TARGET_HP_LABEL_ID));
    BOOST_CHECK(ui.hasComponent(HudController::TARGET_HEALTH_BAR_ID));
    BOOST_CHECK(ui.hasComponent(HudController::HARVEST_LABEL_ID));
    BOOST_CHECK(ui.hasComponent(HudController::HARVEST_BAR_ID));
    BOOST_CHECK_EQUAL(ui.getText(HudController::HEALTH_LABEL_ID), "HP");
    BOOST_CHECK_EQUAL(ui.getText(HudController::STAMINA_LABEL_ID), "SP");
    BOOST_CHECK_EQUAL(ui.getText(HudController::HARVEST_LABEL_ID), "Harvest");
    BOOST_CHECK(!ui.hasComponent(HudController::hotbarSlotId(0)));
}

BOOST_AUTO_TEST_CASE(TestActionHUDWritesPlayerVitalsAsPercent)
{
    auto player = createPlayer();
    player->setMaxHealth(200.0f);
    player->setMaxStamina(200.0f);

    HudController controller(player);
    controller.initializeActionHUD();
    controller.update(0.0f);

    auto& ui = UIManager::Instance();
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HEALTH_BAR_ID), 50.0f, 0.01f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::STAMINA_BAR_ID), 50.0f, 0.01f);

    player->takeDamage(50.0f);
    player->consumeStamina(50.0f);
    controller.update(0.0f);

    BOOST_CHECK_CLOSE(ui.getValue(HudController::HEALTH_BAR_ID), 25.0f, 0.01f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::STAMINA_BAR_ID), 25.0f, 0.01f);
}

BOOST_AUTO_TEST_CASE(TestActionHUDTargetWidgetsFollowTargetState)
{
    auto player = createPlayer();
    EntityHandle playerHandle = player->getHandle();
    EntityHandle target = createNPCTarget();
    HudController controller(player);
    controller.subscribe();
    controller.initializeActionHUD();

    dispatchDamage(playerHandle, target, 25.0f);
    controller.update(0.0f);

    auto& ui = UIManager::Instance();
    BOOST_REQUIRE(controller.hasActiveTarget());
    BOOST_CHECK_EQUAL(ui.getText(HudController::TARGET_NAME_ID), "Human Guard");
    BOOST_CHECK_CLOSE(ui.getValue(HudController::TARGET_HEALTH_BAR_ID),
                      controller.getTargetHealth(), 0.01f);

    controller.update(HudController::TARGET_DISPLAY_DURATION + 0.01f);

    BOOST_CHECK(!controller.hasActiveTarget());
}

BOOST_AUTO_TEST_CASE(TestSetVisibleHidesAndRestoresActionHUDWithoutForceShowingTarget)
{
    auto player = createPlayer();
    HudController controller(player);
    controller.initializeActionHUD();
    controller.update(0.0f);

    auto& ui = UIManager::Instance();
    const float fullHealthPct = ui.getValue(HudController::HEALTH_BAR_ID);
    BOOST_CHECK_CLOSE(fullHealthPct, 100.0f, 0.01f);
    BOOST_CHECK_EQUAL(ui.getText(HudController::TARGET_NAME_ID), "");

    controller.setVisible(false);
    player->takeDamage(40.0f);
    controller.update(0.0f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HEALTH_BAR_ID), fullHealthPct, 0.01f);

    controller.setVisible(true);
    BOOST_CHECK_EQUAL(ui.getText(HudController::TARGET_NAME_ID), "");
    controller.update(0.0f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HEALTH_BAR_ID), 60.0f, 0.01f);
    BOOST_CHECK_EQUAL(ui.getText(HudController::TARGET_NAME_ID), "");
}

BOOST_AUTO_TEST_CASE(TestHarvestProgressShowsAndHidesWidget)
{
    auto player = createPlayer();
    HudController controller(player);
    controller.initializeActionHUD();

    auto& ui = UIManager::Instance();
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HARVEST_BAR_ID), 0.0f, 0.01f);

    controller.setHarvestProgress(true, 0.4f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HARVEST_BAR_ID), 0.4f, 0.01f);

    controller.setHarvestProgress(false, 0.0f);
    controller.setHarvestProgress(true, 0.9f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HARVEST_BAR_ID), 0.9f, 0.01f);

    controller.setVisible(false);
    controller.setHarvestProgress(true, 0.25f);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HARVEST_BAR_ID), 0.9f, 0.01f);

    controller.setVisible(true);
    BOOST_CHECK_CLOSE(ui.getValue(HudController::HARVEST_BAR_ID), 0.25f, 0.01f);

    controller.setHarvestProgress(false, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestHotbarRemainsOptionalWhenOnlyActionHUDInitialized)
{
    auto player = createPlayer();
    HudController controller(player);
    controller.initializeActionHUD();

    auto& ui = UIManager::Instance();
    BOOST_CHECK(ui.hasComponent(HudController::HEALTH_BAR_ID));
    BOOST_CHECK(!ui.hasComponent(HudController::hotbarSlotId(0)));

    controller.initializeHotbarUI();
    BOOST_CHECK(ui.hasComponent(HudController::hotbarSlotId(0)));
}

BOOST_AUTO_TEST_SUITE_END()
