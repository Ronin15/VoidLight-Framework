/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/MainMenuState.hpp"
#include "gameStates/SettingsMenuState.hpp"
#include "managers/UIManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/FontManager.hpp"
#include "managers/GameStateManager.hpp"
#include "managers/TextureManager.hpp"
#include "managers/ParticleManager.hpp"
#include "managers/ParticleEffectType.hpp"
#include "managers/SoundManager.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "utils/MenuNavigation.hpp"
#include "utils/Vector2D.hpp"
#include "utils/JsonReader.hpp"
#include "utils/ResourcePath.hpp"

#include "gpu/GPURenderer.hpp"
#include "gpu/SpriteBatch.hpp"
#include "gpu/GPUVertexPool.hpp"
#include "gpu/GPUTexture.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

bool MainMenuState::enter() {
  GAMESTATE_INFO("Entering MAIN MENU State");

  VoidLight::MenuNavigation::reset();

  // Pause all game managers to reduce power draw while at menu
  GameEngine::Instance().setGlobalPause(true);

  // Re-enable only the ParticleManager: the umbrella pause above also pauses it
  // (GameEngine::setGlobalPause), which would freeze the ambient menu effects.
  // Gameplay/demo states re-apply the full pause on their own enter().
  ParticleManager::Instance().setGlobalPause(false);

  auto& ui = UIManager::Instance();
  auto& fontMgr = FontManager::Instance();

  // Wait briefly for fonts to be loaded before creating UI components.
  // Avoid unbounded waits on macOS where early resize/display events can trigger
  // a reload loop. Proceed after a short timeout and let UI relayout when fonts finish.
  constexpr int kMaxWaitMs = 1500; // 1.5s max wait
  int waitedMs = 0;
  while (!fontMgr.areFontsLoaded() && waitedMs < kMaxWaitMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    waitedMs += 1;
  }

  // Create title using auto-positioning
  ui.createTitleAtTop("mainmenu_title", "VoidLight Engine - Main Menu", 60);

  // Create menu buttons with centered positioning
  int buttonWidth = 300;
  int buttonHeight = 50;
  int buttonSpacing = 20;

  // Calculate relative offsets from center for 8 buttons
  // Total height: 8 buttons * 50px + 7 gaps * 20px = 540px
  // Center the group by starting at -270 from center
  int buttonStep = buttonHeight + buttonSpacing; // 70px between each button
  int firstButtonOffset = -270; // Top of centered button group

  // Use createCenteredButton helper for streamlined button creation
  ui.createCenteredButton("mainmenu_start_game_btn", firstButtonOffset, buttonWidth, buttonHeight, "Start Game");
  ui.createCenteredButton("mainmenu_ai_demo_btn", firstButtonOffset + buttonStep, buttonWidth, buttonHeight, "AI Demo");
  ui.createCenteredButton("mainmenu_advanced_ai_demo_btn", firstButtonOffset + 2 * buttonStep, buttonWidth, buttonHeight, "Advanced AI Demo");
  ui.createCenteredButton("mainmenu_event_demo_btn", firstButtonOffset + 3 * buttonStep, buttonWidth, buttonHeight, "Event Demo");
  ui.createCenteredButton("mainmenu_ui_example_btn", firstButtonOffset + 4 * buttonStep, buttonWidth, buttonHeight, "UI Demo");
  ui.createCenteredButton("mainmenu_overlay_demo_btn", firstButtonOffset + 5 * buttonStep, buttonWidth, buttonHeight, "Overlay Demo");
  ui.createCenteredButton("mainmenu_settings_btn", firstButtonOffset + 6 * buttonStep, buttonWidth, buttonHeight, "Settings");

  // Exit button uses danger style (manual creation + positioning)
  ui.createButtonDanger("mainmenu_exit_btn", {ui.getWidthInPixels()/2 - buttonWidth/2, ui.getHeightInPixels()/2 + firstButtonOffset + 7 * buttonStep, buttonWidth, buttonHeight}, "Exit");
  ui.setComponentPositioning("mainmenu_exit_btn", {UIPositionMode::CENTERED_BOTH, 0, firstButtonOffset + 7 * buttonStep, buttonWidth, buttonHeight});

  // Set up button callbacks - capture mp_stateManager for proper architecture
  ui.setOnClick("mainmenu_start_game_btn", [this]() {
    mp_stateManager->changeState(GameStateId::GAME_PLAY);
  });

  ui.setOnClick("mainmenu_ai_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::AI_DEMO);
  });

  ui.setOnClick("mainmenu_advanced_ai_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::ADVANCED_AI_DEMO);
  });

  ui.setOnClick("mainmenu_event_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::EVENT_DEMO);
  });

  ui.setOnClick("mainmenu_ui_example_btn", [this]() {
    mp_stateManager->changeState(GameStateId::UI_DEMO);
  });

  ui.setOnClick("mainmenu_overlay_demo_btn", [this]() {
    mp_stateManager->changeState(GameStateId::OVERLAY_DEMO);
  });

  ui.setOnClick("mainmenu_settings_btn", [this]() {
    if (auto* settingsState = dynamic_cast<SettingsMenuState*>(
            mp_stateManager->getState(GameStateId::SETTINGS_MENU).get())) {
      settingsState->setReturnState(GameStateId::MAIN_MENU);
    }
    mp_stateManager->changeState(GameStateId::SETTINGS_MENU);
  });

  ui.setOnClick("mainmenu_exit_btn", [this]() {
    openQuitDialog();
  });

  // --- Quit-confirm dialog (hidden by default) ---
  const int dialogWidth  = UIConstants::DEFAULT_DIALOG_WIDTH;
  const int dialogHeight = UIConstants::DEFAULT_DIALOG_HEIGHT;
  const int dialogX = (UIConstants::BASELINE_WIDTH  - dialogWidth)  / 2;
  const int dialogY = (UIConstants::BASELINE_HEIGHT - dialogHeight) / 2;

  ui.createCenteredDialog("mainmenu_quit_dialog_panel", dialogWidth, dialogHeight, "dark");

  ui.createLabel("mainmenu_quit_dialog_title",
                 {dialogX + 20, dialogY + 20, 360, 30},
                 "Quit Game?",
                 "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_title",
                             {UIPositionMode::CENTERED_BOTH, 0, -65, 360, 30});

  ui.createLabel("mainmenu_quit_dialog_text",
                 {dialogX + 20, dialogY + 60, 360, 40},
                 "Exit to desktop?",
                 "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_text",
                             {UIPositionMode::CENTERED_BOTH, 0, -20, 360, 40});

  ui.createButtonSuccess("mainmenu_quit_dialog_yes_btn",
                         {dialogX + 50, dialogY + 120, 100, 40},
                         "Quit",
                         "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_yes_btn",
                             {UIPositionMode::CENTERED_BOTH, 100, 40, 100, 40});
  ui.setOnClick("mainmenu_quit_dialog_yes_btn", []() {
    GameEngine::Instance().setRunning(false);
  });

  ui.createButtonWarning("mainmenu_quit_dialog_cancel_btn",
                         {dialogX + 250, dialogY + 120, 100, 40},
                         "Cancel",
                         "mainmenu_quit_dialog_panel");
  ui.setComponentPositioning("mainmenu_quit_dialog_cancel_btn",
                             {UIPositionMode::CENTERED_BOTH, -100, 40, 100, 40});
  ui.setOnClick("mainmenu_quit_dialog_cancel_btn", [this]() {
    closeQuitDialog();
  });

  // Hide the modal layer (overlay + dialog panel + its children cascade via linkToParent) until triggered.
  ui.setComponentVisible("__overlay",                  false);
  ui.setComponentVisible("mainmenu_quit_dialog_panel", false);

  m_selectedIndex = 0;
  VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);

  // Ambient background: night-meadow diorama + drifting particles + campfire
  // + looping music.
  loadDioramaTiles();

  const float screenW = static_cast<float>(ui.getWidthInPixels());
  const float screenH = static_cast<float>(ui.getHeightInPixels());

  auto& particleMgr = ParticleManager::Instance();
  const Vector2D screenCenter(screenW / 2.0f, screenH / 2.0f);
  // Firefly base definition is sparse/tiny by design; push intensity well past
  // 1.0 so they read as a real presence rather than a couple of stray specks.
  particleMgr.playIndependentEffect(ParticleEffectType::AmbientFirefly,
                                    screenCenter, 2.2f, -1.0f, "menu_ambient");
  particleMgr.playIndependentEffect(ParticleEffectType::AmbientDust,
                                    screenCenter, 0.6f, -1.0f, "menu_ambient");

  // Campfire: fixed world-space position, must be (re)started explicitly on
  // resize - see startCampfireEffects()/update().
  startCampfireEffects(screenW, screenH);

  SoundManager::Instance().playMusic("music_menu_theme");

  return true;
}

