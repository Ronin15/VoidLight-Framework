# GameStateManager

**Code:** `include/managers/GameStateManager.hpp`, `src/managers/GameStateManager.cpp`

## Overview

`GameStateManager` owns the **active game-state stack** and transition order (push / pop / change / clear-stack). It is **state-machine infrastructure**: it lives under `managers/` for historical layout reasons, but it is not a domain manager like AI or World.

**Managers serve the states.** Domain managers do not advance the state graph. States request transitions via this API; domain managers are called by states during `enter` / `exit` (or by this class only for the shared full-screen UI clear).

See [ARCHITECTURE.md](../ARCHITECTURE.md) for the ownership table and transition model.

## Transition model

Standard **exit-then-enter** for replaces:

| API | Behavior |
|-----|----------|
| `changeState(id)` | Exit top → if stack empty, clear UI → enter new. On enter failure, re-enter previous top when possible. |
| `changeStateClearingStack(id)` | Exit entire stack → clear UI → enter new. On enter failure, best-effort restore previous stack. |
| `pushState(id)` | Pause top → enter overlay (no full UI clear). |
| `popState()` | Exit top → resume new top (no full UI clear). |

Full-screen UI clear uses `UIManager::prepareForStateTransition()` as a **service** after exits, before destination `enter()`.

States are identified by `GameStateId` (not free-form strings).

## API (current)

```cpp
void addState(std::unique_ptr<GameState> state);
void pushState(GameStateId stateId);
void popState();
void changeState(GameStateId stateId);
void changeStateClearingStack(GameStateId stateId);

void update(float deltaTime);
void handleInput();
void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer, float interpolationAlpha);
void renderGPUScene(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* scenePass, float interpolationAlpha);
void renderGPUUI(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* swapchainPass);

bool hasState(GameStateId stateId) const;
std::shared_ptr<GameState> getState(GameStateId stateId) const;
void removeState(GameStateId stateId);
void clearAllStates();
```

## Usage sketch

```cpp
GameStateManager gsm;
gsm.addState(std::make_unique<MainMenuState>());
gsm.addState(std::make_unique<GamePlayState>());
gsm.pushState(GameStateId::MAIN_MENU);

// Frame:
gsm.handleInput();
gsm.update(deltaTime);
// GPU path via GameEngine → gsm.recordGPUVertices / renderGPU*

// From a state:
mp_stateManager->changeState(GameStateId::GAME_PLAY);
// Pause over gameplay:
mp_stateManager->pushState(GameStateId::PAUSE);
// Leave gameplay stack to menu:
mp_stateManager->changeStateClearingStack(GameStateId::MAIN_MENU);
```

## Thread safety

Not thread-safe. All transitions and updates run on the main thread.

## See also

- [ARCHITECTURE.md](../ARCHITECTURE.md) — managers serve states; transition lifecycle
- [gameStates/README.md](../gameStates/README.md) — state patterns
- `GameStateId` in `include/gameStates/GameState.hpp`
