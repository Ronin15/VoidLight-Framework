/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/ResourceTemplateManager.hpp"
#include "core/Logger.hpp"
#include "managers/ResourceFactory.hpp"
#include "utils/JsonReader.hpp"
#include "utils/ResourcePath.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string_view>
#include <unordered_set>

using VoidLight::JsonReader;
using VoidLight::JsonValue;
using VoidLight::ResourceFactory;

namespace {

constexpr std::string_view RESOURCE_ATLAS_TEXTURE_ID = "atlas";
constexpr std::string_view FALLBACK_RESOURCE_TEXTURE_ID = "default";

std::string normalizeResourceId(std::string_view source) {
  std::string id;
  id.reserve(source.size());

  bool needsSeparator = false;
  for (const unsigned char ch : source) {
    if (std::isalnum(ch) != 0) {
      if (needsSeparator && !id.empty()) {
        id.push_back('_');
      }
      id.push_back(static_cast<char>(std::tolower(ch)));
      needsSeparator = false;
    } else {
      needsSeparator = !id.empty();
    }
  }

  return id;
}

} // namespace

ResourceTemplateManager &ResourceTemplateManager::Instance() {
  static ResourceTemplateManager instance;
  return instance;
}

ResourceTemplateManager::~ResourceTemplateManager() {
  // Only clean up if not already shut down
  if (!m_isShutdown) {
    clean();
  }
}

bool ResourceTemplateManager::init() {
  if (m_initialized.load(std::memory_order_acquire)) {
    return true; // Already initialized
  }

  std::lock_guard<std::shared_mutex> lock(m_resourceMutex);

  if (m_initialized.load(std::memory_order_acquire)) {
    return true; // Double-check after acquiring lock
  }

  try {
    // Clear any existing data
    m_resourceTemplates.clear();
    m_categoryIndex.clear();
    m_typeIndex.clear();
    m_maxStackSizes.clear();
    m_values.clear();
    m_categories.clear();
    m_types.clear();
    m_nameIndex.clear();
    m_idIndex.clear();

    // PERFORMANCE OPTIMIZATION: Reserve capacity to avoid hashtable rehashing
    const size_t expectedResourceCount = 100; // Adjust based on typical usage
    m_resourceTemplates.reserve(expectedResourceCount);
    m_categoryIndex.reserve(static_cast<size_t>(ResourceCategory::COUNT));
    m_typeIndex.reserve(static_cast<size_t>(ResourceType::COUNT));

    // Initialize category index with pre-reserved vectors
    for (int i = 0; i < static_cast<int>(ResourceCategory::COUNT); ++i) {
      auto &vec = m_categoryIndex[static_cast<ResourceCategory>(i)];
      vec.reserve(expectedResourceCount /
                  static_cast<int>(ResourceCategory::COUNT));
    }

    // Initialize type index with pre-reserved vectors
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
      auto &vec = m_typeIndex[static_cast<ResourceType>(i)];
      vec.reserve(expectedResourceCount /
                  static_cast<int>(ResourceType::COUNT));
    }

    // Initialize ResourceFactory
    ResourceFactory::initialize();

    if (!createDefaultResources()) {
      RESOURCE_ERROR("ResourceTemplateManager::init - Failed to load default resource catalogs");
      m_resourceTemplates.clear();
      m_categoryIndex.clear();
      m_typeIndex.clear();
      m_maxStackSizes.clear();
      m_values.clear();
      m_categories.clear();
      m_types.clear();
      m_nameIndex.clear();
      m_idIndex.clear();
      return false;
    }

    m_initialized.store(true, std::memory_order_release);
    m_isShutdown = false; // Reset shutdown flag on successful init
    m_stats.reset();
    RESOURCE_INFO(std::format("ResourceTemplateManager initialized with {} resource templates",
                  m_resourceTemplates.size()));

    return true;
  } catch (const std::exception &ex) {
    RESOURCE_ERROR("ResourceTemplateManager::init - Exception: " +
                   std::string(ex.what()));
    return false;
  }
}

