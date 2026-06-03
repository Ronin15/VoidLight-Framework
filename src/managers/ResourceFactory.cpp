/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/ResourceFactory.hpp"
#include "core/Logger.hpp"
#include "entities/resources/CurrencyResources.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "entities/resources/ItemResources.hpp"
#include "entities/resources/MaterialResources.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include <algorithm>
#include <format>

namespace VoidLight {

namespace {

int defaultHandsRequiredForSlot(Equipment::EquipmentSlot slot) {
  switch (slot) {
  case Equipment::EquipmentSlot::Weapon:
  case Equipment::EquipmentSlot::Shield:
    return 1;
  default:
    return 0;
  }
}

} // namespace

ResourcePtr ResourceFactory::createFromJson(const JsonValue &json) {
  if (!json.isObject()) {
    RESOURCE_ERROR(
        "ResourceFactory::createFromJson - Invalid JSON: not an object");
    return nullptr;
  }

  // Extract required fields
  if (!json.hasKey("id") || !json.hasKey("name") || !json.hasKey("category") ||
      !json.hasKey("type")) {
    RESOURCE_ERROR("ResourceFactory::createFromJson - Missing required fields "
                   "(id, name, category, type)");
    return nullptr;
  }

  std::string id = json["id"].tryAsString().value_or("");
  std::string name = json["name"].tryAsString().value_or("");
  std::string categoryStr = json["category"].tryAsString().value_or("");
  std::string typeStr = json["type"].tryAsString().value_or("");

  if (id.empty() || name.empty() || categoryStr.empty() || typeStr.empty()) {
    RESOURCE_ERROR("ResourceFactory::createFromJson - Empty required fields");
    return nullptr;
  }

  // Convert strings to enums for potential fallback use
  ResourceCategory category = Resource::stringToCategory(categoryStr);
  ResourceType type = Resource::stringToType(typeStr);

  // Look for specialized creator based on type
  auto &creators = getCreators();
  auto creatorIt = creators.find(typeStr);
  if (creatorIt != creators.end()) {
    try {
      ResourcePtr resource = creatorIt->second(json);
      if (resource) {
        RESOURCE_DEBUG(std::format("ResourceFactory::createFromJson - Created {} resource: {}", typeStr, id));
        return resource;
      }
    } catch (const std::exception &ex) {
      RESOURCE_ERROR(std::format("ResourceFactory::createFromJson - Exception creating {}: {}",
                                 typeStr, ex.what()));
    }
  }

  // Fallback to base Resource creation using handles
  RESOURCE_WARN(std::format(
      "ResourceFactory::createFromJson - No specialized creator for type '{}', creating base Resource with category {}",
      typeStr, categoryStr));

  // Generate a handle for the new resource
  auto handle = ResourceTemplateManager::Instance().generateHandle();
  auto resource = std::make_shared<Resource>(handle, id, name, category, type);
  setCommonProperties(resource, json);
  return resource;
}

bool ResourceFactory::registerCreator(const std::string &typeName,
                                      ResourceCreator creator) {
  auto &creators = getCreators();
  if (creators.find(typeName) != creators.end()) {
    RESOURCE_DEBUG(std::format("ResourceFactory::registerCreator - Type '{}' already registered, skipping",
                               typeName));
    return false;
  }

  creators[typeName] = std::move(creator);
  RESOURCE_DEBUG(std::format("ResourceFactory::registerCreator - Registered creator for type: {}",
                             typeName));
  return true;
}

bool ResourceFactory::hasCreator(const std::string &typeName) {
  auto &creators = getCreators();
  return creators.find(typeName) != creators.end();
}

std::vector<std::string> ResourceFactory::getRegisteredTypes() {
  std::vector<std::string> types;
  const auto &creators = getCreators();
  types.reserve(creators.size());

  for (const auto &[typeName, creator] : creators) {
    types.push_back(typeName);
  }

  return types;
}

void ResourceFactory::initialize() {
  // Check if already initialized to avoid duplicate registrations
  const auto &creators = getCreators();
  if (!creators.empty()) {
    RESOURCE_DEBUG(std::format("ResourceFactory::initialize - Already initialized with {} resource creators",
                   creators.size()));
    return;
  }

  RESOURCE_INFO(
      "ResourceFactory::initialize - Registering default resource creators");

  // Register creators for all resource types
  registerCreator("Equipment", [](const JsonValue &json) -> ResourcePtr {
    return createEquipment(ResourceTemplateManager::Instance().generateHandle(),
                           json);
  });
  registerCreator("Consumable", [](const JsonValue &json) -> ResourcePtr {
    return createConsumable(
        ResourceTemplateManager::Instance().generateHandle(), json);
  });
  registerCreator("QuestItem", [](const JsonValue &json) -> ResourcePtr {
    return createQuestItem(ResourceTemplateManager::Instance().generateHandle(),
                           json);
  });
  registerCreator("Ammunition", [](const JsonValue &json) -> ResourcePtr {
    return createAmmunition(
        ResourceTemplateManager::Instance().generateHandle(), json);
  });
  registerCreator(
      "CraftingComponent", [](const JsonValue &json) -> ResourcePtr {
        return createMaterial(
            ResourceTemplateManager::Instance().generateHandle(), json);
      });
  registerCreator("RawResource", [](const JsonValue &json) -> ResourcePtr {
    return createMaterial(ResourceTemplateManager::Instance().generateHandle(),
                          json);
  });
  registerCreator("Gold", [](const JsonValue &json) -> ResourcePtr {
    return createCurrency(ResourceTemplateManager::Instance().generateHandle(),
                          json);
  });
  registerCreator("Gem", [](const JsonValue &json) -> ResourcePtr {
    return createCurrency(ResourceTemplateManager::Instance().generateHandle(),
                          json);
  });
  registerCreator("FactionToken", [](const JsonValue &json) -> ResourcePtr {
    return createCurrency(ResourceTemplateManager::Instance().generateHandle(),
                          json);
  });
  registerCreator("CraftingCurrency", [](const JsonValue &json) -> ResourcePtr {
    return createCurrency(ResourceTemplateManager::Instance().generateHandle(),
                          json);
  });

  RESOURCE_INFO(std::format("ResourceFactory::initialize - Registered {} resource creators",
                getCreators().size()));
}

void ResourceFactory::clear() {
  auto &creators = getCreators();
  creators.clear();
  RESOURCE_DEBUG("ResourceFactory::clear - Cleared all resource creators");
}


ResourcePtr
ResourceFactory::createEquipment(VoidLight::ResourceHandle handle,
                                 const JsonValue &json) {
  std::string id = json["id"].asString();
  std::string name = json["name"].asString();

  Equipment::EquipmentSlot slot = Equipment::EquipmentSlot::COUNT;
  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];
    if (props.hasKey("slot")) {
      std::string slotStr = props["slot"].tryAsString().value_or("Unknown");
      slot = Equipment::equipmentSlotFromString(slotStr)
                 .value_or(Equipment::EquipmentSlot::COUNT);
    }
  }

  auto equipment = std::make_shared<Equipment>(handle, id, name, slot);
  setCommonProperties(equipment, json);

  // Set equipment-specific properties
  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];
    equipment->setAttackBonus(props["attackBonus"].tryAsInt().value_or(0));
    equipment->setDefenseBonus(props["defenseBonus"].tryAsInt().value_or(0));
    equipment->setSpeedBonus(props["speedBonus"].tryAsInt().value_or(0));
    equipment->setHandsRequired(std::clamp(
        props["handsRequired"].tryAsInt().value_or(defaultHandsRequiredForSlot(slot)),
        0, 2));
    equipment->setWeaponMode(
        Equipment::weaponModeFromString(
            props["weaponMode"].tryAsString().value_or("None"))
            .value_or(Equipment::WeaponMode::None));
    equipment->setAttackRangeOverride(
        static_cast<float>(props["attackRange"].tryAsNumber().value_or(0.0)));
    equipment->setProjectileSpeedOverride(
        static_cast<float>(props["projectileSpeed"].tryAsNumber().value_or(0.0)));
    equipment->setAmmoTypeRequired(
        props["ammoTypeRequired"].tryAsString().value_or(""));

    if (props.hasKey("durability") && props.hasKey("maxDurability")) {
      equipment->setDurability(props["durability"].tryAsInt().value_or(100),
                               props["maxDurability"].tryAsInt().value_or(100));
    }
  }

  return equipment;
}