void MainMenuState::update(float deltaTime) {
  m_animTime += deltaTime;

  // Process UI input (click detection, hover states, callbacks)
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.update(0.0f);
  }

  // Fire/Smoke are fixed-position independent effects (unlike the full-screen
  // AmbientFirefly/AmbientDust, which resample the resolution every spawn on
  // their own) - restart them at the new campfire position if the window/
  // resolution changed since they were started, so the flames stay pinned to
  // the wood-pile diorama sprite (which already recomputes every frame).
  const float screenW = static_cast<float>(ui.getWidthInPixels());
  const float screenH = static_cast<float>(ui.getHeightInPixels());
  if (screenW != m_campfireScreenW || screenH != m_campfireScreenH) {
    startCampfireEffects(screenW, screenH);
  }
  if (m_quitDialogOpen) {
    VoidLight::MenuNavigation::applySelection(kQuitDialogNavOrder, m_selectedIndex);
  } else {
    VoidLight::MenuNavigation::applySelection(kNavOrder, m_selectedIndex);
  }
}

bool MainMenuState::exit() {
  GAMESTATE_INFO("Exiting MAIN MENU State");

  // NOTE: Do NOT unpause here - gameplay states will unpause on enter
  // This keeps systems paused during menu-to-menu transitions

  // Stop the ambient background before tearing down the menu.
  ParticleManager::Instance().stopIndependentEffectsByGroup("menu_ambient");
  SoundManager::Instance().stopMusic();

  // Clear the twilight composite grade so it does not bleed into other states.
  VoidLight::GPURenderer::Instance().setDayNightParams(1.0f, 1.0f, 1.0f, 0.0f);

  // Clean up UI components using simplified method
  auto& ui = UIManager::Instance();
  ui.prepareForStateTransition();

  return true;
}