void ResourceTemplateManager::clean() {
  if (m_isShutdown) {
    return;
  }

  std::lock_guard<std::shared_mutex> lock(m_resourceMutex);

  // Clear all data structures
  m_resourceTemplates.clear();
  m_maxStackSizes.clear();
  m_values.clear();
  m_categories.clear();
  m_types.clear();
  m_categoryIndex.clear();
  m_typeIndex.clear();
  m_nameIndex.clear();
  m_idIndex.clear();

  // Clear handle generation data
  {
    std::lock_guard<std::mutex> handleLock(m_handleMutex);
    m_freedHandleIds.clear();
    m_handleGenerations.clear();
    m_nextHandleId.store(1, std::memory_order_release);
  }

  // Clear ResourceFactory for test isolation
  ResourceFactory::clear();

  m_initialized.store(false, std::memory_order_release);
  m_isShutdown = true;
  m_stats.reset();

  RESOURCE_INFO("ResourceTemplateManager cleaned up");
}
bool ResourceTemplateManager::registerResourceTemplate(
    const ResourcePtr &resource) {
  if (!resource) {
    RESOURCE_ERROR("ResourceTemplateManager::registerResourceTemplate - Null "
                   "resource provided");
    return false;
  }

  VoidLight::ResourceHandle handle = resource->getHandle();
  if (!handle.isValid()) {
    RESOURCE_ERROR("ResourceTemplateManager::registerResourceTemplate - "
                   "Invalid resource handle: " +
                   handle.toString());
    return false;
  }

  // Check if we're already holding the lock (to avoid deadlock during init)
  // Try to acquire the lock with a timeout to detect deadlock
  std::unique_lock<std::shared_mutex> lock(m_resourceMutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    // This indicates we're likely in a recursive call during initialization
    // Log the issue but use the internal method to avoid deadlock
    RESOURCE_ERROR("ResourceTemplateManager::registerResourceTemplate - "
                   "Resource deadlock avoided for: " +
                   handle.toString());
    return false;
  }

  return registerResourceTemplateInternal(resource);
}

bool ResourceTemplateManager::removeResourceTemplate(
    VoidLight::ResourceHandle handle) {
  if (!handle.isValid()) {
    RESOURCE_ERROR(
        "ResourceTemplateManager::removeResourceTemplate - Invalid handle");
    return false;
  }

  std::unique_lock<std::shared_mutex> lock(m_resourceMutex);

  // Check if resource exists
  auto it = m_resourceTemplates.find(handle);
  if (it == m_resourceTemplates.end()) {
    RESOURCE_WARN("ResourceTemplateManager::removeResourceTemplate - Resource "
                  "not found: " +
                  handle.toString());
    return false;
  }

  // Remove from all data structures
  m_resourceTemplates.erase(it);
  m_maxStackSizes.erase(handle);
  m_values.erase(handle);
  m_categories.erase(handle);
  m_types.erase(handle);

  // Remove from indexes
  removeFromIndexes(handle);

  // Release the handle for reuse with incremented generation
  releaseHandle(handle);

  m_stats.resourcesDestroyed.fetch_add(1, std::memory_order_relaxed);

  RESOURCE_DEBUG(
      "ResourceTemplateManager::removeResourceTemplate - Removed resource: " +
      handle.toString());
  return true;
}

bool ResourceTemplateManager::registerResourceTemplateInternal(
    const ResourcePtr &resource) {
  // This method assumes the lock is already held
  VoidLight::ResourceHandle handle = resource->getHandle();

  // Check if already registered
  if (m_resourceTemplates.find(handle) != m_resourceTemplates.end()) {
    RESOURCE_WARN("ResourceTemplateManager::registerResourceTemplateInternal "
                  "- Resource "
                  "already registered: " +
                  handle.toString());
    return false;
  }

  // Check for duplicate names (validation phase)
  const std::string &resourceName = resource->getName();
  if (checkForDuplicateName(resourceName, handle)) {
    RESOURCE_ERROR(
        "ResourceTemplateManager::registerResourceTemplateInternal "
        "- Duplicate resource name detected: '" +
        resourceName +
        "'. Resource names must be unique across all resource types.");
    return false;
  }

  try {
    // Cache frequently accessed properties for performance
    m_maxStackSizes[handle] = resource->getMaxStackSize();
    m_values[handle] = resource->getValue();
    m_categories[handle] = resource->getCategory();
    m_types[handle] = resource->getType();

    // Register the resource template
    m_resourceTemplates[handle] = resource;

    // Update indexes
    updateIndexes(handle, resource->getCategory(), resource->getType());
    updateNameIndex(handle, resourceName);

    m_stats.templatesLoaded.fetch_add(1, std::memory_order_relaxed);

    RESOURCE_DEBUG("ResourceTemplateManager::registerResourceTemplateInternal "
                   "- Registered: " +
                   handle.toString() + " with name: '" + resourceName + "'");
    return true;
  } catch (const std::exception &ex) {
    RESOURCE_ERROR("ResourceTemplateManager::registerResourceTemplateInternal "
                   "- Exception: " +
                   std::string(ex.what()));
    return false;
  }
}

