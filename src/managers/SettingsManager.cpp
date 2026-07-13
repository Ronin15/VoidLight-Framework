/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/SettingsManager.hpp"
#include "core/Logger.hpp"
#include "utils/JsonReader.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <ostream>
#include <string_view>

namespace {
// Writes a JSON string literal (surrounding quotes included) with the standard
// escapes, mirroring JsonReader's writeEscapedString so serialized settings
// round-trip through JsonReader on load.
void writeEscapedJsonString(std::ostream &stream, std::string_view s) {
  stream << '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"':  stream << "\\\""; break;
    case '\\': stream << "\\\\"; break;
    case '\b': stream << "\\b";  break;
    case '\f': stream << "\\f";  break;
    case '\n': stream << "\\n";  break;
    case '\r': stream << "\\r";  break;
    case '\t': stream << "\\t";  break;
    default:
      if (c < 0x20) {
        stream << std::format("\\u{:04x}", static_cast<unsigned int>(c));
      } else {
        stream << static_cast<char>(c);
      }
      break;
    }
  }
  stream << '"';
}
} // namespace

namespace VoidLight {

bool SettingsManager::loadFromFile(const std::string &filepath) {
  JsonReader reader;
  if (!reader.loadFromFile(filepath)) {
    SETTINGS_ERROR(std::format("Failed to load settings from file: {} - {}",
                               filepath, reader.getLastError()));
    return false;
  }

  const JsonValue &root = reader.getRoot();
  if (!root.isObject()) {
    SETTINGS_ERROR(
        std::format("Settings file root is not a JSON object: {}", filepath));
    return false;
  }

  std::unique_lock<std::shared_mutex> lock(m_settingsMutex);

  // Parse each category
  const JsonObject &rootObj = root.asObject();
  for (const auto &[categoryName, categoryValue] : rootObj) {
    if (!categoryValue.isObject()) {
      SETTINGS_WARNING(std::format("Category '{}' is not an object, skipping",
                                   categoryName));
      continue;
    }

    const JsonObject &categoryObj = categoryValue.asObject();
    for (const auto &[key, value] : categoryObj) {
      SettingValue settingValue;

      // Convert JSON value to SettingValue variant
      if (value.isBool()) {
        settingValue = value.asBool();
      } else if (value.isNumber()) {
        double numValue = value.asNumber();
        // Classify as int only when the value is a finite whole number within
        // int range; otherwise keep it as float. Casting an out-of-range double
        // to int is undefined behavior, so range-check before the truncating
        // comparison.
        if (std::isfinite(numValue) &&
            numValue >= static_cast<double>(std::numeric_limits<int>::min()) &&
            numValue <= static_cast<double>(std::numeric_limits<int>::max()) &&
            numValue == std::trunc(numValue)) {
          settingValue = static_cast<int>(numValue);
        } else {
          settingValue = static_cast<float>(numValue);
        }
      } else if (value.isString()) {
        settingValue = value.asString();
      } else {
        SETTINGS_WARNING(
            std::format("Unsupported value type for setting '{}.{}', skipping",
                        categoryName, key));
        continue;
      }

      m_settings[categoryName][key] = settingValue;
    }
  }

  SETTINGS_INFO(std::format("Loaded settings from file: {}", filepath));
  return true;
}

bool SettingsManager::saveToFile(const std::string &filepath) {
  std::shared_lock<std::shared_mutex> lock(m_settingsMutex);

  std::ofstream file(filepath);
  if (!file.is_open()) {
    SETTINGS_ERROR(
        std::format("Failed to open settings file for writing: {}", filepath));
    return false;
  }

  // Build JSON manually for formatting control
  file << "{\n";

  size_t categoryIndex = 0;
  for (const auto &[categoryName, categorySettings] : m_settings) {
    file << "  ";
    writeEscapedJsonString(file, categoryName);
    file << ": {\n";

    size_t keyIndex = 0;
    for (const auto &[key, value] : categorySettings) {
      file << "    ";
      writeEscapedJsonString(file, key);
      file << ": ";

      // Write value based on type
      std::visit(
          [&file](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
              file << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int>) {
              file << arg;
            } else if constexpr (std::is_same_v<T, float>) {
              file << arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
              writeEscapedJsonString(file, arg);
            }
          },
          value);

      if (keyIndex < categorySettings.size() - 1) {
        file << ",";
      }
      file << "\n";
      keyIndex++;
    }

    file << "  }";
    if (categoryIndex < m_settings.size() - 1) {
      file << ",";
    }
    file << "\n";
    categoryIndex++;
  }