void MainMenuState::handleInput() {
  const auto& inputManager = InputManager::Instance();

  if (m_quitDialogOpen) {
    VoidLight::MenuNavigation::readInputs(kQuitDialogNavOrder, m_selectedIndex);
    if (VoidLight::MenuNavigation::cancelPressed()) {
      closeQuitDialog();
    }
    return;
  }

  VoidLight::MenuNavigation::readInputs(kNavOrder, m_selectedIndex);
  if (VoidLight::MenuNavigation::cancelPressed()) {
    openQuitDialog();
  }

  // Developer debug shortcuts for demo states — Debug builds only.
  VOIDLIGHT_DEBUG_ONLY(
    if (inputManager.wasKeyPressed(SDL_SCANCODE_A)) {
        mp_stateManager->changeState(GameStateId::AI_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_E)) {
        mp_stateManager->changeState(GameStateId::EVENT_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_U)) {
        mp_stateManager->changeState(GameStateId::UI_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_O)) {
        mp_stateManager->changeState(GameStateId::OVERLAY_DEMO);
    }
    if (inputManager.wasKeyPressed(SDL_SCANCODE_S)) {
        mp_stateManager->changeState(GameStateId::SETTINGS_MENU);
    }
  )
}

void MainMenuState::openQuitDialog()
{
  m_quitDialogOpen = true;
  auto& ui = UIManager::Instance();

  // The modal overlay (z=500) dims menu buttons and title beneath it; the dialog
  // panel (z=600) and its linked children render above everything. No hide hacks needed.
  ui.setComponentVisible("__overlay",                  true);
  ui.setComponentVisible("mainmenu_quit_dialog_panel", true);

  // Default focus: Cancel (index 0 in kQuitDialogNavOrder)
  m_selectedIndex = 0;
  VoidLight::MenuNavigation::reset();
}