VoidLight::ResourceHandle ResourceTemplateManager::generateHandle() {
  std::lock_guard<std::mutex> lock(m_handleMutex);

  VoidLight::ResourceHandle::HandleId id;
  VoidLight::ResourceHandle::Generation generation;

  // PERFORMANCE OPTIMIZATION: Prefer allocating new IDs over reusing freed ones
  // to reduce unordered_map lookups in the hot path
  if (m_freedHandleIds.size() >
      50) { // Only reuse when we have many freed handles
    // Reuse a freed handle ID with incremented generation
    id = m_freedHandleIds.back();
    m_freedHandleIds.pop_back();

    // Increment the generation for this reused ID to prevent stale handle bugs
    auto genIt = m_handleGenerations.find(id);
    if (genIt != m_handleGenerations.end()) {
      generation = genIt->second + 1;

      // Handle generation overflow (wrap around, but never use
      // 0/INVALID_GENERATION)
      if (generation == VoidLight::ResourceHandle::INVALID_GENERATION) {
        generation = 1;
      }

      genIt->second = generation;
    } else {
      // First time using this ID (shouldn't happen with proper bookkeeping)
      generation = 1;
      m_handleGenerations[id] = generation;
    }
  } else {
    // Allocate a new ID - much faster than map lookups
    id = m_nextHandleId.fetch_add(1, std::memory_order_relaxed);
    generation = 1;
    m_handleGenerations[id] = generation;
  }

  return VoidLight::ResourceHandle(id, generation);
}

void ResourceTemplateManager::releaseHandle(
    VoidLight::ResourceHandle handle) {
  if (!handle.isValid()) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_handleMutex);

  auto id = handle.getId();

  // Check if already in the freed list to avoid duplicates
  // Use std::find since the vector should be relatively small
  auto it = std::find(m_freedHandleIds.begin(), m_freedHandleIds.end(), id);
  if (it == m_freedHandleIds.end()) {
    m_freedHandleIds.push_back(id);
  }
}

bool ResourceTemplateManager::isValidHandle(
    VoidLight::ResourceHandle handle) const {
  if (!handle.isValid()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_handleMutex);

  // Check if the handle's generation matches our records
  auto genIt = m_handleGenerations.find(handle.getId());
  if (genIt == m_handleGenerations.end()) {
    return false; // Unknown handle ID
  }

  return genIt->second == handle.getGeneration();
}

ResourcePtr ResourceTemplateManager::getResourceTemplate(
    VoidLight::ResourceHandle handle) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  auto it = m_resourceTemplates.find(handle);
  if (it != m_resourceTemplates.end()) {
    return it->second;
  }

  return nullptr;
}

std::vector<ResourcePtr> ResourceTemplateManager::getResourcesByCategory(
    ResourceCategory category) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  std::vector<ResourcePtr> result;

  auto categoryIt = m_categoryIndex.find(category);
  if (categoryIt != m_categoryIndex.end()) {
    const auto &handles = categoryIt->second;
    result.reserve(handles.size());

    // PERFORMANCE OPTIMIZATION: Batch lookup to reduce hash map overhead
    // Single pass through handles, direct insertion without intermediate
    // lookups
    for (const auto &handle : handles) {
      auto resourceIt = m_resourceTemplates.find(handle);
      if (resourceIt != m_resourceTemplates.end()) {
        result.emplace_back(resourceIt->second);
      }
    }
  }

  return result;
}

