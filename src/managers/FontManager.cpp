/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "managers/FontManager.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <filesystem>
#include <vector>
#include <cmath>
#include <sstream>
#include <format>

// Font sizing configuration constants
namespace {
// Universal height-based font sizing (all platforms)
constexpr float HEIGHT_RATIO = 90.0f;

// Font size ratios for different text types
constexpr float UI_FONT_RATIO = 0.875f; // 87.5% of base
constexpr float TITLE_FONT_RATIO = 1.5f; // 150% of base
constexpr float TOOLTIP_FONT_RATIO = 0.6f; // 60% of base

// Font size bounds for edge case protection
constexpr int MAX_FONT_SIZE = 100;

// Minimum readable sizes for specific font types (ensure good readability)
constexpr int MIN_BASE_FONT_SIZE = 18;
constexpr int MIN_UI_FONT_SIZE = 16;
constexpr int MIN_TITLE_FONT_SIZE = 24;
constexpr int MIN_TOOLTIP_FONT_SIZE = 12;
}

bool FontManager::init() {
    if (!TTF_Init()) {
        FONT_CRITICAL(std::format("Font system initialization failed: {}", SDL_GetError()));
        return false;
    } else {
        // Reset shutdown flag when reinitializing
        m_isShutdown = false;
        FONT_INFO("Font system initialized with quality hints!");
        return true;
    }
}

bool FontManager::loadFontsForDisplay(const std::string& fontPath, int windowWidth, int windowHeight, float dpiScale) {
    if (m_fontsLoaded.load(std::memory_order_acquire)) {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_fontLoadMutex);
    // Double-check after acquiring the lock
    if (m_fontsLoaded.load(std::memory_order_acquire)) {
        return true;
    }

    // Scan directory for font files if not already done
    if (m_fontFilePaths.empty()) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(fontPath)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (extension == ".ttf" || extension == ".otf") {
                        m_fontFilePaths.push_back(entry.path().string());
                    }
                }
            }
            // Sort to ensure consistent loading order (e.g., Arial first)
            std::sort(m_fontFilePaths.begin(), m_fontFilePaths.end());
        } catch (const std::filesystem::filesystem_error& e) {
            FONT_ERROR(std::format("Filesystem error while scanning for fonts: {}", e.what()));
            return false;
        }
    }

    // Ensure dpiScale is valid
    float const effectiveDpiScale = (dpiScale > 0.0f) ? dpiScale : 1.0f;

    // Calculate font sizes based on LOGICAL window height first
    // Apply minimums for readability, THEN scale by DPI for high-density displays
    int clampedHeight = std::clamp(windowHeight, 480, 8640);
    float const logicalBaseSizeFloat = static_cast<float>(clampedHeight) / HEIGHT_RATIO;

    // Apply minimums at logical scale, then multiply by DPI scale for pixel rendering
    int logicalBase = std::max(static_cast<int>(std::round(logicalBaseSizeFloat)), MIN_BASE_FONT_SIZE);
    int logicalUI = std::max(static_cast<int>(std::round(logicalBaseSizeFloat * UI_FONT_RATIO)), MIN_UI_FONT_SIZE);
    int logicalTitle = std::max(static_cast<int>(std::round(logicalBaseSizeFloat * TITLE_FONT_RATIO)), MIN_TITLE_FONT_SIZE);
    int logicalTooltip = std::max(static_cast<int>(std::round(logicalBaseSizeFloat * TOOLTIP_FONT_RATIO)), MIN_TOOLTIP_FONT_SIZE);

    // Scale by DPI for pixel-perfect rendering on high-density displays
    int baseFontSize = std::min(static_cast<int>(std::round(logicalBase * effectiveDpiScale)), MAX_FONT_SIZE);
    int uiFontSize = std::min(static_cast<int>(std::round(logicalUI * effectiveDpiScale)), MAX_FONT_SIZE);
    int titleFontSize = std::min(static_cast<int>(std::round(logicalTitle * effectiveDpiScale)), MAX_FONT_SIZE);
    int tooltipFontSize = std::min(static_cast<int>(std::round(logicalTooltip * effectiveDpiScale)), MAX_FONT_SIZE);

    FONT_INFO(std::format("Calculated font sizes (dpiScale={}, logical={}): base={}, UI={}, title={}, tooltip={}",
        effectiveDpiScale, logicalBase, baseFontSize, uiFontSize, titleFontSize, tooltipFontSize));

    bool success = true;
    for (const auto& filePath : m_fontFilePaths) {
        std::string filename = std::filesystem::path(filePath).stem().string();
        success &= loadFont(filePath, std::format("fonts_{}", filename), baseFontSize);
        success &= loadFont(filePath, std::format("fonts_UI_{}", filename), uiFontSize);
        success &= loadFont(filePath, std::format("fonts_title_{}", filename), titleFontSize);
        success &= loadFont(filePath, std::format("fonts_tooltip_{}", filename), tooltipFontSize);
    }

    if (success) {
        m_lastWindowWidth = windowWidth;
        m_lastWindowHeight = windowHeight;
        m_lastFontPath = fontPath;
        m_fontsLoaded.store(true, std::memory_order_release);
        FONT_INFO("All font templates loaded successfully.");
    }

    return success;
}

