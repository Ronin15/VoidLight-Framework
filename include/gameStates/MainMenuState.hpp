/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef MAIN_MENU_STATE_HPP
#define MAIN_MENU_STATE_HPP

#include "gameStates/GameState.hpp"

#include <array>
#include <cstddef>
#include <string_view>

class MainMenuState : public GameState {
 public:
  bool enter() override;
  void update(float deltaTime) override;
  void handleInput() override;
  bool exit() override;
  GameStateId getStateId() const override { return GameStateId::MAIN_MENU; }

  // GPU rendering support
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                         float interpolationAlpha) override;
  void renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                      SDL_GPURenderPass* scenePass,
                      float interpolationAlpha) override;
  void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                   SDL_GPURenderPass* swapchainPass) override;
  bool supportsGPURendering() const override { return true; }

 private:
  // Keyboard/gamepad navigation — ordered list of focusable buttons, matches
  // the vertical order on screen. Index wraps on MenuUp/MenuDown.
  static constexpr std::array<std::string_view, 8> kNavOrder{
      "mainmenu_start_game_btn",
      "mainmenu_ai_demo_btn",
      "mainmenu_advanced_ai_demo_btn",
      "mainmenu_event_demo_btn",
      "mainmenu_ui_example_btn",
      "mainmenu_overlay_demo_btn",
      "mainmenu_settings_btn",
      "mainmenu_exit_btn",
  };

  // Quit-confirm dialog navigation — Cancel first so it is the default focus.
  static constexpr std::array<std::string_view, 2> kQuitDialogNavOrder{
      "mainmenu_quit_dialog_cancel_btn",
      "mainmenu_quit_dialog_yes_btn",
  };

  size_t m_selectedIndex{0};
  bool m_quitDialogOpen{false};

  // Ambient night-meadow diorama (static atlas sprites behind the menu UI):
  // a river receding to the horizon, framed by trees, with a campfire. Atlas
  // source rects are parsed once from atlas.json and cached; destination
  // positions are recomputed each frame from the current viewport size.
  struct AtlasRect
  {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
  };

  struct DioramaTiles
  {
    AtlasRect ground;
    AtlasRect water;
    AtlasRect tree;
    AtlasRect rock;
    AtlasRect woodPile;
    AtlasRect flowerYellow;
    AtlasRect flowerBlue;
    AtlasRect bush;
    AtlasRect flowerPink;
    AtlasRect mushroom;
    AtlasRect stump;
  };

  DioramaTiles m_tiles{};
  bool m_dioramaLoaded{false};
  float m_animTime{0.0f};  // Drives the flowing-water shimmer.

  // Fire/Smoke are world-space, fixed-position independent effects (unlike
  // AmbientFirefly/AmbientDust, which are full-screen and re-sample the
  // current resolution every spawn) - their position is captured once when
  // started, so it must be manually refreshed on a resolution/window change
  // or the campfire drifts away from the wood-pile diorama sprite.
  uint32_t m_fireEffectId{0};
  uint32_t m_smokeEffectId{0};
  float m_campfireScreenW{0.0f};
  float m_campfireScreenH{0.0f};

  void loadDioramaTiles();
  void recordDiorama(VoidLight::GPURenderer& gpuRenderer);
  void startCampfireEffects(float screenW, float screenH);

  void openQuitDialog();
  void closeQuitDialog();
};

#endif  // MAIN_MENU_STATE_HPP