void MainMenuState::closeQuitDialog()
{
  m_quitDialogOpen = false;
  auto& ui = UIManager::Instance();

  ui.setComponentVisible("__overlay",                  false);
  ui.setComponentVisible("mainmenu_quit_dialog_panel", false);

  // Return focus to the Exit button (last item in main menu nav order)
  m_selectedIndex = kNavOrder.size() - 1;
  VoidLight::MenuNavigation::reset();
}

void MainMenuState::loadDioramaTiles() {
  if (m_dioramaLoaded) {
    return;
  }

  VoidLight::JsonReader atlasReader;
  if (!atlasReader.loadFromFile(
          VoidLight::ResourcePath::resolve("res/data/atlas.json"))) {
    GAMESTATE_WARN("MainMenuState: failed to load atlas.json for diorama");
    return;
  }

  const auto& atlasRoot = atlasReader.getRoot();
  if (!atlasRoot.hasKey("regions")) {
    GAMESTATE_WARN("MainMenuState: atlas.json missing 'regions' for diorama");
    return;
  }

  const auto& regions = atlasRoot["regions"].asObject();

  auto readRect = [&regions](const std::string& key, AtlasRect& out) -> bool {
    auto it = regions.find(key);
    if (it == regions.end()) {
      return false;
    }
    const auto& r = it->second;
    out.x = static_cast<float>(r["x"].asInt());
    out.y = static_cast<float>(r["y"].asInt());
    out.w = static_cast<float>(r["w"].asInt());
    out.h = static_cast<float>(r["h"].asInt());
    return true;
  };

  bool ok = true;
  ok = readRect("biome_plains", m_tiles.ground) && ok;
  ok = readRect("obstacle_water", m_tiles.water) && ok;
  ok = readRect("summer_obstacle_tree", m_tiles.tree) && ok;
  ok = readRect("rock_small", m_tiles.rock) && ok;
  ok = readRect("wood_pile", m_tiles.woodPile) && ok;
  ok = readRect("flower_yellow", m_tiles.flowerYellow) && ok;
  ok = readRect("flower_blue", m_tiles.flowerBlue) && ok;
  ok = readRect("bush_spring", m_tiles.bush) && ok;
  ok = readRect("flower_pink", m_tiles.flowerPink) && ok;
  ok = readRect("mushroom_tan", m_tiles.mushroom) && ok;
  ok = readRect("stump_obstacle_small", m_tiles.stump) && ok;

  GAMESTATE_WARN_IF(!ok, "MainMenuState: one or more diorama atlas regions missing");

  m_dioramaLoaded = true;
}

