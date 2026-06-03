/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/resources/MaterialResources.hpp"
#include <unordered_map>

// Material base class implementation
Material::Material(VoidLight::ResourceHandle handle, const std::string &id,
                   const std::string &name, ResourceType type)
    : Resource(handle, id, name, ResourceCategory::Material, type) {
  // Materials are generally stackable
  setMaxStackSize(999);
  setConsumable(false);
}

void Material::setTier(int tier) { m_tier = std::max(1, std::min(tier, 10)); }

// CraftingComponent implementation
CraftingComponent::CraftingComponent(VoidLight::ResourceHandle handle,
                                     const std::string &id,
                                     const std::string &name,
                                     ComponentType componentType)
    : Material(handle, id, name, ResourceType::CraftingComponent),
      m_componentType(componentType) {
  // Set default properties based on component type
  switch (componentType) {
  case ComponentType::Metal:
    setTier(2);
    setValue(5.0f);
    break;
  case ComponentType::Wood:
    setTier(1);
    setValue(2.0f);
    break;
  case ComponentType::Leather:
    setTier(2);
    setValue(4.0f);
    break;
  case ComponentType::Fabric:
    setTier(1);
    setValue(3.0f);
    break;
  case ComponentType::Gem:
    setTier(4);
    setValue(15.0f);
    break;
  case ComponentType::Essence:
    setTier(3);
    setValue(10.0f);
    break;
  case ComponentType::Crystal:
    setTier(5);
    setValue(25.0f);
    break;
  case ComponentType::Stone:
    setTier(2);
    setValue(4.0f);
    break;
  case ComponentType::COUNT:
    break;
  }
}

std::string CraftingComponent::componentTypeToString(ComponentType type) {
  static const std::unordered_map<ComponentType, std::string> typeMap = {
      {ComponentType::Metal, "Metal"},     {ComponentType::Wood, "Wood"},
      {ComponentType::Leather, "Leather"}, {ComponentType::Fabric, "Fabric"},
      {ComponentType::Gem, "Gem"},         {ComponentType::Essence, "Essence"},
      {ComponentType::Crystal, "Crystal"}, {ComponentType::Stone, "Stone"}};

  auto it = typeMap.find(type);
  return (it != typeMap.end()) ? it->second : "Unknown";
}

// RawResource implementation
RawResource::RawResource(VoidLight::ResourceHandle handle,
                         const std::string &id, const std::string &name,
                         ResourceOrigin origin)
    : Material(handle, id, name, ResourceType::RawResource), m_origin(origin) {
  // Set default properties based on origin
  switch (origin) {
  case ResourceOrigin::Mining:
    setTier(2);
    setValue(3.0f);
    m_rarity = 3;
    break;
  case ResourceOrigin::Logging:
    setTier(1);
    setValue(1.0f);
    m_rarity = 2;
    break;
  case ResourceOrigin::Harvesting:
    setTier(1);
    setValue(2.0f);
    m_rarity = 2;
    break;
  case ResourceOrigin::Hunting:
    setTier(2);
    setValue(4.0f);
    m_rarity = 4;
    break;
  case ResourceOrigin::Fishing:
    setTier(1);
    setValue(2.5f);
    m_rarity = 3;
    break;
  case ResourceOrigin::Monster:
    setTier(3);
    setValue(8.0f);
    m_rarity = 6;
    break;
  case ResourceOrigin::COUNT:
    break;
  }
}

std::string RawResource::resourceOriginToString(ResourceOrigin origin) {
  static const std::unordered_map<ResourceOrigin, std::string> originMap = {
      {ResourceOrigin::Mining, "Mining"},
      {ResourceOrigin::Logging, "Logging"},
      {ResourceOrigin::Harvesting, "Harvesting"},
      {ResourceOrigin::Hunting, "Hunting"},
      {ResourceOrigin::Fishing, "Fishing"},
      {ResourceOrigin::Monster, "Monster"}};

  auto it = originMap.find(origin);
  return (it != originMap.end()) ? it->second : "Unknown";
}
