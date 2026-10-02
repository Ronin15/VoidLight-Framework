# SettingsManager

## Overview

The SettingsManager is a thread-safe settings management system designed for the VoidLight-Framework. It provides type-safe access to game settings with JSON persistence, category organization, and change notification callbacks. The manager uses a singleton pattern and supports concurrent reads with exclusive writes for optimal performance.

## Architecture

### Design Patterns
- **Singleton Pattern**: Ensures single instance with global access
- **Category Organization**: Settings grouped by logical categories (graphics, audio, gameplay)
- **Type-Safe Variant Storage**: Supports int, float, bool, and string types
- **Observer Pattern**: Change listener callbacks for reactive settings updates

### Core Components

#### Supported Types
```cpp
using SettingValue = std::variant<int, float, bool, std::string>;
```

Supported setting types:
- `int` - Integer values (resolution width/height, max entities)
- `float` - Floating-point values (volume levels, sensitivity)
- `bool` - Boolean flags (vsync, fullscreen, debug mode)
- `std::string` - Text values (language, player name)

#### Category Structure
Settings are organized into categories for logical grouping:
```json
{
  "graphics": {
    "resolution_width": 1920,
    "resolution_height": 1080,
    "vsync": true,
    "fullscreen": false
  },
  "audio": {
    "master_volume": 0.8,
    "music_volume": 0.6,
    "sfx_volume": 0.7
  },
  "gameplay": {
    "difficulty": "normal",
    "auto_save": true
  }
}
```

## Public API Reference

### Singleton Access

#### `static SettingsManager& Instance()`
Gets the singleton instance of SettingsManager.
```cpp
auto& settings = SettingsManager::Instance();
```

### File Operations

#### `bool loadFromFile(const std::string& filepath)`
Loads settings from a JSON file.
- **Parameters**:
  - `filepath`: Path to the JSON settings file (e.g., "res/settings.json")
- **Returns**: `true` if loading successful, `false` otherwise
- **Side Effects**: Replaces current settings with loaded values
- **Thread Safety**: Write operation, blocks concurrent access

```cpp
auto& settings = SettingsManager::Instance();
if (!settings.loadFromFile("res/settings.json")) {
    // Handle load failure - settings remain unchanged
    SETTINGS_ERROR("Failed to load settings, using defaults");
}
```

#### `bool saveToFile(const std::string& filepath)`
Saves current settings to a JSON file with pretty formatting.
- **Parameters**:
  - `filepath`: Path to save the JSON settings file
- **Returns**: `true` if saving successful, `false` otherwise
- **Thread Safety**: Read operation, allows concurrent reads

```cpp
// Save after modifying settings
settings.set("graphics", "resolution_width", 2560);
settings.set("graphics", "resolution_height", 1440);
settings.saveToFile("res/settings.json");
```

### Getting Settings

#### `template<typename T> T get(const std::string& category, const std::string& key, T defaultValue = T{}) const`
Gets a typed setting value with optional default.
- **Template Parameters**:
  - `T`: Type of the setting (int, float, bool, or std::string)
- **Parameters**:
  - `category`: Setting category (e.g., "graphics", "audio")
  - `key`: Setting key within the category
  - `defaultValue`: Value to return if setting doesn't exist or type mismatch
- **Returns**: The setting value or `defaultValue` if not found/type mismatch
- **Thread Safety**: Read operation using shared lock, allows concurrent reads

```cpp
// Get with default values
int width = settings.get<int>("graphics", "resolution_width", 1920);
float volume = settings.get<float>("audio", "master_volume", 1.0f);
bool vsync = settings.get<bool>("graphics", "vsync", true);
std::string lang = settings.get<std::string>("gameplay", "language", "en");

// Type mismatch returns default
int value = settings.get<int>("audio", "master_volume", 100); // Returns 100 (master_volume is float)
```

### Setting Values

#### `template<typename T> bool set(const std::string& category, const std::string& key, const T& value)`
Sets a typed setting value.
- **Template Parameters**:
  - `T`: Type of the setting (int, float, bool, std::string, or convertible to string)
- **Parameters**:
  - `category`: Setting category
  - `key`: Setting key within the category
  - `value`: New value for the setting
