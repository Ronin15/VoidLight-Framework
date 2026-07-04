/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/SoundManager.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <filesystem>
#include <format>

SoundManager::SoundManager() {
  // Member variables are already initialized in the header with brace
  // initialization
}

SoundManager::~SoundManager() {
  // Only clean up if not already shut down
  if (!m_isShutdown) {
    clean();
  }
}

bool SoundManager::init() {
  if (m_initialized) {
    SOUND_WARN("Already initialized");
    return true;
  }

  // Initialize SDL Audio if not already initialized
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
      SOUND_ERROR("Failed to initialize SDL audio subsystem");
      return false;
    }
  }

  // Initialize SDL3_mixer library
  if (!MIX_Init()) {
    SOUND_ERROR("Failed to initialize SDL3_mixer library");
    return false;
  }

  // Create SDL3_mixer instance for audio device output
  m_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (!m_mixer) {
    SOUND_ERROR("Failed to create SDL3_mixer instance");
    MIX_Quit();
    return false;
  }

  // Create separate groups for SFX and music
  m_sfxGroup = MIX_CreateGroup(m_mixer);
  if (!m_sfxGroup) {
    SOUND_ERROR("Failed to create SFX group");
    MIX_DestroyMixer(m_mixer);
    MIX_Quit();
    m_mixer = nullptr;
    return false;
  }

  m_musicGroup = MIX_CreateGroup(m_mixer);
  if (!m_musicGroup) {
    SOUND_ERROR("Failed to create music group");
    MIX_DestroyGroup(m_sfxGroup);
    MIX_DestroyMixer(m_mixer);
    MIX_Quit();
    m_sfxGroup = nullptr;
    m_mixer = nullptr;
    return false;
  }

  m_initialized = true;
  SOUND_INFO(
      "Successfully initialized SDL3_mixer with separate SFX and music groups");
  return true;
}

std::vector<std::string> SoundManager::getSupportedExtensions() const {
  return {".wav", ".mp3", ".ogg", ".flac", ".aiff", ".voc"};
}

bool SoundManager::loadAudio(const std::string &filePath, const std::string &idPrefix) {
  if (!m_initialized) {
    SOUND_ERROR("Not initialized. Call init() first.");
    return false;
  }

  namespace fs = std::filesystem;

  try {
    if (fs::is_directory(filePath)) {
      // Load all supported audio files from directory
      bool loadedAny = false;
      VOIDLIGHT_DEBUG_ONLY(int fileCount = 0;)
      const auto supportedExts = getSupportedExtensions();

      for (const auto &entry : fs::directory_iterator(filePath)) {
        if (!entry.is_regular_file())
          continue;

        const std::string extension = entry.path().extension().string();
        if (std::find(supportedExts.begin(), supportedExts.end(), extension) !=
            supportedExts.end()) {
          const std::string fileName = entry.path().stem().string();
          const std::string fullSoundID = std::format("{}_{}", idPrefix, fileName);

          MIX_Audio *audio =
              MIX_LoadAudio(m_mixer, entry.path().string().c_str(), false);
          if (!audio) {
            SOUND_WARN(std::format("Failed to load audio from {} - SDL Error: {}",
                                   entry.path().string(), SDL_GetError()));
            continue;
          }

          // Free existing audio if it exists
          auto it = m_audioMap.find(fullSoundID);
          if (it != m_audioMap.end()) {
            MIX_DestroyAudio(it->second);
          }

          m_audioMap[fullSoundID] = audio;
          VOIDLIGHT_DEBUG_ONLY(fileCount++;)
          loadedAny = true;
        }
      }

      VOIDLIGHT_DEBUG_ONLY(
      if (loadedAny) {
        SOUND_INFO(std::format("Loaded {} files from directory: {}", fileCount, filePath));
      } else {
        SOUND_WARN(std::format("No supported audio files found in directory: {}", filePath));
      }
      )

      return loadedAny;
    } else {
      // Load single file
      MIX_Audio *audio = MIX_LoadAudio(m_mixer, filePath.c_str(), false);
      if (!audio) {
        SOUND_ERROR(std::format("Failed to load audio: {} - SDL Error: {}",
                                filePath, SDL_GetError()));
        return false;
      }

      // Free existing audio if it exists
      auto it = m_audioMap.find(idPrefix);
      if (it != m_audioMap.end()) {
        MIX_DestroyAudio(it->second);
      }

      m_audioMap[idPrefix] = audio;
      SOUND_INFO(std::format("Loaded audio file: {}", idPrefix));
      return true;
    }
  } catch (const fs::filesystem_error &e) {
    SOUND_ERROR(std::format("Filesystem error loading audio: {}", filePath));
    return false;
  }
}