bool FontManager::loadFont(const std::string& fontFile, const std::string& fontID, int fontSize) {
    auto font = std::shared_ptr<TTF_Font>(TTF_OpenFont(fontFile.c_str(), fontSize), TTF_CloseFont);

    if (!font) {
        FONT_ERROR(std::format("Failed to load font '{}' with size {}: {}", fontFile, fontSize, SDL_GetError()));
        return false;
    }

    TTF_SetFontHinting(font.get(), TTF_HINTING_NORMAL);
    TTF_SetFontKerning(font.get(), true);
    TTF_SetFontStyle(font.get(), TTF_STYLE_NORMAL);

    m_fontMap[fontID] = std::move(font);
    FONT_INFO(std::format("Loaded font '{}' from '{}'", fontID, fontFile));
    return true;
}

std::vector<std::string> FontManager::wrapTextToLines(const std::string& text,
    const std::string& fontID,
    int maxWidth) {
    std::vector<std::string> wrappedLines;

    if (m_isShutdown || maxWidth <= 0) {
        wrappedLines.push_back(text); // Return original text if invalid params
        return wrappedLines;
    }

    auto fontIt = m_fontMap.find(fontID);
    if (fontIt == m_fontMap.end()) {
        FONT_ERROR(std::format("Font '{}' not found for text wrapping", fontID));
        wrappedLines.push_back(text);
        return wrappedLines;
    }

    // First split by explicit newlines
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    // Now wrap each line if it's too wide
    for (const auto& line : lines) {
        if (line.empty()) {
            wrappedLines.push_back("");
            continue;
        }

        std::string workingLine;
        std::istringstream words(line);
        std::string word;

        while (words >> word) {
            std::string testLine = workingLine;
            if (!testLine.empty()) {
                testLine += " ";
            }
            testLine += word;
            int testWidth = 0;

            if (TTF_GetStringSize(fontIt->second.get(), testLine.c_str(), 0, &testWidth, nullptr)) {
                if (testWidth <= maxWidth) {
                    workingLine = testLine;
                } else {
                    if (!workingLine.empty()) {
                        wrappedLines.push_back(workingLine);
                        workingLine = word;
                    } else {
                        // Single word is too long, just add it
                        wrappedLines.push_back(word);
                        workingLine.clear();
                    }
                }
            } else {
                // If measurement fails, just add the word
                workingLine = testLine;
            }
        }

        if (!workingLine.empty()) {
            wrappedLines.push_back(workingLine);
        }
    }

    return wrappedLines;
}

bool FontManager::measureTextWithWrapping(const std::string& text, const std::string& fontID,
    int maxWidth, int* width, int* height) {
    if (m_isShutdown || !width || !height) {
        return false;
    }

    auto fontIt = m_fontMap.find(fontID);
    if (fontIt == m_fontMap.end()) {
        FONT_ERROR(std::format("Font '{}' not found for wrapped measurement", fontID));
        return false;
    }

    auto wrappedLines = wrapTextToLines(text, fontID, maxWidth);
    if (wrappedLines.empty()) {
        *width = 0;
        *height = 0;
        return true;
    }

    TTF_Font* font = fontIt->second.get();
    int const lineHeight = TTF_GetFontHeight(font);
    int maxLineWidth = 0;

    // Measure each wrapped line to get the actual maximum width
    for (const auto& line : wrappedLines) {
        int lineWidth = 0;
        if (!line.empty()) {
            if (TTF_GetStringSize(font, line.c_str(), 0, &lineWidth, nullptr)) {
                maxLineWidth = std::max(maxLineWidth, lineWidth);
            }
        }
    }

    *width = maxLineWidth;
    *height = lineHeight * static_cast<int>(wrappedLines.size());
    return true;
}

