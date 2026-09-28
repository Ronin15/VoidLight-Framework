# VoidLight Quality Check — Full Check Catalog

Detailed detection commands, examples, and quality gates for every quality-gate category. Loaded on demand from `SKILL.md`. Coding-standard rules (section 3) and the Quick Fix Guide live in `references/standards.md`.

## Detailed Checks

### 1. Compilation Quality

**Command:**
```bash
ninja -C build -v 2>&1 | grep -E "(warning|unused|error)" | head -n 100
```

**Working Directory:** `$PROJECT_ROOT/`

**Checks:**
- Count total warnings
- Categorize warning types:
  - Unused variables/parameters
  - Uninitialized members
  - Type conversion warnings
  - Shadowing warnings
  - Deprecated usage
  - Sign comparison warnings

**Quality Gate:** ✓ Zero compilation warnings required

**Common Issues:**
```cpp
// ✗ BAD
int x;  // uninitialized
void func(int unused) { }  // unused parameter

// ✓ GOOD
int x = 0;
void func(int) { }  // CLAUDE.md: drop the name, keep the type (never (void)param or [[maybe_unused]] in production)
```

### 2.1 Static Analysis (cppcheck)

**Command:**
```bash
./tests/cppcheck/cppcheck_focused.sh
```

**Or if script not available:**
```bash
cppcheck --enable=all --suppress=missingIncludeSystem \
  --std=c++20 --quiet \
  src/ include/ 2>&1
```

**Checks:**
- Memory leaks
- Null pointer dereferences
- Buffer overflows
- Use after free
- Double free
- Uninitialized variables
- Dead code / unreachable code
- Thread safety issues

**Quality Gate:** ✓ Zero critical/error severity issues

**Severity Levels:**
- **error:** Must fix (blocks commit)
- **warning:** Should fix (review required)
- **style:** Optional (improve if time permits)
- **performance:** Consider optimizing
- **information:** FYI only

### 2.2 Static Analysis (clang-tidy)

**Command:**
```bash
# clang-tidy is optional and may not be installed — gate on availability first
command -v clang-tidy >/dev/null 2>&1 && ./tests/clang-tidy/clang_tidy_focused.sh \
  || echo "clang-tidy not installed — skipping (cppcheck still covers static analysis)"
```

**Configuration Files:**
- `tests/clang-tidy/.clang-tidy` - Check configuration matching CLAUDE.md standards
- `tests/clang-tidy/clang_tidy_suppressions.txt` - False positive suppressions

**Checks Enabled:**
- `bugprone-*` - Bug-prone patterns (use-after-move, infinite loops, null dereference)
- `clang-analyzer-*` - Deep static analysis
- `cppcoreguidelines-*` - C++ Core Guidelines compliance
- `modernize-*` - Modern C++ patterns (override, nullptr, auto)
- `performance-*` - Performance issues (unnecessary copies, inefficient algorithms)
- `readability-*` - Code readability (naming, braces, const-correctness)

**Disabled Checks (intentional for game dev):**
- `modernize-use-trailing-return-type` - Personal style preference
- `readability-magic-numbers` - Games use many numeric constants
- `cppcoreguidelines-pro-bounds-pointer-arithmetic` - Required for SIMD/buffers
- `misc-include-cleaner` - Too noisy for incremental development

**Severity Levels:**
- **CRITICAL:** `bugprone-infinite-loop`, `bugprone-use-after-move`, `clang-analyzer-*`
- **HIGH:** `performance-*`, `modernize-use-override`, `bugprone-macro-*`
- **MEDIUM:** `misc-const-correctness`, `readability-make-member-function-const`
- **LOW:** `narrowing-conversions`, `readability-braces`, `readability-identifier-*`

**Quality Gate:** ✓ Zero CRITICAL issues, review HIGH issues

**Suppressions:**
The `clang_tidy_suppressions.txt` file handles false positives:
```
# Format: file_pattern:check_name:reason
AIManager.cpp:bugprone-infinite-loop:false positive - loop variable incremented in body
PathfindingGrid.cpp:bugprone-empty-catch:intentional fallback to default threshold
.cpp:misc-const-correctness:variables assigned in conditionals - clang-tidy false positive
```

**Common False Positives:**
1. **misc-const-correctness** - Variables initialized then assigned in if/switch/loops
2. **bugprone-infinite-loop** - Loops with increment inside body (not in for statement)
3. **bugprone-empty-catch** - Intentional fallback-to-default patterns
4. **narrowing-conversions** - Intentional int-to-float for grid coordinates

**Adding New Suppressions:**
Edit `tests/clang-tidy/clang_tidy_suppressions.txt`:
```
FileName.cpp:check-name:reason for suppression
```

### 3. Coding Standards (CLAUDE.md Compliance)

Naming conventions, formatting standards, and automated naming checks live in `references/standards.md`.

### 4. Threading Safety (CRITICAL)

**FORBIDDEN PATTERNS:**

#### 4.1 Static Variables in Threaded Code

**Check Command:**
```bash
# Find static variables in .cpp files (potential threading hazard)
grep -rn "static [^v].*=" src/ --include="*.cpp" | grep -v "static_cast" | grep -v "static const"
```

**Rule from CLAUDE.md:**
> **NEVER static vars in threaded code** (use instance vars, thread_local, or atomics)

**Why This is Critical:**
- VoidLight-Framework runs sequential update + parallel worker batches (main thread owns SDL/render; ThreadSystem workers process batches)
- Static variables shared across worker batches cause data races
- Non-deterministic behavior and crashes

**Example Violations:**
```cpp
// ✗ FORBIDDEN - static variable in threaded code
void AIManager::updateBehaviors()
{
    static int frameCount = 0;  // RACE CONDITION!
    frameCount++;
}

// ✓ GOOD - instance variable
class AIManager
{
    int m_frameCount = 0;  // Thread-safe with proper locking
};

// ✓ GOOD - thread_local if needed per-thread
void AIManager::updateBehaviors()
{
    thread_local int threadFrameCount = 0;
    threadFrameCount++;
}
```

**Quality Gate:** ✓ Zero static variables in threaded code (BLOCKING)

#### 4.2 Raw std::thread Usage

**Check Command:**
```bash
grep -rn "std::thread" src/ include/ | grep -v "ThreadSystem"
```

**Rule from CLAUDE.md:**
> Use ThreadSystem (not raw std::thread)

**Why:**
- ThreadSystem provides WorkerBudget priorities
- Prevents thread explosion
- Better resource management

**Quality Gate:** ✓ No raw std::thread usage

#### 4.3 Mutex Protection

**Check Command:**
```bash
# Find managers that should have mutex protection
grep -rn "class.*Manager" include/managers/
```

**For each manager, verify:**
- Has `std::mutex m_mutex;` member
- Update functions use `std::lock_guard<std::mutex> lock(m_mutex);`
- Render access uses proper locking

**Quality Gate:** ✓ All managers have proper mutex protection

