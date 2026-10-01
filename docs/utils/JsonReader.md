# JsonReader - RFC 8259 Compliant JSON Parser

## Overview

The JsonReader is a custom, dependency-free JSON parser designed specifically for the VoidLight Engine. It provides strict RFC 8259 compliance, robust error handling, and type-safe data access without requiring external libraries.

## Table of Contents

- [Quick Start](#quick-start)
- [Core Classes](#core-classes)
- [Basic Usage](#basic-usage)
- [Type System](#type-system)
- [Error Handling](#error-handling)
- [File Operations](#file-operations)
- [Advanced Features](#advanced-features)
- [Performance Considerations](#performance-considerations)
- [Best Practices](#best-practices)
- [API Reference](#api-reference)
- [Examples](#examples)

## Quick Start

```cpp
#include "utils/JsonReader.hpp"

// Basic parsing
JsonReader reader;
if (reader.parse("{\"name\": \"John\", \"age\": 30}")) {
    const JsonValue& root = reader.getRoot();
    std::string name = root["name"].asString();
    int age = root["age"].asInt();
}

// Load from file
if (reader.loadFromFile("config.json")) {
    // Access parsed data...
}
```

## Core Classes

### JsonReader
The main parser class that handles JSON string parsing and file loading.

**Key Methods:**
- `parse(const std::string& json)` - Parse JSON from string
- `loadFromFile(const std::string& path)` - Parse JSON from file
- `getRoot()` - Get the root JsonValue after successful parsing
- `getLastError()` - Get detailed error message if parsing fails

### JsonValue
Represents a single JSON value with type-safe accessors and operators.

**Key Methods:**
- Type checks: `isNull()`, `isBool()`, `isNumber()`, `isString()`, `isArray()`, `isObject()`
- Safe accessors: `tryAsString()`, `tryAsInt()`, `tryAsNumber()`, `tryAsBool()`
- Direct accessors: `asString()`, `asInt()`, `asNumber()`, `asBool()`
- Container operations: `size()`, `hasKey()`, `operator[]`

### JsonType Enumeration
```cpp
enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};
```

## Basic Usage

### Parsing JSON Strings

```cpp
JsonReader reader;

// Parse basic types
reader.parse("null");           // Null value
reader.parse("true");           // Boolean
reader.parse("42");             // Integer
reader.parse("3.14");           // Floating-point
reader.parse("\"hello\"");      // String

// Parse collections
reader.parse("[1, 2, 3]");      // Array
reader.parse("{\"key\": \"value\"}"); // Object
```

### Accessing Data

```cpp
JsonReader reader;
reader.parse("{\"name\": \"Alice\", \"scores\": [85, 92, 78]}");

const JsonValue& root = reader.getRoot();

// Object access
std::string name = root["name"].asString();

// Array access
const JsonValue& scores = root["scores"];
for (size_t i = 0; i < scores.size(); ++i) {
    int score = scores[i].asInt();
}
```

## Type System

### Type Safety Features

The JsonReader provides multiple layers of type safety:

1. **Type Checking**: Use `is*()` methods to verify types before access
2. **Safe Accessors**: Use `tryAs*()` methods that return `std::optional`
3. **Direct Accessors**: Use `as*()` methods when type is guaranteed

```cpp
const JsonValue& value = root["someKey"];

// Method 1: Type checking
if (value.isString()) {
    std::string str = value.asString();
}

// Method 2: Safe accessors
auto optStr = value.tryAsString();
if (optStr.has_value()) {
    std::string str = optStr.value();
}

// Method 3: Direct access (use when type is guaranteed)
std::string str = value.asString(); // May throw if wrong type
```

### Supported Types

| JSON Type | C++ Type | JsonValue Methods |
|-----------|----------|-------------------|
| `null` | - | `isNull()` |
| `true`/`false` | `bool` | `isBool()`, `asBool()` |
| Numbers | `int`, `double` | `isNumber()`, `asInt()`, `asNumber()` |
| Strings | `std::string` | `isString()`, `asString()` |
| Arrays | Container | `isArray()`, `size()`, `operator[]` |
| Objects | Container | `isObject()`, `size()`, `hasKey()`, `operator[]` |

## Error Handling

### Parsing Errors

The JsonReader provides detailed error messages for invalid JSON:

```cpp
JsonReader reader;

if (!reader.parse("invalid json")) {
    std::string error = reader.getLastError();
    std::cout << "Parse error: " << error << std::endl;
}
```

### Common Error Types

- **Syntax Errors**: Malformed JSON structure
- **Token Errors**: Invalid characters or keywords
- **Structure Errors**: Missing commas, brackets, or quotes
- **File Errors**: File not found or permission issues

### Error Recovery

```cpp
JsonReader reader;

// Parse with error handling
bool success = reader.parse(jsonString);
if (!success) {
    GAMEENGINE_ERROR(std::format("JSON Parse Error: {}", reader.getLastError()));
    // Use default values or alternative data source
    return false;
}

// Continue with valid data
const JsonValue& root = reader.getRoot();
```

## File Operations

### Loading from Files

```cpp
JsonReader reader;

// Load configuration file
if (reader.loadFromFile("config/settings.json")) {
    const JsonValue& config = reader.getRoot();
    
    // Access configuration values
    int windowWidth = config["window"]["width"].asInt();
    bool fullscreen = config["window"]["fullscreen"].asBool();
}
```

### Error Handling for Files

```cpp
JsonReader reader;

if (!reader.loadFromFile("data/items.json")) {
    std::string error = reader.getLastError();
    
    if (error.find("Failed to open file") != std::string::npos) {
        GAMEENGINE_ERROR(std::format("Item data file not found: {}", error));
        // Create default item data
    } else {
        GAMEENGINE_ERROR(std::format("Invalid item data format: {}", error));
        // Handle parsing errors
    }
}
```

## Advanced Features

### Complex Data Structures

```cpp
// Parse nested game data
std::string gameData = R"({
    "player": {
        "name": "Hero",
        "level": 15,
        "stats": {
            "health": 100,
            "mana": 50
        },
        "inventory": [
            {"item": "sword", "quantity": 1},
            {"item": "potion", "quantity": 5}
        ]
    }
})";

JsonReader reader;
if (reader.parse(gameData)) {
    const JsonValue& player = reader.getRoot()["player"];
    
    // Access nested objects
    std::string name = player["name"].asString();
    int health = player["stats"]["health"].asInt();
    
    // Process arrays
    const JsonValue& inventory = player["inventory"];
    for (size_t i = 0; i < inventory.size(); ++i) {
        std::string item = inventory[i]["item"].asString();
        int quantity = inventory[i]["quantity"].asInt();
    }
}
```

### Safe Data Extraction

```cpp
// Extract data with defaults
JsonValue getConfigValue(const JsonValue& config, const std::string& key, const JsonValue& defaultValue) {
    if (config.isObject() && config.hasKey(key)) {
        return config[key];
    }
    return defaultValue;
}

// Usage
const JsonValue& config = reader.getRoot();
int maxEntities = getConfigValue(config, "maxEntities", JsonValue(1000)).asInt();
bool enableAI = getConfigValue(config, "enableAI", JsonValue(true)).asBool();
```

## Performance Considerations

### Memory Usage
- **Efficient Storage**: Uses `std::variant` for type-safe value storage
- **Minimal Overhead**: No external dependencies or bloated structures
- **Move Semantics**: Optimized for C++20 with proper move constructors

### Parsing Performance
- **Single-Pass Parser**: Processes JSON in one pass through the data
- **Stack-Based**: Uses recursion for nested structures (watch stack depth)
- **String Optimization**: Efficient string handling with proper escape processing

### Best Performance Practices

```cpp
// Reuse JsonReader instances
JsonReader reader; // Create once, reuse multiple times

// Parse once, access multiple times
if (reader.parse(jsonData)) {
    const JsonValue& root = reader.getRoot();
    
    // Cache frequently accessed values
    const JsonValue& gameConfig = root["gameConfig"];
    const JsonValue& playerData = root["playerData"];
    
    // Process cached references
}
```

## Best Practices

### 1. Always Check Parse Results

```cpp
JsonReader reader;
if (!reader.parse(jsonString)) {
    GAMEENGINE_ERROR(std::format("JSON parsing failed: {}", reader.getLastError()));
    return false;
}
```

### 2. Use Type-Safe Accessors

```cpp
// Preferred: Safe access with defaults
auto health = root["player"]["health"].tryAsInt().value_or(100);

// Alternative: Type checking
if (root["player"]["health"].isNumber()) {
    int health = root["player"]["health"].asInt();
}
```

### 3. Handle Missing Keys Gracefully

```cpp
const JsonValue& player = root["player"];
if (player.hasKey("optionalField")) {
    // Process optional data
    std::string optional = player["optionalField"].asString();
}
```

### 4. Use Meaningful Error Messages

```cpp
if (!reader.loadFromFile(filename)) {
    GAMEENGINE_ERROR(std::format("Failed to load game data from '{}': {}", 
                     filename, reader.getLastError()));
}
```

### 5. Validate Data Structure

```cpp
bool validatePlayerData(const JsonValue& player) {
    return player.isObject() && 
           player.hasKey("name") && player["name"].isString() &&
           player.hasKey("level") && player["level"].isNumber();
}
```

## API Reference

### JsonReader Class

```cpp
class JsonReader {
public:
    // Parsing methods
    bool parse(const std::string& json);
    bool loadFromFile(const std::string& path);
    
    // Result access
    const JsonValue& getRoot() const;
    const std::string& getLastError() const;
    
    // State management
    void clearError();
};
```

### JsonValue Class

```cpp
class JsonValue {
public:
    // Type checking
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;
    JsonType getType() const;
    
    // Safe accessors (return std::optional)
    std::optional<bool> tryAsBool() const;
    std::optional<int> tryAsInt() const;
    std::optional<double> tryAsNumber() const;
    std::optional<std::string> tryAsString() const;
    
    // Direct accessors (may throw on type mismatch)
    bool asBool() const;
    int asInt() const;
    double asNumber() const;
    const std::string& asString() const;
    
    // Container operations
    size_t size() const;
    bool hasKey(const std::string& key) const;
    const JsonValue& operator[](size_t index) const;
    const JsonValue& operator[](const std::string& key) const;
    
    // Utility
    std::string toString() const;
};
```

## Examples

### Loading Game Configuration

```cpp
class GameConfig {
private:
    JsonReader m_reader;
    
public:
    bool loadConfig(const std::string& filename) {
        if (!m_reader.loadFromFile(filename)) {
            GAMEENGINE_ERROR(std::format("Failed to load config: {}", m_reader.getLastError()));
            return false;
        }
        
        const JsonValue& root = m_reader.getRoot();
        
        // Load window settings
        if (root.hasKey("window")) {
            const JsonValue& window = root["window"];
            m_windowWidth = window["width"].tryAsInt().value_or(1024);
            m_windowHeight = window["height"].tryAsInt().value_or(768);
            m_fullscreen = window["fullscreen"].tryAsBool().value_or(false);
        }
        
        // Load game settings
        if (root.hasKey("game")) {
            const JsonValue& game = root["game"];
            m_maxEntities = game["maxEntities"].tryAsInt().value_or(1000);
            m_debugMode = game["debugMode"].tryAsBool().value_or(false);
        }
        
        return true;
    }
};
```

### Processing Item Data

```cpp
struct ItemDefinition {
    std::string name;
    std::string type;
    int value;
    std::vector<std::string> effects;
};

std::vector<ItemDefinition> loadItems(const std::string& filename) {
    std::vector<ItemDefinition> items;
    
    JsonReader reader;
    if (!reader.loadFromFile(filename)) {
        GAMEENGINE_ERROR(std::format("Failed to load items: {}", reader.getLastError()));
        return items;
    }
    
    const JsonValue& root = reader.getRoot();
    if (!root.hasKey("items") || !root["items"].isArray()) {
        GAMEENGINE_ERROR("Invalid item file format");
        return items;
    }
    
    const JsonValue& itemArray = root["items"];
    for (size_t i = 0; i < itemArray.size(); ++i) {
        const JsonValue& item = itemArray[i];
        
        ItemDefinition def;
        def.name = item["name"].tryAsString().value_or("Unknown");
        def.type = item["type"].tryAsString().value_or("misc");
        def.value = item["value"].tryAsInt().value_or(0);
        
        // Load effects array
        if (item.hasKey("effects") && item["effects"].isArray()) {
            const JsonValue& effects = item["effects"];
            for (size_t j = 0; j < effects.size(); ++j) {
                def.effects.push_back(effects[j].asString());
            }
        }
        
        items.push_back(def);
    }
    
    return items;
}
```

### Error Recovery Pattern

```cpp
class ConfigManager {
private:
    JsonReader m_reader;
    JsonValue m_defaultConfig;
    
    void createDefaultConfig() {
        // Create default configuration as JsonValue
        // This would typically be loaded from a embedded resource
        // or constructed programmatically
    }
    
public:
    bool loadConfiguration(const std::string& filename) {
        createDefaultConfig();
        
        if (!m_reader.loadFromFile(filename)) {
            GAMEENGINE_WARN(std::format("Config file '{}' not found or invalid: {}", 
                              filename, m_reader.getLastError()));
            GAMEENGINE_INFO("Using default configuration");
            return true; // Continue with defaults
        }
        
        // Merge user config with defaults
        mergeWithDefaults(m_reader.getRoot());
        return true;
    }
    
private:
    void mergeWithDefaults(const JsonValue& userConfig) {
        // Implementation would merge user settings with defaults
        // falling back to default values for missing keys
    }
};
```

---

**Note**: The JsonReader is designed to be lightweight and focused on game development needs. For applications requiring JSON schema validation or advanced features, consider complementing this with additional validation layers in your application code.

**Thread Safety**: JsonReader instances are not thread-safe. Use separate instances per thread or implement appropriate synchronization when sharing parsed data across threads.

**Standards Compliance**: This implementation strictly follows RFC 8259 (The JavaScript Object Notation Data Interchange Format) for maximum compatibility.