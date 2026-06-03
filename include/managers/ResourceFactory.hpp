/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef RESOURCE_FACTORY_HPP
#define RESOURCE_FACTORY_HPP

#include "entities/Resource.hpp"
#include "utils/JsonReader.hpp"
#include "utils/ResourceHandle.hpp"
#include <functional>
#include <string>
#include <unordered_map>

namespace VoidLight {

/**
 * @brief Factory for creating Resource instances from JSON data
 *
 * The ResourceFactory provides a registry system for mapping JSON "type" fields
 * to C++ resource class constructors, enabling extensible resource loading
 * from JSON files.
 */
class ResourceFactory {
public:
  // Type alias for resource creator functions
  using ResourceCreator = std::function<ResourcePtr(const JsonValue &json)>;

  /**
   * @brief Create a resource from JSON data
   * @param json JsonValue containing resource definition
   * @return ResourcePtr or nullptr if creation failed
   */
  static ResourcePtr createFromJson(const JsonValue &json);

  /**
   * @brief Register a resource type creator
   * @param typeName String identifier for the resource type
   * @param creator Function that creates resource from JSON
   * @return true if registration succeeded, false if type already exists
   */
  static bool registerCreator(const std::string &typeName,
                              ResourceCreator creator);

  /**
   * @brief Check if a resource type is registered
   * @param typeName String identifier for the resource type
   * @return true if type is registered
   */
  static bool hasCreator(const std::string &typeName);

  /**
   * @brief Get list of all registered resource types
   * @return Vector of registered type names
   */
  static std::vector<std::string> getRegisteredTypes();

  /**
   * @brief Initialize the factory with default resource creators
   * Registers creators for all built-in resource types
   */
  static void initialize();

  /**
   * @brief Clear all registered creators
   *
   * WARNING: This method is intended ONLY for testing purposes to ensure
   * test isolation. DO NOT call this from production code, especially from
   * other singleton destructors, as it can cause undefined behavior due to
   * uncertain destruction order of static objects.
   *
   * The factory will automatically clean itself up at program exit.
   */
  static void clear();

private:
  // Registry of resource creators - use Meyer's singleton pattern for safe
  // initialization
  static std::unordered_map<std::string, ResourceCreator> &getCreators() {
    static std::unordered_map<std::string, ResourceCreator> s_creators;
    return s_creators;
  }
  // Helper methods for creating specific resource types
  static ResourcePtr createEquipment(VoidLight::ResourceHandle handle,
                                     const JsonValue &json);
  static ResourcePtr createConsumable(VoidLight::ResourceHandle handle,
                                      const JsonValue &json);
  static ResourcePtr createQuestItem(VoidLight::ResourceHandle handle,
                                     const JsonValue &json);
  static ResourcePtr createAmmunition(VoidLight::ResourceHandle handle,
                                      const JsonValue &json);
  static ResourcePtr createMaterial(VoidLight::ResourceHandle handle,
                                    const JsonValue &json);
  static ResourcePtr createCurrency(VoidLight::ResourceHandle handle,
                                    const JsonValue &json);

  // Helper method to extract common resource properties from JSON
  static void setCommonProperties(const ResourcePtr& resource, const JsonValue &json);
};

} // namespace VoidLight

#endif // RESOURCE_FACTORY_HPP