### 5. Architecture Compliance

#### 5.1 GPU Frame Lifecycle Ownership

**Background:**
Rendering is GPU-based (GPURenderer/GPUSceneRecorder), not SDL_Renderer. `GameEngine::render()` handles scene+UI and `GameEngine::present()` ends the GPU frame. GameStates implement `recordGPUVertices()` / `renderGPUScene()` / `renderGPUUI()` and must NEVER end the frame, submit command buffers, or present.

**Check Command:**
```bash
# Find frame-ending / present / submit calls inside GameStates (FORBIDDEN — engine owns frame lifetime)
grep -rn "endFrame\|present(\|SubmitGPU\|AcquireGPUSwapchain" src/gameStates/ --include="*.cpp" | grep -v "//"
```

**Rule from CLAUDE.md:**
> One present per frame: `GameEngine::render()` handles scene+UI; `GameEngine::present()` ends the GPU frame. NEVER end the frame, submit command buffers, or present from a GameState.

**Quality Gate:** ✓ No frame end/present/submit from GameStates (engine owns frame lifetime)

#### 5.2 RAII & Smart Pointers

**Check Command:**
```bash
# Find raw new/delete usage (prefer smart pointers)
grep -rn "new " src/ include/ | grep -v "std::make_" | grep -v "//"
grep -rn "delete " src/ include/ | grep -v "//"
```

**Rule from CLAUDE.md:**
> RAII + smart pointers

**Prefer:**
- `std::unique_ptr` for exclusive ownership
- `std::shared_ptr` for shared ownership
- `std::make_unique` / `std::make_shared` for creation

**Quality Gate:** ✓ Minimal raw new/delete (exceptions allowed for SDL resources)

#### 5.3 Smart Pointer Performance (CRITICAL for Hot Paths)

**Background:**
Commit a8aa267e fixed severe performance issues from unnecessary shared_ptr usage in batch processing. Shared_ptr copies trigger atomic ref-counting operations, causing 100ms+ frame spikes.

**Check Commands:**
```bash
# Find potential unnecessary shared_ptr copies in batch/update functions
grep -rn "auto.*=.*shared_ptr" src/ | grep -v ".get()" | grep -v "make_shared"

# Find lambdas capturing shared_ptr (atomic overhead in threads)
grep -rn "\[.*shared_ptr\|EntityPtr\|BehaviorPtr" src/ | grep -v ".get()"

# Find shared_ptr usage in hot-path loops (processBatch, update loops)
grep -rn "for.*EntityPtr\|for.*shared_ptr<" src/
```

**FORBIDDEN PATTERNS:**

**Pattern 1: Unnecessary shared_ptr Copies**
```cpp
// ✗ BAD - Copies shared_ptr, increments ref count unnecessarily
void update() {
    auto batchData = m_sharedBatchData;  // UNNECESSARY COPY
    for (auto& batch : *batchData) {
        // ...
    }
}

// ✓ GOOD - Use member directly
void update() {
    for (auto& batch : *m_sharedBatchData) {  // No copy
        // ...
    }
}
```

**Pattern 2: Capturing shared_ptr in Lambdas**
```cpp
// ✗ BAD - Captures shared_ptr, atomic ref-count ops in every thread
auto data = m_sharedData;
m_futures.push_back(threadSystem.enqueue([data, this]() {
    processData(*data);  // Atomic increment/decrement
}));

// ✓ GOOD - Capture raw pointer, parent keeps ownership
auto* dataPtr = m_sharedData.get();
m_futures.push_back(threadSystem.enqueue([dataPtr, this]() {
    processData(*dataPtr);  // No atomic ops
}));
```

**Pattern 3: shared_ptr in Hot-Path Loops**
```cpp
// ✗ BAD - shared_ptr in tight loop, atomic ops per iteration
for (size_t i = start; i < end; ++i) {
    EntityPtr entity = storage.entities[i];  // Atomic increment
    auto behavior = storage.behaviors[i];    // Atomic increment
    behavior->update(entity, deltaTime);     // More atomic ops
}  // Atomic decrements x 2 per iteration

// ✓ GOOD - Raw pointers in loop, shared_ptr only when needed
for (size_t i = start; i < end; ++i) {
    Entity* entity = storage.entities[i].get();        // No atomic ops
    AIBehavior* behavior = storage.behaviors[i].get(); // No atomic ops

    // Only use shared_ptr for interface requiring ownership
    if (needsSharedOwnership) {
        behavior->executeLogic(storage.entities[i], deltaTime);
    } else {
        behavior->update(entity, deltaTime);  // Raw pointer version
    }
}
```

**When to Use Raw Pointers:**
- ✓ Inside batch processing loops (parent shared_ptr keeps ownership)
- ✓ Lambda captures for thread tasks (task lifetime < parent lifetime)
- ✓ Local function scope when owner exists in caller
- ✓ When shared_lock/mutex guarantees object stability

**When to Keep shared_ptr:**
- ✓ Long-term storage (member variables, containers)
- ✓ Crossing thread boundaries with uncertain lifetimes
- ✓ Interfaces requiring shared ownership semantics
- ✓ Return values transferring ownership

**Performance Impact:**
- Unnecessary shared_ptr copies: 100ms+ frame spikes
- Lambda captures: 2-5x slowdown in parallel tasks
- Hot-path loops: 3-4x slowdown on 10K+ entities

**Quality Gate:** ✓ No unnecessary shared_ptr copies in hot paths (BLOCKING for perf-critical code)

**Reference:** See commit a8aa267e for detailed fix example in AIManager::processBatch()

#### 5.4 String Parameter Regression (CRITICAL)

**Background:**
A refactoring attempt changed `const std::string&` parameters to `std::string_view` for "modernization", but then converted back to `std::string` for map lookups. This introduces allocations where there were none - a severe performance regression.

**Check Commands:**
```bash
# Find string_view parameters that convert to std::string for lookups
grep -rn "std::string \w\+Str\(" src/ --include="*.cpp"

# Find string_view parameters in headers doing map operations
grep -rn "string_view.*find\|string_view.*\[" src/ include/
```

**FORBIDDEN PATTERN:**
```cpp
// ✗ REGRESSION - Allocates on EVERY call
bool hasEvent(std::string_view name) const {
    std::string nameStr(name);  // ALLOCATION!
    return m_map.find(nameStr) != m_map.end();
}

// ✓ CORRECT - Zero-copy when caller passes std::string
bool hasEvent(const std::string& name) const {
    return m_map.find(name) != m_map.end();  // No allocation
}
```

**When string_view is SAFE:**
- **Return types** returning string literals: `std::string_view getName() { return "literal"; }`
- **Literal comparisons only**: `if (type == "Weather")` (no map lookup)
- **constexpr constants**: `constexpr std::string_view NAME = "value";`

**When to use const std::string&:**
- Map lookups (`.find()`, `[]` operator)
- Storing to member variables
- Filesystem APIs (std::ofstream, std::filesystem)
- Any function where caller typically has a `std::string`

