/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef RESOURCE_HPP
#define RESOURCE_HPP

#include "utils/ResourceHandle.hpp"
#include <memory>
#include <string>

// Forward declarations
class Resource;
using ResourcePtr = std::shared_ptr<Resource>;
using ResourceWeakPtr = std::weak_ptr<Resource>;

/**
 * @brief Resource category enumeration for organization and filtering
 */
enum class ResourceCategory : uint8_t {
  Item = 0,         // Equipment, consumables, quest items
  Material = 1,     // Crafting components, raw resources
  Currency = 2,     // Gold, gems, faction tokens
  COUNT = 3
};

/**
 * @brief Resource type enumeration for specific resource identification
 */
enum class ResourceType : uint8_t {
  // Items
  Equipment = 0,
  Consumable = 1,
  QuestItem = 2,
  Ammunition = 3,

  // Materials
  CraftingComponent = 10,
  RawResource = 11,

  // Currency
  Gold = 20,
  Gem = 21,
  FactionToken = 22,
  CraftingCurrency = 23,

  COUNT = 24
};

/**
 * @brief Pure data class for resource templates.
 * 
 * Resources are immutable data definitions that specify properties
 * of items, materials, and currency. They do NOT
 * inherit from Entity and have no world state or behavior.
 */
class Resource {
public:
  Resource(VoidLight::ResourceHandle handle, const std::string &id,
           const std::string &name, ResourceCategory category,
           ResourceType type);
  virtual ~Resource() = default;
  // Resource properties (immutable)
  VoidLight::ResourceHandle getHandle() const { return m_handle; }
  const std::string &getId() const { return m_id; }
  const std::string &getName() const { return m_name; }
  const std::string &getDescription() const { return m_description; }
  ResourceCategory getCategory() const { return m_category; }
  ResourceType getType() const { return m_type; }
  float getValue() const { return m_value; }
  int getMaxStackSize() const { return m_maxStackSize; }
  bool isStackable() const { return m_isStackable; }
  bool isConsumable() const { return m_isConsumable; }
  float getWeight() const { return m_weight; }
  const std::string &getIconTextureId() const { return m_iconTextureId; }
  const std::string &getWorldTextureId() const { return m_worldTextureId; }
  int getNumFrames() const { return m_numFrames; }
  int getAnimSpeed() const { return m_animSpeed; }

  // Atlas coordinates (for sprite atlas rendering)
  int getAtlasX() const { return m_atlasX; }
  int getAtlasY() const { return m_atlasY; }
  int getAtlasW() const { return m_atlasW; }
  int getAtlasH() const { return m_atlasH; }

  // Property setters (for initialization only)
  void setDescription(const std::string &description) {
    m_description = description;
  }
  void setValue(float value) { m_value = value; }
  void setWeight(float weightValue) { m_weight = weightValue; }
  void setMaxStackSize(int maxStack) {
    m_maxStackSize = maxStack;
    m_isStackable = (maxStack > 1);
  }
  void setConsumable(bool consumable) { m_isConsumable = consumable; }
  void setIconTextureId(const std::string &textureId) {
    m_iconTextureId = textureId;
  }
  void setWorldTextureId(const std::string &textureId) {
    m_worldTextureId = textureId;
  }
  void setNumFrames(int frames) {
    m_numFrames = frames;
  }
  void setAnimSpeed(int speed) {
    m_animSpeed = speed;
  }
  void setAtlasX(int x) { m_atlasX = x; }
  void setAtlasY(int y) { m_atlasY = y; }
  void setAtlasW(int w) { m_atlasW = w; }
  void setAtlasH(int h) { m_atlasH = h; }

  // Factory method for proper shared_ptr creation
  template <typename T, typename... Args>
  static std::shared_ptr<T> create(Args &&...args) {
    static_assert(std::is_base_of_v<Resource, T>,
                  "T must derive from Resource");
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  // TODO: Implement proper ISerializable interface later
  // bool serialize(std::ostream &stream) const override;
  // bool deserialize(std::istream &stream) override;

  // Utility functions
  static std::string categoryToString(ResourceCategory category);
  static std::string typeToString(ResourceType type);
  static ResourceCategory stringToCategory(const std::string &categoryStr);
  static ResourceType stringToType(const std::string &typeStr);

protected:
  VoidLight::ResourceHandle m_handle; // Unique handle identifier
  std::string m_id;                      // JSON identifier (e.g., "gold")
  std::string m_name;                    // Display name (e.g., "Gold")
  std::string m_description{""};         // Description text
  ResourceCategory m_category;           // Resource category
  ResourceType m_type;                   // Specific resource type
  float m_value{0.0f};                   // Base value/cost
  float m_weight{0.0f};                  // Weight for encumbrance
  int m_maxStackSize{1};                 // Maximum stack size
  bool m_isStackable{false};             // Can be stacked
  bool m_isConsumable{false};            // Can be consumed/used
  std::string m_iconTextureId{""};       // Texture ID for UI icon
  std::string m_worldTextureId{""};      // Texture ID for world rendering
  int m_numFrames{1};                    // Animation frames for rendering
  int m_animSpeed{100};                  // Animation speed for rendering

  // Atlas coordinates (for sprite atlas rendering)
  int m_atlasX{0};                       // X offset in atlas
  int m_atlasY{0};                       // Y offset in atlas
  int m_atlasW{0};                       // Width in atlas (0 = unmapped)
  int m_atlasH{0};                       // Height in atlas (0 = unmapped)
};

#endif // RESOURCE_HPP
