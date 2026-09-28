# VoidLight Quality Check — Coding Standards & Quick Fixes

Detection recipes for section 3 of the check catalog, plus a Quick Fix Guide. The rules are defined in root `CLAUDE.md` › Core Rules › C++ and APIs (naming, formatting, API shape, copyright) — read them there; this file only says how to check them.

## 3. Coding Standards

### 3.1 Naming

Naming is enforced by clang-tidy `readability-identifier-naming` (`tests/clang-tidy/.clang-tidy`) in the Branch/PR pass. Quick grep for lowercase type names:
```bash
grep -rnE "^\s*(class|struct|enum class) [a-z]\w*\s*(final\s*)?(:.*)?\{?\s*$" include/ src/
```
WARNING.

### 3.2 Formatting

4-space indent, no tabs, Allman braces. Check new/changed lines in the diff rather than the whole tree:
```bash
git diff -U0 main... -- '*.cpp' '*.hpp' | grep -nP "^\+.*\t"      # tabs in added lines
```
INFO/WARNING — match the surrounding file when it predates the rule.

### 3.3 C++ API rules
```bash
grep -rnE "\(void\)\s*[a-z]\w*;" src/ include/ | grep -v "include/core/Logger.hpp"   # unused-param casts
grep -rn "\[\[maybe_unused\]\]" src/                                                 # only allowed on empty virtual base defaults
grep -rnE "constexpr const char\s*\*|const char\s*\*\s*[A-Z_]+\s*=" src/ include/   # C-string constants -> std::string_view
```
Also review diffs for: raw arrays, C-string APIs outside the final SDL/C boundary, `string_view -> string` churn, unchecked `[[nodiscard]]` bool returns (`init()`, `load()`, `create()`). Existing hits may be pre-existing; flag new ones. WARNING.

## Quick Fix Guide

| Violation | Fix |
|-----------|-----|
| Unused parameter | Drop the name: `void f(int)`. |
| Mutable `static` in threaded code | Member state, `thread_local`, or atomic. |
| Raw thread / local thread-count heuristic | `ThreadSystem` task shaped by `WorkerBudget`; report execution after completion. |
| Missing copyright | Add the header from CLAUDE.md › C++ and APIs. |
| `std::cout` / `printf` | Subsystem log macro, e.g. `AI_INFO(std::format(...))`. |
| Log string concat | `GAMEENGINE_INFO(std::format("Value: {}", x));` |
| `if (c) { AI_INFO(...); }` | `AI_INFO_IF(c, ...);` |
| `#ifdef DEBUG` | `VOIDLIGHT_DEBUG_ONLY(...)`. |
| Raw `new`/`delete`, raw-pointer ownership | `std::make_unique` / RAII; references, `std::optional`, or handles for non-owning/optional access. |
| `shared_ptr` copy/capture in hot path | Pass `const&`, capture by reference within the owner's lifetime, or iterate EDM indices/handles. |
| `string_view` param converted for map lookup | Take `const std::string&`. |
| Per-frame local container | Reusable member (or `thread_local` in worker code), `clear()` each frame, `reserve()` when size known. |
| Thread-local buffer `swap`/return-by-value | `void collect(std::vector<T>& out)` + `t_buf.clear()`. |
| UI component not positioned | `ui.setComponentPositioning(id, {UIPositionMode::..., ...})` after create, or use a positioning helper. |
| Clear/end/submit/present in a GameState | Remove; states only record and draw. `GameEngine::present()` owns the frame. |
| Transition in `enter()` | Set intent in `enter()`, call `mp_stateManager->changeState(GameStateId::...)` in `update()`. |
| Cached manager `mp_*` / `mp_*Ctrl` member | Local `auto& x = Manager::Instance();` at function top; `m_controllers.add<T>()` in `enter()`. |
| Behavior per-entity state outside EDM | Add the field to the variant `*StateData` in `include/ai/BehaviorStateData.hpp`. |
| Controller mutates AI state | `Behaviors::queueBehaviorMessage(idx, BehaviorMessage::...)`. |
| Missing manager in exit path | Follow the 11-manager order in CLAUDE.md › State Transitions and Events; mirror `GamePlayState::exit()`. |
| State registers a persistent handler / collision callback | Transient `registerHandlerWithToken()` in `enter()`; collision wiring stays manager-owned. |