**Why This Matters:**
- Each `std::string(view)` conversion allocates heap memory
- Hot-path functions (lookups) called thousands of times per frame
- Frame rate impact: 5-15% degradation on string-heavy systems

**Quality Gate:** ✓ No string_view→string conversions for map lookups (BLOCKING)

#### 5.5 Logger Usage

**Check Commands:**
```bash
# Find std::cout usage (should use Logger instead)
grep -rn "std::cout" src/ | grep -v "//"
grep -rn "std::cerr" src/ | grep -v "//"
grep -rn "printf" src/ | grep -v "//"

# Find string concatenation in logging (should use std::format)
grep -rn 'LOG_.*".*" +' src/ | grep -v "//"
grep -rn 'LOG_.*+ "' src/ | grep -v "//"

# Find inefficient conditional logging (should use *_IF macros)
grep -rn "if.*{.*LOG_\|if.*{.*AI_" src/ --include="*.cpp" | grep -v "_IF("

# Find raw #ifdef DEBUG blocks (should use VOIDLIGHT_DEBUG_ONLY(...))
grep -rn "#ifdef DEBUG\|#if defined(DEBUG)" src/ include/ --include="*.cpp" --include="*.hpp"
```

**Rules from CLAUDE.md:**
> - Use Logger (not std::cout/cerr/printf)
> - Use `std::format()`, never `+` concatenation for logging
> - Use `AI_INFO_IF(cond, msg)` macros when condition only gates logging
> - Use `VOIDLIGHT_DEBUG_ONLY(...)` for debug-only blocks — never raw `#ifdef DEBUG` (both defined in `Logger.hpp`)

**Correct Usage:**
```cpp
// ✗ BAD - raw console output
std::cout << "Entity count: " << count << std::endl;

// ✗ BAD - string concatenation
LOG_INFO("Entity " + name + " spawned");  // ALLOCATIONS!

// ✗ BAD - if block only for logging
if (m_debugMode) {
    AI_INFO("Debug info: " << data);
}

// ✓ GOOD - Logger with std::format
LOG_INFO(std::format("Entity count: {}", count));
LOG_ERROR(std::format("Failed to load: {}", filename));

// ✓ GOOD - conditional logging macro
AI_INFO_IF(m_debugMode, "Debug info: " << data);
```

**Quality Gate:** ✓ No raw console output, no string concat in logs, use *_IF macros

#### 5.6 Buffer Reuse & Per-Frame Allocations (CRITICAL)

**Background:**
Per-frame allocations cause GC pressure and frame spikes. CLAUDE.md requires buffer reuse patterns.

**Check Commands:**
```bash
# Find vectors created inside update/render functions (should be members)
grep -rn "std::vector<.*>" src/ --include="*.cpp" | grep -E "update|render|process" | grep -v "m_"

# Find containers without reserve() when size is known
grep -rn "\.push_back\|\.emplace_back" src/ --include="*.cpp" | head -50

# Find new allocations in hot paths
grep -rn "new \|make_unique\|make_shared" src/ --include="*.cpp" | grep -E "update|render|process"
```

**Rules from CLAUDE.md:**
> Avoid per-frame allocations. Reuse buffers.
> Always `reserve()` when size known.

**FORBIDDEN PATTERNS:**
```cpp
// ✗ BAD - Creates new vector every frame
void Manager::update() {
    std::vector<Entity*> entities;  // ALLOCATION EVERY FRAME!
    for (auto& e : m_entities) {
        entities.push_back(e.get());
    }
}

// ✓ GOOD - Reuse member buffer
class Manager {
    std::vector<Entity*> m_buffer;  // Member, reused
    void update() {
        m_buffer.clear();  // clear() keeps capacity
        for (auto& e : m_entities) {
            m_buffer.push_back(e.get());
        }
    }
};
```

**Reserve Pattern:**
```cpp
// ✗ BAD - Multiple reallocations as vector grows
std::vector<Result> results;
for (int i = 0; i < 1000; ++i) {
    results.push_back(compute(i));  // May reallocate multiple times
}

// ✓ GOOD - Single allocation upfront
std::vector<Result> results;
results.reserve(1000);  // Pre-allocate
for (int i = 0; i < 1000; ++i) {
    results.push_back(compute(i));  // No reallocations
}
```

**Quality Gate:** ✓ No local containers in hot paths, use reserve() when size known (BLOCKING)

#### 5.7 UI Component Positioning (CRITICAL)

**Background:**
UI components need proper positioning modes for resize/fullscreen support.

**Check Commands:**
```bash
# Find UI component creation without setComponentPositioning
grep -rn "createButton\|createLabel\|createPanel\|createSlider" src/gameStates/ --include="*.cpp" -A 3 | grep -v "setComponentPositioning"

# Find UI creation in game states
grep -rn "ui\.create\|m_ui\.create\|m_uiManager\.create" src/gameStates/ --include="*.cpp"
```

**Rule from CLAUDE.md:**
> **Always** call `setComponentPositioning()` after creating components for resize/fullscreen support.

**Available Helpers:**
- `createTitleAtTop()`
- `createButtonAtBottom()`
- `createCenteredButton()`
- `createCenteredDialog()`

**Position Modes:**
- `TOP_ALIGNED`, `BOTTOM_ALIGNED`
- `LEFT_ALIGNED`, `RIGHT_ALIGNED`
- `BOTTOM_RIGHT`
- `CENTERED_H`, `CENTERED_BOTH`

**Correct Pattern:**
```cpp
// ✓ GOOD - Using helper (handles positioning automatically)
ui.createCenteredButton("start_btn", rect, "Start Game");

// ✓ GOOD - Manual with positioning
ui.createButton("settings_btn", rect, "Settings");
ui.setComponentPositioning("settings_btn", {UIPositionMode::BOTTOM_ALIGNED, ...});

// ✗ BAD - No positioning (breaks on resize/fullscreen)
ui.createButton("broken_btn", rect, "Broken");
// Missing setComponentPositioning!
```

**Quality Gate:** ✓ All UI components have positioning set

#### 5.8 Rendering Rules (CRITICAL)

**Background:**
VoidLight-Framework uses GPU rendering with one present per frame. The engine owns frame lifetime; GameStates record/draw only. Loading must use `LoadingState` + async ThreadSystem ops, and transitions must be deferred (flag in `enter()`, transition in `update()`).

**Check Commands:**
```bash
# Find frame end / present / command-buffer submit in GameStates (FORBIDDEN — engine owns the frame)
grep -rn "endFrame\|present(\|SubmitGPU\|AcquireGPUSwapchain" src/gameStates/ --include="*.cpp" | grep -v "//"

# Find blocking render loops outside the LoadingState async pattern
grep -rn "while.*\(render\|recordGPU\)\|for.*\(render\|recordGPU\)" src/gameStates/ --include="*.cpp"

# Find immediate state transitions in enter() (should be deferred to update())
grep -rn "changeState\|pushState" src/gameStates/ --include="*.cpp"  # then confirm none live inside ::enter()
```

