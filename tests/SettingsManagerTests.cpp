/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE SettingsManagerTests
#include <boost/test/unit_test.hpp>
#include "managers/SettingsManager.hpp"
#include <filesystem>
#include <fstream>
#include <thread>

using namespace VoidLight;

// Test fixture for setup/cleanup
struct SettingsTestFixture {
    const std::string testFile = "tests/test_data/test_settings.json";

    SettingsTestFixture() {
        // Ensure test_data directory exists
        std::filesystem::create_directories("tests/test_data");

        // Clear any existing settings
        SettingsManager::Instance().clearAll();
    }

    ~SettingsTestFixture() {
        // Cleanup test files
        try {
            if (std::filesystem::exists(testFile)) {
                std::filesystem::remove(testFile);
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    void createTestFile(const std::string& content) {
        std::ofstream file(testFile);
        file << content;
        file.close();
    }
};

BOOST_FIXTURE_TEST_SUITE(SettingsManagerTestSuite, SettingsTestFixture)

BOOST_AUTO_TEST_CASE(TestGetSetInt) {
    auto& settings = SettingsManager::Instance();

    // Set and get int value
    BOOST_CHECK(settings.set("graphics", "width", 1920));
    BOOST_CHECK_EQUAL(settings.get<int>("graphics", "width", 0), 1920);

    // Test default value when key doesn't exist
    BOOST_CHECK_EQUAL(settings.get<int>("graphics", "nonexistent", 42), 42);
}

BOOST_AUTO_TEST_CASE(TestGetSetFloat) {
    auto& settings = SettingsManager::Instance();

    // Set and get float value
    BOOST_CHECK(settings.set("audio", "volume", 0.75f));
    BOOST_CHECK_CLOSE(settings.get<float>("audio", "volume", 0.0f), 0.75f, 0.001f);

    // Test default value
    BOOST_CHECK_CLOSE(settings.get<float>("audio", "nonexistent", 1.0f), 1.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestGetSetBool) {
    auto& settings = SettingsManager::Instance();

    // Set and get bool value
    BOOST_CHECK(settings.set("graphics", "vsync", true));
    BOOST_CHECK_EQUAL(settings.get<bool>("graphics", "vsync", false), true);

    // Change to false
    BOOST_CHECK(settings.set("graphics", "vsync", false));
    BOOST_CHECK_EQUAL(settings.get<bool>("graphics", "vsync", true), false);
}

BOOST_AUTO_TEST_CASE(TestGetSetString) {
    auto& settings = SettingsManager::Instance();

    // Set and get string value
    BOOST_CHECK(settings.set("gameplay", "difficulty", std::string("hard")));
    BOOST_CHECK_EQUAL(settings.get<std::string>("gameplay", "difficulty", ""), "hard");

    // Test default value
    BOOST_CHECK_EQUAL(settings.get<std::string>("gameplay", "nonexistent", "default"), "default");
}

BOOST_AUTO_TEST_CASE(TestHasMethod) {
    auto& settings = SettingsManager::Instance();

    settings.set("test", "key", 42);

    BOOST_CHECK(settings.has("test", "key"));
    BOOST_CHECK(!settings.has("test", "nonexistent"));
    BOOST_CHECK(!settings.has("nonexistent", "key"));
}

BOOST_AUTO_TEST_CASE(TestRemoveMethod) {
    auto& settings = SettingsManager::Instance();

    settings.set("test", "key1", 1);
    settings.set("test", "key2", 2);

    BOOST_CHECK(settings.has("test", "key1"));
    BOOST_CHECK(settings.remove("test", "key1"));
    BOOST_CHECK(!settings.has("test", "key1"));

    // key2 should still exist
    BOOST_CHECK(settings.has("test", "key2"));

    // Removing non-existent key returns false
    BOOST_CHECK(!settings.remove("test", "nonexistent"));
}

BOOST_AUTO_TEST_CASE(TestClearCategory) {
    auto& settings = SettingsManager::Instance();

    settings.set("category1", "key1", 1);
    settings.set("category1", "key2", 2);
    settings.set("category2", "key1", 3);

    BOOST_CHECK(settings.clearCategory("category1"));

    BOOST_CHECK(!settings.has("category1", "key1"));
    BOOST_CHECK(!settings.has("category1", "key2"));

    // category2 should still exist
    BOOST_CHECK(settings.has("category2", "key1"));

    // Clearing non-existent category returns false
    BOOST_CHECK(!settings.clearCategory("nonexistent"));
}

BOOST_AUTO_TEST_CASE(TestClearAll) {
    auto& settings = SettingsManager::Instance();

    settings.set("cat1", "key1", 1);
    settings.set("cat2", "key2", 2);

    settings.clearAll();

    BOOST_CHECK(!settings.has("cat1", "key1"));
    BOOST_CHECK(!settings.has("cat2", "key2"));
}

BOOST_AUTO_TEST_CASE(TestGetCategories) {
    auto& settings = SettingsManager::Instance();

    settings.clearAll();
    settings.set("graphics", "key", 1);
    settings.set("audio", "key", 2);
    settings.set("input", "key", 3);

    std::vector<std::string> categories;
    settings.getCategories(categories);

    BOOST_CHECK_EQUAL(categories.size(), 3);

    // Check all categories are present (order doesn't matter)
    bool hasGraphics = false, hasAudio = false, hasInput = false;
    for (const auto& cat : categories) {
        if (cat == "graphics") hasGraphics = true;
        if (cat == "audio") hasAudio = true;
        if (cat == "input") hasInput = true;
    }

    BOOST_CHECK(hasGraphics && hasAudio && hasInput);
}

BOOST_AUTO_TEST_CASE(TestGetKeys) {
    auto& settings = SettingsManager::Instance();

    settings.clearAll();
    settings.set("test", "key1", 1);
    settings.set("test", "key2", 2);
    settings.set("test", "key3", 3);

    std::vector<std::string> keys;
    settings.getKeys("test", keys);

    BOOST_CHECK_EQUAL(keys.size(), 3);

    // Empty category
    std::vector<std::string> emptyKeys;
    settings.getKeys("nonexistent", emptyKeys);
    BOOST_CHECK_EQUAL(emptyKeys.size(), 0);
}

BOOST_AUTO_TEST_CASE(TestLoadFromFile) {
    auto& settings = SettingsManager::Instance();

    // Create test JSON file
    std::string jsonContent = R"({
  "graphics": {
    "width": 1920,
    "height": 1080,
    "vsync": true
  },
  "audio": {
    "volume": 0.8,
    "muted": false
  },
  "gameplay": {
    "difficulty": "hard"
  }
})";

    createTestFile(jsonContent);

    // Load settings
    BOOST_CHECK(settings.loadFromFile(testFile));

    // Verify loaded values
    BOOST_CHECK_EQUAL(settings.get<int>("graphics", "width", 0), 1920);
    BOOST_CHECK_EQUAL(settings.get<int>("graphics", "height", 0), 1080);
    BOOST_CHECK_EQUAL(settings.get<bool>("graphics", "vsync", false), true);
    BOOST_CHECK_CLOSE(settings.get<float>("audio", "volume", 0.0f), 0.8f, 0.001f);
    BOOST_CHECK_EQUAL(settings.get<bool>("audio", "muted", true), false);
    BOOST_CHECK_EQUAL(settings.get<std::string>("gameplay", "difficulty", ""), "hard");
}

BOOST_AUTO_TEST_CASE(TestSaveToFile) {
    auto& settings = SettingsManager::Instance();

    settings.clearAll();
    settings.set("graphics", "width", 1024);
    settings.set("graphics", "fullscreen", true);
    settings.set("audio", "master_volume", 0.9f);
    settings.set("gameplay", "mode", std::string("adventure"));

    // Save to file
    BOOST_CHECK(settings.saveToFile(testFile));

    // Verify file exists
    BOOST_CHECK(std::filesystem::exists(testFile));

    // Clear settings and reload to verify persistence
    settings.clearAll();
    BOOST_CHECK(settings.loadFromFile(testFile));

    BOOST_CHECK_EQUAL(settings.get<int>("graphics", "width", 0), 1024);
    BOOST_CHECK_EQUAL(settings.get<bool>("graphics", "fullscreen", false), true);
    BOOST_CHECK_CLOSE(settings.get<float>("audio", "master_volume", 0.0f), 0.9f, 0.001f);
    BOOST_CHECK_EQUAL(settings.get<std::string>("gameplay", "mode", ""), "adventure");
}

// A whole-valued float (e.g. 1.0f) must survive a save/load round-trip as a
// float, not collapse to an int. Regression guard for the JSON layer preserving
// integer-vs-real token identity (JsonValue::isIntegerNumber()).
BOOST_AUTO_TEST_CASE(TestWholeValuedFloatRoundTrip) {
    auto& settings = SettingsManager::Instance();

    settings.clearAll();
    settings.set("audio", "master_volume", 1.0f);   // whole-valued float
    settings.set("audio", "balance", 0.0f);         // whole-valued float (zero)
    settings.set("graphics", "vsync_level", 2);     // genuine int

    BOOST_CHECK(settings.saveToFile(testFile));

    settings.clearAll();
    BOOST_CHECK(settings.loadFromFile(testFile));

    // Floats reload as floats (would return the default if collapsed to int).
    BOOST_CHECK_CLOSE(settings.get<float>("audio", "master_volume", -1.0f), 1.0f, 0.001f);
    BOOST_CHECK_CLOSE(settings.get<float>("audio", "balance", -1.0f), 0.0f, 0.001f);

    // The genuine int reloads as an int, and get<float> on it returns the
    // default (type identity preserved in both directions).
    BOOST_CHECK_EQUAL(settings.get<int>("graphics", "vsync_level", -1), 2);
    BOOST_CHECK_CLOSE(settings.get<float>("graphics", "vsync_level", 42.0f), 42.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestChangeListener) {
    auto& settings = SettingsManager::Instance();

    int callbackCount = 0;
    std::string lastCategory;
    std::string lastKey;

    // Register listener
    auto callbackId = settings.registerChangeListener("graphics",
        [&](const std::string& category, const std::string& key, const SettingsManager::SettingValue&) {
            callbackCount++;
            lastCategory = category;
            lastKey = key;
        });

    // Make changes
    settings.set("graphics", "width", 1920);
    settings.set("graphics", "height", 1080);
    settings.set("audio", "volume", 0.5f);  // Different category, shouldn't trigger

    // Should have been called twice (only for graphics category)
    BOOST_CHECK_EQUAL(callbackCount, 2);
    BOOST_CHECK_EQUAL(lastCategory, "graphics");
    BOOST_CHECK_EQUAL(lastKey, "height");

    // Unregister and verify no more calls
    settings.unregisterChangeListener(callbackId);
    settings.set("graphics", "vsync", true);

    BOOST_CHECK_EQUAL(callbackCount, 2);  // Should still be 2
}

BOOST_AUTO_TEST_CASE(TestGlobalChangeListener) {
    auto& settings = SettingsManager::Instance();

    int callbackCount = 0;

    // Register global listener (empty category string)
    auto callbackId = settings.registerChangeListener("",
        [&](const std::string&, const std::string&, const SettingsManager::SettingValue&) {
            callbackCount++;
        });

    // Make changes to different categories
    settings.set("graphics", "width", 1920);
    settings.set("audio", "volume", 0.5f);
    settings.set("input", "sensitivity", 1.0f);

    // Should have been called for all changes
    BOOST_CHECK_EQUAL(callbackCount, 3);

    settings.unregisterChangeListener(callbackId);
}

BOOST_AUTO_TEST_CASE(TestThreadSafety) {
    auto& settings = SettingsManager::Instance();

    settings.clearAll();

    const int numThreads = 10;
    const int operationsPerThread = 100;

    std::vector<std::thread> threads;

    // Launch multiple threads doing concurrent operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&settings, t, count=operationsPerThread]() {
            for (int i = 0; i < count; ++i) {
                std::string category = "category" + std::to_string(t);
                std::string key = "key" + std::to_string(i);

                // Write
                settings.set(category, key, i * t);

                // Read
                int value = settings.get<int>(category, key, -1);

                // Should not be -1 (default value)
                BOOST_CHECK(value != -1);

                // Check existence
                BOOST_CHECK(settings.has(category, key));
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all values were written
    for (int t = 0; t < numThreads; ++t) {
        std::string category = "category" + std::to_string(t);
        for (int i = 0; i < operationsPerThread; ++i) {
            std::string key = "key" + std::to_string(i);
            BOOST_CHECK(settings.has(category, key));
        }
    }
}

BOOST_AUTO_TEST_CASE(TestInvalidFile) {
    auto& settings = SettingsManager::Instance();

    // Try to load non-existent file
    BOOST_CHECK(!settings.loadFromFile("nonexistent_file.json"));

    // Try to load invalid JSON
    createTestFile("{ invalid json }");
    BOOST_CHECK(!settings.loadFromFile(testFile));
}

BOOST_AUTO_TEST_CASE(TestTypeMismatch) {
    auto& settings = SettingsManager::Instance();

    // Store as int
    settings.set("test", "value", 42);

    // Try to read as different type - should return default
    BOOST_CHECK_CLOSE(settings.get<float>("test", "value", 99.9f), 99.9f, 0.001f);
    BOOST_CHECK_EQUAL(settings.get<bool>("test", "value", true), true);
    BOOST_CHECK_EQUAL(settings.get<std::string>("test", "value", "default"), "default");
}

BOOST_AUTO_TEST_SUITE_END()
