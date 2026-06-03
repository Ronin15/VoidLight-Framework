/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CURRENCY_RESOURCES_HPP
#define CURRENCY_RESOURCES_HPP

#include "entities/Resource.hpp"

/**
 * @brief Base class for currency resources (gold, gems, faction tokens)
 */
class Currency : public Resource {
public:
  Currency(VoidLight::ResourceHandle handle, const std::string &id,
           const std::string &name, ResourceType type);
  ~Currency() override = default;

  // Currency-specific properties
  float getExchangeRate() const { return m_exchangeRate; }
  void setExchangeRate(float rate) { m_exchangeRate = rate; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

protected:
  float m_exchangeRate{1.0f}; // Exchange rate to base currency (gold)
};

/**
 * @brief Gold currency (base currency)
 */
class Gold : public Currency {
public:
  Gold(VoidLight::ResourceHandle handle, const std::string &id,
       const std::string &name);
  ~Gold() override = default;
};

/**
 * @brief Gem currency (precious stones)
 */
class Gem : public Currency {
public:
  enum class GemType : uint8_t {
    Ruby = 0,
    Emerald = 1,
    Sapphire = 2,
    Diamond = 3,
    COUNT = 4
  };

  Gem(VoidLight::ResourceHandle handle, const std::string &id,
      const std::string &name, GemType gemType);
  ~Gem() override = default;

  GemType getGemType() const { return m_gemType; }
  int getClarity() const { return m_clarity; }
  void setClarity(int clarity) { m_clarity = clarity; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

  static std::string gemTypeToString(GemType type);

private:
  GemType m_gemType;
  int m_clarity{5}; // Clarity rating 1-10 (affects value)
};

/**
 * @brief Faction token currency (reputation-based currency)
 */
class FactionToken : public Currency {
public:
  FactionToken(VoidLight::ResourceHandle handle, const std::string &id,
               const std::string &name, const std::string &factionId);
  ~FactionToken() override = default;

  const std::string &getFactionId() const { return m_factionId; }
  int getReputation() const { return m_reputation; }
  void setReputation(int reputation) { m_reputation = reputation; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

private:
  std::string m_factionId;
  int m_reputation{0}; // Required reputation to earn this token
};

/**
 * @brief Crafting currency (essence, energy, and other recipe spendables)
 */
class CraftingCurrency : public Currency {
public:
  CraftingCurrency(VoidLight::ResourceHandle handle, const std::string &id,
                   const std::string &name);
  ~CraftingCurrency() override = default;

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;
};

#endif // CURRENCY_RESOURCES_HPP