std::vector<ResourcePtr>
ResourceTemplateManager::getResourcesByType(ResourceType type) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  std::vector<ResourcePtr> result;

  auto typeIt = m_typeIndex.find(type);
  if (typeIt != m_typeIndex.end()) {
    const auto &handles = typeIt->second;
    result.reserve(handles.size());

    // PERFORMANCE OPTIMIZATION: Batch lookup to reduce hash map overhead
    // Single pass through handles, direct insertion without intermediate
    // lookups
    for (const auto &handle : handles) {
      auto resourceIt = m_resourceTemplates.find(handle);
      if (resourceIt != m_resourceTemplates.end()) {
        result.emplace_back(resourceIt->second);
      }
    }
  }

  return result;
}

ResourcePtr
ResourceTemplateManager::getResourceByName(const std::string &name) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  auto nameIt = m_nameIndex.find(name);
  if (nameIt != m_nameIndex.end()) {
    auto resourceIt = m_resourceTemplates.find(nameIt->second);
    if (resourceIt != m_resourceTemplates.end()) {
      return resourceIt->second;
    }
  }

  return nullptr;
}

ResourcePtr
ResourceTemplateManager::getResourceById(const std::string &id) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  auto idIt = m_idIndex.find(id);
  if (idIt != m_idIndex.end()) {
    auto resourceIt = m_resourceTemplates.find(idIt->second);
    if (resourceIt != m_resourceTemplates.end()) {
      return resourceIt->second;
    }
  }

  return nullptr;
}

TextureSource ResourceTemplateManager::getIconTextureSource(
    VoidLight::ResourceHandle handle) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  auto resourceIt = m_resourceTemplates.find(handle);
  if (resourceIt == m_resourceTemplates.end() || !resourceIt->second) {
    return {};
  }

  const ResourcePtr &resource = resourceIt->second;
  if (resource->getAtlasW() > 0 && resource->getAtlasH() > 0) {
    return TextureSource{
        std::string(RESOURCE_ATLAS_TEXTURE_ID),
        resource->getAtlasX(),
        resource->getAtlasY(),
        resource->getAtlasW(),
        resource->getAtlasH(),
        true};
  }

  const std::string &iconTextureId = !resource->getIconTextureId().empty()
      ? resource->getIconTextureId()
      : resource->getWorldTextureId();

  return TextureSource{
      iconTextureId.empty() ? std::string(FALLBACK_RESOURCE_TEXTURE_ID)
                            : iconTextureId,
      0,
      0,
      0,
      0,
      false};
}

VoidLight::ResourceHandle
ResourceTemplateManager::getHandleByName(const std::string &name) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  auto nameIt = m_nameIndex.find(name);
  if (nameIt != m_nameIndex.end()) {
    return nameIt->second;
  }

  return VoidLight::ResourceHandle(); // Invalid handle
}

VoidLight::ResourceHandle
ResourceTemplateManager::getHandleById(const std::string &id) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  auto idIt = m_idIndex.find(id);
  if (idIt != m_idIndex.end()) {
    return idIt->second;
  }

  return VoidLight::ResourceHandle(); // Invalid handle
}

ResourceStats ResourceTemplateManager::getStats() const { return m_stats; }
ResourcePtr ResourceTemplateManager::createResource(
    VoidLight::ResourceHandle handle) const {
  auto templateResource = getResourceTemplate(handle);
  if (!templateResource) {
    RESOURCE_ERROR(
        "ResourceTemplateManager::createResource - Unknown resource: " +
        handle.toString());
    return nullptr;
  }

  try {
    // Create a copy of the template
    ResourcePtr newResource = std::make_shared<Resource>(*templateResource);

    m_stats.resourcesCreated.fetch_add(1, std::memory_order_relaxed);

    return newResource;
  } catch (const std::exception &ex) {
    RESOURCE_ERROR("ResourceTemplateManager::createResource - Exception: " +
                   std::string(ex.what()));
    return nullptr;
  }
}

bool ResourceTemplateManager::loadResourcesFromJson(
    const std::string &filename) {
  JsonReader reader;
  if (!reader.loadFromFile(filename)) {
    RESOURCE_ERROR("ResourceTemplateManager::loadResourcesFromJson - Failed to "
                   "load file: " +
                   filename + " - " + reader.getLastError());
    return false;
  }

  return loadResourcesFromJsonString(reader.getRoot().toString());
}

