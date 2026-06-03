# ResourceFactory

**Code:** `include/managers/ResourceFactory.hpp`, `src/managers/ResourceFactory.cpp`

## Overview

The `ResourceFactory` is a static class that provides a centralized mechanism for creating `Resource` instances from JSON data. It uses a registry of creator functions to map JSON "type" fields to C++ resource class constructors, allowing for an extensible resource loading system.

## Table of Contents

- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [API Reference](#api-reference)
- [Best Practices](#best-practices)

## Quick Start

### Registering a Custom Resource

```cpp
// In your game's initialization code
#include "managers/ResourceFactory.hpp"
#include "MyCustomResource.hpp"

// Register a creator function for your custom resource type
ResourceFactory::registerCreator("MyCustomType",
    [](const JsonValue& json) -> ResourcePtr {
    auto handle = ResourceTemplateManager::Instance().generateHandle();
    auto resource = std::make_shared<MyCustomResource>(
        handle,
        json["id"].tryAsString().value_or(""),
        json["name"].tryAsString().value_or(""));

    if (json.hasKey("damage")) {
        resource->setDamage(
            static_cast<float>(json["damage"].tryAsNumber().value_or(0.0)));
    }

    return resource;
});
```

### Creating a Resource from JSON

```cpp
// Assuming you have a JsonValue object from a JsonReader
JsonValue resourceJson = ...;

// Create the resource using the factory
ResourcePtr resource = ResourceFactory::createFromJson(resourceJson);

if (resource) {
    // Use the created resource
}
```

## Architecture

The `ResourceFactory` is a static class, meaning you don't need to create an instance of it. It maintains a static map of resource type names to creator functions. When `createFromJson` is called, it looks up the appropriate creator function based on the "type" field in the JSON data and invokes it to create the resource instance.

The factory is initialized with a set of default resource creators for the engine's built-in resource types.

## API Reference

### Core Methods

- `static ResourcePtr createFromJson(const JsonValue& json)`: Creates a `Resource` instance from a `JsonValue`.
- `static bool registerCreator(const std::string& typeName, ResourceCreator creator)`: Registers a new resource creator function.
- `static bool hasCreator(const std::string& typeName)`: Checks if a creator is registered for a given type.
- `static std::vector<std::string> getRegisteredTypes()`: Returns a list of all registered resource types.
- `static void initialize()`: Initializes the factory with default resource creators.
- `static void clear()`: Clears all registered creators (for testing purposes only).

## Adding Custom Resource Types

To add a new resource type to the engine, follow these steps:

### Step 1: Create Resource Class

Create a class that inherits from the `Resource` base class:

```cpp
// include/resources/MyCustomResource.hpp
#pragma once
#include "resources/Resource.hpp"

namespace VoidLight {

class MyCustomResource : public Resource {
public:
    MyCustomResource(VoidLight::ResourceHandle handle,
                     const std::string& id,
                     const std::string& name)
        : Resource(handle, id, name,
                   ResourceCategory::Item,
                   ResourceType::QuestItem) {}

    // Custom properties
    float getDamage() const { return m_damage; }
    void setDamage(float damage) { m_damage = damage; }

    int getDurability() const { return m_durability; }
    void setDurability(int durability) { m_durability = durability; }

private:
    float m_damage{10.0f};
    int m_durability{100};
};

} // namespace VoidLight
```

### Step 2: Implement JSON Parsing

Create a factory function that parses JSON and creates the resource:

```cpp
// In your initialization code or a dedicated registration file
#include "managers/ResourceFactory.hpp"
#include "resources/MyCustomResource.hpp"
#include "utils/JsonReader.hpp"

ResourcePtr createCustomResourceFromJson(const JsonValue& json) {
    auto handle = ResourceTemplateManager::Instance().generateHandle();
    auto resource = std::make_shared<MyCustomResource>(
        handle,
        json["id"].tryAsString().value_or(""),
        json["name"].tryAsString().value_or(""));

    // Parse custom properties
    if (json.hasKey("damage")) {
        resource->setDamage(
            static_cast<float>(json["damage"].tryAsNumber().value_or(0.0)));
    }
    if (json.hasKey("durability")) {
        resource->setDurability(json["durability"].tryAsInt().value_or(0));
    }

    return resource;
}
```

### Step 3: Register with Factory

Register your creator function during game initialization:

```cpp
// In Game::init() or similar initialization function
void registerCustomResources() {
    ResourceFactory::registerCreator(
        "custom_type",
        createCustomResourceFromJson
    );
}
```

### Step 4: Define in JSON

Create resource definitions in a role-appropriate catalog, or in a custom
catalog loaded after the built-in split catalogs:

```json
{
    "resources": [
        {
            "id": "fire_sword",
            "type": "custom_type",
            "category": "Item",
            "name": "Fire Sword",
            "description": "A sword wreathed in flames",
            "damage": 25.0,
            "durability": 150
        },
        {
            "id": "ice_dagger",
            "type": "custom_type",
            "category": "Item",
            "name": "Ice Dagger",
            "description": "A dagger of frozen steel",
            "damage": 15.0,
            "durability": 80
        }
    ]
}
```

### Step 5: Load Resources

Load your resources using ResourceTemplateManager:

```cpp
// Load all resources from file
auto& rtm = ResourceTemplateManager::Instance();
rtm.loadResourcesFromJson("res/data/custom_weapons.json");

// Access by stable JSON ID
ResourceHandle fireHandle = rtm.getHandleById("fire_sword");
ResourcePtr fireSword = rtm.getResourceTemplate(fireHandle);

// Cast to specific type if needed
auto customRes = std::dynamic_pointer_cast<MyCustomResource>(fireSword);
if (customRes) {
    float damage = customRes->getDamage();
}
```

### Registration Order

Resources should be registered **before** loading any JSON files that use them:

```cpp
void Game::init() {
    // 1. Initialize managers
    ResourceFactory::initialize();  // Registers built-in types

    // 2. Register custom types
    registerCustomResources();

    // 3. Load resource files (now includes custom types)
    ResourceTemplateManager::Instance().loadResourcesFromJson("res/data/items.json");
    ResourceTemplateManager::Instance().loadResourcesFromJson("res/data/weapons.json");
    ResourceTemplateManager::Instance().loadResourcesFromJson("res/data/equipment.json");
    ResourceTemplateManager::Instance().loadResourcesFromJson("res/data/materials.json");
    ResourceTemplateManager::Instance().loadResourcesFromJson("res/data/currency.json");
    ResourceTemplateManager::Instance().loadResourcesFromJson("res/data/custom_weapons.json");
}
```

## Best Practices

- **Register Early**: Register all your custom resource creators during your game's initialization phase.
- **Handle Null**: Always check if `createFromJson` returns a `nullptr`, which indicates that the resource creation failed.
- **Extensibility**: Use the factory to create all your resource types to maintain a consistent and extensible resource loading pipeline.
- **Testing**: When writing tests for your resources, you can use the `clear()` method to isolate your tests from the default registered types.
- **Type Safety**: Always validate JSON fields exist before accessing them to avoid runtime errors.
- **Consistent Naming**: Use snake_case for JSON type names (e.g., "custom_type", "fire_weapon").
