/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "events/EventFactory.hpp"
#include "events/MerchantSpawnEvent.hpp"
#include "events/NPCSpawnEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "events/SceneChangeEvent.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "events/WorldEvent.hpp"
#include "events/CameraEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cctype>
#include <format>

EventFactory::EventFactory() {
    registerBuiltInEventCreators();
}

void EventFactory::registerBuiltInEventCreators() {
    registerCustomEventCreator("Weather", [this](const EventDefinition& def) {
        std::string weatherType = def.params.count("weatherType") ? def.params.at("weatherType") : "Clear";
        float intensity = def.numParams.count("intensity") ? def.numParams.at("intensity") : 0.5f;
        float transitionTime = def.numParams.count("transitionTime") ? def.numParams.at("transitionTime") : 5.0f;

        return createWeatherEvent(def.name, weatherType, intensity, transitionTime);
    });

    registerCustomEventCreator("SceneChange", [this](const EventDefinition& def) {
        std::string targetScene = def.params.count("targetScene") ? def.params.at("targetScene") : "";
        std::string transitionType = def.params.count("transitionType") ? def.params.at("transitionType") : "fade";
        float duration = def.numParams.count("duration") ? def.numParams.at("duration") : 1.0f;

        return createSceneChangeEvent(def.name, targetScene, transitionType, duration);
    });

    registerCustomEventCreator("NPCSpawn", [this](const EventDefinition& def) {
        std::string npcType = def.params.count("npcType") ? def.params.at("npcType") : "";
        int count = static_cast<int>(def.numParams.count("count") ? def.numParams.at("count") : 1.0f);
        float spawnRadius = def.numParams.count("spawnRadius") ? def.numParams.at("spawnRadius") : 0.0f;

        return createNPCSpawnEvent(def.name, npcType, count, spawnRadius);
    });

    registerCustomEventCreator("MerchantSpawn", [this](const EventDefinition& def) {
        std::string merchantClass = def.params.count("merchantClass")
                                        ? def.params.at("merchantClass")
                                        : "GeneralMerchant";
        std::string merchantRace = def.params.count("merchantRace")
                                       ? def.params.at("merchantRace")
                                       : "Human";
        int count = static_cast<int>(def.numParams.count("count")
                                         ? def.numParams.at("count")
                                         : 1.0f);
        float spawnRadius = def.numParams.count("spawnRadius")
                                ? def.numParams.at("spawnRadius")
                                : 0.0f;

        return createMerchantSpawnEvent(def.name, merchantClass, merchantRace,
                                        count, spawnRadius);
    });

    // Particle effect creator
    registerCustomEventCreator("ParticleEffect", [this](const EventDefinition& def) {
        std::string effectName = def.params.count("effectName") ? def.params.at("effectName") : "Fire";
        float x = def.numParams.count("x") ? def.numParams.at("x") : 0.0f;
        float y = def.numParams.count("y") ? def.numParams.at("y") : 0.0f;
        float intensity = def.numParams.count("intensity") ? def.numParams.at("intensity") : 1.0f;
        float duration = def.numParams.count("duration") ? def.numParams.at("duration") : -1.0f;
        std::string groupTag = def.params.count("groupTag") ? def.params.at("groupTag") : "";
        std::string sound = def.params.count("soundEffect") ? def.params.at("soundEffect") : "";
        return createParticleEffectEvent(def.name, effectName, x, y, intensity, duration, groupTag, sound);
    });

    // World event creators
    registerCustomEventCreator("WorldLoaded", [this](const EventDefinition& def) {
        std::string worldId = def.params.count("worldId") ? def.params.at("worldId") : "";
        int width = static_cast<int>(def.numParams.count("width") ? def.numParams.at("width") : 0.0f);
        int height = static_cast<int>(def.numParams.count("height") ? def.numParams.at("height") : 0.0f);
        return createWorldLoadedEvent(def.name, worldId, width, height);
    });
    registerCustomEventCreator("WorldUnloaded", [this](const EventDefinition& def) {
        std::string worldId = def.params.count("worldId") ? def.params.at("worldId") : "";
        return createWorldUnloadedEvent(def.name, worldId);
    });
    registerCustomEventCreator("TileChanged", [this](const EventDefinition& def) {
        int x = static_cast<int>(def.numParams.count("x") ? def.numParams.at("x") : 0.0f);
        int y = static_cast<int>(def.numParams.count("y") ? def.numParams.at("y") : 0.0f);
        std::string changeType = def.params.count("changeType") ? def.params.at("changeType") : "";
        return createTileChangedEvent(def.name, x, y, changeType);
    });
    registerCustomEventCreator("WorldGenerated", [this](const EventDefinition& def) {
        std::string worldId = def.params.count("worldId") ? def.params.at("worldId") : "";
        int width = static_cast<int>(def.numParams.count("width") ? def.numParams.at("width") : 0.0f);
        int height = static_cast<int>(def.numParams.count("height") ? def.numParams.at("height") : 0.0f);
        float genTime = def.numParams.count("generationTime") ? def.numParams.at("generationTime") : 0.0f;
        return createWorldGeneratedEvent(def.name, worldId, width, height, genTime);
    });

    // Camera event creators
    registerCustomEventCreator("CameraMoved", [this](const EventDefinition& def) {
        float newX = def.numParams.count("newX") ? def.numParams.at("newX") : 0.0f;
        float newY = def.numParams.count("newY") ? def.numParams.at("newY") : 0.0f;
        float oldX = def.numParams.count("oldX") ? def.numParams.at("oldX") : 0.0f;
        float oldY = def.numParams.count("oldY") ? def.numParams.at("oldY") : 0.0f;
        return createCameraMovedEvent(def.name, newX, newY, oldX, oldY);
    });
    registerCustomEventCreator("CameraModeChanged", [this](const EventDefinition& def) {
        int newMode = static_cast<int>(def.numParams.count("newMode") ? def.numParams.at("newMode") : 0.0f);
        int oldMode = static_cast<int>(def.numParams.count("oldMode") ? def.numParams.at("oldMode") : 0.0f);
        return createCameraModeChangedEvent(def.name, newMode, oldMode);
    });
    registerCustomEventCreator("CameraShake", [this](const EventDefinition& def) {
        float duration = def.numParams.count("duration") ? def.numParams.at("duration") : 0.0f;
        float intensity = def.numParams.count("intensity") ? def.numParams.at("intensity") : 0.0f;
        return createCameraShakeEvent(def.name, duration, intensity);
    });

    // Resource change (numeric handle inputs expected)
    registerCustomEventCreator("ResourceChange", [this](const EventDefinition& def) {
        uint32_t resId = static_cast<uint32_t>(def.numParams.count("resourceId") ? def.numParams.at("resourceId") : 0.0f);
        uint16_t resGen = static_cast<uint16_t>(def.numParams.count("resourceGen") ? def.numParams.at("resourceGen") : 0.0f);
        int oldQ = static_cast<int>(def.numParams.count("oldQuantity") ? def.numParams.at("oldQuantity") : 0.0f);
        int newQ = static_cast<int>(def.numParams.count("newQuantity") ? def.numParams.at("newQuantity") : 0.0f);
        std::string reason = def.params.count("reason") ? def.params.at("reason") : "";
        return createResourceChangeEvent(def.name, resId, resGen, oldQ, newQ, reason);
    });
}

