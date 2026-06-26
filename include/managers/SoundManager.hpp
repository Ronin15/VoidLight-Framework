/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef SOUND_MANAGER_HPP
#define SOUND_MANAGER_HPP

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

class SoundManager {
public:
  ~SoundManager();

  static SoundManager &Instance() {
    static SoundManager instance;
    return instance;
  }

  /**
   * @brief Initializes the SoundManager and SDL audio subsystem
   * @return true if initialization successful, false otherwise
   */
  [[nodiscard]] bool init();

  /**
   * @brief Loads a sound effect from a file or all sound effects from a
   * directory
   * @param filePath Path to sound file or directory containing supported audio
   * files
   * @param soundID Unique identifier for the sound(s). Used as prefix when
   * loading directory
   * @return true if at least one sound was loaded successfully, false otherwise
   */
  [[nodiscard]] bool loadSFX(const std::string &filePath, const std::string &soundID);

  /**
   * @brief Loads a music file for background music playback
   * @param filePath Path to music file or directory containing music files
   * @param musicID Unique identifier for the music track(s)
   * @return true if music was loaded successfully, false otherwise
   */
  [[nodiscard]] bool loadMusic(const std::string &filePath, const std::string &musicID);

  /**
   * @brief Plays a loaded sound effect
   * @param soundID Unique identifier of the sound effect to play
   * @param loops Number of additional loops to play (0 = play once, default: 0)
   * @param volume Volume level from 0.0-1.0 (default: 1.0)
   */
  void playSFX(const std::string &soundID, int loops = 0, float volume = 1.0f);

  /**
   * @brief Plays a loaded music track
   * @param musicID Unique identifier of the music track to play
   * @param loops Number of loops (-1 = infinite loop, default: -1)
   * @param volume Volume level from 0.0-1.0 (default: 1.0)
   */
  void playMusic(const std::string &musicID, int loops = -1,
                 float volume = 1.0f);

  /**
   * @brief Pauses currently playing music
   */
  void pauseMusic();

  /**
   * @brief Resumes paused music playback
   */
  void resumeMusic();

  /**
   * @brief Stops currently playing music
   */
  void stopMusic();

  /**
   * @brief Checks if music is currently playing
   * @return true if music is playing, false otherwise
   */
  bool isMusicPlaying() const;

  /**
   * @brief Sets the global music volume level
   * @param volume Volume level from 0.0-1.0
   */
  void setMusicVolume(float volume);

  /**
   * @brief Sets the global sound effects volume level
   * @param volume Volume level from 0.0-1.0
   */
  void setSFXVolume(float volume);

  /**
   * @brief Cleans up all audio resources and shuts down SDL audio subsystem
   */
  void clean();

  /**
   * @brief Removes a sound effect from memory
   * @param soundID Unique identifier of the sound effect to remove
   */
  void clearSFX(const std::string &soundID);

  /**
   * @brief Removes a music track from memory
   * @param musicID Unique identifier of the music track to remove
   */
  void clearMusic(const std::string &musicID);

  /**
   * @brief Checks if a sound effect is loaded in memory
   * @param soundID Unique identifier of the sound effect to check
   * @return true if sound effect is loaded, false otherwise
   */
  bool isSFXLoaded(const std::string &soundID) const;

  /**
   * @brief Checks if a music track is loaded in memory
   * @param musicID Unique identifier of the music track to check
   * @return true if music track is loaded, false otherwise
   */
  bool isMusicLoaded(const std::string &musicID) const;

  /**
   * @brief Gets the current music volume level
   * @return Current music volume (0.0-1.0)
   */
  float getMusicVolume() const { return m_musicVolume; }

  /**
   * @brief Gets the current sound effects volume level
   * @return Current SFX volume (0.0-1.0)
   */
  float getSFXVolume() const { return m_sfxVolume; }

  /**
   * @brief Checks if SoundManager has been shut down
   * @return true if manager is shut down, false otherwise
   */
  bool isShutdown() const { return m_isShutdown.load(); }

private:
  // Core SDL3_mixer components
  MIX_Mixer *m_mixer{nullptr};
  MIX_Group *m_sfxGroup{nullptr};
  MIX_Group *m_musicGroup{nullptr};

  // Audio storage
  std::unordered_map<std::string, MIX_Audio *> m_audioMap{};

  // Track management for active sounds
  std::unordered_map<std::string, std::vector<MIX_Track *>> m_activeSfxTracks{};
  std::vector<MIX_Track *> m_activeMusicTracks{};
  std::unordered_map<MIX_Track *, std::string>
      m_trackToAudioMap{}; // Track -> AudioID mapping

  // State management
  bool m_initialized{false};
  std::atomic<bool> m_sfxLoaded{false};
  std::atomic<bool> m_musicLoaded{false};
  std::mutex m_loadMutex{};
  std::atomic<bool> m_isShutdown{false};
  float m_musicVolume{1.0f};
  float m_sfxVolume{1.0f};

  // Internal helper methods
  [[nodiscard]] bool loadAudio(const std::string &filePath, const std::string &idPrefix);
  MIX_Track *createAndConfigureTrack(MIX_Group *group, const std::string &tag);
  void cleanupStoppedTracks();
  std::vector<std::string> getSupportedExtensions() const;

  // Delete copy constructor and assignment operator
  SoundManager(const SoundManager &) = delete;            // Prevent copying
  SoundManager &operator=(const SoundManager &) = delete; // Prevent assignment

  SoundManager();
};

#endif // SOUND_MANAGER_HPP