void MainMenuState::startCampfireEffects(float screenW, float screenH) {
  auto& particleMgr = ParticleManager::Instance();

  if (m_fireEffectId != 0) {
    particleMgr.stopIndependentEffect(m_fireEffectId);
  }
  if (m_smokeEffectId != 0) {
    particleMgr.stopIndependentEffect(m_smokeEffectId);
  }

  // Must match the wood-pile placement in recordDiorama (campfireCenter,
  // woodW). Fire and smoke form one combined column: flames spawn at the top
  // of the wood pile and climb roughly woodW*1.0-1.9 above center (see
  // createFireEffect gravity/life), so smoke spawns above that flame
  // envelope and climbs further still, keeping the two visually separated
  // instead of overlapping at the base.
  const Vector2D campfireCenter(screenW * 0.26f, screenH * 0.80f);
  const float woodW = screenH * 0.065f;
  const Vector2D fireOrigin = campfireCenter - Vector2D(0.0f, woodW * 0.35f);
  const Vector2D smokeOrigin = campfireCenter - Vector2D(0.0f, woodW * 1.6f);
  m_fireEffectId = particleMgr.playIndependentEffect(
      ParticleEffectType::Fire, fireOrigin, 0.9f, -1.0f, "menu_ambient");
  m_smokeEffectId = particleMgr.playIndependentEffect(
      ParticleEffectType::Smoke, smokeOrigin, 0.5f, -1.0f, "menu_ambient");

  m_campfireScreenW = screenW;
  m_campfireScreenH = screenH;
}