ResourcePtr
ResourceFactory::createConsumable(VoidLight::ResourceHandle handle,
                                  const JsonValue &json) {
  std::string id = json["id"].asString();
  std::string name = json["name"].asString();

  auto consumable = std::make_shared<Consumable>(handle, id, name);
  setCommonProperties(consumable, json);

  // Set consumable-specific properties
  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];

    // Map effect string to enum
    if (props.hasKey("effect")) {
      std::string effectStr = props["effect"].tryAsString().value_or("HealHP");
      Consumable::ConsumableEffect effect =
          Consumable::ConsumableEffect::HealHP;

      if (effectStr == "RestoreMP")
        effect = Consumable::ConsumableEffect::RestoreMP;
      else if (effectStr == "BoostAttack")
        effect = Consumable::ConsumableEffect::BoostAttack;
      else if (effectStr == "BoostDefense")
        effect = Consumable::ConsumableEffect::BoostDefense;
      else if (effectStr == "BoostSpeed")
        effect = Consumable::ConsumableEffect::BoostSpeed;
      else if (effectStr == "RestoreStamina")
        effect = Consumable::ConsumableEffect::RestoreStamina;
      else if (effectStr == "Teleport")
        effect = Consumable::ConsumableEffect::Teleport;

      consumable->setEffect(effect);
    }

    consumable->setEffectPower(props["effectPower"].tryAsInt().value_or(10));
    consumable->setEffectDuration(
        props["effectDuration"].tryAsInt().value_or(0));
  }

  return consumable;
}