**Rules from CLAUDE.md:**
> - **One present per frame**: `GameEngine::render()` handles scene+UI; `GameEngine::present()` ends the GPU frame.
> - **NEVER** end the frame, submit command buffers, or present from a GameState.
> - Use `LoadingState` with async ThreadSystem ops, not blocking manual rendering.
> - Deferred transitions: set flag in `enter()`, transition in `update()`.

**FORBIDDEN PATTERNS:**
```cpp
// ✗ FORBIDDEN - Ending the GPU frame / presenting from a GameState
void MyState::renderGPUScene() {
    // ... record draws ...
    GPURenderer::Instance().endFrame();  // NEVER — GameEngine::present() owns this
}

// ✗ FORBIDDEN - Blocking render loop in loading
void LoadingScreen::show() {
    while (loading) {
        // manual draw + present loop — BREAKS FRAME TIMING!
    }
}

// ✗ FORBIDDEN - Immediate transition in enter()
void MyState::enter() {
    mp_stateManager->changeState("NextState");  // TIMING ISSUES!
}
```

**Correct Patterns:**
```cpp
// ✓ GOOD - Record/draw only; engine ends the frame in present()
void MyState::renderGPUScene() {
    // record vertices / submit draws to the recorder only
}

// ✓ GOOD - Use LoadingState with ThreadSystem
void LoadingState::enter() {
    m_loadingTask = ThreadSystem::Instance().enqueueTaskWithResult([this]() {
        loadResources();  // Async
    });
}

// ✓ GOOD - Deferred transition
void MyState::enter() {
    m_shouldTransition = true;  // Set flag
}
void MyState::update(float dt) {
    if (m_shouldTransition) {
        m_shouldTransition = false;
        mp_stateManager->changeState("NextState");  // Safe in update
    }
}
```

**Quality Gate:** ✓ No frame end/present/submit from GameStates; deferred transitions only (BLOCKING)

#### 5.9 Singleton Manager Access (CRITICAL)

**Background:**
Use local references at function start for manager access. Singleton `Instance()` calls are inlined by the compiler - no performance difference vs cached member pointers. Local references are cleaner (no `enter()` boilerplate, no stale pointer risk, smaller class size).

**Check Commands:**
```bash
# Find duplicate Instance() calls in the same function (GameStates are hot paths)
for file in src/gameStates/*.cpp; do
  echo "=== $file ==="
  awk '/^void.*::|^bool.*::/{fn=$2; sub(/\(.*/, "", fn); delete seen}
       /::Instance\(\)/{
         mgr=$0; sub(/.*&[[:space:]]*/, "", mgr); sub(/[[:space:]]*=.*/, "", mgr);
         if (seen[mgr]++) print "  DUPLICATE in "fn": "mgr
       }' "$file"
done

# Find cached mp_* member pointers to managers (OBSOLETE PATTERN)
grep -rn "mp_.*Mgr\|mp_.*Manager\|mp_edm\|mp_world\|mp_ui\|mp_particle\|mp_event" include/gameStates/ --include="*.hpp"
```

**FORBIDDEN PATTERNS:**

**Pattern 1: Cached Member Pointers (OBSOLETE)**
```cpp
// ✗ OBSOLETE - Cached member pointers add complexity without performance benefit
class GameState {
    UIManager* mp_uiMgr = nullptr;      // REMOVE - use local reference
    WorldManager* mp_worldMgr = nullptr; // REMOVE - use local reference
};

bool GameState::enter() {
    mp_uiMgr = &UIManager::Instance();  // OBSOLETE PATTERN
    mp_worldMgr = &WorldManager::Instance();
}

// ✓ GOOD - Local references at function start
bool GameState::enter() {
    auto& ui = UIManager::Instance();
    auto& worldMgr = WorldManager::Instance();
    ui.createButton(...);
}
```

**Pattern 2: Duplicate Instance() Calls in Same Function**
```cpp
// ✗ BAD - Multiple Instance() calls for same manager
void GameState::handleInput() {
    if (InputManager::Instance().wasKeyPressed(KEY_A)) {
        AIManager::Instance().doSomething();  // First call
    }
    if (InputManager::Instance().wasKeyPressed(KEY_B)) {
        AIManager::Instance().doSomethingElse();  // DUPLICATE!
    }
}

// ✓ GOOD - Cache at function start
void GameState::handleInput() {
    const auto& inputMgr = InputManager::Instance();
    auto& aiMgr = AIManager::Instance();

    if (inputMgr.wasKeyPressed(KEY_A)) {
        aiMgr.doSomething();
    }
    if (inputMgr.wasKeyPressed(KEY_B)) {
        aiMgr.doSomethingElse();
    }
}
```

**Pattern 3: Instance() Called in Nested Blocks Instead of Top**
```cpp
// ✗ BAD - Instance() called inside branches
void GameState::update(float dt) {
    if (condition) {
        auto& mgr = SomeManager::Instance();  // Inside if block
        mgr.process();
    } else {
        auto& mgr = SomeManager::Instance();  // DUPLICATE in else!
        mgr.processAlternate();
    }
}

// ✓ GOOD - Cache once at top, use in all branches
void GameState::update(float dt) {
    auto& mgr = SomeManager::Instance();

    if (condition) {
        mgr.process();
    } else {
        mgr.processAlternate();
    }
}
```

**Managers to Check (Common in GameStates):**
- `AIManager::Instance()`
- `UIManager::Instance()`
- `InputManager::Instance()`
- `EventManager::Instance()`
- `ParticleManager::Instance()`
- `CollisionManager::Instance()`
- `PathfinderManager::Instance()`
- `WorldManager::Instance()`
- `GameTimeManager::Instance()`
- `EntityDataManager::Instance()`
- `GameEngine::Instance()`

**Performance Impact:**
- Each redundant Instance() call: ~10-50 nanoseconds
- In tight loops or 60Hz update paths: Adds up to measurable overhead
- GameStates with 5-10 duplicate calls: ~0.5-1μs wasted per frame
- At 10K entities with behaviors: Can add 1-5ms per frame

**Quality Gate:** ✓ No cached mp_* manager pointers; no duplicate Instance() calls within same function (BLOCKING for GameStates)

#### 5.10 Controller Access Pattern (CRITICAL)

**Background:**
Controllers are state-scoped objects owned by `ControllerRegistry`, not singletons. Access via `m_controllers.get<T>()`. Cache reference at function top only when used **multiple times** in the same function. Single use → call directly.