void MainMenuState::recordDiorama(VoidLight::GPURenderer& gpuRenderer) {
  if (!m_dioramaLoaded) {
    return;
  }

  auto atlasTex = TextureManager::Instance().getGPUTextureData("atlas");
  if (!atlasTex || !atlasTex->texture) {
    return;
  }

  const auto* sceneTexture = gpuRenderer.getSceneTexture();
  if (!sceneTexture) {
    return;
  }

  auto& vertexPool = gpuRenderer.getSpriteVertexPool();
  auto* writePtr = static_cast<VoidLight::SpriteVertex*>(vertexPool.getMappedPtr());
  if (!writePtr) {
    return;
  }

  auto& batch = gpuRenderer.getSpriteBatch();
  batch.begin(writePtr, vertexPool.getMaxVertices(),
              atlasTex->texture->get(), gpuRenderer.getNearestSampler(),
              atlasTex->width, atlasTex->height,
              static_cast<float>(sceneTexture->getHeight()));

  const float screenW = static_cast<float>(GameEngine::Instance().getWidthInPixels());
  const float screenH = static_cast<float>(GameEngine::Instance().getHeightInPixels());

  // Aspect-preserving placement helper (dst height derived from source ratio),
  // with a twilight tint applied via the sprite shader's per-vertex color.
  auto drawTile = [&batch](const AtlasRect& t, float dstX, float dstY, float dstW,
                           uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) {
    const float aspect = (t.w > 0.0f) ? t.h / t.w : 1.0f;
    batch.draw(t.x, t.y, t.w, t.h, dstX, dstY, dstW, dstW * aspect, r, g, b, 255);
  };
  auto toByte = [](float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
  };

  const float yH = screenH * 0.44f;  // horizon / river vanishing line

  // Sunset sky: horizontal bands from the top of the screen down to the
  // horizon, lerping from a cool dusk indigo to a warm sunset glow right at
  // the horizon line. The engine has no dedicated solid-color scene
  // primitive wired up yet, so this reuses the ground tile as a fill source
  // - but samples only a tiny near-uniform texel crop from it (not the full
  // detailed tile) so stretching it full-width doesn't smear visible grass
  // texture across the sky.
  constexpr float kSkyFillTexel = 4.0f;
  constexpr int kSkyBands = 32; // More bands than before for a smoother blend (less visible banding)
  for (int si = 0; si < kSkyBands; ++si) {
    const float s0 = static_cast<float>(si) / kSkyBands;
    const float s1 = static_cast<float>(si + 1) / kSkyBands;
    const float ySkyTop = yH * s0;
    const float ySkyBot = yH * s1;
    // Ease toward the horizon so the warm band concentrates near the ground.
    const float glow = s1 * s1 * s1;
    // Horizon color leans orange rather than yellow (lower green, and blue
    // fades out toward the horizon instead of holding steady) - dusk indigo
    // (60,55,95) blends into a warm sunset orange (255,130,60).
    const uint8_t r = toByte((60.0f + 195.0f * glow));
    const uint8_t g = toByte((55.0f + 75.0f * glow));
    const uint8_t b = toByte((95.0f - 35.0f * glow));
    batch.draw(m_tiles.ground.x, m_tiles.ground.y, kSkyFillTexel, kSkyFillTexel,
               0.0f, ySkyTop, screenW,
               ySkyBot - ySkyTop + 1.0f, r, g, b, 255);
  }

  // Meadow floor, tiled from the horizon down. Lightened from the original
  // night-dark tint now that the composite grade itself is less severe.
  constexpr float kTile = 48.0f;
  for (float ty = yH; ty < screenH; ty += kTile) {
    for (float tx = 0.0f; tx < screenW; tx += kTile) {
      batch.draw(m_tiles.ground.x, m_tiles.ground.y, m_tiles.ground.w,
                 m_tiles.ground.h, tx, ty, kTile, kTile, 175, 190, 155, 255);
    }
  }

  // River receding to the horizon, offset to the right so it runs behind the
  // grass instead of directly behind the (screen-centered) button column: a
  // stack of bands whose width and tile size shrink toward the vanishing
  // point (squared depth compresses perspective). A sine travelling along the
  // river's length modulates band brightness, which reads as flowing,
  // shimmering water; bands also fade with atmospheric depth.
  const float riverCx = screenW * 0.76f;
  const float nearHalf = screenW * 0.13f;
  const float farHalf = screenW * 0.007f;
  constexpr int kBands = 28;
  for (int bi = 0; bi < kBands; ++bi) {
    const float t0 = static_cast<float>(bi) / kBands;
    const float t1 = static_cast<float>(bi + 1) / kBands;
    const float yTop = yH + (screenH - yH) * t0 * t0;
    const float yBot = yH + (screenH - yH) * t1 * t1;
    const float bandH = yBot - yTop + 1.0f;
    const float depth = 0.25f * (t0 + t1) * (t0 + t1);  // 0 horizon -> 1 near
    const float halfW = farHalf + (nearHalf - farHalf) * depth;
    const float flow = 0.78f + 0.22f * std::sin(depth * 14.0f - m_animTime * 1.6f);
    const float dim = 0.55f + 0.45f * depth;
    const float bright = flow * dim;
    const uint8_t r = toByte(bright * 150.0f);
    const uint8_t g = toByte(bright * 195.0f);
    const uint8_t b = toByte(bright * 235.0f);
    const float tw = 6.0f + 34.0f * depth;
    for (float x = riverCx - halfW; x < riverCx + halfW; x += tw) {
      const float w = std::min(tw, riverCx + halfW - x);
      batch.draw(m_tiles.water.x, m_tiles.water.y, m_tiles.water.w,
                 m_tiles.water.h, x, yTop, w, bandH, r, g, b, 255);
    }
  }

  // Framing trees: two large in the foreground, two smaller (darker) near the
  // horizon for depth. Right pair pulled in from the edge to clear the river.
  const float treeW = screenH * 0.24f;
  drawTile(m_tiles.tree, screenW * 0.02f, screenH * 0.60f, treeW, 148, 160, 138);
  drawTile(m_tiles.tree, screenW * 0.90f, screenH * 0.58f, treeW, 148, 160, 138);
  const float treeMidW = screenH * 0.12f;
  drawTile(m_tiles.tree, screenW * 0.17f, yH + screenH * 0.02f, treeMidW, 110, 128, 118);
  drawTile(m_tiles.tree, screenW * 0.62f, yH + screenH * 0.02f, treeMidW, 110, 128, 118);

  // Mid-ground decorations, between the horizon and the foreground band
  // below - previously this whole strip (behind/around the button column
  // and the smaller framing trees) was empty. Kept clear of the button
  // column (centered, ~490-790px wide, y~150-630), the mid-trees, and the
  // river (whose left edge is still close to screen center up here, well
  // before it widens toward the bottom of the screen).
  const float midFlowerW = screenH * 0.045f;
  drawTile(m_tiles.bush, screenW * 0.255f, screenH * 0.50f, midFlowerW * 1.2f, 140, 155, 130);
  drawTile(m_tiles.mushroom, screenW * 0.33f, screenH * 0.58f, midFlowerW * 0.55f, 190, 180, 155);
  drawTile(m_tiles.flowerPink, screenW * 0.29f, screenH * 0.68f, midFlowerW * 0.7f, 205, 180, 190);
  drawTile(m_tiles.mushroom, screenW * 0.18f, screenH * 0.605f, midFlowerW * 0.6f, 190, 180, 155);
  drawTile(m_tiles.bush, screenW * 0.655f, screenH * 0.60f, midFlowerW * 1.1f, 140, 155, 130);
  drawTile(m_tiles.flowerYellow, screenW * 0.66f, screenH * 0.68f, midFlowerW * 0.7f, 195, 190, 155);

  // Campfire: a ring of small stones around the wood pile (the atlas has no
  // dedicated fire-pit sprite) so it reads as a proper pit rather than a bare
  // pile of logs sitting on the grass. Fire/Smoke particles (started in
  // enter(), same campfireCenter/woodW) rise from the top of the pile.
  const Vector2D campfireCenter(screenW * 0.26f, screenH * 0.80f);
  const float woodW = screenH * 0.065f;
  const float ringR = woodW * 0.85f;
  constexpr int kRingStones = 7;
  const float stoneW = woodW * 0.28f;
  for (int si = 0; si < kRingStones; ++si) {
    const float angle = (static_cast<float>(si) / kRingStones) * 6.2832f;
    // Flatten vertically so the ring reads as lying on the ground, not floating.
    const float sx = campfireCenter.getX() + ringR * std::cos(angle) - stoneW * 0.5f;
    const float sy = campfireCenter.getY() + ringR * 0.55f * std::sin(angle) - stoneW * 0.5f;
    drawTile(m_tiles.rock, sx, sy, stoneW, 150, 150, 155);
  }
  drawTile(m_tiles.woodPile, campfireCenter.getX() - woodW * 0.5f,
           campfireCenter.getY() - woodW * 0.5f, woodW, 225, 175, 120);

  // Riverbank rocks, and decorations scattered across the meadow foreground
  // (kept clear of the centered button column and the moved river - verified
  // against the river's per-band bounds computed above, not just eyeballed).
  const float rockW = screenH * 0.05f;
  drawTile(m_tiles.rock, riverCx - nearHalf - rockW, screenH * 0.86f, rockW, 150, 152, 158);
  const float flowerW = screenH * 0.045f;
  drawTile(m_tiles.bush, screenW * 0.55f, screenH * 0.84f, flowerW * 1.8f, 150, 165, 140);
  drawTile(m_tiles.bush, screenW * 0.08f, screenH * 0.87f, flowerW * 1.6f, 150, 165, 140);
  drawTile(m_tiles.bush, screenW * 0.93f, screenH * 0.90f, flowerW * 1.6f, 150, 165, 140);
  drawTile(m_tiles.flowerYellow, screenW * 0.20f, screenH * 0.90f, flowerW, 205, 200, 165);
  drawTile(m_tiles.flowerYellow, screenW * 0.12f, screenH * 0.96f, flowerW * 0.85f, 205, 200, 165);
  drawTile(m_tiles.flowerBlue, screenW * 0.90f, screenH * 0.96f, flowerW, 195, 200, 190);
  drawTile(m_tiles.flowerBlue, screenW * 0.58f, screenH * 0.97f, flowerW * 0.85f, 195, 200, 190);
  drawTile(m_tiles.rock, screenW * 0.46f, screenH * 0.97f, rockW * 0.6f, 150, 152, 158);

  // Additional foreground decorations, filling gaps left of the campfire/
  // river and in the far corners.
  drawTile(m_tiles.flowerYellow, screenW * 0.242f, screenH * 0.94f, flowerW, 205, 200, 165);
  drawTile(m_tiles.bush, screenW * 0.336f, screenH * 0.905f, flowerW * 1.5f, 150, 165, 140);
  drawTile(m_tiles.flowerBlue, screenW * 0.406f, screenH * 0.955f, flowerW * 0.85f, 195, 200, 190);
  drawTile(m_tiles.rock, screenW * 0.031f, screenH * 0.955f, rockW * 0.5f, 150, 152, 158);
  drawTile(m_tiles.flowerYellow, screenW * 0.977f, screenH * 0.885f, flowerW * 0.8f, 205, 200, 165);

  // Further decorations for variety (new atlas tiles not previously used
  // here), positions checked against the river's per-band bounds above and
  // against every other decoration's bounding box to avoid overlap.
  drawTile(m_tiles.mushroom, screenW * 0.157f, screenH * 0.885f, flowerW * 0.7f, 200, 190, 165);
  drawTile(m_tiles.stump, screenW * 0.30f, screenH * 0.965f, rockW * 0.7f, 150, 130, 110);
  drawTile(m_tiles.flowerPink, screenW * 0.365f, screenH * 0.845f, flowerW * 0.85f, 215, 190, 200);
  drawTile(m_tiles.mushroom, screenW * 0.485f, screenH * 0.955f, flowerW * 0.6f, 200, 190, 165);
  drawTile(m_tiles.mushroom, screenW * 0.985f, screenH * 0.965f, flowerW * 0.55f, 200, 190, 165);

  batch.end();
}