- **Returns**: `true` if set successful, `false` for unsupported types
- **Side Effects**: Triggers change callbacks if registered
- **Thread Safety**: Write operation, blocks concurrent access

```cpp
// Set various types
settings.set("graphics", "resolution_width", 2560);
settings.set("audio", "master_volume", 0.8f);
settings.set("graphics", "fullscreen", true);
settings.set("gameplay", "player_name", std::string("Player1"));

// String-convertible types
settings.set("gameplay", "difficulty", "hard"); // const char* converts to std::string
```

### Checking and Removing

#### `bool has(const std::string& category, const std::string& key) const`
Checks if a setting exists.
- **Returns**: `true` if setting exists, `false` otherwise
- **Thread Safety**: Read operation

```cpp
if (settings.has("graphics", "resolution_width")) {
    int width = settings.get<int>("graphics", "resolution_width");
}
```

#### `bool remove(const std::string& category, const std::string& key)`
Removes a setting.
- **Returns**: `true` if setting was removed, `false` if it didn't exist
- **Side Effects**: Removes empty categories automatically
- **Thread Safety**: Write operation

```cpp
settings.remove("gameplay", "legacy_option");
```

#### `bool clearCategory(const std::string& category)`
Clears all settings in a category.
- **Returns**: `true` if category existed and was cleared, `false` otherwise
- **Thread Safety**: Write operation

```cpp
// Reset all audio settings
settings.clearCategory("audio");
```

#### `void clearAll()`
Clears all settings.
- **Thread Safety**: Write operation

```cpp
// Complete reset
settings.clearAll();
```

### Change Listeners

#### `size_t registerChangeListener(const std::string& category, ChangeCallback callback)`
Registers a callback for setting changes.
- **Parameters**:
  - `category`: Category to watch (empty string watches all categories)
  - `callback`: Function to call when settings change
- **Returns**: Callback ID that can be used to unregister
- **Thread Safety**: Callback is invoked outside locks to prevent deadlock

```cpp
// Watch specific category
size_t graphicsListenerId = settings.registerChangeListener("graphics",
    [](const std::string& category, const std::string& key, const SettingsManager::SettingValue& newValue) {
        SETTINGS_INFO("Graphics setting changed: " + key);

        // Handle specific setting
        if (key == "resolution_width" || key == "resolution_height") {
            // Update display resolution
            applyDisplaySettings();
        }
    }
);

// Watch all categories
size_t allListenerId = settings.registerChangeListener("",
    [](const std::string& category, const std::string& key, const SettingsManager::SettingValue& newValue) {
        SETTINGS_INFO("Setting changed: " + category + "." + key);
    }
);
```

#### `void unregisterChangeListener(size_t callbackId)`
Unregisters a change listener.
```cpp
settings.unregisterChangeListener(graphicsListenerId);
```

### Introspection

#### `void getCategories(std::vector<std::string>& outCategories) const`
Gets all category names.
- **Side Effects**: Clears `outCategories` and populates with category names
- **Thread Safety**: Read operation

```cpp
std::vector<std::string> categories;
settings.getCategories(categories);
for (const auto& category : categories) {
    SETTINGS_INFO("Category: " + category);
}
```

#### `void getKeys(const std::string& category, std::vector<std::string>& outKeys) const`
Gets all keys in a category.
- **Side Effects**: Clears `outKeys` and populates with keys in the category
- **Thread Safety**: Read operation

```cpp
std::vector<std::string> keys;
settings.getKeys("graphics", keys);
for (const auto& key : keys) {
    SETTINGS_INFO("Graphics setting: " + key);
}
```

## Integration Examples

### GameEngine Integration
```cpp
// In GameEngine::init() (after SDL_Init / ResourcePath::init())
const std::string settingsPath = VoidLight::ResourcePath::resolve("res/settings.json");
auto& settingsManager = VoidLight::SettingsManager::Instance();
if (!settingsManager.loadFromFile(settingsPath)) {
    GAMEENGINE_WARN("Failed to load settings.json - using defaults");
}

// Read window configuration with defaults (no defaults are written back)
const int width = settingsManager.get<int>("graphics", "resolution_width", 1280);
const int height = settingsManager.get<int>("graphics", "resolution_height", 720);
bool fullscreen = settingsManager.get<bool>("graphics", "fullscreen", false);
```