bool SoundManager::loadSFX(const std::string &filePath,
                           const std::string &soundID) {
  std::lock_guard<std::mutex> lock(m_loadMutex);
  return loadAudio(filePath, soundID);
}

bool SoundManager::loadMusic(const std::string &filePath,
                             const std::string &musicID) {
  std::lock_guard<std::mutex> lock(m_loadMutex);
  return loadAudio(filePath, std::format("music_{}", musicID));
}

MIX_Track *SoundManager::createAndConfigureTrack(MIX_Group *group,
                                                 const std::string &tag) {
  MIX_Track *track = MIX_CreateTrack(m_mixer);
  if (!track) {
    return nullptr;
  }

  // Set the track's group
  if (!MIX_SetTrackGroup(track, group)) {
    MIX_DestroyTrack(track);
    return nullptr;
  }

  // Tag the track for easier management
  if (!MIX_TagTrack(track, tag.c_str())) {
    MIX_DestroyTrack(track);
    return nullptr;
  }

  return track;
}

void SoundManager::cleanupStoppedTracks() {
  // Clean up stopped SFX tracks
  for (auto it = m_activeSfxTracks.begin(); it != m_activeSfxTracks.end();) {
    auto &tracks = it->second;
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                                [this](MIX_Track *track) {
                                  if (!MIX_TrackPlaying(track)) {
                                    m_trackToAudioMap.erase(track);
                                    MIX_UntagTrack(track, "sfx");
                                    MIX_DestroyTrack(track);
                                    return true;
                                  }
                                  return false;
                                }),
                 tracks.end());

    if (tracks.empty()) {
      it = m_activeSfxTracks.erase(it);
    } else {
      ++it;
    }
  }

  // Clean up stopped music tracks
  m_activeMusicTracks.erase(std::remove_if(m_activeMusicTracks.begin(),
                                           m_activeMusicTracks.end(),
                                           [this](MIX_Track *track) {
                                             if (!MIX_TrackPlaying(track)) {
                                               m_trackToAudioMap.erase(track);
                                               MIX_UntagTrack(track, "music");
                                               MIX_DestroyTrack(track);
                                               return true;
                                             }
                                             return false;
                                           }),
                            m_activeMusicTracks.end());
}

void SoundManager::playSFX(const std::string &soundID, int loops,
                           float volume) {
  if (!m_initialized) {
    SOUND_WARN("Not initialized. Call init() first.");
    return;
  }

  cleanupStoppedTracks();

  auto it = m_audioMap.find(soundID);
  if (it == m_audioMap.end()) {
    SOUND_WARN(std::format("Sound effect not found: {}", soundID));
    return;
  }

  // Create a new track for this SFX
  MIX_Track *track = createAndConfigureTrack(m_sfxGroup, "sfx");
  if (!track) {
    SOUND_ERROR(std::format("Failed to create track for SFX: {}", soundID));
    return;
  }

  // Set track audio
  if (!MIX_SetTrackAudio(track, it->second)) {
    SOUND_ERROR(std::format("Failed to set track audio for SFX: {}", soundID));
    MIX_UntagTrack(track, "sfx");
    MIX_DestroyTrack(track);
    return;
  }

  // Set track volume (combine with global SFX volume)
  float finalVolume = std::clamp(volume * m_sfxVolume, 0.0f, 10.0f);
  if (!MIX_SetTrackGain(track, finalVolume)) {
    SOUND_WARN(std::format("Failed to set track volume for SFX: {}", soundID));
  }

  // Configure playback properties
  SDL_PropertiesID props = SDL_CreateProperties();
  if (loops > 0) {
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
  }

  // Start playback
  if (!MIX_PlayTrack(track, props)) {
    SOUND_ERROR(std::format("Failed to play SFX: {}", soundID));
    MIX_UntagTrack(track, "sfx");
    MIX_DestroyTrack(track);
    SDL_DestroyProperties(props);
    return;
  }

  SDL_DestroyProperties(props);

  // Track the active sound
  m_activeSfxTracks[soundID].push_back(track);
  m_trackToAudioMap[track] = soundID;
}

void SoundManager::playMusic(const std::string &musicID, int loops,
                             float volume) {
  if (!m_initialized) {
    SOUND_WARN("Not initialized. Call init() first.");
    return;
  }

  // Silence whatever's currently playing immediately — the requested track
  // itself starts after MUSIC_START_DELAY_SEC, via update() below.
  stopActiveMusicTracks();

  m_pendingMusic.musicID = musicID;
  m_pendingMusic.loops = loops;
  m_pendingMusic.volume = volume;
  m_pendingMusic.delayRemaining = MUSIC_START_DELAY_SEC;
  m_pendingMusic.active = true;
}

