/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/resources/EquipmentResources.hpp"

#include <algorithm>

namespace {

constexpr std::array<Equipment::EquipmentSlotInfo, Equipment::SLOT_COUNT>
    EQUIPMENT_SLOT_DEFINITIONS{{
        {Equipment::EquipmentSlot::Weapon, "weapon", "Weapon"},
        {Equipment::EquipmentSlot::Shield, "shield", "Shield"},
        {Equipment::EquipmentSlot::Helmet, "helmet", "Helmet"},
        {Equipment::EquipmentSlot::Chest, "chest", "Chest"},
        {Equipment::EquipmentSlot::Legs, "legs", "Legs"},
        {Equipment::EquipmentSlot::Boots, "boots", "Boots"},
        {Equipment::EquipmentSlot::Gloves, "gloves", "Gloves"},
        {Equipment::EquipmentSlot::Ring, "ring", "Ring"},
        {Equipment::EquipmentSlot::Necklace, "necklace", "Necklace"},
    }};

} // namespace

Equipment::Equipment(VoidLight::ResourceHandle handle, const std::string &id,
                     const std::string &name, EquipmentSlot slot)
    : Item(handle, id, name, ResourceType::Equipment), m_equipmentSlot(slot) {
  setMaxStackSize(1);
  setConsumable(false);
}

void Equipment::setHandsRequired(int handsRequired) {
  m_handsRequired = std::clamp(handsRequired, 0, 2);
}

void Equipment::setAttackRangeOverride(float attackRange) {
  m_attackRangeOverride = std::max(0.0f, attackRange);
}

void Equipment::setProjectileSpeedOverride(float projectileSpeed) {
  m_projectileSpeedOverride = std::max(0.0f, projectileSpeed);
}

std::string Equipment::equipmentSlotToString(EquipmentSlot slot) {
  const auto slotIndex = equipmentSlotIndex(slot);
  if (!slotIndex) {
    return "Unknown";
  }

  return std::string(EQUIPMENT_SLOT_DEFINITIONS[*slotIndex].label);
}

std::optional<Equipment::WeaponMode>
Equipment::weaponModeFromString(std::string_view modeName) {
  if (modeName == "melee" || modeName == "Melee") {
    return WeaponMode::Melee;
  }
  if (modeName == "ranged" || modeName == "Ranged") {
    return WeaponMode::Ranged;
  }
  if (modeName == "none" || modeName == "None") {
    return WeaponMode::None;
  }

  return std::nullopt;
}

const std::array<Equipment::EquipmentSlotInfo, Equipment::SLOT_COUNT> &
Equipment::equipmentSlotDefinitions() {
  return EQUIPMENT_SLOT_DEFINITIONS;
}

std::optional<Equipment::EquipmentSlot>
Equipment::equipmentSlotFromString(std::string_view slotName) {
  const auto it = std::ranges::find_if(
      EQUIPMENT_SLOT_DEFINITIONS,
      [slotName](const EquipmentSlotInfo &definition) {
        return definition.id == slotName || definition.label == slotName;
      });
  if (it == EQUIPMENT_SLOT_DEFINITIONS.end()) {
    return std::nullopt;
  }

  return it->slot;
}

std::optional<size_t> Equipment::equipmentSlotIndex(EquipmentSlot slot) {
  const size_t slotIndex = static_cast<size_t>(slot);
  if (slotIndex >= EQUIPMENT_SLOT_DEFINITIONS.size() ||
      EQUIPMENT_SLOT_DEFINITIONS[slotIndex].slot != slot) {
    return std::nullopt;
  }

  return slotIndex;
}

std::optional<size_t> Equipment::equipmentSlotIndex(std::string_view slotId) {
  const auto slot = equipmentSlotFromString(slotId);
  if (!slot) {
    return std::nullopt;
  }

  return equipmentSlotIndex(*slot);
}