`GameEngine` registers no change listener; settings are read at init and
applied explicitly by `SettingsMenuState` on apply.

### Settings Menu Integration
```cpp
// In SettingsMenuState
class SettingsMenuState : public GameState {
private:
    std::unordered_map<std::string, SettingsManager::SettingValue> m_tempSettings;

    void loadCurrentSettings() {
        auto& settings = SettingsManager::Instance();

        // Load current values into temp storage
        m_tempSettings["graphics.vsync"] = settings.get<bool>("graphics", "vsync", true);
        m_tempSettings["audio.master_volume"] = settings.get<float>("audio", "master_volume", 0.8f);
        // ... more settings
    }

    void applySettings() {
        auto& settings = SettingsManager::Instance();

        // Apply temp settings to actual settings
        settings.set("graphics", "vsync", std::get<bool>(m_tempSettings["graphics.vsync"]));
        settings.set("audio", "master_volume", std::get<float>(m_tempSettings["audio.master_volume"]));
        // ... more settings

        // Persist to disk
        settings.saveToFile("res/settings.json");
    }

    void cancelSettings() {
        // Discard temp settings, reload from file
        loadCurrentSettings();
    }
};
```

### Audio System Integration
```cpp
// In GameEngine::init(), after SoundManager loads audio (SoundManager does not
// read SettingsManager itself and has no master volume)
auto& settingsMgr = SettingsManager::Instance();
bool const muted = settingsMgr.get<bool>("audio", "muted", false);
float const masterVolume = settingsMgr.get<float>("audio", "master_volume", 1.0f);
float const musicVolume = settingsMgr.get<float>("audio", "music_volume", 0.7f);
float const sfxVolume = settingsMgr.get<float>("audio", "sfx_volume", 0.8f);

soundMgr.setMusicVolume(muted ? 0.0f : masterVolume * musicVolume);
soundMgr.setSFXVolume(muted ? 0.0f : masterVolume * sfxVolume);

// A change listener could re-apply these on edit (no production caller registers one):
// settingsMgr.registerChangeListener("audio", callback);
```

## Performance Considerations

### Thread Safety Model
- **Shared Mutex**: `std::shared_mutex` allows multiple concurrent reads or single write
- **Read Operations**: `get()`, `has()`, `getCategories()`, `getKeys()`, `saveToFile()`
- **Write Operations**: `set()`, `remove()`, `clearCategory()`, `clearAll()`, `loadFromFile()`
- **Lock-Free Notifications**: Callbacks are invoked outside locks to prevent deadlock

### Memory Efficiency
- **Category Map**: Nested `unordered_map` for O(1) average lookup
- **Variant Storage**: 32 bytes per setting (variant overhead + largest type)
- **Typical Memory**: ~100KB for 1000 settings (including map overhead)

### Best Practices
1. **Load Once**: Load settings during initialization, not every frame
2. **Batch Changes**: Group multiple `set()` calls, save once at the end
3. **Avoid Hot Path**: Don't call `get()` in performance-critical loops - cache values
4. **Use Listeners**: Register change callbacks instead of polling for changes
5. **Default Values**: Always provide sensible defaults to `get()`

```cpp
// BAD: Getting settings every frame
void update(float deltaTime) {
    int maxEntities = settings.get<int>("gameplay", "max_entities", 1000); // Avoid!
    // ...
}

// GOOD: Cache settings, update on change
class AIManager {
    int m_maxEntities = 1000;

    void init() {
        auto& settings = SettingsManager::Instance();
        m_maxEntities = settings.get<int>("gameplay", "max_entities", 1000);

        // Update cache when setting changes
        settings.registerChangeListener("gameplay",
            [this](const auto& cat, const auto& key, const auto& value) {
                if (key == "max_entities") {
                    m_maxEntities = std::get<int>(value);
                }
            }
        );
    }

    void update(float deltaTime) {
        // Use cached value
        if (m_activeEntities.size() > m_maxEntities) {
            cullEntities();
        }
    }
};
```

## Error Handling

### Common Issues
1. **Type Mismatch**: `get<T>()` returns default value if stored type doesn't match T
2. **Missing Settings**: Always provide default values to `get()`
3. **File I/O Errors**: `loadFromFile()` and `saveToFile()` return `false` on failure
4. **Invalid JSON**: `loadFromFile()` logs error and returns `false` for malformed JSON