ResourcePtr
ResourceFactory::createQuestItem(VoidLight::ResourceHandle handle,
                                 const JsonValue &json) {
  std::string id = json["id"].asString();
  std::string name = json["name"].asString();
  std::string questId = "";

  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];
    questId = props["questId"].tryAsString().value_or("");
  }

  auto questItem = std::make_shared<QuestItem>(handle, id, name, questId);
  setCommonProperties(questItem, json);

  return questItem;
}

ResourcePtr
ResourceFactory::createAmmunition(VoidLight::ResourceHandle handle,
                                  const JsonValue &json) {
  std::string id = json["id"].asString();
  std::string name = json["name"].asString();
  std::string ammoType = "Unknown";

  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];
    ammoType = props["ammoType"].tryAsString().value_or("Unknown");
  }

  auto ammunition = std::make_shared<Ammunition>(handle, id, name, ammoType);
  setCommonProperties(ammunition, json);
  ammunition->setConsumable(false);
  ammunition->setAmmoType(ammoType);
  return ammunition;
}

ResourcePtr ResourceFactory::createMaterial(VoidLight::ResourceHandle handle,
                                            const JsonValue &json) {
  std::string id = json["id"].asString();
  std::string name = json["name"].asString();
  std::string typeStr = json["type"].asString();

  ResourceType type = Resource::stringToType(typeStr);

  if (type == ResourceType::CraftingComponent) {
    // Determine component type from JSON or default to Metal
    CraftingComponent::ComponentType componentType =
        CraftingComponent::ComponentType::Metal;
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      if (props.hasKey("componentType")) {
        std::string compTypeStr =
            props["componentType"].tryAsString().value_or("Metal");
        if (compTypeStr == "Wood")
          componentType = CraftingComponent::ComponentType::Wood;
        else if (compTypeStr == "Leather")
          componentType = CraftingComponent::ComponentType::Leather;
        else if (compTypeStr == "Fabric")
          componentType = CraftingComponent::ComponentType::Fabric;
        else if (compTypeStr == "Gem")
          componentType = CraftingComponent::ComponentType::Gem;
        else if (compTypeStr == "Essence")
          componentType = CraftingComponent::ComponentType::Essence;
        else if (compTypeStr == "Crystal")
          componentType = CraftingComponent::ComponentType::Crystal;
        else if (compTypeStr == "Stone")
          componentType = CraftingComponent::ComponentType::Stone;
      }
    }

    auto craftingComponent =
        std::make_shared<CraftingComponent>(handle, id, name, componentType);
    setCommonProperties(craftingComponent, json);

    // Set crafting component specific properties
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      craftingComponent->setTier(props["tier"].tryAsInt().value_or(1));
      craftingComponent->setPurity(
          static_cast<float>(props["purity"].tryAsNumber().value_or(1.0)));
    }

    return craftingComponent;
  } else if (type == ResourceType::RawResource) {
    // Determine resource origin from JSON or default to Mining
    RawResource::ResourceOrigin origin = RawResource::ResourceOrigin::Mining;
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      if (props.hasKey("origin")) {
        std::string originStr =
            props["origin"].tryAsString().value_or("Mining");
        if (originStr == "Logging")
          origin = RawResource::ResourceOrigin::Logging;
        else if (originStr == "Harvesting")
          origin = RawResource::ResourceOrigin::Harvesting;
        else if (originStr == "Hunting")
          origin = RawResource::ResourceOrigin::Hunting;
        else if (originStr == "Fishing")
          origin = RawResource::ResourceOrigin::Fishing;
        else if (originStr == "Monster")
          origin = RawResource::ResourceOrigin::Monster;
      }
    }

    auto rawResource = std::make_shared<RawResource>(handle, id, name, origin);
    setCommonProperties(rawResource, json);

    // Set raw resource specific properties
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      rawResource->setTier(props["tier"].tryAsInt().value_or(1));
      rawResource->setRarity(props["rarity"].tryAsInt().value_or(1));
    }

    return rawResource;
  }

  // Fallback to base Material class
  auto material = std::make_shared<Material>(handle, id, name, type);
  setCommonProperties(material, json);

  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];
    material->setTier(props["tier"].tryAsInt().value_or(1));
  }

  return material;
}
ResourcePtr ResourceFactory::createCurrency(VoidLight::ResourceHandle handle,
                                            const JsonValue &json) {
  std::string id = json["id"].asString();
  std::string name = json["name"].asString();
  std::string typeStr = json["type"].asString();

  ResourceType type = Resource::stringToType(typeStr);

  if (type == ResourceType::Gold) {
    auto gold = std::make_shared<Gold>(handle, id, name);
    setCommonProperties(gold, json);

    // Set currency-specific properties
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      gold->setExchangeRate(static_cast<float>(
          props["exchangeRate"].tryAsNumber().value_or(1.0)));
    }

    return gold;
  } else if (type == ResourceType::Gem) {
    // Determine gem type from JSON or default to Ruby
    Gem::GemType gemType = Gem::GemType::Ruby;
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      if (props.hasKey("gemType")) {
        std::string gemTypeStr =
            props["gemType"].tryAsString().value_or("Ruby");
        if (gemTypeStr == "Emerald")
          gemType = Gem::GemType::Emerald;
        else if (gemTypeStr == "Sapphire")
          gemType = Gem::GemType::Sapphire;
        else if (gemTypeStr == "Diamond")
          gemType = Gem::GemType::Diamond;
      }
    }

    auto gem = std::make_shared<Gem>(handle, id, name, gemType);
    setCommonProperties(gem, json);

    // Set gem-specific properties
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      gem->setExchangeRate(static_cast<float>(
          props["exchangeRate"].tryAsNumber().value_or(1.0)));
      gem->setClarity(props["clarity"].tryAsInt().value_or(5));
    }

    return gem;
  } else if (type == ResourceType::FactionToken) {
    std::string factionId = "";
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      factionId = props["factionId"].tryAsString().value_or("");
    }

    auto factionToken =
        std::make_shared<FactionToken>(handle, id, name, factionId);
    setCommonProperties(factionToken, json);

    // Set faction token specific properties
    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      factionToken->setExchangeRate(static_cast<float>(
          props["exchangeRate"].tryAsNumber().value_or(1.0)));
      factionToken->setReputation(props["reputation"].tryAsInt().value_or(0));
    }

    return factionToken;
  } else if (type == ResourceType::CraftingCurrency) {
    auto craftingCurrency = std::make_shared<CraftingCurrency>(handle, id, name);
    setCommonProperties(craftingCurrency, json);

    if (json.hasKey("properties") && json["properties"].isObject()) {
      const JsonValue &props = json["properties"];
      craftingCurrency->setExchangeRate(static_cast<float>(
          props["exchangeRate"].tryAsNumber().value_or(0.0)));
    }

    return craftingCurrency;
  }

  // Fallback to base Currency class
  auto currency = std::make_shared<Currency>(handle, id, name, type);
  setCommonProperties(currency, json);

  if (json.hasKey("properties") && json["properties"].isObject()) {
    const JsonValue &props = json["properties"];
    currency->setExchangeRate(
        static_cast<float>(props["exchangeRate"].tryAsNumber().value_or(1.0)));
  }

  return currency;
}
void ResourceFactory::setCommonProperties(const ResourcePtr& resource,
                                          const JsonValue &json) {
  if (!resource)
    return;

  // Set basic properties
  if (json.hasKey("description")) {
    resource->setDescription(json["description"].tryAsString().value_or(""));
  }

  if (json.hasKey("value")) {
    resource->setValue(
        static_cast<float>(json["value"].tryAsNumber().value_or(0.0)));
  }

  if (json.hasKey("maxStackSize")) {
    resource->setMaxStackSize(json["maxStackSize"].tryAsInt().value_or(1));
  }

  if (json.hasKey("consumable")) {
    resource->setConsumable(json["consumable"].tryAsBool().value_or(false));
  }

  // Support unified textureId field (sets both icon and world texture)
  if (json.hasKey("textureId")) {
    std::string textureId = json["textureId"].tryAsString().value_or("");
    resource->setIconTextureId(textureId);
    resource->setWorldTextureId(textureId);
  }

  // Legacy support: separate iconTextureId/worldTextureId override unified field
  if (json.hasKey("iconTextureId")) {
    resource->setIconTextureId(
        json["iconTextureId"].tryAsString().value_or(""));
  }

  if (json.hasKey("worldTextureId")) {
    resource->setWorldTextureId(
        json["worldTextureId"].tryAsString().value_or(""));
  }

  if (json.hasKey("numFrames")) {
    resource->setNumFrames(json["numFrames"].tryAsInt().value_or(0));
  }

  if (json.hasKey("animSpeed")) {
    resource->setAnimSpeed(json["animSpeed"].tryAsInt().value_or(0));
  }

  // Note: Atlas coordinates (atlasX/Y/W/H) are now looked up from atlas.json
  // in ResourceTemplateManager::createDefaultResources(), not read from
  // resource JSON files. This allows a single source of truth for sprite coords.
}

} // namespace VoidLight