void SoundManager::update(float deltaTime) {
  if (!m_pendingMusic.active) {
    return;
  }

  m_pendingMusic.delayRemaining -= deltaTime;
  if (m_pendingMusic.delayRemaining <= 0.0f) {
    // Copy out before clearing — playMusicImmediate() below does not touch
    // m_pendingMusic, but this keeps the pending state consistent either way.
    const std::string musicID = m_pendingMusic.musicID;
    const int loops = m_pendingMusic.loops;
    const float volume = m_pendingMusic.volume;
    m_pendingMusic.active = false;
    playMusicImmediate(musicID, loops, volume);
  }
}

void SoundManager::playMusicImmediate(const std::string &musicID, int loops,
                                      float volume) {
  cleanupStoppedTracks();

  const std::string fullMusicID = std::format("music_{}", musicID);
  auto it = m_audioMap.find(fullMusicID);
  if (it == m_audioMap.end()) {
    SOUND_WARN(std::format("Music track not found: {}", musicID));
    return;
  }

  // Stop any currently playing music
  stopMusic();

  // Create a new track for this music
  MIX_Track *track = createAndConfigureTrack(m_musicGroup, "music");
  if (!track) {
    SOUND_ERROR(std::format("Failed to create track for music: {}", musicID));
    return;
  }

  // Set track audio
  if (!MIX_SetTrackAudio(track, it->second)) {
    SOUND_ERROR(std::format("Failed to set track audio for music: {}", musicID));
    MIX_UntagTrack(track, "music");
    MIX_DestroyTrack(track);
    return;
  }

  // Set track volume (combine with global music volume)
  float finalVolume = std::clamp(volume * m_musicVolume, 0.0f, 10.0f);
  if (!MIX_SetTrackGain(track, finalVolume)) {
    SOUND_WARN(std::format("Failed to set track volume for music: {}", musicID));
  }

  // Configure playback properties
  SDL_PropertiesID props = SDL_CreateProperties();
  if (loops != 0) {
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
  }

  // Start playback
  if (!MIX_PlayTrack(track, props)) {
    SOUND_ERROR(std::format("Failed to play music: {}", musicID));
    MIX_UntagTrack(track, "music");
    MIX_DestroyTrack(track);
    SDL_DestroyProperties(props);
    return;
  }

  SDL_DestroyProperties(props);

  // Track the active music
  m_activeMusicTracks.push_back(track);
  m_trackToAudioMap[track] = fullMusicID;

  SOUND_INFO(std::format("Started playing music: {}", musicID));
}

void SoundManager::pauseMusic() {
  if (!m_initialized) {
    return;
  }

  cleanupStoppedTracks();

  // Pause all music tracks
  for (MIX_Track *track : m_activeMusicTracks) {
    if (MIX_TrackPlaying(track) && !MIX_TrackPaused(track)) {
      MIX_PauseTrack(track);
    }
  }
}

void SoundManager::resumeMusic() {
  if (!m_initialized) {
    return;
  }

  cleanupStoppedTracks();

  // Resume all paused music tracks
  for (MIX_Track *track : m_activeMusicTracks) {
    if (MIX_TrackPaused(track)) {
      MIX_ResumeTrack(track);
    }
  }
}

void SoundManager::stopMusic() {
  if (!m_initialized) {
    return;
  }

  // Cancel any not-yet-started delayed playMusic() request too.
  m_pendingMusic.active = false;

  stopActiveMusicTracks();
}

void SoundManager::stopActiveMusicTracks() {
  cleanupStoppedTracks();

  // Stop all music tracks
  for (MIX_Track *track : m_activeMusicTracks) {
    if (MIX_TrackPlaying(track)) {
      MIX_StopTrack(track, 0); // Stop immediately
    }
  }

  // Clean up stopped tracks
  cleanupStoppedTracks();
}

bool SoundManager::isMusicPlaying() const {
  if (!m_initialized) {
    return false;
  }

  // Cast away const for this method
  const_cast<SoundManager*>(this)->cleanupStoppedTracks();

  // Check if any music track is currently playing
  return std::any_of(m_activeMusicTracks.begin(), m_activeMusicTracks.end(),
    [](MIX_Track* track) {
      return MIX_TrackPlaying(track) && !MIX_TrackPaused(track);
    });
}

void SoundManager::setMusicVolume(float volume) {
  volume = std::clamp(volume, 0.0f, 10.0f);
  m_musicVolume = volume;

  if (m_initialized) {
    // Update volume for all active music tracks
    cleanupStoppedTracks();
    for (MIX_Track *track : m_activeMusicTracks) {
      MIX_SetTrackGain(track, volume);
    }
  }
}

