/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef EQUIPMENT_RESOURCES_HPP
#define EQUIPMENT_RESOURCES_HPP

#include "entities/resources/ItemResources.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Equipment items (weapons, armor, accessories)
 */
class Equipment : public Item {
public:
  enum class EquipmentSlot : uint8_t {
    Weapon = 0,
    Shield = 1,
    Helmet = 2,
    Chest = 3,
    Legs = 4,
    Boots = 5,
    Gloves = 6,
    Ring = 7,
    Necklace = 8,
    COUNT = 9
  };

  enum class WeaponMode : uint8_t {
    None = 0,
    Melee = 1,
    Ranged = 2
  };

  static constexpr size_t SLOT_COUNT =
      static_cast<size_t>(EquipmentSlot::COUNT);

  struct EquipmentSlotInfo {
    EquipmentSlot slot;
    std::string_view id;
    std::string_view label;
  };

  Equipment(VoidLight::ResourceHandle handle, const std::string &id,
            const std::string &name, EquipmentSlot slot);
  ~Equipment() override = default;

  EquipmentSlot getEquipmentSlot() const { return m_equipmentSlot; }
  int getAttackBonus() const { return m_attackBonus; }
  int getDefenseBonus() const { return m_defenseBonus; }
  int getSpeedBonus() const { return m_speedBonus; }
  int getHandsRequired() const { return m_handsRequired; }
  WeaponMode getWeaponMode() const { return m_weaponMode; }
  float getAttackRangeOverride() const { return m_attackRangeOverride; }
  float getProjectileSpeedOverride() const { return m_projectileSpeedOverride; }
  const std::string &getAmmoTypeRequired() const { return m_ammoTypeRequired; }

  void setAttackBonus(int bonus) { m_attackBonus = bonus; }
  void setDefenseBonus(int bonus) { m_defenseBonus = bonus; }
  void setSpeedBonus(int bonus) { m_speedBonus = bonus; }
  void setHandsRequired(int handsRequired);
  void setWeaponMode(WeaponMode mode) { m_weaponMode = mode; }
  void setAttackRangeOverride(float attackRange);
  void setProjectileSpeedOverride(float projectileSpeed);
  void setAmmoTypeRequired(const std::string &ammoType) { m_ammoTypeRequired = ammoType; }

  static std::string equipmentSlotToString(EquipmentSlot slot);
  static std::optional<WeaponMode> weaponModeFromString(std::string_view modeName);
  static const std::array<EquipmentSlotInfo, SLOT_COUNT> &
  equipmentSlotDefinitions();
  static std::optional<EquipmentSlot>
  equipmentSlotFromString(std::string_view slotName);
  static std::optional<size_t> equipmentSlotIndex(EquipmentSlot slot);
  static std::optional<size_t> equipmentSlotIndex(std::string_view slotId);

private:
  EquipmentSlot m_equipmentSlot;
  int m_attackBonus{0};
  int m_defenseBonus{0};
  int m_speedBonus{0};
  int m_handsRequired{0};
  WeaponMode m_weaponMode{WeaponMode::None};
  float m_attackRangeOverride{0.0f};
  float m_projectileSpeedOverride{0.0f};
  std::string m_ammoTypeRequired;
};

#endif // EQUIPMENT_RESOURCES_HPP