bool ResourceTemplateManager::loadResourcesFromJsonString(
    const std::string &jsonString) {
  JsonReader reader;
  if (!reader.parse(jsonString)) {
    RESOURCE_ERROR("ResourceTemplateManager::loadResourcesFromJsonString - "
                   "Failed to parse JSON: " +
                   reader.getLastError());
    return false;
  }

  JsonValue root = reader.getRoot();

  if (!root.isObject()) {
    RESOURCE_ERROR("ResourceTemplateManager::loadResourcesFromJsonString - "
                   "Root JSON is not an object");
    return false;
  }

  // Check if we have a resources array
  if (!root.hasKey("resources") || !root["resources"].isArray()) {
    RESOURCE_ERROR("ResourceTemplateManager::loadResourcesFromJsonString - "
                   "Missing or invalid 'resources' array");
    return false;
  }

  JsonValue &resourcesArray = root["resources"];
  std::unordered_set<std::string> batchIds;
  batchIds.reserve(resourcesArray.size());

  for (size_t i = 0; i < resourcesArray.size(); ++i) {
    JsonValue &resourceJson = resourcesArray[i];
    if (!resourceJson.isObject()) {
      RESOURCE_ERROR(std::format(
          "ResourceTemplateManager::loadResourcesFromJsonString - "
          "Resource at index {} is not an object", i));
      return false;
    }
    if (!resourceJson.hasKey("name") || !resourceJson["name"].isString()) {
      RESOURCE_ERROR(std::format(
          "ResourceTemplateManager::loadResourcesFromJsonString - "
          "Resource at index {} missing or invalid 'name' field", i));
      return false;
    }
    if (resourceJson.hasKey("id") && !resourceJson["id"].isString()) {
      RESOURCE_ERROR(std::format(
          "ResourceTemplateManager::loadResourcesFromJsonString - "
          "Resource at index {} has non-string 'id' field", i));
      return false;
    }

    const std::string explicitId =
        resourceJson.hasKey("id") ? resourceJson["id"].asString() : "";
    const std::string idSource =
        explicitId.empty() ? resourceJson["name"].asString() : explicitId;
    const std::string resolvedId = normalizeResourceId(idSource);
    if (resolvedId.empty()) {
      RESOURCE_ERROR(std::format(
          "ResourceTemplateManager::loadResourcesFromJsonString - "
          "Resource at index {} has no usable ID source", i));
      return false;
    }
    if (!batchIds.insert(resolvedId).second) {
      RESOURCE_ERROR(std::format(
          "ResourceTemplateManager::loadResourcesFromJsonString - "
          "Duplicate resource id '{}' in current catalog", resolvedId));
      return false;
    }
    if (m_idIndex.find(resolvedId) != m_idIndex.end()) {
      RESOURCE_ERROR(std::format(
          "ResourceTemplateManager::loadResourcesFromJsonString - "
          "Duplicate resource id '{}' already registered", resolvedId));
      return false;
    }

    resourceJson["id"] = JsonValue(resolvedId);
  }

  size_t loadedCount = 0;
  size_t failedCount = 0;

  RESOURCE_INFO(std::format(
      "ResourceTemplateManager::loadResourcesFromJsonString - Loading {} resources from JSON",
      resourcesArray.size()));

  // Process each resource in the array
  for (size_t i = 0; i < resourcesArray.size(); ++i) {
    const JsonValue &resourceJson = resourcesArray[i];

    try {
      ResourcePtr resource = ResourceFactory::createFromJson(resourceJson);
      if (resource) {
        if (registerResourceTemplateInternal(resource)) {
          loadedCount++;

          // Extract the resource ID from JSON for debug logging and ID index
          if (resourceJson.hasKey("id") && resourceJson["id"].isString()) {
            std::string resourceId = resourceJson["id"].asString();

            // Update ID index for fast JSON ID lookups
            updateIdIndex(resource->getHandle(), resourceId);

            RESOURCE_DEBUG(
                "ResourceTemplateManager::loadResourcesFromJsonString "
                "- Loaded resource: " +
                resourceId + " -> " + resource->getHandle().toString());
          } else {
            RESOURCE_WARN(std::format(
                "ResourceTemplateManager::loadResourcesFromJsonString - "
                "Resource at index {} missing or invalid 'id' field", i));
          }

          RESOURCE_DEBUG("ResourceTemplateManager::loadResourcesFromJsonString "
                         "- Loaded resource: " +
                         resource->getHandle().toString());
        } else {
          failedCount++;
          RESOURCE_WARN("ResourceTemplateManager::loadResourcesFromJsonString "
                        "- Failed to register resource: " +
                        resource->getHandle().toString());
        }
      } else {
        failedCount++;
        RESOURCE_WARN(std::format("ResourceTemplateManager::loadResourcesFromJsonString - "
                      "Failed to create resource from JSON at index {}", i));
      }
    } catch (const std::exception &ex) {
      failedCount++;
      RESOURCE_ERROR(std::format("ResourceTemplateManager::loadResourcesFromJsonString - "
                     "Exception processing resource at index {}: {}", i, ex.what()));
    }
  }

  RESOURCE_INFO(std::format(
      "ResourceTemplateManager::loadResourcesFromJsonString - Completed: {} loaded, {} failed",
      loadedCount, failedCount));

  return failedCount ==
         0; // Return true only if all resources were loaded successfully
}