void SoundManager::setSFXVolume(float volume) {
  volume = std::clamp(volume, 0.0f, 10.0f);
  m_sfxVolume = volume;

  if (m_initialized) {
    // Update volume for all active SFX tracks
    cleanupStoppedTracks();
    for (const auto &pair : m_activeSfxTracks) {
      for (MIX_Track *track : pair.second) {
        MIX_SetTrackGain(track, volume);
      }
    }
  }
}

void SoundManager::clean() {
  if (!m_initialized || m_isShutdown)
    return;

  // Stop and destroy all active tracks
  for (const auto &pair : m_activeSfxTracks) {
    for (MIX_Track *track : pair.second) {
      if (MIX_TrackPlaying(track)) {
        MIX_StopTrack(track, 0);
      }
      MIX_UntagTrack(track, "sfx");
      MIX_DestroyTrack(track);
    }
  }
  m_activeSfxTracks.clear();

  for (MIX_Track *track : m_activeMusicTracks) {
    if (MIX_TrackPlaying(track)) {
      MIX_StopTrack(track, 0);
    }
    MIX_UntagTrack(track, "music");
    MIX_DestroyTrack(track);
  }
  m_activeMusicTracks.clear();
  m_trackToAudioMap.clear();

  // Destroy groups first to ensure no references to audio
  if (m_sfxGroup) {
    MIX_DestroyGroup(m_sfxGroup);
    m_sfxGroup = nullptr;
  }
  if (m_musicGroup) {
    MIX_DestroyGroup(m_musicGroup);
    m_musicGroup = nullptr;
  }

  // Free all audio objects while mixer is still active
  for (auto &pair : m_audioMap) {
    if (pair.second) {
      MIX_DestroyAudio(pair.second);
      pair.second = nullptr;
    }
  }
  m_audioMap.clear();

  // Destroy mixer after all audio is freed
  if (m_mixer) {
    MIX_DestroyMixer(m_mixer);
    m_mixer = nullptr;
  }

  // Quit SDL3_mixer library after everything is destroyed
  MIX_Quit();

  // Quit SDL audio subsystem if we initialized it
  if (SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }

  m_initialized = false;
  m_isShutdown = true;
  SOUND_INFO("SoundManager cleaned up");
}

void SoundManager::clearSFX(const std::string &soundID) {
  // Stop any playing instances of this SFX
  auto trackIt = m_activeSfxTracks.find(soundID);
  if (trackIt != m_activeSfxTracks.end()) {
    for (MIX_Track *track : trackIt->second) {
      if (MIX_TrackPlaying(track)) {
        MIX_StopTrack(track, 0);
      }
      m_trackToAudioMap.erase(track);
      MIX_UntagTrack(track, "sfx");
      MIX_DestroyTrack(track);
    }
    m_activeSfxTracks.erase(trackIt);
  }

  // Remove the audio from memory
  auto audioIt = m_audioMap.find(soundID);
  if (audioIt != m_audioMap.end()) {
    MIX_DestroyAudio(audioIt->second);
    m_audioMap.erase(audioIt);
    SOUND_INFO(std::format("Cleared sound effect: {}", soundID));
  }
}

void SoundManager::clearMusic(const std::string &musicID) {
  const std::string fullMusicID = std::format("music_{}", musicID);

  // Stop any playing instances of this music
  auto newEnd = std::remove_if(
      m_activeMusicTracks.begin(), m_activeMusicTracks.end(),
      [this, &fullMusicID](MIX_Track *track) {
        auto it = m_trackToAudioMap.find(track);
        if (it != m_trackToAudioMap.end() && it->second == fullMusicID) {
          if (MIX_TrackPlaying(track)) {
            MIX_StopTrack(track, 0);
          }
          MIX_UntagTrack(track, "music");
          MIX_DestroyTrack(track);
          m_trackToAudioMap.erase(it);
          return true;
        }
        return false;
      });
  m_activeMusicTracks.erase(newEnd, m_activeMusicTracks.end());

  // Remove the audio from memory
  auto audioIt = m_audioMap.find(fullMusicID);
  if (audioIt != m_audioMap.end()) {
    MIX_DestroyAudio(audioIt->second);
    m_audioMap.erase(audioIt);
    SOUND_INFO(std::format("Cleared music track: {}", musicID));
  }
}

bool SoundManager::isSFXLoaded(const std::string &soundID) const {
  return m_audioMap.find(soundID) != m_audioMap.end();
}

bool SoundManager::isMusicLoaded(const std::string &musicID) const {
  return m_audioMap.find(std::format("music_{}", musicID)) != m_audioMap.end();
}