bool FontManager::isFontLoaded(const std::string& fontID) const {
    return m_fontMap.find(fontID) != m_fontMap.end();
}

void FontManager::clearFont(const std::string& fontID) {
    // No need to manually call TTF_CloseFont as the unique_ptr will handle it
    if (m_fontMap.erase(fontID) > 0) {
        FONT_INFO(std::format("Cleared font: {}", fontID));
    }
}

bool FontManager::reloadFontsForDisplay(const std::string& fontPath, int windowWidth, int windowHeight, float dpiScale) {
    if (m_isShutdown) {
        FONT_WARN("Cannot reload fonts - FontManager is shut down");
        return false;
    }

    FONT_INFO("Reloading fonts for display change...");

    // Clear existing fonts and caches without shutting down the manager
    m_fontMap.clear();
    destroyGPUTextObjects();
    m_fontsLoaded.store(false, std::memory_order_release);

    // Reset display tracking
    m_lastWindowWidth = 0;
    m_lastWindowHeight = 0;
    m_lastFontPath.clear();

    // Reload fonts with new dimensions and DPI scale
    return loadFontsForDisplay(fontPath, windowWidth, windowHeight, dpiScale);
}

bool FontManager::measureText(const std::string& text, const std::string& fontID, int* width, int* height) {
    if (m_isShutdown || !width || !height) {
        return false;
    }

    auto fontIt = m_fontMap.find(fontID);
    if (fontIt == m_fontMap.end()) {
        FONT_ERROR(std::format("Font '{}' not found for measurement", fontID));
        return false;
    }

    // Use TTF_GetStringSize for accurate text measurement
    return TTF_GetStringSize(fontIt->second.get(), text.c_str(), 0, width, height);
}

bool FontManager::getFontMetrics(const std::string& fontID, int* lineHeight, int* ascent, int* descent) {
    if (m_isShutdown || !lineHeight || !ascent || !descent) {
        return false;
    }

    auto fontIt = m_fontMap.find(fontID);
    if (fontIt == m_fontMap.end()) {
        FONT_ERROR(std::format("Font '{}' not found for metrics", fontID));
        return false;
    }

    TTF_Font* font = fontIt->second.get();
    *lineHeight = TTF_GetFontHeight(font);
    *ascent = TTF_GetFontAscent(font);
    *descent = TTF_GetFontDescent(font);

    return true;
}

bool FontManager::measureMultilineText(const std::string& text, const std::string& fontID,
    int maxWidth, int* width, int* height) {
    if (m_isShutdown || !width || !height) {
        return false;
    }

    auto fontIt = m_fontMap.find(fontID);
    if (fontIt == m_fontMap.end()) {
        FONT_ERROR(std::format("Font '{}' not found for multiline measurement", fontID));
        return false;
    }

    // Split text by newlines
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    if (lines.empty()) {
        *width = 0;
        *height = 0;
        return true;
    }

    TTF_Font* font = fontIt->second.get();
    int const lineHeight = TTF_GetFontHeight(font);
    int maxLineWidth = 0;

    // Measure each line
    for (const auto& line : lines) {
        int lineWidth = 0;
        if (!line.empty()) {
            if (!TTF_GetStringSize(font, line.c_str(), 0, &lineWidth, nullptr)) {
                FONT_ERROR(std::format("Failed to measure line: {}", line));
                return false;
            }
        }
        maxLineWidth = std::max(maxLineWidth, lineWidth);
    }

    // Apply max width constraint if specified
    if (maxWidth > 0 && maxLineWidth > maxWidth) {
        maxLineWidth = maxWidth;
    }

    *width = maxLineWidth;
    *height = lineHeight * static_cast<int>(lines.size());

    return true;
}

void FontManager::clean() {
    if (m_isShutdown) {
        return;
    }

    // Mark the manager as shutting down before freeing resources
    m_isShutdown = true;

    // No need to manually close fonts as the unique_ptr will handle it
    FONT_INFO(std::format("{} fonts freed", m_fontMap.size()));
    m_fontMap.clear();
    m_fontsLoaded.store(false, std::memory_order_release);
    destroyGPUTextObjects();

    // Clear display tracking
    m_lastWindowWidth = 0;
    m_lastWindowHeight = 0;
    m_lastFontPath.clear();
    FONT_INFO("FontManager resources cleaned - TTF will be cleaned by SDL_Quit()");
}