size_t ResourceTemplateManager::getResourceTemplateCount() const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  return m_resourceTemplates.size();
}

bool ResourceTemplateManager::hasResourceTemplate(
    VoidLight::ResourceHandle handle) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  return m_resourceTemplates.find(handle) != m_resourceTemplates.end();
}

size_t ResourceTemplateManager::getMemoryUsage() const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  size_t totalSize = 0;

  // Account for m_resourceTemplates map itself
  totalSize +=
      m_resourceTemplates.size() * (sizeof(std::string) + sizeof(ResourcePtr));

  // Account for the actual strings and Resource objects in
  // m_resourceTemplates
  for (const auto &[resourceId, resource] : m_resourceTemplates) {
    totalSize += sizeof(resourceId); // Size of the handle
    if (resource) {
      totalSize += sizeof(Resource);           // Base Resource object size
      totalSize += resource->getName().size(); // Resource name
      totalSize += resource->getDescription().size(); // Resource description
    }
  }

  for (const auto &[categoryKey, resourceIds] : m_categoryIndex) {
    totalSize += resourceIds.size() * sizeof(VoidLight::ResourceHandle);
  }

  for (const auto &[typeKey, resourceIds] : m_typeIndex) {
    totalSize += resourceIds.size() * sizeof(VoidLight::ResourceHandle);
  }

  return totalSize;
}

void ResourceTemplateManager::updateIndexes(VoidLight::ResourceHandle handle,
                                            ResourceCategory category,
                                            ResourceType type) {
  std::lock_guard<std::mutex> lock(m_indexMutex);

  // Add to category index
  m_categoryIndex[category].push_back(handle);

  // Add to type index
  m_typeIndex[type].push_back(handle);
}

void ResourceTemplateManager::updateNameIndex(
    VoidLight::ResourceHandle handle, const std::string &name) {
  std::lock_guard<std::mutex> lock(m_indexMutex);
  m_nameIndex[name] = handle;
}

void ResourceTemplateManager::updateIdIndex(VoidLight::ResourceHandle handle,
                                            const std::string &id) {
  std::lock_guard<std::mutex> lock(m_indexMutex);
  m_idIndex[id] = handle;
}

void ResourceTemplateManager::removeFromIndexes(
    VoidLight::ResourceHandle handle) {
  std::lock_guard<std::mutex> lock(m_indexMutex);

  // PERFORMANCE OPTIMIZATION: Use cached properties to avoid O(n) searches
  // Get resource properties from our cached maps instead of linear searches
  auto categoryIt = m_categories.find(handle);
  auto typeIt = m_types.find(handle);

  // Remove from category index using cached category (O(n) -> O(log n))
  if (categoryIt != m_categories.end()) {
    auto &handles = m_categoryIndex[categoryIt->second];
    auto it = std::find(handles.begin(), handles.end(), handle);
    if (it != handles.end()) {
      handles.erase(it);
    }
  }

  // Remove from type index using cached type (O(n) -> O(log n))
  if (typeIt != m_types.end()) {
    auto &handles = m_typeIndex[typeIt->second];
    auto it = std::find(handles.begin(), handles.end(), handle);
    if (it != handles.end()) {
      handles.erase(it);
    }
  }

  // Remove from name and ID indexes (reverse lookup still needed but only once
  // each)
  auto nameIt = std::find_if(
      m_nameIndex.begin(), m_nameIndex.end(),
      [handle](const auto &pair) { return pair.second == handle; });
  if (nameIt != m_nameIndex.end()) {
    m_nameIndex.erase(nameIt);
  }

  auto idIt = std::find_if(
      m_idIndex.begin(), m_idIndex.end(),
      [handle](const auto &pair) { return pair.second == handle; });
  if (idIt != m_idIndex.end()) {
    m_idIndex.erase(idIt);
  }
}