bool EventFactory::init() {
    registerBuiltInEventCreators();
    EVENT_INFO("EventFactory initialized");
    return true;
}

void EventFactory::clean() {
    m_eventCreators.clear();
    registerBuiltInEventCreators();
    EVENT_INFO("EventFactory cleaned");
}

EventPtr EventFactory::createEvent(const EventDefinition& def) {
    // Check if we have a creator for this event type
    auto it = m_eventCreators.find(def.type);
    if (it != m_eventCreators.end()) {
        // Use the registered creator function
        EventPtr event = it->second(def);

        // Set common event properties if specified in the definition
        if (event) {
            // Set update frequency if specified
            if (def.numParams.count("updateFrequency")) {
                event->setUpdateFrequency(static_cast<int>(def.numParams.at("updateFrequency")));
            }

            // Set cooldown if specified
            if (def.numParams.count("cooldown")) {
                event->setCooldown(def.numParams.at("cooldown"));
            }

            // Set one-time flag if specified
            if (def.boolParams.count("oneTime")) {
                event->setOneTime(def.boolParams.at("oneTime"));
            }

            // Set active state if specified
            if (def.boolParams.count("active")) {
                event->setActive(def.boolParams.at("active"));
            }
        }

        return event;
    }

    EVENT_ERROR(std::format("Unknown event type '{}'", def.type));
    return nullptr;
}

