# VoidLight Quality Check — Coding Standards & Quick Fixes

Coding-standard rules mirrored from `CLAUDE.md` (section 3), plus the Quick Fix Guide for the most common violations. Loaded on demand from `SKILL.md`. Detection commands and the full check catalog live in `references/checks.md`.

### 3. Coding Standards (CLAUDE.md Compliance)

#### 3.1 Naming Conventions

**Check Commands:**
```bash
# Find potential naming violations
grep -rn "class [a-z]" src/ include/              # Classes must be UpperCamelCase
grep -rn "^[A-Z][a-z]*(" src/ --include="*.cpp"   # Functions should be lowerCamelCase (src/ has no top-level .cpp; recurse)
```

**Standards:**

| Item | Convention | Example |
|------|-----------|---------|
| Classes/Enums | UpperCamelCase | `GameEngine`, `EntityType` |
| Functions/Variables | lowerCamelCase | `updateEntity()`, `deltaTime` |
| Member Variables | `m_` prefix | `m_entityCount` |
| Member Pointers | `mp_` prefix | `mp_renderer` |
| Constants | ALL_CAPS | `MAX_ENTITIES` |
| Namespaces | lowercase | `namespace utils` |

**Automated Checks:**
```bash
# Check for member variables without m_ prefix (in .cpp files)
grep -rn "^\s*[a-z][a-zA-Z0-9]*\s*;" src/ include/ | grep -v "m_" | grep -v "mp_"

# Check for class names starting with lowercase
grep -rn "^class [a-z]" include/
```

**Quality Gate:** ✓ All naming conventions followed

#### 3.2 Formatting Standards

**Standards:**
- **Indentation:** 4 spaces (no tabs)
- **Braces:** Allman style (braces on new line)
- **Line length:** Reasonable (no hard limit, but keep readable)

**Example:**
```cpp
// ✓ GOOD - Allman braces, 4-space indent
void GameEngine::update(float deltaTime)
{
    if (m_isRunning)
    {
        processEvents();
        updateSystems(deltaTime);
    }
}

// ✗ BAD - K&R braces, wrong indent
void GameEngine::update(float deltaTime) {
  if (m_isRunning) {
    processEvents();
  }
}
```

## Quick Fix Guide

**Most Common Violations:**

1. **Unused parameters:** drop the name, keep the type (CLAUDE.md — never `(void)param;` or `[[maybe_unused]]` in production, except empty virtual base defaults)
   ```cpp
   void func(int) { }
   ```

2. **Static variable in threaded code:**
   ```cpp
   // Move to class member or use thread_local
   ```

3. **Missing copyright:**
   ```cpp
   /* Copyright (c) 2025 Hammer Forged Games
    * All rights reserved.
    * Licensed under the MIT License - see LICENSE file for details
   */
   ```

4. **Using std::cout:**
   ```cpp
   LOG_INFO("message");  // instead of std::cout
   ```

5. **Raw new/delete:**
   ```cpp
   auto ptr = std::make_unique<Type>();  // instead of new
   ```

6. **Unnecessary shared_ptr copies:**
   ```cpp
   // Instead of: auto copy = m_sharedPtr;
   // Use member directly or capture raw pointer in lambdas
   auto* rawPtr = m_sharedPtr.get();
   ```

7. **shared_ptr in hot-path loops:**
   ```cpp
   // Inside batch processing loops, use raw pointers
   Entity* entity = storage.entities[i].get();
   // Keep shared_ptr in storage, use raw in tight loops
   ```

8. **String concatenation in logging:**
   ```cpp
   // Instead of: LOG_INFO("Value: " + std::to_string(x));
   LOG_INFO(std::format("Value: {}", x));
   ```

9. **Conditional logging without *_IF macro:**
   ```cpp
   // Instead of: if (debug) { AI_INFO("msg"); }
   AI_INFO_IF(debug, "msg");
   ```

10. **Per-frame allocations:**
    ```cpp
    // Instead of: void update() { std::vector<T> temp; ... }
    // Use member buffer: m_buffer.clear(); m_buffer.push_back(...);
    ```

11. **Missing reserve() for known sizes:**
    ```cpp
    std::vector<T> vec;
    vec.reserve(knownSize);  // Add before push_back loop
    ```

12. **UI component without positioning:**
    ```cpp
    ui.createButton("id", rect, "text");
    ui.setComponentPositioning("id", {UIPositionMode::CENTERED_BOTH, ...});
    ```

13. **SDL_RenderPresent in GameState:**
    ```cpp
    // NEVER call SDL_RenderPresent/Clear in GameState::render()
    // Only draw content, GameEngine handles Present
    ```

14. **Immediate state transition in enter():**
    ```cpp
    // Instead of: void enter() { pushState<Next>(); }
    // Use deferred: m_shouldTransition = true; // then transition in update()
    ```

15. **Duplicate Manager::Instance() calls:**
    ```cpp
    // Instead of calling Instance() multiple times in same function:
    void handleInput() {
        // Cache ALL managers at function start as local references
        const auto& inputMgr = InputManager::Instance();
        auto& aiMgr = AIManager::Instance();
        auto& ui = UIManager::Instance();
        // ... use cached references throughout
    }
    ```

16. **Cached mp_* member pointers (OBSOLETE):**
    ```cpp
    // OBSOLETE - Don't cache manager pointers as class members
    // mp_uiMgr = &UIManager::Instance();  // REMOVE THIS PATTERN

    // CORRECT - Use local references at function start
    auto& ui = UIManager::Instance();
    ui.createButton(...);
    ```

17. **Per-entity state in AIBehavior member variables:**
    ```cpp
    // ✗ FORBIDDEN - EntityHandle/timers/counters as behavior members
    class MyBehavior : public AIBehavior {
        EntityHandle m_target{};  // MOVE TO EDM BehaviorData
        float m_timer{0.0f};      // MOVE TO EDM BehaviorData
    };

    // ✓ CORRECT - Access via EDM BehaviorData
    void MyBehavior::executeLogic(BehaviorContext& ctx) {
        auto& state = ctx.behaviorData->state.custom;
        state.target = newTarget;  // Per-entity, thread-safe
    }
    ```

18. **Controller directly mutating AI behavior state:**
    ```cpp
    // ✗ FORBIDDEN - Direct EDM behavior state mutation from controller
    guardState.alertLevel = 3;  // LAYER VIOLATION

    // ✓ CORRECT - Send behavior message
    Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);
    ```

19. **Missing manager in state transition:**
    ```cpp
    // ✓ All managers must be transitioned in exit(), especially:
    aiMgr.prepareForStateTransition();
    bgSimMgr.prepareForStateTransition();  // Commonly missed!
    // ... rest of managers ... edm last
    ```

20. **Thread-local vector capacity destroyed by swap/return:**
    ```cpp
    // ✗ BAD - swap destroys capacity, causes per-frame allocations
    result.swap(t_deferredEvents);

    // ✓ CORRECT - ref-based with clear() preserves capacity
    void collect(std::vector<Event>& out) {
        out.insert(out.end(), make_move_iterator(...), make_move_iterator(...));
        t_deferredEvents.clear();  // Keeps capacity
    }
    ```
