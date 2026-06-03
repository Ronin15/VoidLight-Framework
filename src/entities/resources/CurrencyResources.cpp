/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/resources/CurrencyResources.hpp"
#include <unordered_map>

// Currency base class implementation
Currency::Currency(VoidLight::ResourceHandle handle, const std::string &id,
                   const std::string &name, ResourceType type)
    : Resource(handle, id, name, ResourceCategory::Currency, type) {
  // Currencies are highly stackable
  setMaxStackSize(9999999);
  setConsumable(false);
}

// Gold currency implementation
Gold::Gold(VoidLight::ResourceHandle handle, const std::string &id,
           const std::string &name)
    : Currency(handle, id, name, ResourceType::Gold) {
  setValue(1.0f);
  setExchangeRate(1.0f); // Base currency
}

// Gem currency implementation
Gem::Gem(VoidLight::ResourceHandle handle, const std::string &id,
         const std::string &name, GemType gemType)
    : Currency(handle, id, name, ResourceType::Gem), m_gemType(gemType) {
  // Set default values based on gem type
  switch (gemType) {
  case GemType::Ruby:
    setValue(10.0f);
    setExchangeRate(10.0f);
    setClarity(5);
    break;
  case GemType::Emerald:
    setValue(12.0f);
    setExchangeRate(12.0f);
    setClarity(6);
    break;
  case GemType::Sapphire:
    setValue(15.0f);
    setExchangeRate(15.0f);
    setClarity(7);
    break;
  case GemType::Diamond:
    setValue(25.0f);
    setExchangeRate(25.0f);
    setClarity(8);
    break;
  case GemType::COUNT:
    break;
  }
}

std::string Gem::gemTypeToString(GemType type) {
  static const std::unordered_map<GemType, std::string> typeMap = {
      {GemType::Ruby, "Ruby"},
      {GemType::Emerald, "Emerald"},
      {GemType::Sapphire, "Sapphire"},
      {GemType::Diamond, "Diamond"}};

  auto it = typeMap.find(type);
  return (it != typeMap.end()) ? it->second : "Unknown";
}

// FactionToken implementation
FactionToken::FactionToken(VoidLight::ResourceHandle handle,
                           const std::string &id, const std::string &name,
                           const std::string &factionId)
    : Currency(handle, id, name, ResourceType::FactionToken),
      m_factionId(factionId) {
  setValue(1.0f);
  setExchangeRate(0.0f); // Cannot be exchanged for gold
}

CraftingCurrency::CraftingCurrency(VoidLight::ResourceHandle handle,
                                   const std::string &id,
                                   const std::string &name)
    : Currency(handle, id, name, ResourceType::CraftingCurrency) {
  setValue(1.0f);
  setExchangeRate(0.0f);
}
