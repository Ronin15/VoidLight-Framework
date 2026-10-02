# SoundManager

**Code:** `include/managers/SoundManager.hpp`, `src/managers/SoundManager.cpp`

**Singleton Access:** Use `SoundManager::Instance()` to access the manager.

## Overview

The SoundManager provides a centralized audio system for the VoidLight Engine, handling both sound effects and music playback. It supports multiple audio formats with volume control, efficient resource management, and seamless integration with game states.

## Key Features

- **Multi-Format Support**: WAV, MP3, OGG, FLAC, AIFF, VOC audio files
- **Sound Effects**: Short audio clips with volume and loop control
- **Music Playback**: Background music with seamless looping
- **Volume Management**: Independent volume control for SFX and music
- **Directory Loading**: Batch load audio files from directories
- **Memory Management**: Automatic audio resource cleanup

## Quick Start

### Initialization

```cpp
#include "managers/SoundManager.hpp"

// Initialize the audio system
if (!SoundManager::Instance().init()) {
    std::cerr << "Failed to initialize SoundManager" << std::endl;
    return false;
}
```

### Basic Sound Effects

```cpp
// Load a single sound effect
bool success = SoundManager::Instance().loadSFX(
    "res/sfx/jump.wav",      // Audio file path
    "jump"                   // Sound ID for reference
);

// Play the sound effect
SoundManager::Instance().playSFX("jump");

// Play with custom volume (0.0 to 1.0)
SoundManager::Instance().playSFX("jump", 0, 0.7f); // loops=0, volume=0.7
```

### Background Music

```cpp
// Load and play background music
SoundManager::Instance().loadMusic(
    "res/music/background.ogg", // Music file path
    "bg_music"                  // Music ID
);

// Play music with looping
SoundManager::Instance().playMusic("bg_music"); // loops=-1 (infinite) by default

// Set music volume
SoundManager::Instance().setMusicVolume(0.6f);
```

## Sound Effects Management

### Single File and Directory Loading

```cpp
// Load individual sound files
SoundManager& soundMgr = SoundManager::Instance();
soundMgr.loadSFX("res/sfx/explosion.wav", "explosion");
soundMgr.loadSFX("res/sfx/pickup.ogg", "pickup");

// Load all supported audio files from a directory
soundMgr.loadSFX("res/sfx/", "sfx"); // IDs: sfx_filename
```

### Playing Sound Effects

```cpp
// Basic playback
soundMgr.playSFX("explosion");

// With custom volume and loops
soundMgr.playSFX("pickup", 2, 0.8f); // Play 3 times at 80% volume
```

### Volume Control

```cpp
// Set global SFX volume (0.0 = silent, 1.0 = full volume)
soundMgr.setSFXVolume(0.7f);

// Get current SFX volume
float currentVolume = soundMgr.getSFXVolume();
```

## Music Management

### Loading and Playing Music

```cpp
// Load background music tracks
soundMgr.loadMusic("res/music/menu.ogg", "menu_music");
soundMgr.loadMusic("res/music/gameplay.mp3", "game_music");

// Play music (loops=-1 for infinite loop)
soundMgr.playMusic("menu_music");

// Play music once (no loop)
soundMgr.playMusic("game_music", 0);
```

### Music Control

```cpp
// Pause/resume music
soundMgr.pauseMusic();
soundMgr.resumeMusic();

// Stop music
soundMgr.stopMusic();

// Check if music is playing
bool isPlaying = soundMgr.isMusicPlaying();
```

### Music Volume

```cpp
// Set music volume (independent of SFX volume)
soundMgr.setMusicVolume(0.4f);

// Get current music volume
float musicVol = soundMgr.getMusicVolume();
```

## Audio Resource Management

### Unloading and Cleanup

```cpp
// Remove a specific sound effect
soundMgr.clearSFX("pickup");

// Remove a specific music track
soundMgr.clearMusic("menu_music");

// Clean up all audio (called automatically on shutdown)
soundMgr.clean();
```

### Querying Loaded Audio

```cpp
// Check if audio is loaded
bool hasSFX = soundMgr.isSFXLoaded("jump");
bool hasMusic = soundMgr.isMusicLoaded("bg_music");
```

## API Reference

### Core Methods

```cpp
// Initialization and cleanup
[[nodiscard]] bool init();
void update(float deltaTime);   // GameEngine::update(); starts delayed (pending) music
void clean();
bool isShutdown() const;

// Sound effects
[[nodiscard]] bool loadSFX(const std::string& filePath, const std::string& soundID);
void playSFX(const std::string& soundID, int loops = 0, float volume = 1.0f);
void clearSFX(const std::string& soundID);
bool isSFXLoaded(const std::string& soundID) const;

// Music
[[nodiscard]] bool loadMusic(const std::string& filePath, const std::string& musicID);
void playMusic(const std::string& musicID, int loops = -1, float volume = 1.0f);
void pauseMusic();
void resumeMusic();
void stopMusic();
void clearMusic(const std::string& musicID);
bool isMusicLoaded(const std::string& musicID) const;
bool isMusicPlaying() const;

// Volume control
void setMusicVolume(float volume); // clamped to 0.0-10.0 (1.0 = unity gain)
float getMusicVolume() const;
void setSFXVolume(float volume);   // clamped to 0.0-10.0 (1.0 = unity gain)
float getSFXVolume() const;
```

## Best Practices

- Load all required audio at game or state startup for best performance.
- Use descriptive sound and music IDs for easy management.
- Use directory loading for batch import of SFX or music.
- Adjust SFX and music volumes independently for best player experience.
- Always check if audio is loaded before playing.
- Call `clean()` on shutdown to free resources.

## Integration with Other Systems

The SoundManager works seamlessly with:
- **[EventManager](../events/EventManager.md)**: Audio can be triggered by game events through EventManager
- **[GameEngine](../core/GameEngine.md)**: Integrated into main game loop
- **GameStates**: State-specific audio loading and management

The SoundManager provides essential audio capabilities that enhance the game experience while maintaining efficient resource usage and easy integration with game logic.
