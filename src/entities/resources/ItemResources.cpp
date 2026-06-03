/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/resources/ItemResources.hpp"
#include <unordered_map>

// Item base class implementation
Item::Item(VoidLight::ResourceHandle handle, const std::string &id,
           const std::string &name, ResourceType type)
    : Resource(handle, id, name, ResourceCategory::Item, type) {
  // Items are moderately stackable
  setMaxStackSize(50);
  setConsumable(false);
}

void Item::setDurability(int durability, int maxDurability) {
  m_durability = durability;
  m_maxDurability = maxDurability;
}

// Consumable implementation
Consumable::Consumable(VoidLight::ResourceHandle handle,
                       const std::string &id, const std::string &name)
    : Item(handle, id, name, ResourceType::Consumable) {
  // Consumables are highly stackable
  setMaxStackSize(100);
  setConsumable(true);
}

std::string Consumable::consumableEffectToString(ConsumableEffect effect) {
  static const std::unordered_map<ConsumableEffect, std::string> effectMap = {
      {ConsumableEffect::HealHP, "Heal HP"},
      {ConsumableEffect::RestoreMP, "Restore MP"},
      {ConsumableEffect::BoostAttack, "Boost Attack"},
      {ConsumableEffect::BoostDefense, "Boost Defense"},
      {ConsumableEffect::BoostSpeed, "Boost Speed"},
      {ConsumableEffect::RestoreStamina, "Restore Stamina"},
      {ConsumableEffect::Teleport, "Teleport"}};

  auto it = effectMap.find(effect);
  return (it != effectMap.end()) ? it->second : "Unknown";
}

// QuestItem implementation
QuestItem::QuestItem(VoidLight::ResourceHandle handle, const std::string &id,
                     const std::string &name, const std::string &questId)
    : Item(handle, id, name, ResourceType::QuestItem), m_questId(questId) {
  // Quest items are not stackable and not consumable
  setMaxStackSize(1);
  setConsumable(false);
}

Ammunition::Ammunition(VoidLight::ResourceHandle handle, const std::string &id,
                       const std::string &name, const std::string &ammoType)
    : Item(handle, id, name, ResourceType::Ammunition), m_ammoType(ammoType) {
  setMaxStackSize(200);
  setConsumable(true);
}
