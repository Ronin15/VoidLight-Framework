/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef VOIDLIGHT_MENU_NAVIGATION_HPP
#define VOIDLIGHT_MENU_NAVIGATION_HPP

#include <cstddef>
#include <span>
#include <string_view>

namespace VoidLight {

class MenuNavigation {
public:
  // Applies the keyboard-focus highlight to navOrder[index] when keyboard nav
  // is "engaged": either a gamepad is connected, or the user has pressed a
  // mapped MenuUp/Down/Left/Right since the last reset(). Otherwise clears
  // any existing keyboard selection so mouse hover renders normally. Safe to
  // call every frame.
  static void applySelection(std::span<const std::string_view> navOrder,
                             size_t index);

  static void step(std::span<const std::string_view> navOrder, size_t &index,
                   int delta);

  // Reads MenuUp/Down/Confirm edges via the action-mapping system. Fires only
  // when the corresponding Command has a binding that was just pressed; if a
  // command is unmapped, it does nothing. Pressing MenuUp/MenuDown also
  // engages keyboard-focus highlighting until the next reset().
  static void readInputs(std::span<const std::string_view> navOrder,
                         size_t &index, bool enabled = true);

  // Returns true this frame if MenuCancel was pressed via any mapped binding.
  [[nodiscard]] static bool cancelPressed();

  // Returns true this frame if MenuLeft was pressed via any mapped binding.
  // Side effect: engages keyboard-focus highlighting.
  [[nodiscard]] static bool leftPressed();

  // Returns true this frame if MenuRight was pressed via any mapped binding.
  // Side effect: engages keyboard-focus highlighting.
  [[nodiscard]] static bool rightPressed();

  // Clears the keyboard-nav focus flag. Call from menu state enter() so each
  // menu visit starts with the highlight hidden until a nav key is pressed
  // (mouse-only users keep clean hover until they engage keyboard nav).
  static void reset();

private:
  // True once the user has pressed a menu nav action this visit.
  // Reset by reset() (called from menu state enter()).
  static inline bool s_keyboardNavUsed = false;
};

} // namespace VoidLight

#endif