**Check Commands:**
```bash
# Find cached mp_*Ctrl member pointers (OBSOLETE PATTERN)
grep -rn "mp_.*Ctrl" include/gameStates/ --include="*.hpp"

# Find duplicate get<Controller>() calls in same function
for file in src/gameStates/*.cpp; do
  echo "=== $file ==="
  awk '/^void.*::|^bool.*::/{fn=$2; sub(/\(.*/, "", fn); delete seen}
       /m_controllers\.get</{
         ctrl=$0; sub(/.*get</, "", ctrl); sub(/>.*/, "", ctrl);
         if (seen[ctrl]++) print "  DUPLICATE in "fn": "ctrl
       }' "$file"
done
```

**FORBIDDEN PATTERNS:**

**Pattern 1: Cached Controller Member Pointers (OBSOLETE)**
```cpp
// ✗ OBSOLETE - No cached controller pointers
class GamePlayState {
    CombatController* mp_combatCtrl{nullptr};  // REMOVE
};

bool GamePlayState::enter() {
    mp_combatCtrl = &m_controllers.add<CombatController>(m_player);  // OBSOLETE
}

// ✓ GOOD - Just add, no cached pointer
bool GamePlayState::enter() {
    m_controllers.add<CombatController>(m_player);
}
```

**Pattern 2: Duplicate get<T>() Calls in Same Function**
```cpp
// ✗ BAD - Multiple get<>() calls for same controller
void GamePlayState::updateCombatHUD() {
    if (m_controllers.get<CombatController>()->hasActiveTarget()) {
        auto target = m_controllers.get<CombatController>()->getTargetedNPC();  // DUPLICATE!
    }
}

// ✓ GOOD - Cache reference at top when used multiple times
void GamePlayState::updateCombatHUD() {
    auto& combatCtrl = *m_controllers.get<CombatController>();

    if (combatCtrl.hasActiveTarget()) {
        auto target = combatCtrl.getTargetedNPC();  // dot notation
    }
}
```

**Pattern 3: Single Use - No Caching Needed**
```cpp
// ✓ GOOD - Single use, call directly (no need to cache)
void GamePlayState::update(float dt) {
    m_controllers.get<WeatherController>()->getCurrentWeather();  // OK - only used once
}
```

**Caching Rule Summary:**
| Usage Count | Pattern |
|-------------|---------|
| Single use | `m_controllers.get<T>()->method()` |
| Multiple uses | `auto& ctrl = *m_controllers.get<T>(); ctrl.method1(); ctrl.method2();` |

**Quality Gate:** ✓ No cached mp_*Ctrl pointers; cache reference when used multiple times (BLOCKING for GameStates)

#### 5.11 Behavior Entity State Pattern (CRITICAL)

