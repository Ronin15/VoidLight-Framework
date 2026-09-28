/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef FONT_MANAGER_HPP
#define FONT_MANAGER_HPP

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
// filesystem is used in the implementation file

class FontManager {
public:
    ~FontManager() {
        if (!m_isShutdown) {
            clean();
        }
    }

    static FontManager& Instance() {
        static FontManager instance;
        return instance;
    }

    /**
   * @brief Initializes the TTF font system
   * @return true if initialization successful, false otherwise
   */
    [[nodiscard]] bool init();

    /**
   * @brief Loads a font with specified size from file or directory
   * @param fontFile Path to font file or directory containing TTF/OTF files
   * @param fontID Unique identifier for the font(s). Used as prefix when loading directory
   * @param fontSize Size of the font in points
   * @return true if at least one font was loaded successfully, false otherwise
   */
    [[nodiscard]] bool loadFont(const std::string& fontFile, const std::string& fontID, int fontSize);

    /**
   * @brief Loads default fonts with sizes calculated based on display characteristics
   * @param fontPath Path to font file or directory containing TTF/OTF files
   * @param windowWidth Current window width in logical pixels
   * @param windowHeight Current window height in logical pixels
   * @param dpiScale DPI scale factor for high-DPI displays (e.g., 2.0 for Retina)
   * @return true if fonts were loaded successfully, false otherwise
   */
    [[nodiscard]] bool loadFontsForDisplay(const std::string& fontPath, int windowWidth, int windowHeight, float dpiScale = 1.0f);

    /**
   * @brief Checks if a font is loaded in memory
   * @param fontID Unique identifier of the font to check
   * @return true if font is loaded, false otherwise
   */
    bool isFontLoaded(const std::string& fontID) const;

    /**
   * @brief Removes a specific font from memory
   * @param fontID Unique identifier of the font to remove
   */
    void clearFont(const std::string& fontID);

    /**
   * @brief Safely reloads all fonts for display changes (e.g., DPI changes)
   * @param fontPath Directory containing font files
   * @param windowWidth Current window width in logical pixels
   * @param windowHeight Current window height in logical pixels
   * @param dpiScale DPI scale factor for high-DPI displays (e.g., 2.0 for Retina)
   * @return true if fonts were successfully reloaded, false otherwise
   */
    [[nodiscard]] bool reloadFontsForDisplay(const std::string& fontPath, int windowWidth, int windowHeight, float dpiScale = 1.0f);

    /**
   * @brief Cleans up all font resources and shuts down TTF system
   */
    void clean();

    /**
   * @brief Checks if FontManager has been shut down
   * @return true if manager is shut down, false otherwise
   */
    bool isShutdown() const { return m_isShutdown; }

    /**
   * @brief Checks if all fonts have been loaded successfully
   * @return true if fonts are loaded and ready, false otherwise
   */
    bool areFontsLoaded() const { return m_fontsLoaded.load(std::memory_order_acquire); }

    /**
   * @brief Measures text dimensions for a given font and string
   * @param text Text string to measure
   * @param fontID Font identifier to use for measurement
   * @param width Pointer to store calculated width
   * @param height Pointer to store calculated height
   * @return true if measurement successful, false otherwise
   */
    bool measureText(const std::string& text, const std::string& fontID, int* width, int* height);

    /**
   * @brief Gets font metrics (line height, etc.) for auto-sizing calculations
   * @param fontID Font identifier to get metrics for
   * @param lineHeight Pointer to store line height
   * @param ascent Pointer to store font ascent
   * @param descent Pointer to store font descent
   * @return true if metrics retrieved successfully, false otherwise
   */
    bool getFontMetrics(const std::string& fontID, int* lineHeight, int* ascent, int* descent);

    /**
   * @brief Calculates optimal size for multi-line text content
   * @param text Multi-line text string (may contain newlines)
   * @param fontID Font identifier to use
   * @param maxWidth Maximum width constraint (0 = no limit)
   * @param width Pointer to store calculated total width
   * @param height Pointer to store calculated total height
   * @return true if calculation successful, false otherwise
   */
    bool measureMultilineText(const std::string& text, const std::string& fontID,
        int maxWidth, int* width, int* height);

    /**
   * @brief Measure text with word wrapping
   * @param text Text to measure
   * @param fontID Font identifier to use
   * @param maxWidth Maximum width for wrapping
   * @param width Pointer to store calculated total width
   * @param height Pointer to store calculated total height
   * @return true if calculation successful, false otherwise
   */
    bool measureTextWithWrapping(const std::string& text, const std::string& fontID,
        int maxWidth, int* width, int* height);

    /**
   * @brief Wrap text into lines that fit within specified width
   * @param text Text to wrap
   * @param fontID Font identifier to use
   * @param maxWidth Maximum width constraint
   * @return Vector of wrapped lines
   */
    std::vector<std::string> wrapTextToLines(const std::string& text,
        const std::string& fontID,
        int maxWidth);

    /**
   * @brief Prepare an atlas-backed GPU text object for subsequent draw data queries
   * @param key Stable identifier for the GPU text object lifetime
   * @param text Text string to render
   * @param fontID Unique identifier of the font to use
   * @param width Optional pointer to receive the logical text width
   * @param height Optional pointer to receive the logical text height
   * @return true if the text object is ready, false otherwise
   */
    bool prepareGPUText(const std::string& key, const std::string& text,
        const std::string& fontID, int* width = nullptr,
        int* height = nullptr);

    /**
   * @brief Set the upper-left position of a prepared GPU text object in pixels
   * @param key Stable identifier passed to prepareGPUText()
   * @param x X offset of the text's upper-left corner
   * @param y Y offset of the text's upper-left corner
   * @return true if the position was updated, false otherwise
   */
    bool setGPUTextPosition(const std::string& key, int x, int y);

    /**
   * @brief Get SDL3_ttf GPU draw data for a prepared text object
   * @param key Stable identifier passed to prepareGPUText()
   * @return Draw sequence list owned by SDL3_ttf, or nullptr if unavailable
   */
    TTF_GPUAtlasDrawSequence* getGPUTextDrawData(const std::string& key);

    /**
   * @brief Clear prepared GPU text objects (call on state transitions or when memory is needed)
   */
    void clearGPUTextCache();

private:
    std::unordered_map<std::string, std::shared_ptr<TTF_Font>> m_fontMap{};
    std::atomic<bool> m_fontsLoaded{false};
    bool m_isShutdown{false};
    std::vector<std::string> m_fontFilePaths{};
    std::mutex m_fontLoadMutex{};

    // Display size tracking to prevent unnecessary font reloads
    int m_lastWindowWidth{0};
    int m_lastWindowHeight{0};
    std::string m_lastFontPath{};

    struct GPUTextEntry {
        TTF_Text* text{nullptr};
        std::string fontID{};
        std::string stringValue{};
        int width{0};
        int height{0};
    };

    bool ensureGPUTextEngine();
    void destroyGPUTextObjects();

    TTF_TextEngine* mp_gpuTextEngine{nullptr};
    std::unordered_map<std::string, GPUTextEntry> m_gpuTextEntries{};

    // Delete copy constructor and assignment operator
    FontManager(const FontManager&) = delete; // Prevent copying
    FontManager& operator=(const FontManager&) = delete; // Prevent assignment

    FontManager() = default;
};

#endif // FONT_MANAGER_HPP