bool ResourceTemplateManager::checkForDuplicateName(
    const std::string &name, VoidLight::ResourceHandle currentHandle) const {
  // This method assumes the lock is already held by the caller
  auto nameIt = m_nameIndex.find(name);
  if (nameIt != m_nameIndex.end()) {
    // Name exists - check if it's for a different resource
    if (nameIt->second != currentHandle) {
      // Different resource has the same name - this is a duplicate
      return true;
    }
  }
  return false;
}

bool ResourceTemplateManager::createDefaultResources() {
  RESOURCE_INFO(
      "ResourceTemplateManager::createDefaultResources - Creating default "
      "resource templates");

  try {
    // Load resource catalogs before atlas mapping so all entries share one
    // texture-coordinate pass.
    constexpr std::array<std::string_view, 5> resourceCatalogs{
        "items.json", "weapons.json", "equipment.json", "materials.json",
        "currency.json"};
    bool allCatalogsLoaded = true;
    for (std::string_view catalog : resourceCatalogs) {
      const std::string catalogPath = VoidLight::ResourcePath::resolve(
          std::format("res/data/{}", catalog));
      const bool catalogLoaded = loadResourcesFromJson(catalogPath);
      allCatalogsLoaded = allCatalogsLoaded && catalogLoaded;

      RESOURCE_WARN_IF(!catalogLoaded,
          std::format("ResourceTemplateManager::createDefaultResources - "
                      "Failed to load {}", catalog));
    }

    if (!allCatalogsLoaded) {
      return false;
    }

    // Apply atlas coordinates from atlas.json (following WorldManager pattern)
    JsonReader atlasReader;
    if (atlasReader.loadFromFile(VoidLight::ResourcePath::resolve("res/data/atlas.json"))) {
      const auto& atlasRoot = atlasReader.getRoot();
      if (atlasRoot.hasKey("regions")) {
        const auto& regions = atlasRoot["regions"].asObject();
        size_t coordsApplied = 0;

        for (auto& [handle, resource] : m_resourceTemplates) {
          std::string texId = resource->getIconTextureId();
          if (texId.empty()) {
            texId = resource->getWorldTextureId();
          }
          auto it = regions.find(texId);
          if (it != regions.end()) {
            const auto& r = it->second;
            resource->setAtlasX(r["x"].asInt());
            resource->setAtlasY(r["y"].asInt());
            resource->setAtlasW(r["w"].asInt());
            resource->setAtlasH(r["h"].asInt());
            ++coordsApplied;
          }
        }
        RESOURCE_DEBUG(std::format(
            "ResourceTemplateManager::createDefaultResources - Applied atlas "
            "coordinates to {} resources", coordsApplied));
      }
    }

    RESOURCE_INFO("ResourceTemplateManager::createDefaultResources - Default "
                  "resources loaded from JSON files");
    return true;
  } catch (const std::exception &ex) {
    RESOURCE_ERROR(
        "ResourceTemplateManager::createDefaultResources - Exception: " +
        std::string(ex.what()));
    return false;
  }
}

// Fast property access methods (cache-optimized)
int ResourceTemplateManager::getMaxStackSize(
    VoidLight::ResourceHandle handle) const {
  // PERFORMANCE OPTIMIZATION: Check validity first to avoid lock on invalid
  // handles
  if (!handle.isValid()) {
    return 1; // Default stack size for invalid handles
  }

  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  auto it = m_maxStackSizes.find(handle);
  return (it != m_maxStackSizes.end()) ? it->second : 1; // Default stack size
}

float ResourceTemplateManager::getValue(
    VoidLight::ResourceHandle handle) const {
  // PERFORMANCE OPTIMIZATION: Check validity first to avoid lock on invalid
  // handles
  if (!handle.isValid()) {
    return 0.0f; // Default value for invalid handles
  }

  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  auto it = m_values.find(handle);
  return (it != m_values.end()) ? it->second : 0.0f; // Default value
}

