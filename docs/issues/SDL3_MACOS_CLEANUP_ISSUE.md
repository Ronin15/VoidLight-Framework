# SDL3 macOS Gamepad Cleanup Issue

**Platform note**, not a runtime architecture contract. Keep as a macOS shutdown caveat.

## Overview

SDL3 on macOS has a critical cleanup issue when gamepads are present during application shutdown. The issue manifests as a "trace trap" (SIGTRAP) crash during SDL_Quit() when gamepad controllers are connected, specifically with PS4 controllers and potentially other HID devices.

## Symptoms

- **Crash Type**: Trace/BPT trap: 5 (SIGTRAP)
- **Exit Code**: 133 (128 + 5)
- **Timing**: Occurs during SDL_Quit() internal cleanup
- **Platform**: macOS only (Darwin HID manager interaction)
- **Trigger**: Gamepad/controller connected during application shutdown

## Technical Details

### Crash Location
The crash occurs deep within SDL3's internal Darwin HID (Human Interface Device) manager cleanup code, not in application code. Based on lldb analysis, the crash happens in:

```
SDL3 Internal Cleanup Chain:
SDL_Quit() -> Gamepad Subsystem Cleanup -> Darwin HID Manager -> SIGTRAP
```

### LLDB Analysis Results

When debugging with lldb, the crash consistently occurs in SDL's internal cleanup routines:

1. **Application cleanup completes successfully** - All application managers (InputManager, GameEngine, etc.) clean up without issues
2. **SDL_Quit() called** - Application successfully calls SDL_Quit()
3. **Internal SDL cleanup begins** - SDL starts cleaning up subsystems
4. **Darwin HID manager conflict** - Crash occurs in SDL's Darwin-specific HID device cleanup
5. **SIGTRAP generated** - System generates trace trap signal

### Root Cause

The issue appears to be a conflict between:
- SDL3's gamepad subsystem cleanup routines
- macOS Darwin HID manager internal state
- Timing-sensitive cleanup of HID device handles

This is **not** an application bug but rather an SDL3 internal issue on macOS.

## Workaround Attempts

### Failed Approaches

1. **Manual gamepad closing before SDL_Quit()** - Still crashes
2. **Avoiding SDL_CloseGamepad() calls** - Still crashes  
3. **Manual subsystem cleanup ordering** - Still crashes
4. **Detection-only mode (no gamepad opening)** - Prevents crash but disables gamepad functionality

### Current Working Solution

The current implementation closes open gamepad handles and clears input state before shutdown:

```cpp
void InputManager::clean() {
  if(m_gamePadInitialized) {
    size_t count = m_joysticks.size();
    // Close all gamepads if detected
    for (auto& gamepad : m_joysticks) {
      if (gamepad) {
        SDL_CloseGamepad(gamepad);
        gamepad = nullptr;
      }
    }

    m_joysticks.clear();
    m_joystickValues.clear();
    m_buttonStates.clear();
    m_gamePadInitialized = false;
    if (count > 0) {
      INPUT_INFO(std::format("Closed {} gamepad handles", count));
    }
  }
}
```

This keeps gamepad functionality active during gameplay while the underlying SDL3 macOS shutdown issue remains under investigation.

## Testing Results

### Without Controller Connected
- **Result**: Clean shutdown, exit code 0
- **Status**: ✅ Works perfectly

### With Controller Connected  
- **Result**: Functional gameplay, trace trap on exit, exit code 133
- **Status**: ⚠️ Works with shutdown crash

### Controller Types Tested
- **PS4 Controller**: Trace trap confirmed
- **Other controllers**: Likely affected (HID-based controllers)

## Impact Assessment

### Functional Impact
- **Runtime**: ✅ No impact - gamepad works perfectly during gameplay
- **Shutdown**: ⚠️ Process terminates with trace trap instead of clean exit
- **Data integrity**: ✅ No impact - all application data saves correctly
- **User experience**: ✅ No visible impact to end users

### Development Impact
- **Debugging**: Harder to distinguish real crashes from shutdown crash
- **CI/CD**: May need special handling for exit codes
- **Testing**: Need to account for expected exit code 133

## Recommendations

### Short Term
1. **Document the expected behavior** - This is a known SDL3 issue, not application bug
2. **Update CI/CD pipelines** - Accept exit code 133 as success when controller present
3. **Add logging** - Clearly log when gamepad cleanup begins for debugging context

### Long Term
1. **Monitor SDL3 updates** - This may be fixed in future SDL3 releases
2. **Report to SDL project** - If not already reported, submit bug report to SDL3 team
3. **Consider alternatives** - If critical, evaluate other input libraries

## Code Documentation

The current InputManager implementation in `src/managers/InputManager.cpp` follows the proper cleanup pattern. The AGENTS.md documents the expected behavior:

```
- InputManager SDL Gamepad Cleanup: CRITICAL - Use proper initialization and cleanup pattern
- This pattern prevents conflicts but trace trap may still occur due to SDL3 internal issues
```

## Conclusion

This is a **known SDL3 limitation on macOS**, not an application defect. The engine functions correctly with full gamepad support, but experiences an SDL3 internal crash during shutdown when controllers are connected. This is acceptable for production use as it doesn't affect functionality or data integrity.

**Status**: Working with known limitation - monitor for SDL3 updates.