#include "gpu/GPUDevice.hpp"
#include <utility>

bool FontManager::ensureGPUTextEngine() {
    if (m_isShutdown) {
        return false;
    }

    if (mp_gpuTextEngine) {
        return true;
    }

    auto& gpuDevice = VoidLight::GPUDevice::Instance();
    if (!gpuDevice.isInitialized() || !gpuDevice.get()) {
        FONT_ERROR("Cannot create GPU text engine before GPUDevice initialization");
        return false;
    }

    mp_gpuTextEngine = TTF_CreateGPUTextEngine(gpuDevice.get());
    if (!mp_gpuTextEngine) {
        FONT_ERROR(std::format("Failed to create SDL3_ttf GPU text engine: {}",
            SDL_GetError()));
        return false;
    }

    TTF_SetGPUTextEngineWinding(mp_gpuTextEngine,
        TTF_GPU_TEXTENGINE_WINDING_CLOCKWISE);
    return true;
}

void FontManager::destroyGPUTextObjects() {
    for (auto& [key, entry] : m_gpuTextEntries) {
        if (entry.text) {
            TTF_DestroyText(entry.text);
            entry.text = nullptr;
        }
    }
    m_gpuTextEntries.clear();

    if (mp_gpuTextEngine) {
        TTF_DestroyGPUTextEngine(mp_gpuTextEngine);
        mp_gpuTextEngine = nullptr;
    }
}

bool FontManager::prepareGPUText(const std::string& key, const std::string& text,
    const std::string& fontID, int* width,
    int* height) {
    if (m_isShutdown || key.empty() || text.empty()) {
        return false;
    }

    if (!ensureGPUTextEngine()) {
        return false;
    }

    auto fontIt = m_fontMap.find(fontID);
    if (fontIt == m_fontMap.end()) {
        FONT_ERROR(std::format("Font '{}' not found for GPU text", fontID));
        return false;
    }

    auto& entry = m_gpuTextEntries[key];
    if (!entry.text) {
        entry.text = TTF_CreateText(mp_gpuTextEngine, fontIt->second.get(),
            text.c_str(), 0);
        if (!entry.text) {
            FONT_ERROR(std::format("Failed to create SDL3_ttf GPU text '{}': {}",
                key, SDL_GetError()));
            return false;
        }
    } else {
        if (entry.fontID != fontID &&
            !TTF_SetTextFont(entry.text, fontIt->second.get())) {
            FONT_ERROR(std::format("Failed to update font for GPU text '{}': {}",
                key, SDL_GetError()));
            return false;
        }
        if (entry.stringValue != text &&
            !TTF_SetTextString(entry.text, text.c_str(), 0)) {
            FONT_ERROR(std::format("Failed to update string for GPU text '{}': {}",
                key, SDL_GetError()));
            return false;
        }
    }

    entry.fontID = fontID;
    entry.stringValue = text;

    if (!TTF_GetTextSize(entry.text, &entry.width, &entry.height)) {
        FONT_ERROR(std::format("Failed to measure GPU text '{}': {}", key,
            SDL_GetError()));
        return false;
    }

    // Keep text objects at local origin; callers translate returned draw data.
    if (!TTF_SetTextPosition(entry.text, 0, 0)) {
        FONT_ERROR(std::format("Failed to reset GPU text origin for '{}': {}",
            key, SDL_GetError()));
        return false;
    }

    if (width) {
        *width = entry.width;
    }
    if (height) {
        *height = entry.height;
    }

    return true;
}

bool FontManager::setGPUTextPosition(const std::string& key, int x, int y) {
    auto it = m_gpuTextEntries.find(key);
    if (it == m_gpuTextEntries.end() || !it->second.text) {
        return false;
    }

    if (!TTF_SetTextPosition(it->second.text, x, y)) {
        FONT_ERROR(std::format("Failed to set GPU text position for '{}': {}", key,
            SDL_GetError()));
        return false;
    }

    return true;
}

TTF_GPUAtlasDrawSequence* FontManager::getGPUTextDrawData(
    const std::string& key) {
    auto it = m_gpuTextEntries.find(key);
    if (it == m_gpuTextEntries.end() || !it->second.text) {
        return nullptr;
    }

    return TTF_GetGPUTextDrawData(it->second.text);
}

void FontManager::clearGPUTextCache() {
    destroyGPUTextObjects();
    FONT_DEBUG("GPU text objects cleared");
}