**Background:**
AIBehavior subclasses must store all per-entity mutable state in EDM BehaviorData, not as member variables. This ensures:
- Thread-safe batch processing (each entity's data at unique memory location)
- Architectural consistency (single source of truth in EDM)
- Efficient cloning (behavior config only, no per-entity state)

**Check Commands:**
```bash
# AIBehavior subclasses are declared in include/ai/BehaviorConfig.hpp; implementations live in src/ai/behaviors/.
# Discover the behavior header(s) dynamically rather than assuming a fixed path:
BEHAVIOR_HDRS=$(grep -rln "class .*Behavior" include/ai/ --include="*.hpp")

# Find EntityHandle members in behavior classes (likely per-entity state)
grep -rn "EntityHandle m_" $BEHAVIOR_HDRS

# Find mutable bool/float/int members that aren't static constexpr (may be per-entity state)
grep -rn "bool m_\|float m_\|int m_" $BEHAVIOR_HDRS | grep -v "static constexpr"

# Find mutable state that changes during executeLogic (should be in EDM)
grep -rn "m_.*=" src/ai/behaviors/ --include="*.cpp" | grep "executeLogic\|update"
```

**FORBIDDEN PATTERNS:**

**Pattern 1: EntityHandle Member Variables for Targets**
```cpp
// ✗ FORBIDDEN - Per-entity target stored in behavior instance
class AttackBehavior : public AIBehavior {
    EntityHandle m_targetHandle{};      // MOVE TO EDM BehaviorData
    bool m_hasExplicitTarget{false};    // MOVE TO EDM BehaviorData
};

// ✓ CORRECT - Target stored in EDM BehaviorData::state.attack
struct BehaviorData {
    struct AttackState {
        EntityHandle explicitTarget;
        bool hasExplicitTarget;
        // ... other state
    };
};

// Access via context in executeLogic:
void AttackBehavior::executeLogic(BehaviorContext& ctx) {
    auto& attack = ctx.behaviorData->state.attack;
    if (attack.hasExplicitTarget && attack.explicitTarget.isValid()) {
        // Use EDM data
    }
}
```

**Pattern 2: Mutable State Modified During executeLogic**
```cpp
// ✗ FORBIDDEN - Mutable state in behavior instance
class SomeBehavior : public AIBehavior {
    float m_lastActionTime{0.0f};  // Changes per-entity - MOVE TO EDM
    int m_actionCount{0};           // Changes per-entity - MOVE TO EDM
};

void SomeBehavior::executeLogic(BehaviorContext& ctx) {
    m_lastActionTime = currentTime;  // RACE if same behavior used by multiple entities!
    m_actionCount++;
}

// ✓ CORRECT - State in EDM, accessed via context
void SomeBehavior::executeLogic(BehaviorContext& ctx) {
    auto& state = ctx.behaviorData->state.custom;
    state.lastActionTime = currentTime;  // Per-entity, thread-safe
    state.actionCount++;
}
```

**What CAN be Member Variables (Configuration):**
```cpp
class AttackBehavior : public AIBehavior {
    // ✓ GOOD - Configuration parameters (set once, read-only during update)
    float m_attackRange{80.0f};
    float m_attackDamage{10.0f};
    float m_attackSpeed{1.0f};
    AttackMode m_attackMode{AttackMode::MELEE};

    // ✓ GOOD - Static constants
    static constexpr float COMBO_TIMEOUT = 3.0f;

    // ✓ GOOD - RNG (thread_local for thread safety, or mutable with no cross-entity dependency)
    mutable std::mt19937 m_rng{std::random_device{}()};
};
```

**What MUST be in EDM BehaviorData:**
- Target handles (EntityHandle)
- Timers that tick per-entity (float lastAttackTime, float cooldownTimer)
- Counters that increment per-entity (int comboCount, int attacksThisSecond)
- State flags that change during executeLogic (bool isCharging, bool isRetreating)
- Positions that track per-entity movement (Vector2D lastPosition, Vector2D targetPosition)

**Thread Safety Reasoning:**
1. Behaviors are cloned per-entity via `clone()` - but state STILL shouldn't be in members
2. Batch processing partitions entities - different threads access different `m_behaviorData[edmIndex]`
3. EDM BehaviorData follows established thread-safe per-entity indexing pattern
4. Consolidating state in EDM provides single source of truth

**How to Fix Violations:**
1. Add new fields to the appropriate per-variant state struct in `include/ai/BehaviorStateData.hpp` (e.g., `WanderStateData`, `AttackStateData`)
2. For fields shared across all behaviors (flocking, messages, movement cache), add to `BehaviorData` in `EntityDataManager.hpp`
3. Change behavior methods to take `edmIndex` parameter when accessing state
4. Access variant state via `edm.get<Variant>State(ref.index).X` where `ref = edm.getBehaviorConfigRef(edmIndex)`; access shared header via `edm.getBehaviorData(edmIndex).X` or `ctx.sharedState.X`

**Quality Gate:** ✓ No per-entity mutable state in AIBehavior member variables (BLOCKING)

#### 5.12 Controller → AI Layer Boundary (CRITICAL)

**Background:**
Controllers are state-scoped event bridges. They must NEVER directly mutate AI behavior state in EDM (guard alertLevel, behavior flags, flee state, etc.). The AI layer owns behavior state — controllers communicate via behavior messages. See CLAUDE.md: AI section (lock-free EDM access via `BehaviorContext`) and Controllers.

**Check Commands:**
```bash
# Find controllers directly accessing behavior-specific state in EDM
grep -rn "guardState\.\|fleeState\.\|attackState\.\|\.alertLevel\|\.hasActiveThreat" src/controllers/ --include="*.cpp"

# Find controllers mutating BehaviorData fields directly
grep -rn "getBehaviorData\|getGuardState\|getFleeState\|getAttackState" src/controllers/ --include="*.cpp" | grep -v "const"

# Verify controllers use behavior messages instead
grep -rn "queueBehaviorMessage\|deferBehaviorMessage" src/controllers/ --include="*.cpp"
```

**FORBIDDEN PATTERNS:**
```cpp
// ✗ FORBIDDEN - Controller directly mutates AI behavior state
void SocialController::alertNearbyGuards(const Vector2D& location) {
    auto& edm = EntityDataManager::Instance();
    for (size_t idx : nearbyGuards) {
        auto& guardState = edm.getGuardState(idx);
        guardState.alertLevel = 3;             // LAYER VIOLATION!
        guardState.hasActiveThreat = true;     // LAYER VIOLATION!
        guardState.lastKnownThreatPosition = location;  // LAYER VIOLATION!
    }
}

// ✓ CORRECT - Controller sends behavior messages, AI layer handles state
void SocialController::alertNearbyGuards(const Vector2D& location) {
    auto& edm = EntityDataManager::Instance();
    m_nearbyGuardBuffer.clear();
    AIManager::Instance().queryEdmIndicesInRadius(location, GUARD_ALERT_RANGE,
                                                   m_nearbyGuardBuffer, true);
    for (size_t idx : m_nearbyGuardBuffer) {
        if (edm.getBehaviorData(idx).behaviorType != BehaviorType::Guard) continue;
        Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);  // Main thread
    }
}
```

**Message API:**
- `Behaviors::queueBehaviorMessage(idx, msg)` — Main thread (controllers, event handlers)
- `Behaviors::deferBehaviorMessage(idx, msg)` — Worker threads (batch processing)

**Quality Gate:** ✓ No direct AI behavior state mutation from controllers (BLOCKING)

#### 5.13 State Transition Completeness (CRITICAL)

**Background:**
ALL game states with AI entities must call `prepareForStateTransition()` on ALL relevant managers in BOTH exit paths (transitioning-to-loading AND full exit). Missing a manager causes stale data on re-entry. See CLAUDE.md: "State Transitions" in the Threading section.

**Check Commands:**
```bash
# Find game states with exit() methods (discover dynamically — do not hardcode state names)
grep -rln "::exit()" src/gameStates/ --include="*.cpp"

# For each AI-enabled state (one that touches AIManager), verify managers are transitioned
for f in $(grep -rln "AIManager::Instance" src/gameStates/ --include="*.cpp"); do
  echo "=== $(basename "$f") ==="
  grep -c "prepareForStateTransition" "$f" 2>/dev/null || echo "MISSING"
done

# Check for BackgroundSimulationManager specifically (commonly missed)
grep -rn "BackgroundSimulationManager.*prepareForStateTransition\|bgSimMgr.*prepareForStateTransition" src/gameStates/ --include="*.cpp"
```

**Required Manager Transition Order (from CLAUDE.md — full chain):**
States with AI entities must transition these managers in this order (demo states may skip managers they never initialized):
1. `AIManager` — Pauses batch processing, waits for pending futures
2. `ProjectileManager`
3. `BackgroundSimulationManager` — Clears simulation tiers and accumulators
4. `WorldManager`
5. `WorldResourceManager` — Clears spatial indices and registries
6. `EventManager` — Drains deferred event queues
7. `CollisionManager` — Clears collision bodies and spatial hash
8. `PathfinderManager` — Cancels pending path requests
9. `EntityDataManager` — Clears all entity data
10. `WorkerBudgetManager`
11. `ParticleManager`

> Call `prepareForStateTransition()` before cleanup. Note EDM is **not** last in the full chain — `WorkerBudgetManager` and `ParticleManager` follow it.

**FORBIDDEN PATTERN:**
```cpp
// ✗ BAD - Missing BackgroundSimulationManager (stale tiers on re-entry)
bool GamePlayState::exit() {
    AIManager &aiMgr = AIManager::Instance();
    aiMgr.prepareForStateTransition();
    // BackgroundSimulationManager MISSING!
    EntityDataManager &edm = EntityDataManager::Instance();
    edm.prepareForStateTransition();
}

// ✓ CORRECT - All managers transitioned
bool GamePlayState::exit() {
    AIManager &aiMgr = AIManager::Instance();
    BackgroundSimulationManager &bgSimMgr = BackgroundSimulationManager::Instance();
    // ... other managers ...
    EntityDataManager &edm = EntityDataManager::Instance();

    aiMgr.prepareForStateTransition();
    bgSimMgr.prepareForStateTransition();
    // ... other managers in CLAUDE.md order ...
    edm.prepareForStateTransition();  // followed by WorkerBudgetManager, ParticleManager
}
```

**Quality Gate:** ✓ All managers transitioned in both exit paths of every AI-enabled state (BLOCKING)

#### 5.14 Thread-Local Capacity Preservation (CRITICAL)

**Background:**
Thread-local vectors used for deferred event collection (or any per-thread buffering) must preserve their capacity across frames. Using `swap()` with an empty vector or returning by value destroys the capacity, causing per-frame heap allocations on every worker thread. See CLAUDE.md: "Thread-Local" in the Threading section.

**Check Commands:**
```bash
# Find thread_local vectors being swapped (destroys capacity)
grep -rn "swap.*t_\|t_.*swap" src/ --include="*.cpp"

# Find thread_local vectors returned by value (destroys capacity)
grep -rn "return.*t_\|return std::move.*t_" src/ --include="*.cpp"

# Verify thread_local collections use clear() pattern
grep -rn "thread_local.*vector" src/ --include="*.cpp" -A 5 | grep -E "clear|swap|return"
```

**FORBIDDEN PATTERNS:**
```cpp
// ✗ BAD - swap() destroys thread_local capacity, reallocates next frame
thread_local std::vector<DeferredEvent> t_deferredEvents;

std::vector<DeferredEvent> collectDeferredEvents() {
    std::vector<DeferredEvent> result;
    result.swap(t_deferredEvents);  // t_deferredEvents now has capacity 0!
    return result;
}

// ✗ BAD - return by value also destroys capacity
std::vector<DeferredEvent> collectDeferredEvents() {
    return std::move(t_deferredEvents);  // Capacity moved away!
}

// ✓ CORRECT - ref-based API preserves thread_local capacity
void collectDeferredEvents(std::vector<DeferredEvent>& out) {
    out.insert(out.end(),
               std::make_move_iterator(t_deferredEvents.begin()),
               std::make_move_iterator(t_deferredEvents.end()));
    t_deferredEvents.clear();  // Keeps capacity for next frame
}
```

**Why This Matters:**
- Each worker thread has its own thread_local vector
- `swap()` or return-by-value leaves the thread_local with capacity 0
- Next frame, the vector must reallocate (heap allocation per worker per frame)
- With 10 workers at 60 FPS: 600 unnecessary allocations/second

**Quality Gate:** ✓ Thread-local vectors preserve capacity via clear(), never swap/return-by-value (BLOCKING)

#### 5.15 World Lifecycle Cleanup (CRITICAL)

**Background:**
Manager-local caches, spatial indices, and reverse lookups that describe world state must be cleared on **world unload**, not only on state exit. Missing this causes stale handles and cache entries to survive a world change. See CLAUDE.md: "World lifecycle" under State Transitions.

**Check Commands:**
```bash
# Verify managers that subscribe to WorldLoaded/WorldUnloaded also clear caches
grep -rn "WorldUnloaded\|WorldLoaded" src/managers/ --include="*.cpp"

# Find spatial indices / reverse lookups that may not be world-lifecycle cleared
grep -rn "m_.*Index\|m_.*Lookup\|m_.*Cache" include/managers/ --include="*.hpp" | grep -v "//"
```

**FORBIDDEN PATTERN:**
```cpp
// ✗ BAD - Spatial index populated on world load, never cleared on unload
void SomeManager::onWorldLoaded(const EventData&) {
    rebuildSpatialIndex();
}
// Missing onWorldUnloaded handler → stale entries survive world change

// ✓ CORRECT - Paired handlers
void SomeManager::onWorldLoaded(const EventData&)   { rebuildSpatialIndex(); }
void SomeManager::onWorldUnloaded(const EventData&) { m_spatialIndex.clear(); m_reverseLookup.clear(); }
```

**Reviewer checklist** (not grep-able — eyeball during review):
- Does every manager-local cache/index tied to world geometry have a matching unload path?
- Do stored EntityHandles or chunk coords get invalidated when the world changes?
- Are active-world bookkeeping counters reset?

**Quality Gate:** ✓ No world-scoped caches surviving world unload (BLOCKING)

#### 5.16 Second Source of Truth (WARNING)

**Background:**
Cross-frame state stored only in manager-local scratch when render, collision, save/load, or future frames depend on it creates a duplicate source of truth. That state belongs in EDM.

**Check Commands:**
```bash
# Find manager-local vectors/maps keyed by entity that may hold cross-frame state
grep -rn "std::unordered_map<EntityHandle\|std::unordered_map<size_t.*Entity" include/managers/ --include="*.hpp"

# Find mutable per-entity state stored in manager members (not EDM)
grep -rn "m_entity.*State\|m_per.*Entity" include/managers/ --include="*.hpp"
```

**Reviewer checklist:**
- Does render/collision/save-load or the next frame read this state? → Move to EDM.
- Is the manager-local copy drifting from an EDM field that already exists? → Consolidate.
- Is the data truly transient-within-a-single-frame scratch? → Acceptable as manager-local.

**Quality Gate:** ⚠ Review any per-entity cross-frame state stored outside EDM (WARNING)

#### 5.17 Render Controller Lifecycle Ownership (WARNING)

**Background:**
Render controllers (HUDController, CameraRenderController, etc.) read canonical state and emit draw calls. They must not own destruction/teardown of gameplay systems. Lifecycle belongs to managers and game states.

**Check Commands:**
```bash
# Find render controllers doing cleanup/teardown work
grep -rn "cleanup\|destroy\|clear\|unsubscribe\|prepareForStateTransition" src/controllers/render/ --include="*.cpp"

# Find render controllers mutating manager state
grep -rn "Manager::Instance().*\(set\|remove\|delete\|clear\)" src/controllers/render/ --include="*.cpp"
```

**Reviewer checklist:**
- Does a render controller call `prepareForStateTransition()` or `clear*()` on a manager? → Drift.
- Does it unsubscribe handlers that were registered elsewhere? → Drift.
- Is it only reading state + emitting draw commands? → Conforming.

**Quality Gate:** ⚠ Render controllers must not own lifecycle teardown (WARNING)

#### 5.18 Event Contract Bypass (WARNING)

**Background:**
Direct state mutation that skips an event other systems subscribe to (UI dirtying, log updates, collision response, achievement tracking) silently breaks downstream consumers. The contract is: if writing X has historically fired event Y, new writers must fire Y too.

**Check Commands:**
```bash
# Find direct mutation of fields that have event-fire counterparts elsewhere
# (manual inspection — this is a semantic check, not a syntactic one)

# Find places that set common "event-bearing" state without a trigger call
grep -rn "setHealth\|setGold\|setWeather\|setTimeOfDay" src/ --include="*.cpp" -A 3 | grep -v "trigger\|fire\|emit\|dispatch"
```

**Reviewer checklist:**
- Does the mutated field have other writers that fire an event? → New writer must fire it too.
- Does UI or logging depend on an event this path skips? → Drift.
- Is the event immediate vs deferred dispatch consistent with existing writers?

**Quality Gate:** ⚠ New state mutations must preserve existing event contracts (WARNING)

#### 5.19 EDM Policy Creep (WARNING)

**Background:**
EDM stores persistent per-entity *state*. Thresholds, decision weights, AI tuning constants, and policy belong in the behavior layer, config, or manager — not EDM. New EDM fields should describe what the entity *is*, not how it *decides*.

**Check Commands:**
```bash
# Find recently-added EDM fields that look like policy/thresholds
grep -rn "Threshold\|Weight\|Multiplier\|Tuning\|Decision" include/managers/EntityDataManager.hpp

# Find constants in EDM that belong in config/behavior layer
grep -rn "constexpr.*=" include/managers/EntityDataManager.hpp | grep -iE "threshold|weight|factor|multiplier"
```

**Reviewer checklist:**
- Does the field describe *what the entity is* (position, health, emotion state)? → EDM is correct.
- Does it describe *how a behavior decides* (flee threshold, aggression weight)? → Behavior layer.
- Does it vary per-entity based on personality? → State (EDM) derived from config (behavior layer).

**Quality Gate:** ⚠ EDM must remain pure state, not policy (WARNING)

### 6. Copyright & Legal Compliance

**Check Command:**
```bash
# Find files missing copyright header
find src/ include/ -type f \( -name "*.cpp" -o -name "*.hpp" \) -exec grep -L "Copyright (c) 2025 Hammer Forged Games" {} \;
```

**Required Header:**
```cpp
/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/
```

**Quality Gate:** ✓ All source files have copyright header

### 7. Test Coverage

**Check Command:**
```bash
# For modified files, check if corresponding test exists
# Example: if src/managers/NewManager.cpp exists, check for tests/NewManager_tests.cpp
```

**Rules:**
- New managers must have test file in `tests/`
- New managers must have test script in `tests/test_scripts/run_*_tests.sh`
- Test script must be added to `run_all_tests.sh`

**Quality Gate:** ✓ New code has corresponding tests

## Quality Report Format

```markdown
=== VOIDLIGHT QUALITY GATE REPORT ===
Generated: YYYY-MM-DD HH:MM:SS
Branch: <current-branch>

## Compilation Quality
✓/✗ Status: <PASSED/FAILED>
  Warnings: <count>
  Errors: <count>

<details if failures>

## Static Analysis (cppcheck)
✓/✗ Status: <PASSED/FAILED>
  Errors: <count>
  Warnings: <count>

<list of issues>

## Static Analysis (clang-tidy)
✓/✗ Status: <PASSED/FAILED>
  Critical: <count>
  High: <count>
  Medium: <count>
  Low: <count>

<list of critical/high issues>

## Coding Standards
✓/✗ Naming Conventions: <PASSED/FAILED>
  <violations if any>

✓/✗ Formatting: <PASSED/FAILED>
  <violations if any>

## Threading Safety (CRITICAL)
✓/✗ Static Variables: <PASSED/FAILED>
  <violations - BLOCKING>

✓/✗ ThreadSystem Usage: <PASSED/FAILED>
  <violations if any>

✓/✗ Mutex Protection: <PASSED/FAILED>
  <violations if any>

## Architecture Compliance
✓/✗ Rendering Rules: <PASSED/FAILED>
  <violations - BLOCKING if SDL_RenderClear/Present outside GameEngine>
✓/✗ RAII/Smart Pointers: <PASSED/FAILED>
✓/✗ Smart Pointer Performance: <PASSED/FAILED>
  <violations if any - BLOCKING for perf-critical code>
✓/✗ Logger Usage: <PASSED/FAILED>
  <check for std::format usage, *_IF macros>
✓/✗ Buffer Reuse: <PASSED/FAILED>
  <violations if any - BLOCKING for hot paths>
✓/✗ UI Positioning: <PASSED/FAILED>
  <missing setComponentPositioning calls>
✓/✗ Singleton Manager Access: <PASSED/FAILED>
  <cached mp_* pointers or duplicate Instance() calls - BLOCKING for GameStates>
✓/✗ Behavior Entity State: <PASSED/FAILED>
  <per-entity state in behavior members instead of EDM - BLOCKING>
✓/✗ Controller→AI Boundary: <PASSED/FAILED>
  <direct behavior state mutation from controllers - BLOCKING>
✓/✗ State Transition Completeness: <PASSED/FAILED>
  <missing manager transitions in exit paths - BLOCKING>
✓/✗ Thread-Local Capacity: <PASSED/FAILED>
  <thread_local vectors losing capacity via swap/return - BLOCKING>
✓/✗ World Lifecycle Cleanup: <PASSED/FAILED>
  <world-scoped caches surviving unload - BLOCKING>
⚠ Second Source of Truth: <REVIEW>
  <per-entity cross-frame state outside EDM - WARNING>
⚠ Render Controller Lifecycle: <REVIEW>
  <render controllers owning teardown - WARNING>
⚠ Event Contract Bypass: <REVIEW>
  <direct mutation skipping established events - WARNING>
⚠ EDM Policy Creep: <REVIEW>
  <policy/thresholds in EDM instead of behavior layer - WARNING>

## Legal Compliance
✓/✗ Copyright Headers: <PASSED/FAILED>
  Missing: <count> files
  <list files>

## Test Coverage
✓/✗ Tests Exist: <PASSED/FAILED>
  <missing tests>

---
## OVERALL STATUS: ✓ PASSED / ✗ FAILED

✓ Ready to commit
✗ Fix <count> violations before commit

### Critical Issues (BLOCKING)
<list blocking issues>

### Warnings (Review Required)
<list warnings>

### Recommendations
<specific fixes>
```

## Exit Codes

- **0:** All checks passed
- **1:** Critical violations (static vars, threading issues)
- **2:** Compilation warnings/errors
- **3:** Static analysis failures
- **4:** Missing copyright headers
- **5:** Multiple categories failed

## Severity Classification

**BLOCKING (Must Fix):**
- Static variables in threaded code
- Per-entity mutable state in AIBehavior member variables (use EDM BehaviorData)
- Unnecessary shared_ptr copies in hot paths (perf-critical code)
- Per-frame allocations in hot paths (local containers in update/render)
- Duplicate Manager::Instance() calls in GameState functions
- Cached mp_* manager pointers in GameState headers (use local references)
- SDL_RenderClear/Present outside GameEngine
- Compilation errors
- Critical cppcheck errors
- Critical clang-tidy issues (bugprone-*, clang-analyzer-*)
- Missing copyright headers on new files
- Controller directly mutating AI behavior state in EDM (use behavior messages)
- Missing manager in state transition exit paths (especially BackgroundSimulationManager)
- Thread-local vector capacity destroyed by swap/return-by-value (use ref+clear)
- World-scoped manager caches/indices surviving world unload (missing unload handler)

**WARNING (Should Fix):**
- Per-entity cross-frame state stored in manager-local scratch instead of EDM (second source of truth)
- Render controllers owning lifecycle/teardown work (should only read + draw)
- Direct state mutation bypassing established event contracts (UI/log/collision consumers break silently)
- Policy/thresholds/decision weights added to EDM instead of behavior layer (EDM policy creep)
- Compilation warnings
- cppcheck warnings
- High clang-tidy issues (performance-*, modernize-use-override)
- Naming convention violations
- Missing tests for new code
- String concatenation in logging (use std::format)
- Conditional blocks only for logging (use *_IF macros)
- Missing UI component positioning
- Missing reserve() calls for known sizes

**INFO (Consider Fixing):**
- Style suggestions
- Performance hints
- Code organization recommendations
- Deferred transition patterns in GameStates

## Usage as Git Pre-Commit Hook

This Skill can be integrated as a git hook:

```bash
# .git/hooks/pre-commit
#!/bin/bash
# Ask Claude to run quality check
claude-code "run quality check on my changes"

if [ $? -ne 0 ]; then
    echo "Quality check failed. Fix issues before committing."
    exit 1
fi
```
