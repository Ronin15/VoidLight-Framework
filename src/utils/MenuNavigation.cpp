/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "utils/MenuNavigation.hpp"

#include "managers/InputManager.hpp"
#include "managers/UIManager.hpp"

#include <string>

namespace VoidLight {

void MenuNavigation::applySelection(std::span<const std::string_view> navOrder,
                                    size_t index) {
  if (navOrder.empty()) {
    return;
  }
  if (index >= navOrder.size()) {
    index = 0;
  }
  auto &ui = UIManager::Instance();
  const bool focusEngaged =
      InputManager::Instance().isGamepadConnected() || s_keyboardNavUsed;
  if (!focusEngaged) {
    ui.clearKeyboardSelection();
    return;
  }
  const std::string_view target = navOrder[index];
  if (ui.getKeyboardSelection() == target) {
    return;
  }
  ui.setKeyboardSelection(std::string(target));
}

void MenuNavigation::step(std::span<const std::string_view> navOrder,
                          size_t &index, int delta) {
  if (navOrder.empty()) {
    return;
  }
  const int n = static_cast<int>(navOrder.size());
  int idx = static_cast<int>(index) + delta;
  idx = ((idx % n) + n) % n;
  index = static_cast<size_t>(idx);
  applySelection(navOrder, index);
}

void MenuNavigation::readInputs(std::span<const std::string_view> navOrder,
                                size_t &index, bool enabled) {
  if (!enabled || navOrder.empty()) {
    return;
  }
  const auto &input = InputManager::Instance();
  if (input.isCommandPressed(InputManager::Command::MenuUp)) {
    s_keyboardNavUsed = true;
    step(navOrder, index, -1);
  }
  if (input.isCommandPressed(InputManager::Command::MenuDown)) {
    s_keyboardNavUsed = true;
    step(navOrder, index, +1);
  }
  if (input.isCommandPressed(InputManager::Command::MenuConfirm)) {
    if (index < navOrder.size()) {
      UIManager::Instance().simulateClick(std::string(navOrder[index]));
    }
  }
}

bool MenuNavigation::cancelPressed() {
  return InputManager::Instance().isCommandPressed(
      InputManager::Command::MenuCancel);
}

bool MenuNavigation::leftPressed() {
  const bool pressed = InputManager::Instance().isCommandPressed(
      InputManager::Command::MenuLeft);
  if (pressed) {
    s_keyboardNavUsed = true;
  }
  return pressed;
}

bool MenuNavigation::rightPressed() {
  const bool pressed = InputManager::Instance().isCommandPressed(
      InputManager::Command::MenuRight);
  if (pressed) {
    s_keyboardNavUsed = true;
  }
  return pressed;
}

void MenuNavigation::reset() { s_keyboardNavUsed = false; }

} // namespace VoidLight
