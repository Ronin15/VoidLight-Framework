/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef ITEM_RESOURCES_HPP
#define ITEM_RESOURCES_HPP

#include "entities/Resource.hpp"

#include <string>

/**
 * @brief Base class for all item resources (equipment, consumables, quest
 * items)
 */
class Item : public Resource {
public:
  Item(VoidLight::ResourceHandle handle, const std::string &id,
       const std::string &name, ResourceType type);
  ~Item() override = default;

  // Item-specific properties
  int getDurability() const { return m_durability; }
  int getMaxDurability() const { return m_maxDurability; }
  void setDurability(int durability, int maxDurability);

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

protected:
  int m_durability{100};    // Current durability
  int m_maxDurability{100}; // Maximum durability
};

/**
 * @brief Consumable items (potions, food, scrolls)
 */
class Consumable : public Item {
public:
  enum class ConsumableEffect : uint8_t {
    HealHP = 0,
    RestoreMP = 1,
    BoostAttack = 2,
    BoostDefense = 3,
    BoostSpeed = 4,
    RestoreStamina = 5,
    Teleport = 6,
    COUNT = 7
  };

  Consumable(VoidLight::ResourceHandle handle, const std::string &id,
             const std::string &name);
  ~Consumable() override = default;

  ConsumableEffect getEffect() const { return m_effect; }
  int getEffectPower() const { return m_effectPower; }
  int getEffectDuration() const { return m_effectDuration; }

  void setEffect(ConsumableEffect effect) { m_effect = effect; }
  void setEffectPower(int power) { m_effectPower = power; }
  void setEffectDuration(int duration) { m_effectDuration = duration; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

  static std::string consumableEffectToString(ConsumableEffect effect);

private:
  ConsumableEffect m_effect{ConsumableEffect::HealHP};
  int m_effectPower{10};   // Strength of the effect
  int m_effectDuration{0}; // Duration in seconds (0 = instant)
};

/**
 * @brief Quest items (keys, documents, special objects)
 */
class QuestItem : public Item {
public:
  QuestItem(VoidLight::ResourceHandle handle, const std::string &id,
            const std::string &name, const std::string &questId = "");
  ~QuestItem() override = default;

  const std::string &getQuestId() const { return m_questId; }
  bool isQuestSpecific() const { return !m_questId.empty(); }

  void setQuestId(const std::string &questId) { m_questId = questId; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

private:
  std::string m_questId{""}; // Associated quest ID (empty = general quest item)
};

/**
 * @brief Ammunition items consumed by compatible ranged weapons
 */
class Ammunition : public Item {
public:
  Ammunition(VoidLight::ResourceHandle handle, const std::string &id,
             const std::string &name, const std::string &ammoType);
  ~Ammunition() override = default;

  const std::string &getAmmoType() const { return m_ammoType; }
  void setAmmoType(const std::string &ammoType) { m_ammoType = ammoType; }

private:
  std::string m_ammoType;
};

#endif // ITEM_RESOURCES_HPP
