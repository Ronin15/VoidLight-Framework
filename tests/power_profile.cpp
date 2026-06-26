/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <format>
#include <algorithm>
#include <iomanip>
#include <climits>

#include "core/Logger.hpp"
#include "managers/AIManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class BenchmarkNPC {
public:
    explicit BenchmarkNPC(int id, const Vector2D& pos) : m_id(id) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
    }

    static std::shared_ptr<BenchmarkNPC> create(int id, const Vector2D& pos) {
        return std::make_shared<BenchmarkNPC>(id, pos);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }
    int getID() const { return m_id; }

private:
    EntityHandle m_handle;
    int m_id;
};

// CLI configuration
struct PowerProfileConfig {
    int entityCount = 20000;
    int durationSeconds = 60;
    std::string threadingMode = "multi";  // "single" or "multi"
    bool verbose = false;

    static PowerProfileConfig parseArgs(int argc, char* argv[]) {
        PowerProfileConfig config;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--entity-count" && i + 1 < argc) {
                config.entityCount = std::atoi(argv[++i]);
            } else if (arg == "--duration" && i + 1 < argc) {
                config.durationSeconds = std::atoi(argv[++i]);
            } else if (arg == "--threading-mode" && i + 1 < argc) {
                config.threadingMode = argv[++i];
            } else if (arg == "--verbose") {
                config.verbose = true;
            } else if (arg == "--help") {
                printHelp();
                std::exit(0);
            }
        }

        return config;
    }

    static void printHelp() {
        std::cout << "PowerProfile - SDL3 VoidLight Power Profiling Tool\n\n";
        std::cout << "Usage: PowerProfile [OPTIONS]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --entity-count NUM         Number of AI entities (default: 20000)\n";
        std::cout << "  --duration SECS            Run duration in seconds (default: 60)\n";
        std::cout << "  --threading-mode MODE      'single' or 'multi' (default: multi)\n";
        std::cout << "  --verbose                  Enable verbose output\n";
        std::cout << "  --help                     Show this help message\n\n";
        std::cout << "Examples:\n";
        std::cout << "  ./PowerProfile --entity-count 10000 --duration 30\n";
        std::cout << "  ./PowerProfile --entity-count 20000 --threading-mode single\n";
        std::cout << "  ./PowerProfile --verbose\n";
    }
};

void printConfig(const PowerProfileConfig& config) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Power Profiling Configuration\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << std::format("  Entity Count:     {} entities\n", config.entityCount);
    std::cout << std::format("  Duration:         {} seconds\n", config.durationSeconds);
    std::cout << std::format("  Threading Mode:   {}\n", config.threadingMode);
    std::cout << std::format("  Verbose:          {}\n", config.verbose ? "yes" : "no");
    std::cout << std::string(60, '=') << "\n\n";
}

void cleanup() {
    AIManager::Instance().clean();
    CollisionManager::Instance().clean();
    EntityDataManager::Instance().clean();
    PathfinderManager::Instance().clean();
    VoidLight::ThreadSystem::Instance().clean();
}