ResourceCategory ResourceTemplateManager::getCategory(
    VoidLight::ResourceHandle handle) const {
  // PERFORMANCE OPTIMIZATION: Check validity first to avoid lock on invalid
  // handles
  if (!handle.isValid()) {
    return ResourceCategory::Item; // Default category for invalid handles
  }

  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  auto it = m_categories.find(handle);
  return (it != m_categories.end())
             ? it->second
             : ResourceCategory::Item; // Default category
}

ResourceType
ResourceTemplateManager::getType(VoidLight::ResourceHandle handle) const {
  // PERFORMANCE OPTIMIZATION: Check validity first to avoid lock on invalid
  // handles
  if (!handle.isValid()) {
    return ResourceType::Equipment; // Default type for invalid handles
  }

  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
  auto it = m_types.find(handle);
  return (it != m_types.end()) ? it->second
                               : ResourceType::Equipment; // Default type
}

// Cache-friendly bulk operations for better performance
std::vector<int> ResourceTemplateManager::getMaxStackSizes(
    const std::vector<VoidLight::ResourceHandle> &handles) const {
  std::vector<int> results;
  results.reserve(handles.size());

  // PERFORMANCE OPTIMIZATION: Early return for empty input
  if (handles.empty()) {
    return results;
  }

  // PERFORMANCE OPTIMIZATION: Filter out invalid handles first to avoid lock
  // overhead
  std::vector<VoidLight::ResourceHandle> validHandles;
  validHandles.reserve(handles.size());

  for (const auto &handle : handles) {
    if (handle.isValid()) {
      validHandles.push_back(handle);
    } else {
      results.push_back(1); // Default value for invalid handles
    }
  }

  if (!validHandles.empty()) {
    std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

    // Process valid handles with single lock acquisition
    size_t validIndex = 0;
    for (size_t i = 0; i < handles.size(); ++i) {
      if (handles[i].isValid() && validIndex < validHandles.size()) {
        auto it = m_maxStackSizes.find(validHandles[validIndex]);
        if (results.size() <= i) {
          results.resize(i + 1, 1); // Fill gaps with default values
        }
        results[i] = (it != m_maxStackSizes.end()) ? it->second : 1;
        validIndex++;
      }
    }
  }

  // Ensure results vector has correct size
  results.resize(handles.size(), 1);
  return results;
}

std::vector<float> ResourceTemplateManager::getValues(
    const std::vector<VoidLight::ResourceHandle> &handles) const {
  std::vector<float> results;
  results.reserve(handles.size());

  // PERFORMANCE OPTIMIZATION: Early return for empty input
  if (handles.empty()) {
    return results;
  }

  // PERFORMANCE OPTIMIZATION: Single lock acquisition for all valid handles
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  for (const auto &handle : handles) {
    if (handle.isValid()) {
      auto it = m_values.find(handle);
      results.push_back((it != m_values.end()) ? it->second : 0.0f);
    } else {
      results.push_back(0.0f); // Default value for invalid handles
    }
  }

  return results;
}

void ResourceTemplateManager::getPropertiesBatch(
    const std::vector<VoidLight::ResourceHandle> &handles,
    std::vector<int> &maxStackSizes, std::vector<float> &values,
    std::vector<ResourceCategory> &categories,
    std::vector<ResourceType> &types) const {
  std::shared_lock<std::shared_mutex> lock(m_resourceMutex);

  const size_t count = handles.size();
  maxStackSizes.clear();
  values.clear();
  categories.clear();
  types.clear();

  maxStackSizes.reserve(count);
  values.reserve(count);
  categories.reserve(count);
  types.reserve(count);

  // Single pass through all handles for maximum cache efficiency
  for (const auto &handle : handles) {
    // Look up all properties in one go
    auto stackIt = m_maxStackSizes.find(handle);
    auto valueIt = m_values.find(handle);
    auto categoryIt = m_categories.find(handle);
    auto typeIt = m_types.find(handle);

    maxStackSizes.push_back((stackIt != m_maxStackSizes.end()) ? stackIt->second
                                                               : 1);
    values.push_back((valueIt != m_values.end()) ? valueIt->second : 0.0f);
    categories.push_back((categoryIt != m_categories.end())
                             ? categoryIt->second
                             : ResourceCategory::Item);
    types.push_back((typeIt != m_types.end()) ? typeIt->second
                                              : ResourceType::Equipment);
  }
}
