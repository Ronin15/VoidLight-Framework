/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ResourceFactoryTests
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>

#include "core/Logger.hpp"
#include "entities/resources/CurrencyResources.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "entities/resources/ItemResources.hpp"
#include "entities/resources/MaterialResources.hpp"
#include "managers/ResourceFactory.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "ResourceTestAccess.hpp"
#include "utils/JsonReader.hpp"

using namespace VoidLight;

class ResourceFactoryTestFixture {
public:
  ResourceFactoryTestFixture() {
    // Ensure isolation, then initialize the factory with default creators
    ResourceTestAccess::resetFactory();
    ResourceFactory::initialize();
  }

  ~ResourceFactoryTestFixture() {
    // Clean up for isolation using test helper
    ResourceTestAccess::resetFactory();
  }

protected:
  // Helper method to create JsonValue from string
  JsonValue parseJson(const std::string &jsonString) {
    JsonReader reader;
    BOOST_REQUIRE(reader.parse(jsonString));
    return reader.getRoot();
  }
};

BOOST_FIXTURE_TEST_SUITE(ResourceFactoryTestSuite, ResourceFactoryTestFixture)

BOOST_AUTO_TEST_CASE(TestFactoryInitialization) {
  // Test that default creators are registered
  auto registeredTypes = ResourceFactory::getRegisteredTypes();
  BOOST_CHECK(registeredTypes.size() > 0);

  // Check for specific types
  BOOST_CHECK(ResourceFactory::hasCreator("Equipment"));
  BOOST_CHECK(ResourceFactory::hasCreator("Consumable"));
  BOOST_CHECK(ResourceFactory::hasCreator("QuestItem"));
  BOOST_CHECK(ResourceFactory::hasCreator("CraftingComponent"));
  BOOST_CHECK(ResourceFactory::hasCreator("RawResource"));
  BOOST_CHECK(ResourceFactory::hasCreator("Gold"));
  BOOST_CHECK(ResourceFactory::hasCreator("Gem"));
  BOOST_CHECK(ResourceFactory::hasCreator("Ammunition"));
  BOOST_CHECK(ResourceFactory::hasCreator("CraftingCurrency"));
}

