/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "core/Logger.hpp"

namespace VoidLight {

// Static member definitions - always compiled in both debug and release builds
std::atomic<bool> Logger::s_benchmarkMode{false};
std::mutex Logger::s_logMutex{};

} // namespace VoidLight

#ifndef DEBUG
// Release builds only - file-based logging implementation

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

namespace VoidLight {
namespace {

// Internal file logger implementation for release builds
class FileLogger {
public:
    static FileLogger& Instance() {
        static FileLogger instance;
        return instance;
    }

    void write(const char* level, const char* system, const char* message) {
        std::lock_guard<std::mutex> lock(m_fileMutex);

        if (!m_initialized) {
            initialize();
        }

        if (!m_fileStream.is_open()) {
            return; // Silently fail if file couldn't be opened
        }

        // Format: YYYY-MM-DD HH:MM:SS.mmm [LEVEL] [SYSTEM] message
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
            1000;

        std::tm timeinfo{};
#ifdef _WIN32
        localtime_s(&timeinfo, &time_t_now);
#else
        localtime_r(&time_t_now, &timeinfo);
#endif

        m_fileStream << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << '.'
                     << std::setfill('0') << std::setw(3) << ms.count() << " ["
                     << level << "] [" << system << "] " << message << '\n';

        ++m_messageCount;

        // Flush on CRITICAL or every 50 messages
        if (std::strcmp(level, "CRITICAL") == 0 || m_messageCount >= 50) {
            m_fileStream.flush();
            m_messageCount = 0;
        }
    }

private:
    FileLogger() = default;

    ~FileLogger() {
        if (m_fileStream.is_open()) {
            m_fileStream.flush();
            m_fileStream.close();
        }
    }

    // Non-copyable
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    void initialize() {
        m_initialized = true;

        // Get writable directory using SDL_GetPrefPath
        // VOIDLIGHT_APP_NAME is defined via CMake from ${PROJECT_NAME}
        const char* prefPath = SDL_GetPrefPath("HammerForgedGames", VOIDLIGHT_APP_NAME);
        if (prefPath == nullptr) {
            return; // Cannot get pref path, file logging disabled
        }

        namespace fs = std::filesystem;
        fs::path logDir = fs::path(prefPath) / "logs";

        // Create logs directory if it doesn't exist
        std::error_code ec;
        fs::create_directories(logDir, ec);
        if (ec) {
            return; // Cannot create directory
        }

        // Clean up old log files (keep last 5)
        cleanOldLogs(logDir, 5);

        // Generate timestamped filename
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm timeinfo{};
#ifdef _WIN32
        localtime_s(&timeinfo, &time_t_now);
#else
        localtime_r(&time_t_now, &timeinfo);
#endif

        std::ostringstream filename;
        filename << "voidlight_" << std::put_time(&timeinfo, "%Y%m%d_%H%M%S")
                 << ".log";

        fs::path logPath = logDir / filename.str();
        m_fileStream.open(logPath, std::ios::out | std::ios::app);

        if (m_fileStream.is_open()) {
            // Write header with app name
            m_fileStream << "=== " << VOIDLIGHT_APP_NAME << " Log ===\n";
            m_fileStream << "Started: "
                         << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
                         << "\n";
            m_fileStream
                << "==========================================\n\n";
            m_fileStream.flush();
        }
    }

    void cleanOldLogs(const std::filesystem::path& logDir, size_t keepCount) {
        namespace fs = std::filesystem;

        std::vector<fs::directory_entry> logFiles;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(logDir, ec)) {
            if (entry.path().extension() == ".log" &&
                entry.path().filename().string().starts_with("voidlight_")) {
                logFiles.push_back(entry);
            }
        }

        if (logFiles.size() <= keepCount) {
            return;
        }

        // Sort by modification time (oldest first)
        std::sort(logFiles.begin(), logFiles.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });

        // Remove oldest files
        size_t toRemove = logFiles.size() - keepCount;
        for (size_t i = 0; i < toRemove; ++i) {
            fs::remove(logFiles[i].path(), ec);
        }
    }

    std::mutex m_fileMutex;
    std::ofstream m_fileStream;
    bool m_initialized = false;
    size_t m_messageCount = 0;
};

} // anonymous namespace

// Logger::Log implementations for release builds - write to file instead of console
void Logger::Log(const char* level, const char* system,
    const std::string& message) {
    if (s_benchmarkMode.load(std::memory_order_relaxed)) {
        return;
    }
    FileLogger::Instance().write(level, system, message.c_str());
}

void Logger::Log(const char* level, const char* system, const char* message) {
    if (s_benchmarkMode.load(std::memory_order_relaxed)) {
        return;
    }
    FileLogger::Instance().write(level, system, message);
}

} // namespace VoidLight

#endif // ifndef DEBUG
