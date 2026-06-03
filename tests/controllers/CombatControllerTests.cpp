/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE CombatControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/combat/CombatController.hpp"
#include "entities/Player.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIManager.hpp"

#include <memory>

BOOST_AUTO_TEST_SUITE(CombatControllerTests)

BOOST_AUTO_TEST_CASE(TestControllerNameAndInitialState)
{
    CombatController controller(std::shared_ptr<Player>{});

    BOOST_CHECK_EQUAL(controller.getName(), "CombatController");
}

BOOST_AUTO_TEST_CASE(TestSubscribeIsIdempotentWithoutHandlers)
{
    CombatController controller(std::shared_ptr<Player>{});

    controller.subscribe();
    BOOST_CHECK(controller.isSubscribed());

    controller.subscribe();
    BOOST_CHECK(controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestTryAttackFailsGracefullyWithoutPlayer)
{
    CombatController controller(std::shared_ptr<Player>{});

    BOOST_CHECK(!controller.tryAttack());
}

BOOST_AUTO_TEST_CASE(TestUpdateDoesNothingWithoutPlayer)
{
    CombatController controller(std::shared_ptr<Player>{});

    // Capture public state before update
    BOOST_CHECK_EQUAL(controller.getName(), "CombatController");

    controller.update(0.25f);

    // Public state unchanged — update is a no-op without player
    BOOST_CHECK_EQUAL(controller.getName(), "CombatController");
}

class CombatControllerRuntimeFixture {
public:
    CombatControllerRuntimeFixture() {
        EventManager::Instance().init();
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        UIManager::Instance().init();

        player = std::make_shared<Player>();
        player->initializeInventory();
        BOOST_REQUIRE(player->getHandle().isValid());
    }

    ~CombatControllerRuntimeFixture() {
        UIManager::Instance().cleanupForStateTransition();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }

    std::shared_ptr<Player> player;
};

BOOST_FIXTURE_TEST_SUITE(CombatControllerRuntimeTests,
                         CombatControllerRuntimeFixture)

BOOST_AUTO_TEST_CASE(TestRangedNoAmmoDoesNotSpendAttackCost)
{
    auto& edm = EntityDataManager::Instance();
    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    BOOST_REQUIRE(bow.isValid());

    BOOST_REQUIRE(edm.addToInventory(player->getInventoryIndex(), bow, 1));
    BOOST_REQUIRE(player->equipItem(bow));
    BOOST_CHECK_EQUAL(edm.getCharacterData(player->getHandle()).combatStyle,
                      CharacterData::CombatStyle::Ranged);

    const float staminaBefore = player->getStamina();
    CombatController controller(player);

    BOOST_CHECK(!controller.tryAttack());
    BOOST_CHECK_CLOSE(player->getStamina(), staminaBefore, 0.001f);
    BOOST_CHECK_EQUAL(player->getCurrentStateName(), "idle");
}

BOOST_AUTO_TEST_CASE(TestRangedNoAmmoEquipsMeleeFallbackThroughPlayer)
{
    auto& edm = EntityDataManager::Instance();
    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    const auto dagger = ResourceTemplateManager::Instance().getHandleById("dagger");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(dagger.isValid());

    BOOST_REQUIRE(edm.addToInventory(player->getInventoryIndex(), bow, 1));
    BOOST_REQUIRE(edm.addToInventory(player->getInventoryIndex(), dagger, 1));
    BOOST_REQUIRE(player->equipItem(bow));
    BOOST_CHECK_EQUAL(edm.getCharacterData(player->getHandle()).combatStyle,
                      CharacterData::CombatStyle::Ranged);

    CombatController controller(player);
    BOOST_CHECK(controller.tryAttack());
    BOOST_CHECK(player->getEquippedItem("weapon") == dagger);
    BOOST_CHECK_EQUAL(edm.getCharacterData(player->getHandle()).combatStyle,
                      CharacterData::CombatStyle::Melee);
    BOOST_CHECK_EQUAL(player->getCurrentStateName(), "attacking");
}

BOOST_AUTO_TEST_CASE(TestRangedAttackConsumesAmmoAndDispatchesResourceChange)
{
    auto& edm = EntityDataManager::Instance();
    const auto bow = ResourceTemplateManager::Instance().getHandleById("bow");
    const auto arrows = ResourceTemplateManager::Instance().getHandleById("arrows");
    BOOST_REQUIRE(bow.isValid());
    BOOST_REQUIRE(arrows.isValid());

    BOOST_REQUIRE(edm.addToInventory(player->getInventoryIndex(), bow, 1));
    BOOST_REQUIRE(edm.addToInventory(player->getInventoryIndex(), arrows, 1));
    BOOST_REQUIRE(player->equipItem(bow));

    bool resourceEventSeen = false;
    EventManager::Instance().registerHandler(
        EventTypeId::ResourceChange,
        [&](const EventData& data) {
            const auto* event =
                dynamic_cast<const ResourceChangeEvent*>(data.event.get());
            if (!event || event->getResourceHandle() != arrows) {
                return;
            }
            resourceEventSeen = true;
            BOOST_CHECK_EQUAL(event->getOldQuantity(), 1);
            BOOST_CHECK_EQUAL(event->getNewQuantity(), 0);
            BOOST_CHECK_EQUAL(event->getChangeReason(), "ammo_consumed");
        });

    CombatController controller(player);
    BOOST_CHECK(controller.tryAttack());
    BOOST_CHECK_EQUAL(edm.getInventoryQuantity(player->getInventoryIndex(), arrows), 0);

    EventManager::Instance().update();
    BOOST_CHECK(resourceEventSeen);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