EventPtr EventFactory::createWeatherEvent(const std::string& name, const std::string& weatherType,
                                         float intensity, float transitionTime) {
    // Create the weather event
    auto event = std::make_shared<WeatherEvent>(name, weatherType);

    // Configure the weather parameters
    WeatherParams params;
    params.intensity = intensity;
    params.transitionTime = transitionTime;

    // Additional parameter adjustments based on weather type
    if (weatherType == "Rainy" || weatherType == "Stormy") {
        params.visibility = 0.7f - (intensity * 0.4f); // Reduce visibility more with higher intensity
        params.particleEffect = (intensity > 0.7f) ? "heavy_rain" : "rain";
        params.soundEffect = (intensity > 0.7f) ? "thunder_storm" : "rain_ambient";
    } else if (weatherType == "Foggy") {
        params.visibility = 0.8f - (intensity * 0.7f); // Fog drastically reduces visibility
        params.particleEffect = "fog";
    } else if (weatherType == "Snowy") {
        params.visibility = 0.8f - (intensity * 0.3f);
        params.particleEffect = (intensity > 0.7f) ? "heavy_snow" : "snow";
        params.soundEffect = "snow_ambient";
    } else if (weatherType == "Clear") {
        params.visibility = 1.0f;
        params.intensity = 0.0f; // Override intensity for clear weather
    }

    event->setWeatherParams(params);

    return event;
}

EventPtr EventFactory::createSceneChangeEvent(const std::string& name, const std::string& targetScene,
                                            const std::string& transitionType, float duration) {
    // Create the scene change event
    auto event = std::make_shared<SceneChangeEvent>(name, targetScene);

    // Set transition type
    TransitionType type = getTransitionTypeFromString(transitionType);
    event->setTransitionType(type);

    // Configure transition parameters
    TransitionParams params;
    params.duration = duration;

    // Additional parameter adjustments based on transition type
    switch (type) {
        case TransitionType::Fade:
            params.colorR = params.colorG = params.colorB = 0.0f; // Black fade
            params.colorA = 1.0f;
            params.soundEffect = "transition_fade";
            break;
        case TransitionType::Dissolve:
            params.soundEffect = "transition_dissolve";
            break;
        case TransitionType::Wipe:
            params.direction = 0.0f; // Right to left wipe by default
            params.soundEffect = "transition_wipe";
            break;
        case TransitionType::Slide:
            params.direction = 270.0f; // Bottom to top slide by default
            params.soundEffect = "transition_slide";
            break;
        case TransitionType::Instant:
            params.duration = 0.0f;
            params.playSound = false;
            break;
        default:
            break;
    }

    event->setTransitionParams(params);

    return event;
}