BOOST_AUTO_TEST_CASE(TestCreateEquipmentFromJson) {
  std::string jsonString = R"({
        "id": "test_sword",
        "name": "Test Sword",
        "category": "Item",
        "type": "Equipment",
        "description": "A test sword for testing",
        "value": 100,
        "maxStackSize": 1,
        "consumable": false,
        "properties": {
            "slot": "Weapon",
            "handsRequired": 2,
            "attackBonus": 15,
            "defenseBonus": 2,
            "speedBonus": 0,
            "weaponMode": "ranged",
            "attackRange": 375,
            "projectileSpeed": 280,
            "ammoTypeRequired": "Arrow",
            "durability": 100,
            "maxDurability": 100
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Sword");
  BOOST_CHECK_EQUAL(static_cast<int>(resource->getCategory()),
                    static_cast<int>(ResourceCategory::Item));
  BOOST_CHECK_EQUAL(static_cast<int>(resource->getType()),
                    static_cast<int>(ResourceType::Equipment));

  // Test that it's actually an Equipment object
  auto equipment = std::dynamic_pointer_cast<Equipment>(resource);
  BOOST_REQUIRE(equipment != nullptr);
  BOOST_CHECK_EQUAL(static_cast<int>(equipment->getEquipmentSlot()),
                    static_cast<int>(Equipment::EquipmentSlot::Weapon));
  BOOST_CHECK_EQUAL(equipment->getAttackBonus(), 15);
  BOOST_CHECK_EQUAL(equipment->getDefenseBonus(), 2);
  BOOST_CHECK_EQUAL(equipment->getHandsRequired(), 2);
  BOOST_CHECK_EQUAL(static_cast<int>(equipment->getWeaponMode()),
                    static_cast<int>(Equipment::WeaponMode::Ranged));
  BOOST_CHECK_CLOSE(equipment->getAttackRangeOverride(), 375.0f, 0.001f);
  BOOST_CHECK_CLOSE(equipment->getProjectileSpeedOverride(), 280.0f, 0.001f);
  BOOST_CHECK_EQUAL(equipment->getAmmoTypeRequired(), "Arrow");
}

BOOST_AUTO_TEST_CASE(TestCreateShieldEquipmentFromJson) {
  std::string jsonString = R"({
        "id": "test_shield",
        "name": "Test Shield",
        "category": "Item",
        "type": "Equipment",
        "description": "A test shield for testing",
        "value": 100,
        "maxStackSize": 1,
        "consumable": false,
        "properties": {
            "slot": "Shield",
            "handsRequired": 1,
            "attackBonus": 0,
            "defenseBonus": 15,
            "durability": 100,
            "maxDurability": 100
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  auto equipment = std::dynamic_pointer_cast<Equipment>(resource);
  BOOST_REQUIRE(equipment != nullptr);
  BOOST_CHECK_EQUAL(static_cast<int>(equipment->getEquipmentSlot()),
                    static_cast<int>(Equipment::EquipmentSlot::Shield));
  BOOST_CHECK_EQUAL(equipment->getDefenseBonus(), 15);
  BOOST_CHECK_EQUAL(equipment->getHandsRequired(), 1);
}

BOOST_AUTO_TEST_CASE(TestCreateConsumableFromJson) {
  std::string jsonString = R"({
        "id": "test_potion",
        "name": "Test Potion",
        "category": "Item",
        "type": "Consumable",
        "description": "A test healing potion",
        "value": 50,
        "maxStackSize": 10,
        "consumable": true,
        "properties": {
            "effect": "HealHP",
            "effectPower": 50,
            "effectDuration": 0
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Potion");
  BOOST_CHECK(resource->isConsumable());

  // Test that it's actually a Consumable object
  auto consumable = std::dynamic_pointer_cast<Consumable>(resource);
  BOOST_REQUIRE(consumable != nullptr);
  BOOST_CHECK_EQUAL(static_cast<int>(consumable->getEffect()),
                    static_cast<int>(Consumable::ConsumableEffect::HealHP));
  BOOST_CHECK_EQUAL(consumable->getEffectPower(), 50);
  BOOST_CHECK_EQUAL(consumable->getEffectDuration(), 0);
}

BOOST_AUTO_TEST_CASE(TestCreateStaminaConsumableFromJson) {
  std::string jsonString = R"({
        "id": "test_stamina_drink",
        "name": "Test Stamina Drink",
        "category": "Item",
        "type": "Consumable",
        "description": "A test stamina drink",
        "value": 8,
        "maxStackSize": 10,
        "consumable": true,
        "properties": {
            "effect": "RestoreStamina",
            "effectPower": 20,
            "effectDuration": 0
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  auto consumable = std::dynamic_pointer_cast<Consumable>(resource);
  BOOST_REQUIRE(consumable != nullptr);
  BOOST_CHECK_EQUAL(static_cast<int>(consumable->getEffect()),
                    static_cast<int>(Consumable::ConsumableEffect::RestoreStamina));
  BOOST_CHECK_EQUAL(consumable->getEffectPower(), 20);
}

BOOST_AUTO_TEST_CASE(TestCreateQuestItemFromJson) {
  std::string jsonString = R"({
        "id": "test_key",
        "name": "Test Key",
        "category": "Item",
        "type": "QuestItem",
        "description": "A key for testing purposes",
        "value": 0,
        "maxStackSize": 1,
        "consumable": false,
        "properties": {
            "questId": "test_quest_123"
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Key");

  // Test that it's actually a QuestItem object
  auto questItem = std::dynamic_pointer_cast<QuestItem>(resource);
  BOOST_REQUIRE(questItem != nullptr);
  BOOST_CHECK_EQUAL(questItem->getQuestId(), "test_quest_123");
  BOOST_CHECK(questItem->isQuestSpecific());
}

BOOST_AUTO_TEST_CASE(TestCreateCraftingComponentFromJson) {
  std::string jsonString = R"({
        "id": "test_essence",
        "name": "Test Essence",
        "category": "Material",
        "type": "CraftingComponent",
        "description": "A magical essence for testing",
        "value": 200,
        "maxStackSize": 50,
        "consumable": false,
        "properties": {
            "componentType": "Essence",
            "tier": 3,
            "purity": 0.8
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Essence");

  // Test that it's actually a CraftingComponent object
  auto craftingComponent =
      std::dynamic_pointer_cast<CraftingComponent>(resource);
  BOOST_REQUIRE(craftingComponent != nullptr);
  BOOST_CHECK_EQUAL(
      static_cast<int>(craftingComponent->getComponentType()),
      static_cast<int>(CraftingComponent::ComponentType::Essence));
  BOOST_CHECK_EQUAL(craftingComponent->getTier(), 3);
  BOOST_CHECK_CLOSE(craftingComponent->getPurity(), 0.8f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestCreateRawResourceFromJson) {
  std::string jsonString = R"({
        "id": "test_ore",
        "name": "Test Ore",
        "category": "Material",
        "type": "RawResource",
        "description": "Raw ore for testing",
        "value": 25,
        "maxStackSize": 100,
        "consumable": false,
        "properties": {
            "origin": "Mining",
            "tier": 2,
            "rarity": 4
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Ore");

  // Test that it's actually a RawResource object
  auto rawResource = std::dynamic_pointer_cast<RawResource>(resource);
  BOOST_REQUIRE(rawResource != nullptr);
  BOOST_CHECK_EQUAL(static_cast<int>(rawResource->getOrigin()),
                    static_cast<int>(RawResource::ResourceOrigin::Mining));
  BOOST_CHECK_EQUAL(rawResource->getTier(), 2);
  BOOST_CHECK_EQUAL(rawResource->getRarity(), 4);
}

BOOST_AUTO_TEST_CASE(TestCreateGoldFromJson) {
  std::string jsonString = R"({
        "id": "test_gold",
        "name": "Test Gold",
        "category": "Currency",
        "type": "Gold",
        "description": "Gold coins for testing",
        "value": 1,
        "maxStackSize": 10000,
        "consumable": false,
        "properties": {
            "exchangeRate": 1.0
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Gold");

  // Test that it's actually a Gold object
  auto gold = std::dynamic_pointer_cast<Gold>(resource);
  BOOST_REQUIRE(gold != nullptr);
  BOOST_CHECK_CLOSE(gold->getExchangeRate(), 1.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestCreateGemFromJson) {
  std::string jsonString = R"({
        "id": "test_emerald",
        "name": "Test Emerald",
        "category": "Currency",
        "type": "Gem",
        "description": "Emerald gem for testing",
        "value": 100,
        "maxStackSize": 1000,
        "consumable": false,
        "properties": {
            "gemType": "Emerald",
            "exchangeRate": 100.0,
            "clarity": 8
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Emerald");

  // Test that it's actually a Gem object
  auto gem = std::dynamic_pointer_cast<Gem>(resource);
  BOOST_REQUIRE(gem != nullptr);
  BOOST_CHECK_EQUAL(static_cast<int>(gem->getGemType()),
                    static_cast<int>(Gem::GemType::Emerald));
  BOOST_CHECK_CLOSE(gem->getExchangeRate(), 100.0f, 0.001f);
  BOOST_CHECK_EQUAL(gem->getClarity(), 8);
}

BOOST_AUTO_TEST_CASE(TestCreateAmmunitionFromJson) {
  std::string jsonString = R"({
        "id": "test_arrows",
        "name": "Test Arrows",
        "category": "Item",
        "type": "Ammunition",
        "description": "Ammo for testing",
        "value": 2,
        "maxStackSize": 200,
        "consumable": true,
        "properties": {
            "ammoType": "Arrow"
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Arrows");
  BOOST_CHECK_EQUAL(static_cast<int>(resource->getCategory()),
                    static_cast<int>(ResourceCategory::Item));

  auto ammunition = std::dynamic_pointer_cast<Ammunition>(resource);
  BOOST_REQUIRE(ammunition != nullptr);
  BOOST_CHECK_EQUAL(ammunition->getAmmoType(), "Arrow");
}

BOOST_AUTO_TEST_CASE(TestCreateCraftingCurrencyFromJson) {
  std::string jsonString = R"({
        "id": "test_essence",
        "name": "Test Essence",
        "category": "Currency",
        "type": "CraftingCurrency",
        "description": "Crafting currency for testing",
        "value": 0,
        "maxStackSize": 10000,
        "consumable": false,
        "properties": {
            "exchangeRate": 0.0
        }
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Essence");
  BOOST_CHECK_EQUAL(static_cast<int>(resource->getCategory()),
                    static_cast<int>(ResourceCategory::Currency));

  auto craftingCurrency = std::dynamic_pointer_cast<CraftingCurrency>(resource);
  BOOST_REQUIRE(craftingCurrency != nullptr);
  BOOST_CHECK_CLOSE(craftingCurrency->getExchangeRate(), 0.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestInvalidJsonHandling) {
  // Test empty JSON
  std::string emptyJson = "{}";
  JsonValue json = parseJson(emptyJson);
  ResourcePtr resource = ResourceFactory::createFromJson(json);
  BOOST_CHECK(resource == nullptr);

  // Test missing required fields
  std::string incompleteJson = R"({"id": "test", "name": "Test"})";
  json = parseJson(incompleteJson);
  resource = ResourceFactory::createFromJson(json);
  BOOST_CHECK(resource == nullptr);

  // Test invalid JSON type (not object)
  std::string invalidJson = R"("not an object")";
  json = parseJson(invalidJson);
  resource = ResourceFactory::createFromJson(json);
  BOOST_CHECK(resource == nullptr);
}

BOOST_AUTO_TEST_CASE(TestUnknownTypeHandling) {
  std::string jsonString = R"({
        "id": "test_unknown",
        "name": "Test Unknown",
        "category": "Item",
        "type": "UnknownType",
        "description": "Unknown type for testing",
        "value": 10,
        "maxStackSize": 1,
        "consumable": false
    })";

  JsonValue json = parseJson(jsonString);
  ResourcePtr resource = ResourceFactory::createFromJson(json);

  // Should fallback to base Resource class
  BOOST_REQUIRE(resource != nullptr);
  BOOST_CHECK_EQUAL(resource->getName(), "Test Unknown");
}

BOOST_AUTO_TEST_CASE(TestCustomCreatorRegistration) {
  // Test registering a custom creator
  bool registered = ResourceFactory::registerCreator(
      "CustomType", [](const JsonValue &json) -> ResourcePtr {
        // Create a proper ResourceHandle for the custom resource
        auto handle = ResourceTemplateManager::Instance().generateHandle();
        return std::make_shared<Resource>(
            handle, json["id"].asString(), json["name"].asString(),
            ResourceCategory::Item, ResourceType::Equipment);
      });

  BOOST_CHECK(registered);
  BOOST_CHECK(ResourceFactory::hasCreator("CustomType"));

  // Test that registering the same type again fails
  bool registeredAgain = ResourceFactory::registerCreator(
      "CustomType",
      [](const JsonValue & /*json*/) -> ResourcePtr { return nullptr; });

  BOOST_CHECK(!registeredAgain);
}

BOOST_AUTO_TEST_SUITE_END()
