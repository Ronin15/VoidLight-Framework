/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/Resource.hpp"
#include "core/Logger.hpp"
#include <format>
#include <unordered_map>

Resource::Resource(VoidLight::ResourceHandle handle, const std::string &id,
                   const std::string &name, ResourceCategory category,
                   ResourceType type)
    : m_handle(handle), m_id(id), m_name(name), m_category(category),
      m_type(type) {

  // Set default properties based on category
  switch (category) {
  case ResourceCategory::Item:
    m_value = 10.0f;
    m_weight = 1.0f;
    m_maxStackSize = 10;
    m_isStackable = true;
    m_isConsumable = false;
    break;
  case ResourceCategory::Material:
    m_value = 5.0f;
    m_weight = 0.5f;
    m_maxStackSize = 50;
    m_isStackable = true;
    m_isConsumable = false;
    break;
  case ResourceCategory::Currency:
    m_value = 1.0f;
    m_weight = 0.01f;
    m_maxStackSize = 999999;
    m_isStackable = true;
    m_isConsumable = false;
    break;
  default:
    m_value = 1.0f;
    m_weight = 1.0f;
    m_maxStackSize = 1;
    m_isStackable = false;
    m_isConsumable = false;
    break;
  }

  RESOURCE_INFO(std::format("Created resource: {} (Handle: {})", m_name, m_handle.toString()));
}

std::string Resource::categoryToString(ResourceCategory category) {
  static const std::unordered_map<ResourceCategory, std::string> categoryMap = {
      {ResourceCategory::Item, "Item"},
      {ResourceCategory::Material, "Material"},
      {ResourceCategory::Currency, "Currency"}};

  auto it = categoryMap.find(category);
  return (it != categoryMap.end()) ? it->second : "Unknown";
}

std::string Resource::typeToString(ResourceType type) {
  static const std::unordered_map<ResourceType, std::string> typeMap = {
      // Items
      {ResourceType::Equipment, "Equipment"},
      {ResourceType::Consumable, "Consumable"},
      {ResourceType::QuestItem, "QuestItem"},
      {ResourceType::Ammunition, "Ammunition"},

      // Materials
      {ResourceType::CraftingComponent, "CraftingComponent"},
      {ResourceType::RawResource, "RawResource"},

      // Currency
      {ResourceType::Gold, "Gold"},
      {ResourceType::Gem, "Gem"},
      {ResourceType::FactionToken, "FactionToken"},
      {ResourceType::CraftingCurrency, "CraftingCurrency"}};

  auto it = typeMap.find(type);
  return (it != typeMap.end()) ? it->second : "Unknown";
}

ResourceCategory Resource::stringToCategory(const std::string &categoryStr) {
  static const std::unordered_map<std::string, ResourceCategory> categoryMap = {
      {"Item", ResourceCategory::Item},
      {"Material", ResourceCategory::Material},
      {"Currency", ResourceCategory::Currency}};

  auto it = categoryMap.find(categoryStr);
  return (it != categoryMap.end()) ? it->second : ResourceCategory::Item;
}

ResourceType Resource::stringToType(const std::string &typeStr) {
  static const std::unordered_map<std::string, ResourceType> typeMap = {
      // Items
      {"Equipment", ResourceType::Equipment},
      {"Consumable", ResourceType::Consumable},
      {"QuestItem", ResourceType::QuestItem},
      {"Ammunition", ResourceType::Ammunition},

      // Materials
      {"CraftingComponent", ResourceType::CraftingComponent},
      {"RawResource", ResourceType::RawResource},

      // Currency
      {"Gold", ResourceType::Gold},
      {"Gem", ResourceType::Gem},
      {"FactionToken", ResourceType::FactionToken},
      {"CraftingCurrency", ResourceType::CraftingCurrency}};

  auto it = typeMap.find(typeStr);
  return (it != typeMap.end()) ? it->second : ResourceType::Equipment;
}