EventPtr EventFactory::createNPCSpawnEvent(const std::string& name, const std::string& npcType,
                                         int count, float spawnRadius) {
    // Create spawn parameters
    SpawnParameters params;
    params.npcType = npcType;
    params.count = count;
    params.spawnRadius = spawnRadius;

    // Additional default settings
    params.fadeIn = true;
    params.fadeTime = 0.5f;
    params.playSpawnEffect = false;
    params.spawnSoundID = "";

    // Create the event with the parameters
    auto event = std::make_shared<NPCSpawnEvent>(name, params);

    // Default to a circle spawn area around origin
    event->setSpawnArea(0.0f, 0.0f, spawnRadius);

    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createMerchantSpawnEvent(const std::string& name,
                                                const std::string& merchantClass,
                                                const std::string& merchantRace,
                                                int count,
                                                float spawnRadius) {
    MerchantSpawnParameters params;
    params.merchantClass = merchantClass.empty() ? "GeneralMerchant" : merchantClass;
    params.merchantRace = merchantRace.empty() ? "Human" : merchantRace;
    params.count = count;
    params.spawnRadius = spawnRadius;

    auto event = std::make_shared<MerchantSpawnEvent>(name, params);
    event->setSpawnArea(0.0f, 0.0f, spawnRadius);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createParticleEffectEvent(const std::string& name,
                                       const std::string& effectName,
                                       float x, float y,
                                       float intensity,
                                       float duration,
                                       const std::string& groupTag,
                                       const std::string& soundEffect) {
    ParticleEffectType effectType = ParticleEffectEvent::stringToEffectType(effectName);
    auto event = std::make_shared<ParticleEffectEvent>(name, effectType, x, y, intensity, duration, groupTag, soundEffect);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createWorldLoadedEvent(const std::string&, const std::string& worldId,
                                    int width, int height) {
    auto event = std::make_shared<WorldLoadedEvent>(worldId, width, height);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createWorldUnloadedEvent(const std::string&, const std::string& worldId) {
    auto event = std::make_shared<WorldUnloadedEvent>(worldId);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createTileChangedEvent(const std::string&, int x, int y, const std::string& changeType) {
    auto event = std::make_shared<TileChangedEvent>(x, y, changeType);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createWorldGeneratedEvent(const std::string&, const std::string& worldId,
                                       int width, int height, float generationTime) {
    auto event = std::make_shared<WorldGeneratedEvent>(worldId, width, height, generationTime);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createCameraMovedEvent(const std::string&,
                                    float newX, float newY, float oldX, float oldY) {
    auto event = std::make_shared<CameraMovedEvent>(Vector2D(newX, newY), Vector2D(oldX, oldY));
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createCameraModeChangedEvent(const std::string&, int newMode, int oldMode) {
    auto event = std::make_shared<CameraModeChangedEvent>(
        static_cast<CameraModeChangedEvent::Mode>(newMode),
        static_cast<CameraModeChangedEvent::Mode>(oldMode));
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createCameraShakeEvent(const std::string&, float duration, float intensity) {
    auto event = std::make_shared<CameraShakeStartedEvent>(duration, intensity);
    return std::static_pointer_cast<Event>(event);
}

EventPtr EventFactory::createResourceChangeEvent(const std::string&,
                                       uint32_t resourceId,
                                       uint16_t resourceGen,
                                       int oldQuantity,
                                       int newQuantity,
                                       const std::string& reason) {
    VoidLight::ResourceHandle handle(resourceId, resourceGen);
    // Owner is unknown at factory-level, use invalid handle
    auto event = std::make_shared<ResourceChangeEvent>(EntityHandle{}, handle, oldQuantity, newQuantity, reason);
    return std::static_pointer_cast<Event>(event);
}

void EventFactory::registerCustomEventCreator(const std::string& eventType,
                                           std::function<EventPtr(const EventDefinition&)> creatorFunc) {
    m_eventCreators[eventType] = std::move(creatorFunc);
}

std::vector<EventPtr> EventFactory::createEventSequence(const std::string& name,
                                                     const std::vector<EventDefinition>& events,
                                                     bool) {
    std::vector<EventPtr> createdEvents;
    createdEvents.reserve(events.size());

    // Create all events in the sequence
    for (size_t i = 0; i < events.size(); ++i) {
        // Create a copy of the definition to modify
        EventDefinition def = events[i];

        // If no name is provided in the definition, generate one based on sequence
        if (def.name.empty()) {
            def.name = std::format("{}_{}", name, i + 1);
        }

        // Create the event
        EventPtr event = createEvent(def);
        if (event) {
            createdEvents.push_back(event);
        }
    }

    return createdEvents;
}


TransitionType EventFactory::getTransitionTypeFromString(const std::string& transitionType) {
    // Convert string to lowercase for case-insensitive comparison
    std::string type = transitionType;
    std::transform(type.begin(), type.end(), type.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    if (type == "fade") return TransitionType::Fade;
    if (type == "dissolve") return TransitionType::Dissolve;
    if (type == "wipe") return TransitionType::Wipe;
    if (type == "slide") return TransitionType::Slide;
    if (type == "instant") return TransitionType::Instant;

    // Default to custom type
    return TransitionType::Custom;
}
