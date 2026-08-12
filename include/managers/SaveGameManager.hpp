/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef SAVE_GAME_MANAGER_HPP
#define SAVE_GAME_MANAGER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

// Forward declarations
class Player;
class Vector2D;

// SaveGame header structure - used at the beginning of save files
struct SaveGameHeader {
    char signature[9]{'F', 'O', 'R', 'G', 'E', 'S', 'A', 'V', 'E'}; // File signature "FORGESAVE"
    uint32_t version{1};                                          // Save format version
    time_t timestamp{0};                                          // Save timestamp
    uint32_t dataSize{0};                                         // Size of data section
};

// SaveGame data structure for metadata access
struct SaveGameData {
    std::string saveName{};
    std::string timestamp{};
    int playerLevel{0};
    float playerHealth{100.0f};
    float playerXPos{0.0f};   // Added player X position
    float playerYPos{0.0f};   // Added player Y position
    std::string currentLevel{};
    // Add more fields as needed
};

/**
 * @brief Manages save/load operations for game data using binary serialization
 *
 * @note All string parameters in this class use const std::string& (not string_view)
 * because they are passed directly to filesystem APIs (std::ofstream, std::ifstream,
 * std::filesystem::path) which require std::string for construction/opening.
 */
class SaveGameManager {
public:
    ~SaveGameManager() {
        if (!m_isShutdown) {
            clean();
        }
    }

    static SaveGameManager& Instance() {
        static SaveGameManager instance;
        return instance;
    }

    // Initialize save game system
    [[nodiscard]] bool init();

    // Check if initialized
    bool isInitialized() const { return m_isInitialized; }

    // Save game data to a file
    // Returns true if save was successful
    [[nodiscard]] bool save(const std::string& saveFileName, const Player& player);

    // Save game data to a slot (creates a file with a standard naming convention)
    // Returns true if save was successful
    [[nodiscard]] bool saveToSlot(int slotNumber, const Player& player);

    // Load game data from a file
    // Returns true if load was successful
    [[nodiscard]] bool load(const std::string& saveFileName, Player& player) const;

    // Load game data from a slot
    // Returns true if load was successful
    [[nodiscard]] bool loadFromSlot(int slotNumber, Player& player) const;

    // Delete a save file
    // Returns true if deletion was successful
    [[nodiscard]] bool deleteSave(const std::string& saveFileName) const;

    // Delete a save slot
    // Returns true if deletion was successful
    [[nodiscard]] bool deleteSlot(int slotNumber) const;

    // Get a list of all save files in the save directory
    std::vector<std::string> getSaveFiles() const;

    // Get information about a specific save file
    SaveGameData getSaveInfo(const std::string& saveFileName) const;

    // Get information about all save slots
    std::vector<SaveGameData> getAllSaveInfo() const;

    // Check if a save file exists
    bool saveExists(const std::string& saveFileName) const;

    // Check if a save slot is in use
    bool slotExists(int slotNumber) const;

    // Validate if a file is a valid save file
    bool isValidSaveFile(const std::string& saveFileName) const;

    // Set the base directory for save files
    void setSaveDirectory(const std::string& directory);



    // Clean up resources
    void clean();

    // Check if SaveGameManager has been shut down
    bool isShutdown() const { return m_isShutdown; }

private:
    std::string m_saveDirectory{"res"};  // Default save directory
    bool m_isInitialized{false};
    bool m_isShutdown{false};

    // Helper methods
    std::string getSlotFileName(int slotNumber) const;
    std::string getFullSavePath(const std::string& saveFileName) const;
    bool ensureSaveDirectoryExists() const;
    SaveGameData extractSaveInfo(const std::string& saveFileName) const;

    // Binary file operations
    bool writeHeader(std::ofstream& file, uint32_t dataSize) const;
    bool readHeader(std::ifstream& file, SaveGameHeader& header) const;
    bool writeString(std::ofstream& file, const std::string& str) const;
    bool readString(std::ifstream& file, std::string& str) const;
    // Fast binary serialization
    bool readVector2D(std::ifstream& file, Vector2D& vec) const;

    // Delete copy constructor and assignment operator
    SaveGameManager(const SaveGameManager&) = delete;  // prevent copy construction
    SaveGameManager& operator=(const SaveGameManager&) = delete;  // prevent assignment

    SaveGameManager() = default;
};

#endif  // SAVE_GAME_MANAGER_HPP
