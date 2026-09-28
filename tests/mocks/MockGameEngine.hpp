/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef MOCK_GAME_ENGINE_HPP
#define MOCK_GAME_ENGINE_HPP

/**
 * @brief Mock GameEngine class for testing without full game dependencies
 */
class MockGameEngine {
public:
    static MockGameEngine& Instance() {
        static MockGameEngine instance;
        return instance;
    }

    int getWindowWidth() const { return 1024; }
    int getWindowHeight() const { return 768; }

private:
    MockGameEngine() = default;
    ~MockGameEngine() = default;
    MockGameEngine(const MockGameEngine&) = delete;
    MockGameEngine& operator=(const MockGameEngine&) = delete;
};

// Define GameEngine as an alias to MockGameEngine for testing
using GameEngine = MockGameEngine;

#endif // MOCK_GAME_ENGINE_HPP