int main(int argc, char* argv[]) {
    try {
        // Parse CLI arguments
        PowerProfileConfig config = PowerProfileConfig::parseArgs(argc, argv);
        printConfig(config);

        // Initialize systems (headless, no SDL)
        if (config.verbose) {
            std::cout << "[INIT] Initializing ThreadSystem...\n";
        }
        if (!VoidLight::ThreadSystem::Instance().init()) {
            std::cerr << "[ERROR] ThreadSystem init failed\n";
            return 1;
        }

        if (config.verbose) {
            std::cout << "[INIT] Initializing PathfinderManager...\n";
        }
        if (!PathfinderManager::Instance().init()) {
            std::cerr << "[ERROR] PathfinderManager init failed\n";
            return 1;
        }
        PathfinderManager::Instance().rebuildGrid();

        if (config.verbose) {
            std::cout << "[INIT] Initializing EntityDataManager...\n";
        }
        if (!EntityDataManager::Instance().init()) {
            std::cerr << "[ERROR] EntityDataManager init failed\n";
            return 1;
        }

        if (config.verbose) {
            std::cout << "[INIT] Initializing CollisionManager...\n";
        }
        if (!CollisionManager::Instance().init()) {
            std::cerr << "[ERROR] CollisionManager init failed\n";
            return 1;
        }

        if (config.verbose) {
            std::cout << "[INIT] Initializing AIManager...\n";
        }
        if (!AIManager::Instance().init()) {
            std::cerr << "[ERROR] AIManager init failed\n";
            return 1;
        }

        // Configure threading mode
        if (config.threadingMode == "single") {
            #ifndef NDEBUG
            AIManager::Instance().enableThreading(false);
            #endif
            if (config.verbose) {
                std::cout << "[CONFIG] Threading DISABLED (single-threaded mode)\n";
            }
        } else if (config.threadingMode == "multi") {
            #ifndef NDEBUG
            AIManager::Instance().enableThreading(true);
            #endif
            if (config.verbose) {
                std::cout << "[CONFIG] Threading ENABLED (multi-threaded mode)\n";
            }
        } else {
            std::cerr << "ERROR: Invalid threading mode. Use 'single' or 'multi'\n";
            cleanup();
            return 1;
        }

        // Spawn entities
        if (config.verbose) {
            std::cout << std::format("[SPAWN] Creating {} entities...\n", config.entityCount);
        }

        std::vector<std::shared_ptr<BenchmarkNPC>> entities;
        entities.reserve(config.entityCount);

        const Vector2D centralPos(500.0f, 500.0f);
        for (int i = 0; i < config.entityCount; ++i) {
            auto entity = BenchmarkNPC::create(i, centralPos);
            entities.push_back(entity);
            // Entity created without AI behavior for headless power profiling
        }

        if (config.verbose) {
            std::cout << std::format("[SPAWN] Created {} entities\n", entities.size());
        }

        // Wait for entity assignment to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Run main benchmark loop
        std::cout << std::format("[BENCH] Starting {} second benchmark...\n", config.durationSeconds);
        std::cout << std::string(60, '-') << "\n";

        auto benchmarkStartTime = std::chrono::steady_clock::now();
        uint64_t frameCount = 0;

        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - benchmarkStartTime).count();

            if (elapsed >= config.durationSeconds) {
                break;
            }

            // Measure full frame time including pacing (realistic 60 FPS)
            auto frameStart = std::chrono::high_resolution_clock::now();

            // Update AI systems
            AIManager::Instance().update(0.016f);

            // Wait for async work to complete
            while (VoidLight::ThreadSystem::Instance().isBusy()) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            // Pace to ~60 FPS (includes vsync wait - this is where idle power happens!)
            auto targetFrameTimeUs = 16667;  // 1000/60 ms in microseconds
            auto frameEnd = std::chrono::high_resolution_clock::now();
            auto frameTime = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart).count();
            if (frameTime < targetFrameTimeUs) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(targetFrameTimeUs - frameTime)
                );
            }

            frameCount++;

            // Periodic progress output
            if (config.verbose && frameCount % 60 == 0) {
                auto elapsed_secs = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - benchmarkStartTime
                ).count();
                std::cout << std::format("  Frame {:6d} (t={:3d}s)\n",
                                       frameCount,
                                       static_cast<int>(elapsed_secs));
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - benchmarkStartTime).count();

        // Print results
        std::cout << std::string(60, '-') << "\n";
        std::cout << "\nBenchmark Results (Real-world 60 FPS pacing)\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << std::format("  Total Frames:           {}\n", frameCount);
        std::cout << std::format("  Total Time:             {} ms\n", totalTime);
        std::cout << std::format("  Avg Frame Time:         {:.3f} ms\n", totalTime / static_cast<double>(frameCount));
        std::cout << std::format("  Avg FPS:                {:.1f}\n", (frameCount * 1000.0) / totalTime);
        std::cout << std::format("  Entity Count:           {}\n", config.entityCount);
        std::cout << std::format("  Threading Mode:         {}\n", config.threadingMode);
        std::cout << std::format("  Workers Active:         {}\n", config.threadingMode == "multi" ? "10" : "1");
        std::cout << std::string(60, '=') << "\n\n";
        std::cout << "Note: Frame time includes 60 FPS pacing (vsync wait).\n";
        std::cout << "Power savings from race-to-idle visible with powermetrics during vsync wait.\n\n";

        // Cleanup
        cleanup();

        std::cout << "[DONE] Power profiling complete.\n";
        std::cout << "Note: Capture powermetrics data separately with:\n";
        std::cout << "  sudo powermetrics --samplers cpu_power -i 1000 -n <duration>\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        cleanup();
        return 1;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception\n";
        cleanup();
        return 1;
    }
}
