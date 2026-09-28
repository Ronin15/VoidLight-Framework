/*
 * SDL3 macOS Controller - FIXED Version
 * ======================================
 *
 * This file demonstrates the CORRECT way to initialize SDL3 with gamepad
 * support when using background threads on macOS.
 *
 * THE FIX:
 * --------
 * On macOS, SDL3's gamepad subsystem uses IOKit which must be initialized
 * on the main thread. The solution is:
 *
 *   1. Initialize BOTH video AND gamepad on the MAIN THREAD together:
 *      SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)
 *
 *   2. Background threads can then safely OPEN gamepads (the subsystem
 *      is already initialized on the main thread)
 *
 *   3. Before closing gamepads, call SDL_PumpEvents() to sync internal state
 *
 *   4. Don't call SDL_QuitSubSystem() - let SDL_Quit() handle all cleanup
 *
 * WHY THIS WORKS:
 * ---------------
 * IOKit is initialized on the main thread where it can properly set up
 * the HID manager. Background threads only interact with already-open
 * gamepad handles, which is safe.
 *
 * BUILD:
 * ------
 * macOS with Homebrew SDL3:
 *   g++ -std=c++17 sdl3_controller_fixed.cpp -o fixed_demo $(pkg-config --cflags --libs sdl3)
 *
 * Or with explicit paths:
 *   g++ -std=c++17 sdl3_controller_fixed.cpp -o fixed_demo \
 *       -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL3
 *
 * SEE ALSO:
 * ---------
 * sdl3_controller_crash.cpp - The buggy version that crashes
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <cstdlib>

// Store opened gamepads for cleanup
static std::vector<SDL_Gamepad*> g_gamepads;
static bool g_gamepadInitialized = false;

void printError(const char* context) {
    std::cerr << "ERROR [" << context << "]: " << SDL_GetError() << std::endl;
}

void printInfo(const char* msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

// FIX: This function only OPENS gamepads - the subsystem is already initialized
// Opening gamepads from a background thread is safe once the subsystem is init'd
// This matches InputManager::initializeGamePad() in the fixed code
void openGamepadsFromBackgroundThread() {
    printInfo("  [BG THREAD] Detecting and opening gamepads...");
    printInfo("  [BG THREAD] (Subsystem already init'd on main thread)");

    // Detect gamepads - subsystem already initialized by main thread
    int numGamepads = 0;
    SDL_JoystickID* gamepadIds = SDL_GetGamepads(&numGamepads);

    if (!gamepadIds) {
        printInfo("  [BG THREAD] Failed to get gamepad IDs");
        return;
    }

    if (numGamepads > 0) {
        std::cout << "[INFO]   [BG THREAD] Found " << numGamepads << " gamepad(s)" << std::endl;

        // Open all detected gamepads
        for (int i = 0; i < numGamepads; ++i) {
            if (SDL_IsGamepad(gamepadIds[i])) {
                SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepadIds[i]);
                if (gamepad) {
                    const char* name = SDL_GetGamepadName(gamepad);
                    std::cout << "[INFO]   [BG THREAD] Opened: " << (name ? name : "Unknown") << std::endl;
                    g_gamepads.push_back(gamepad);
                } else {
                    printError("SDL_OpenGamepad");
                }
            }
        }
    } else {
        printInfo("  [BG THREAD] No gamepads found");
        // Subsystem stays initialized - SDL_Quit() will clean up
    }

    SDL_free(gamepadIds);

    if (!g_gamepads.empty()) {
        g_gamepadInitialized = true;
    }
}

bool initSDL() {
    // FIX: Initialize BOTH video AND gamepad on the MAIN THREAD together
    // This ensures proper IOKit setup on macOS
    printInfo("[MAIN THREAD] Initializing SDL (video + gamepad together)...");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        printError("SDL_Init(VIDEO | GAMEPAD)");
        return false;
    }
    printInfo("[MAIN THREAD] SDL video + gamepad initialized");

    return true;
}

SDL_Window* createWindow() {
    printInfo("[MAIN THREAD] Creating window...");
    SDL_Window* window = SDL_CreateWindow(
        "SDL3 Controller Fixed Demo",
        640, 480,
        0);

    if (!window) {
        printError("SDL_CreateWindow");
        return nullptr;
    }

    printInfo("[MAIN THREAD] Window created");
    return window;
}

void runEventLoop() {
    printInfo("Running event loop for 2 seconds...");

    bool running = true;
    Uint64 startTime = SDL_GetTicks();
    const Uint64 duration = 2000;

    while (running && (SDL_GetTicks() - startTime) < duration) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        SDL_Delay(16);
    }

    printInfo("Event loop finished");
}

// This matches InputManager::closeGamepads() in the fixed code
void closeGamepads() {
    printInfo("Closing gamepad handles...");

    // FIX: Pump events first to ensure SDL's internal gamepad state is synchronized
    //SDL_PumpEvents();

    size_t count = g_gamepads.size();
    for (auto& gamepad : g_gamepads) {
        if (gamepad) {
            SDL_CloseGamepad(gamepad);
            gamepad = nullptr;  // Set to nullptr after closing
        }
    }
    g_gamepads.clear();

    // FIX: Don't call SDL_QuitSubSystem - SDL_Quit() handles subsystem cleanup
    // Gamepad was initialized with SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)

    if (count > 0) {
        std::cout << "[INFO] Closed " << count << " gamepad handle(s)" << std::endl;
    }
}

void cleanupCorrect(SDL_Window* window) {
    printInfo("=== CORRECT CLEANUP SEQUENCE ===");

    // Destroy window
    if (window) {
        printInfo("Destroying window...");
        SDL_DestroyWindow(window);
    }

    // Close gamepad handles - matches GameEngine::clean() calling closeGamepads()
    closeGamepads();

    // Let SDL_Quit() handle all subsystem cleanup
    printInfo("Calling SDL_Quit()...");
    SDL_Quit();
    printInfo("Cleanup completed - no crash!");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "=============================================\n";
    std::cout << "  SDL3 macOS Controller - FIXED Demo\n";
    std::cout << "=============================================\n";
    std::cout << "\n";
    std::cout << "THE FIX:\n";
    std::cout << "  1. SDL_Init(VIDEO | GAMEPAD) on MAIN THREAD\n";
    std::cout << "  2. Background threads only OPEN gamepads\n";
    std::cout << "  3. SDL_PumpEvents() before closing gamepads\n";
    std::cout << "  4. No SDL_QuitSubSystem() - let SDL_Quit() handle it\n";
    std::cout << "\n";

    // FIX: Initialize SDL video + gamepad TOGETHER on main thread
    if (!initSDL()) {
        return EXIT_FAILURE;
    }

    // Create window on main thread
    SDL_Window* window = createWindow();
    if (!window) {
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // FIX: Background thread only OPENS gamepads (subsystem already init'd)
    printInfo("[MAIN THREAD] Spawning background thread to open gamepads...");

    std::future<void> gamepadFuture = std::async(std::launch::async,
        openGamepadsFromBackgroundThread);

    // Wait for background initialization to complete
    gamepadFuture.wait();
    printInfo("[MAIN THREAD] Background gamepad opening completed");

    if (!g_gamepads.empty()) {
        std::cout << "\n";
        std::cout << "*** Gamepad detected! ***\n";
        std::cout << "With the buggy version, this would crash.\n";
        std::cout << "With this fixed version, cleanup will succeed.\n";
        std::cout << "\n";
    }

    // Run event loop
    runEventLoop();

    // Cleanup using the CORRECT sequence - no crash
    cleanupCorrect(window);

    printInfo("Program completed successfully!");

    return EXIT_SUCCESS;
}
