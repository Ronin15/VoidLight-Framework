/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef MATERIAL_RESOURCES_HPP
#define MATERIAL_RESOURCES_HPP

#include "entities/Resource.hpp"

/**
 * @brief Base class for all material resources (crafting components, raw
 * resources)
 */
class Material : public Resource {
public:
  Material(VoidLight::ResourceHandle handle, const std::string &id,
           const std::string &name, ResourceType type);
  ~Material() override = default;

  // Material-specific properties
  int getTier() const { return m_tier; }
  void setTier(int tier);

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

protected:
  int m_tier{1}; // Material tier/quality (1-10)
};

/**
 * @brief Crafting components (processed materials for crafting)
 */
class CraftingComponent : public Material {
public:
  enum class ComponentType : uint8_t {
    Metal = 0,
    Wood = 1,
    Leather = 2,
    Fabric = 3,
    Gem = 4,
    Essence = 5,
    Crystal = 6,
    Stone = 7,
    COUNT = 8
  };

  CraftingComponent(VoidLight::ResourceHandle handle, const std::string &id,
                    const std::string &name, ComponentType componentType);
  ~CraftingComponent() override = default;

  ComponentType getComponentType() const { return m_componentType; }
  float getPurity() const { return m_purity; }
  void setPurity(float purity) { m_purity = purity; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

  static std::string componentTypeToString(ComponentType type);

private:
  ComponentType m_componentType;
  float m_purity{1.0f}; // Purity affects crafting quality (0.0 - 1.0)
};

/**
 * @brief Raw resources (unprocessed materials from gathering)
 */
class RawResource : public Material {
public:
  enum class ResourceOrigin : uint8_t {
    Mining = 0,
    Logging = 1,
    Harvesting = 2,
    Hunting = 3,
    Fishing = 4,
    Monster = 5,
    COUNT = 6
  };

  RawResource(VoidLight::ResourceHandle handle, const std::string &id,
              const std::string &name, ResourceOrigin origin);
  ~RawResource() override = default;

  ResourceOrigin getOrigin() const { return m_origin; }
  int getRarity() const { return m_rarity; }
  void setRarity(int rarity) { m_rarity = rarity; }

  // TODO: Implement proper serialization later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

  static std::string resourceOriginToString(ResourceOrigin origin);

private:
  ResourceOrigin m_origin;
  int m_rarity{1}; // Rarity level (1-10, higher = more rare)
};

#endif // MATERIAL_RESOURCES_HPP