  file << "}\n";
  file.close();

  SETTINGS_INFO(std::format("Saved settings to file: {}", filepath));
  return true;
}

bool SettingsManager::has(const std::string &category,
                          const std::string &key) const {
  std::shared_lock<std::shared_mutex> lock(m_settingsMutex);

  auto categoryIt = m_settings.find(category);
  if (categoryIt == m_settings.end()) {
    return false;
  }

  return categoryIt->second.find(key) != categoryIt->second.end();
}

bool SettingsManager::remove(const std::string &category,
                             const std::string &key) {
  std::unique_lock<std::shared_mutex> lock(m_settingsMutex);

  auto categoryIt = m_settings.find(category);
  if (categoryIt == m_settings.end()) {
    return false;
  }

  auto keyIt = categoryIt->second.find(key);
  if (keyIt == categoryIt->second.end()) {
    return false;
  }

  categoryIt->second.erase(keyIt);

  // Remove category if empty
  if (categoryIt->second.empty()) {
    m_settings.erase(categoryIt);
  }

  return true;
}

bool SettingsManager::clearCategory(const std::string &category) {
  std::unique_lock<std::shared_mutex> lock(m_settingsMutex);

  auto categoryIt = m_settings.find(category);
  if (categoryIt == m_settings.end()) {
    return false;
  }

  m_settings.erase(categoryIt);
  return true;
}

void SettingsManager::clearAll() {
  std::unique_lock<std::shared_mutex> lock(m_settingsMutex);
  m_settings.clear();
}

size_t SettingsManager::registerChangeListener(const std::string &category,
                                               ChangeCallback callback) {
  std::lock_guard<std::mutex> lock(m_listenersMutex);

  size_t id = m_nextCallbackId++;
  m_listeners.push_back({id, category, std::move(callback)});

  return id;
}

void SettingsManager::unregisterChangeListener(size_t callbackId) {
  std::lock_guard<std::mutex> lock(m_listenersMutex);

  m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                   [callbackId](const ListenerInfo &info) {
                                     return info.id == callbackId;
                                   }),
                    m_listeners.end());
}

void SettingsManager::getCategories(
    std::vector<std::string> &outCategories) const {
  std::shared_lock<std::shared_mutex> lock(m_settingsMutex);

  outCategories.clear();
  outCategories.reserve(m_settings.size());

  for (const auto &[category, _] : m_settings) {
    outCategories.push_back(category);
  }
}

void SettingsManager::getKeys(const std::string &category,
                              std::vector<std::string> &outKeys) const {
  std::shared_lock<std::shared_mutex> lock(m_settingsMutex);

  outKeys.clear();

  auto categoryIt = m_settings.find(category);
  if (categoryIt == m_settings.end()) {
    return;
  }

  outKeys.reserve(categoryIt->second.size());

  for (const auto &[key, _] : categoryIt->second) {
    outKeys.push_back(key);
  }
}

void SettingsManager::notifyListeners(const std::string &category,
                                      const std::string &key,
                                      const SettingValue &newValue) {
  std::lock_guard<std::mutex> lock(m_listenersMutex);

  for (const auto &listener : m_listeners) {
    // Call listener if it's watching all categories or this specific category
    if (listener.category.empty() || listener.category == category) {
      listener.callback(category, key, newValue);
    }
  }
}

std::string SettingsManager::variantToString(const SettingValue &value) const {
  return std::visit(
      [](auto &&arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
          return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
          return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, float>) {
          return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return arg;
        }
        return "";
      },
      value);
}

} // namespace VoidLight
