/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef SETTINGS_MANAGER_HPP
#define SETTINGS_MANAGER_HPP

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace VoidLight {

/**
 * @brief Thread-safe settings management system with category organization
 *
 * Provides type-safe access to game settings with JSON persistence,
 * change notifications, and default value support.
 *
 * Usage:
 *   auto& settings = SettingsManager::Instance();
 *   settings.loadFromFile("res/settings.json");
 *   int width = settings.get<int>("graphics", "resolution_width", 1920);
 *   settings.set("graphics", "vsync", true);
 *   settings.saveToFile("res/settings.json");
 */
class SettingsManager {
public:
    ~SettingsManager() = default;

    /**
     * @brief Gets the singleton instance of SettingsManager
     * @return Reference to the SettingsManager singleton instance
     */
    static SettingsManager& Instance() {
        static SettingsManager instance;
        return instance;
    }

    /**
     * @brief Supported setting value types
     */
    using SettingValue = std::variant<int, float, bool, std::string>;

    /**
     * @brief Callback function type for change notifications
     * @param category The category that changed
     * @param key The setting key that changed
     * @param newValue The new value of the setting
     */
    using ChangeCallback = std::function<void(const std::string& category,
                                             const std::string& key,
                                             const SettingValue& newValue)>;

    /**
     * @brief Loads settings from a JSON file
     * @param filepath Path to the JSON settings file
     * @return true if loading successful, false otherwise
     */
    [[nodiscard]] bool loadFromFile(const std::string& filepath);

    /**
     * @brief Saves current settings to a JSON file
     * @param filepath Path to save the JSON settings file
     * @return true if saving successful, false otherwise
     */
    bool saveToFile(const std::string& filepath);

    /**
     * @brief Gets a typed setting value with optional default
     * @tparam T Type of the setting (int, float, bool, or std::string)
     * @param category Setting category (e.g., "graphics", "audio")
     * @param key Setting key within the category
     * @param defaultValue Value to return if setting doesn't exist
     * @return The setting value or defaultValue if not found
     *
     * Thread-safe for concurrent reads
     */
    template<typename T>
    T get(const std::string& category, const std::string& key, T defaultValue = T{}) const;

    /**
     * @brief Sets a typed setting value
     * @tparam T Type of the setting (int, float, bool, or std::string)
     * @param category Setting category (e.g., "graphics", "audio")
     * @param key Setting key within the category
     * @param value New value for the setting
     * @return true if set successful, false otherwise
     *
     * Thread-safe write operation. Triggers change callbacks if registered.
     */
    template<typename T>
    bool set(const std::string& category, const std::string& key, const T& value);

    /**
     * @brief Checks if a setting exists
     * @param category Setting category
     * @param key Setting key within the category
     * @return true if setting exists, false otherwise
     */
    bool has(const std::string& category, const std::string& key) const;

    /**
     * @brief Removes a setting
     * @param category Setting category
     * @param key Setting key within the category
     * @return true if setting was removed, false if it didn't exist
     */
    bool remove(const std::string& category, const std::string& key);

    /**
     * @brief Clears all settings in a category
     * @param category Category to clear
     * @return true if category existed and was cleared, false otherwise
     */
    bool clearCategory(const std::string& category);

    /**
     * @brief Clears all settings
     */
    void clearAll();

    /**
     * @brief Registers a callback for setting changes
     * @param category Category to watch (empty string watches all categories)
     * @param callback Function to call when settings change
     * @return Callback ID that can be used to unregister
     */
    size_t registerChangeListener(const std::string& category, ChangeCallback callback);

    /**
     * @brief Unregisters a change listener
     * @param callbackId ID returned from registerChangeListener
     */
    void unregisterChangeListener(size_t callbackId);

    /**
     * @brief Gets all category names
     * @param outCategories Output vector to populate with category names
     *
     * Thread-safe for concurrent reads
     */
    void getCategories(std::vector<std::string>& outCategories) const;

    /**
     * @brief Gets all keys in a category
     * @param category Category name
     * @param outKeys Output vector to populate with keys in the category
     *
     * Thread-safe for concurrent reads
     */
    void getKeys(const std::string& category, std::vector<std::string>& outKeys) const;

private:
    /**
     * @brief Internal structure to store settings by category
     */
    using CategorySettings = std::unordered_map<std::string, SettingValue>;
    std::unordered_map<std::string, CategorySettings> m_settings;

    /**
     * @brief Thread-safe read-write lock
     * Allows multiple concurrent reads or single write
     */
    mutable std::shared_mutex m_settingsMutex;

    /**
     * @brief Change listener storage
     */
    struct ListenerInfo {
        size_t id;
        std::string category;
        ChangeCallback callback;
    };
    std::vector<ListenerInfo> m_listeners;
    mutable std::mutex m_listenersMutex;
    size_t m_nextCallbackId = 0;

    /**
     * @brief Notifies all registered listeners of a change
     * @param category Category that changed
     * @param key Key that changed
     * @param newValue New value
     */
    void notifyListeners(const std::string& category, const std::string& key, const SettingValue& newValue);

    /**
     * @brief Helper to convert variant to string for JSON serialization
     */
    std::string variantToString(const SettingValue& value) const;

    // Delete copy constructor and assignment operator
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    SettingsManager() = default;
};

// Template implementations must be in header for linking

template<typename T>
T SettingsManager::get(const std::string& category, const std::string& key, T defaultValue) const {
    std::shared_lock<std::shared_mutex> lock(m_settingsMutex);

    auto categoryIt = m_settings.find(category);
    if (categoryIt == m_settings.end()) {
        return defaultValue;
    }

    auto keyIt = categoryIt->second.find(key);
    if (keyIt == categoryIt->second.end()) {
        return defaultValue;
    }

    // Try to extract the value with the correct type
    try {
        if constexpr (std::is_same_v<T, int>) {
            return std::get<int>(keyIt->second);
        } else if constexpr (std::is_same_v<T, float>) {
            return std::get<float>(keyIt->second);
        } else if constexpr (std::is_same_v<T, bool>) {
            return std::get<bool>(keyIt->second);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return std::get<std::string>(keyIt->second);
        } else {
            // Unsupported type, return default
            return defaultValue;
        }
    } catch (const std::bad_variant_access&) {
        // Type mismatch, return default
        return defaultValue;
    }
}

template<typename T>
bool SettingsManager::set(const std::string& category, const std::string& key, const T& value) {
    SettingValue settingValue;

    // Convert to SettingValue variant
    if constexpr (std::is_same_v<T, int>) {
        settingValue = value;
    } else if constexpr (std::is_same_v<T, float>) {
        settingValue = value;
    } else if constexpr (std::is_same_v<T, bool>) {
        settingValue = value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        settingValue = value;
    } else if constexpr (std::is_convertible_v<T, std::string>) {
        settingValue = std::string(value);
    } else {
        // Unsupported type
        return false;
    }

    {
        std::unique_lock<std::shared_mutex> lock(m_settingsMutex);
        m_settings[category][key] = settingValue;
    }

    // Notify listeners outside the lock to prevent deadlock
    notifyListeners(category, key, settingValue);

    return true;
}

} // namespace VoidLight

#endif // SETTINGS_MANAGER_HPP