### Logging
```cpp
// Settings operations log to SETTINGS_* macros
SETTINGS_INFO("message");    // Info level
SETTINGS_WARNING("message"); // Warning level
SETTINGS_ERROR("message");   // Error level
```

### Recovery Strategies
```cpp
// Graceful degradation
auto& settings = SettingsManager::Instance();
if (!settings.loadFromFile("res/settings.json")) {
    SETTINGS_WARNING("Failed to load settings, using defaults");
    applyDefaultSettings(); // Populate with hardcoded defaults
    settings.saveToFile("res/settings.json"); // Create valid file
}
```

## JSON File Format

### File Structure
```json
{
  "graphics": {
    "resolution_width": 1920,
    "resolution_height": 1080,
    "vsync": true,
    "fullscreen": false,
    "max_fps": 144
  },
  "audio": {
    "master_volume": 0.8,
    "music_volume": 0.6,
    "sfx_volume": 0.7,
    "mute_on_focus_loss": true
  },
  "gameplay": {
    "difficulty": "normal",
    "auto_save": true,
    "save_interval": 300,
    "player_name": "Player1"
  },
  "debug": {
    "show_fps": false,
    "show_collision_boxes": false,
    "log_level": "info"
  }
}
```

### Type Detection
- **Boolean**: `true`, `false`
- **Integer**: Whole numbers (1920, -5, 0)
- **Float**: Decimal numbers (0.8, 3.14, -2.5)
- **String**: Quoted text ("normal", "Player1")

### Automatic Formatting
`saveToFile()` automatically formats JSON with:
- 2-space indentation
- Category grouping
- Alphabetical ordering within categories (not guaranteed)

## Testing

### Unit Tests
Located in `tests/SettingsManagerTests.cpp`:
- Type-safe get/set operations
- Category management
- File persistence (load/save)
- Change listener callbacks
- Thread safety validation
- Default value handling

### Test Execution
```bash
# Run SettingsManager tests
./bin/debug/SettingsManagerTests

# With verbose output
./bin/debug/SettingsManagerTests --log_level=all
```

## Thread Safety Guarantees

### Safe Operations
✓ Concurrent reads (`get()`, `has()`, `getCategories()`, `getKeys()`)
✓ Concurrent reads with single write
✓ Callback invocation outside locks (no deadlock)
✓ Safe iteration during introspection

### Unsafe Patterns (Avoided)
✗ Modifying settings inside change listener (use deferred updates)
✗ Calling `loadFromFile()` during active use (coordinate state transitions)

### Example: Thread-Safe Reading
```cpp
// Thread 1: Update thread
auto& settings = SettingsManager::Instance();
settings.set("graphics", "vsync", false); // Exclusive write lock

// Thread 2: Render thread (concurrent)
int width = settings.get<int>("graphics", "resolution_width", 1920); // Shared read lock

// Thread 3: Background thread (concurrent)
bool vsync = settings.get<bool>("graphics", "vsync", true); // Shared read lock
```

## Migration and Versioning

### Adding New Settings
```cpp
// Always use default values for new settings
int newSetting = settings.get<int>("gameplay", "new_feature_level", 1);

// No explicit migration needed - defaults handle missing keys
```

### Removing Deprecated Settings
```cpp
// Optional: Clean up old settings on load
auto& settings = SettingsManager::Instance();
settings.loadFromFile("res/settings.json");

// Remove deprecated settings
settings.remove("graphics", "old_deprecated_option");
settings.saveToFile("res/settings.json"); // Persist cleanup
```

### Category Reorganization
```cpp
// Move setting from one category to another
auto& settings = SettingsManager::Instance();

// Read from old location with default
int value = settings.get<int>("old_category", "setting_name", 100);

// Write to new location
settings.set("new_category", "setting_name", value);

// Clean up old location
settings.remove("old_category", "setting_name");
```

## See Also

- [JsonReader Documentation](../utils/JsonReader.md) - JSON loading utilities
- [EventManager Documentation](../events/EventManager.md) - Event-driven architecture
- [GameStateManager Documentation](GameStateManager.md) - State management integration
- [UIManager Guide](../ui/UIManager_Guide.md) - Settings UI implementation
