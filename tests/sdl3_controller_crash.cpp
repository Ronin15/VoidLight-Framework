/*
 * SDL3 macOS Controller Crash Reproduction
 * =========================================
 *
 * This file demonstrates a crash that occurs on macOS when SDL3's gamepad
 * subsystem is initialized from a BACKGROUND THREAD.
 *
 * THE BUG:
 * --------
 * On macOS, SDL3's gamepad subsystem uses IOKit which must be initialized
 * on the main thread. When you:
 *   1. Initialize SDL_INIT_VIDEO on the main thread
 *   2. Initialize SDL_INIT_GAMEPAD from a BACKGROUND THREAD via SDL_InitSubSystem()
 *   3. Open gamepad handles from that background thread
 *   4. Try to close those gamepad handles during cleanup
 *
 * The crash only occurs when a gamepad is actually connected and opened.
 * The IOKit HID resources created for the gamepad are tied to the wrong
 * thread context, causing a crash when SDL_CloseGamepad() is called.
 *
 * TO REPRODUCE:
 * -------------
 * 1. Connect a gamepad to your Mac (Xbox, PS4/PS5, Switch Pro, etc.)
 * 2. Compile and run this program
 * 3. Observe the crash on cleanup
 *
 * BUILD:
 * ------
 * macOS with Homebrew SDL3:
 *   g++ -std=c++17 sdl3_controller_crash.cpp -o crash_demo $(pkg-config --cflags --libs sdl3)
 *
 * Or with explicit paths:
 *   g++ -std=c++17 sdl3_controller_crash.cpp -o crash_demo \
 *       -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL3
 *
 * SEE ALSO:
 * ---------
 * sdl3_controller_fixed.cpp - The corrected version that doesn't crash
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <cstdlib>

// Store opened gamepads for cleanup
static std::vector<SDL_Gamepad*> g_gamepads;
static bool g_gamepadSubsystemInitialized = false;

void printError(const char* context) {
    std::cerr << "ERROR [" << context << "]: " << SDL_GetError() << std::endl;
}

void printInfo(const char* msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

// BUG: This function initializes SDL_INIT_GAMEPAD from a background thread
// On macOS, this causes IOKit to be set up incorrectly
void initializeGamepadFromBackgroundThread() {
    printInfo("  [BG THREAD] Initializing gamepad subsystem...");

    // BUG: Calling SDL_InitSubSystem from a background thread
    // On macOS, IOKit requires main thread initialization
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        printError("SDL_InitSubSystem(GAMEPAD) from background thread");
        return;
    }

    printInfo("  [BG THREAD] Gamepad subsystem initialized");

    // Detect and open gamepads
    int numGamepads = 0;
    SDL_JoystickID* gamepadIds = SDL_GetGamepads(&numGamepads);

    if (!gamepadIds || numGamepads == 0) {
        printInfo("  [BG THREAD] No gamepads detected");
        // Original code: quit subsystem immediately if no gamepads found
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        printInfo("  [BG THREAD] Subsystem quit (no gamepads)");
        if (gamepadIds) {
            SDL_free(gamepadIds);
        }
        return; // g_gamepadSubsystemInitialized stays false
    }

    std::cout << "[INFO]   [BG THREAD] Found " << numGamepads << " gamepad(s)" << std::endl;

    // Open all detected gamepads
    for (int i = 0; i < numGamepads; ++i) {
        SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepadIds[i]);
        if (gamepad) {
            const char* name = SDL_GetGamepadName(gamepad);
            std::cout << "[INFO]   [BG THREAD] Opened: " << (name ? name : "Unknown") << std::endl;
            g_gamepads.push_back(gamepad);
        }
    }

    SDL_free(gamepadIds);

    // Only set this if gamepads were actually opened
    if (!g_gamepads.empty()) {
        g_gamepadSubsystemInitialized = true;
    }
}

bool initSDL() {
    // Main thread: Initialize ONLY video
    printInfo("[MAIN THREAD] Initializing SDL (video only)...");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printError("SDL_Init(VIDEO)");
        return false;
    }
    printInfo("[MAIN THREAD] SDL video initialized");

    return true;
}

SDL_Window* createWindow() {
    printInfo("[MAIN THREAD] Creating window...");
    SDL_Window* window = SDL_CreateWindow(
        "SDL3 Controller Crash Demo",
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

void cleanupBuggy(SDL_Window* window) {
    printInfo("=== CLEANUP ===");

    // Destroy window first
    if (window) {
        printInfo("Destroying window...");
        SDL_DestroyWindow(window);
    }

    // Only do gamepad cleanup if gamepads were initialized and opened
    // This matches the original code exactly
    if (g_gamepadSubsystemInitialized) {
        std::cout << "[INFO] Closing " << g_gamepads.size() << " gamepad(s)..." << std::endl;
        printInfo(">>> CRASH OCCURS HERE - closing handles opened from wrong thread <<<");
        for (SDL_Gamepad* gamepad : g_gamepads) {
            if (gamepad) {
                SDL_CloseGamepad(gamepad);
            }
        }
        g_gamepads.clear();

        printInfo("Calling SDL_QuitSubSystem(SDL_INIT_GAMEPAD)...");
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    } else {
        // No gamepads were opened - subsystem was already quit in background thread
        printInfo("No gamepads were opened - no crash expected");
    }

    printInfo("Calling SDL_Quit()...");
    SDL_Quit();
    printInfo("Cleanup completed");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "=============================================\n";
    std::cout << "  SDL3 macOS Controller Crash Demo\n";
    std::cout << "=============================================\n";
    std::cout << "\n";
    std::cout << "THE BUG: SDL_InitSubSystem(SDL_INIT_GAMEPAD) is called\n";
    std::cout << "from a BACKGROUND THREAD. On macOS, IOKit requires\n";
    std::cout << "main thread initialization.\n";
    std::cout << "\n";
    std::cout << "The crash only occurs when a gamepad is connected.\n";
    std::cout << "Without a gamepad, no crash will occur.\n";
    std::cout << "\n";

    // Initialize SDL video on main thread
    if (!initSDL()) {
        return EXIT_FAILURE;
    }

    // Create window on main thread
    SDL_Window* window = createWindow();
    if (!window) {
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // BUG: Initialize gamepad subsystem from a BACKGROUND THREAD
    // This is the problematic pattern that causes the crash
    printInfo("[MAIN THREAD] Spawning background thread for gamepad init...");

    std::future<void> gamepadFuture = std::async(std::launch::async,
        initializeGamepadFromBackgroundThread);

    // Wait for background initialization to complete
    gamepadFuture.wait();
    printInfo("[MAIN THREAD] Background gamepad init completed");

    if (g_gamepads.empty()) {
        std::cout << "\n";
        std::cout << "*** No gamepads detected ***\n";
        std::cout << "Connect a gamepad and run again to see the crash.\n";
        std::cout << "\n";
    } else {
        std::cout << "\n";
        std::cout << "*** Gamepad detected! ***\n";
        std::cout << "The program will likely crash during cleanup.\n";
        std::cout << "\n";
    }

    // Run event loop
    runEventLoop();

    // Cleanup - this will crash on macOS
    cleanupBuggy(window);

    printInfo("Program completed (no crash - was gamepad connected?)");

    return EXIT_SUCCESS;
}