void MainMenuState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                       float interpolationAlpha) {
  // Twilight color grade for the scene (composite shader does
  // mix(scene, scene*tint, alpha) - see res/shaders/composite.frag.glsl).
  // Kept mild so the mood reads as dusk, not full night: the sunset sky/river
  // gradient already carries most of the atmosphere. The UI composites
  // afterwards, so buttons/text stay legible regardless.
  gpuRenderer.setDayNightParams(0.80f, 0.68f, 0.72f, 0.32f);

  // Ambient scene (behind the UI): diorama sprites + drifting particles.
  recordDiorama(gpuRenderer);
  ParticleManager::Instance().recordGPUVertices(gpuRenderer, 0.0f, 0.0f,
                                                interpolationAlpha);

  // UIManager records its vertices (buttons, panels, text) over the scene.
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.recordGPUVertices(gpuRenderer);
  }
}

void MainMenuState::renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                                    SDL_GPURenderPass* scenePass,
                                    float) {
  auto& batch = gpuRenderer.getSpriteBatch();
  if (batch.hasSprites()) {
    if (const auto* sceneTexture = gpuRenderer.getSceneTexture()) {
      float orthoMatrix[16];
      VoidLight::GPURenderer::createOrthoMatrix(
          0.0f, static_cast<float>(sceneTexture->getWidth()),
          0.0f, static_cast<float>(sceneTexture->getHeight()),
          orthoMatrix);
      gpuRenderer.pushViewProjection(scenePass, orthoMatrix);
      batch.render(scenePass, gpuRenderer.getSpriteAlphaPipeline(),
                   gpuRenderer.getSpriteVertexPool().getGPUBuffer());
    }
  }

  ParticleManager::Instance().renderGPU(gpuRenderer, scenePass);
}

void MainMenuState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                 SDL_GPURenderPass* swapchainPass) {
  // Render UIManager components to swapchain
  auto& ui = UIManager::Instance();
  if (!ui.isShutdown()) {
    ui.renderGPU(gpuRenderer, swapchainPass);
  }
}
