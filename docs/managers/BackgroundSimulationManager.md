# BackgroundSimulationManager

**Code:** `include/managers/BackgroundSimulationManager.hpp`, `src/managers/BackgroundSimulationManager.cpp`

**Singleton Access:** Use `BackgroundSimulationManager::Instance()` to access the manager.

## Overview

BackgroundSimulationManager processes entities that are outside the active camera area but still need basic simulation to maintain world consistency. It handles **Background tier** entities with simplified updates at a reduced rate (10Hz by default).

### Processing by Tier

| Tier | Manager | Update Rate | Features |
|------|---------|-------------|----------|
| **Active** | AIManager, CollisionManager | 60 Hz | Full AI, collision, rendering |
| **Background** | BackgroundSimulationManager | 10 Hz | Position-only updates, no collision |
| **Hibernated** | None | 0 Hz | Data stored only, no processing |

## Simulation Tier Details

### Active Tier

Entities in the Active tier receive full simulation:
- **AI Processing**: Full behavior execution via AIManager
- **Collision Detection**: Participates in CollisionManager narrowphase
- **Rendering**: Visible on screen
- **Update Rate**: Every frame (60 Hz)

**When to use**: Entities near the player/camera that need full gameplay interaction.

### Background Tier

Entities in the Background tier receive reduced simulation:
- **Position Updates**: Simplified movement (continue current velocity)
- **No Collision**: Skipped to reduce CPU cost
- **No Rendering**: Off-screen
- **Update Rate**: 10 Hz (configurable)
- **State Preservation**: Behavior state maintained in EDM

**When to use**: Entities outside the visible area but still in the "active world" zone. Maintains world consistency (NPCs continue patrolling, animals continue grazing) without full simulation cost.

**What Background Entities Do**:
```cpp
// Simplified Background tier update (simulateNPC)
void BackgroundSimulationManager::simulateNPC(float dt, size_t index);
// Position integration + 0.98 velocity decay at 10 Hz. No AI, no collision.
```

### Hibernated Tier

Entities in the Hibernated tier receive no updates:
- **No Processing**: Completely dormant
- **Data Stored**: All EDM data preserved (position, state, inventory)
- **No CPU Cost**: Zero frame-to-frame overhead
- **Reactivation**: Instantly resume when transitioning to Background/Active

**When to use**: Entities far from the player that don't need any simulation. Their state is preserved for when the player returns.

### Tier Transitions

Entities automatically transition between tiers based on distance from the reference point (usually the player):

```
Distance from player:
├─ 0 to activeRadius       → Active (full simulation)
├─ activeRadius to backgroundRadius → Background (10Hz position)
└─ > backgroundRadius      → Hibernated (no updates)
```

**Transition Events**:
- **Active → Background**: Entity leaves visible area, continues with simplified updates
- **Background → Hibernated**: Entity far enough to stop all processing
- **Hibernated → Background**: Entity approaching active zone, begin position updates
- **Background → Active**: Entity visible, resume full AI/collision

### Tier Configuration

```cpp
auto& bsm = BackgroundSimulationManager::Instance();

// Configure tier radii based on screen size
bsm.configureForScreenSize(1920, 1080);
// Results in:
//   activeRadius ≈ 1650 (1.5x half-diagonal)
//   backgroundRadius ≈ 2200 (2x half-diagonal)

// Or set manually
bsm.setActiveRadius(1500.0f);
bsm.setBackgroundRadius(5000.0f);

// Adjust Background tier update rate
bsm.setUpdateRate(10.0f);  // 10 Hz (default)
bsm.setUpdateRate(5.0f);   // 5 Hz (power saving)
bsm.setUpdateRate(30.0f);  // 30 Hz (more responsive)
```

### Tier State in EDM

Entity tier is stored in `EntityHotData`:

```cpp
struct EntityHotData {
    // ...
    SimulationTier tier;  // Active, Background, or Hibernated
    // ...
};

// Check entity tier
const auto& hot = edm.getHotDataByIndex(edmIndex);
if (hot.tier == SimulationTier::Active) {
    // Full processing
} else if (hot.tier == SimulationTier::Background) {
    // Simplified processing
} else {
    // Hibernated - skip entirely
}
```

### Key Features

- **Power-Efficient**: Zero CPU when paused or no background entities
- **Accumulator-Based Timing**: Fixed 10Hz updates regardless of frame rate
- **Tier Management**: Updates simulation tiers every 60 frames (~1 second)
- **WorkerBudget Integration**: Adaptive batch sizing for parallel processing
- **Screen-Size Aware**: Configurable radii based on display dimensions

## Architecture

```
GameEngine::update()
    │
    ├─► AIManager::update()           // Active tier (60 Hz)
    ├─► CollisionManager::update()    // Active tier (60 Hz)
    │
    └─► BackgroundSimulationManager::update()
            │
            ├─► Phase 1: Tier updates (every 60 frames)
            │   └─► EntityDataManager::updateSimulationTiers()
            │
            └─► Phase 2: Background entity processing (10 Hz)
                └─► Simplified position updates
```

## Public API Reference

### Lifecycle

```cpp
static BackgroundSimulationManager& Instance();
bool init();
void clean();
void prepareForStateTransition();
[[nodiscard]] bool isInitialized() const noexcept;
[[nodiscard]] bool isShutdown() const noexcept;
```

### Main Update

```cpp
/**
 * @brief Main update - handles tier recalc AND background entity processing
 * @param referencePoint Player/camera position for tier distance calculation
 * @param deltaTime Frame delta time (for accumulator)
 *
 * Power-efficient single entry point:
 * - Phase 1: Tier updates every 60 frames (~1 sec at 60Hz)
 * - Phase 2: Background entity processing at 10Hz (only if entities exist)
 */
void update(const Vector2D& referencePoint, float deltaTime);
```

### Global Pause

```cpp
void setGlobalPause(bool paused);
[[nodiscard]] bool isGloballyPaused() const noexcept;
```

### Tier Management

```cpp
void setReferencePoint(const Vector2D& position);
[[nodiscard]] Vector2D getReferencePoint() const;
void updateTiers();
void invalidateTiers();  // Force tier update on next frame
[[nodiscard]] bool hasWork() const noexcept;
```

### Configuration

```cpp
// Tier radii
void setActiveRadius(float radius);
void setBackgroundRadius(float radius);
[[nodiscard]] float getActiveRadius() const;
[[nodiscard]] float getBackgroundRadius() const;

// Screen-size aware configuration
void configureForScreenSize(int screenWidth, int screenHeight);

// Update rate
void setUpdateRate(float hz);  // Default: 10 Hz
[[nodiscard]] float getUpdateRate() const;
```

### Synchronization

```cpp
void waitForAsyncCompletion();  // Call before state transitions
```

### Performance Metrics

```cpp
struct PerfStats {
    double lastUpdateMs;
    double avgUpdateMs;
    size_t lastEntitiesProcessed;
    size_t lastBatchCount;
    size_t lastTierChanges;
    uint64_t totalUpdates;
    bool lastWasThreaded;
};

[[nodiscard]] const PerfStats& getPerfStats() const;
void resetPerfStats();
```

## Usage Examples

### Basic Integration

```cpp
// In GameEngine::update()
void GameEngine::update(float dt) {
    // Process Active tier entities
    AIManager::Instance().update(dt);
    CollisionManager::Instance().update(dt);

    // Process Background tier entities (power-efficient, 10Hz)
    BackgroundSimulationManager::Instance().update(playerPosition, dt);
}
```

### Screen-Size Configuration

```cpp
// In GameEngine::init() after window creation
BackgroundSimulationManager::Instance().configureForScreenSize(1920, 1080);
// Results in:
// - Active radius: ~1650 pixels (1.5x half-diagonal)
// - Background radius: ~2200 pixels (2x half-diagonal)
```

### State Transitions

```cpp
void GameState::exit() {
    // Wait for any async background processing
    BackgroundSimulationManager::Instance().waitForAsyncCompletion();

    // Now safe to clean up
    // ...
}
```

### Custom Update Rate

```cpp
// For a more responsive background simulation
BackgroundSimulationManager::Instance().setUpdateRate(30.0f);  // 30 Hz

// For power-saving mode
BackgroundSimulationManager::Instance().setUpdateRate(5.0f);   // 5 Hz
```

## Threading Model

BackgroundSimulationManager follows the AIManager threading pattern:

### Single-Threaded Path (< 500 entities)
- Direct processing on main thread
- No threading overhead

### Multi-Threaded Path (>= 500 entities)
- Uses WorkerBudget for optimal batch sizing
- Submits batches to ThreadSystem
- Per-batch output buffers (zero contention)

### Threading Thresholds

| Constant | Value | Purpose |
|----------|-------|---------|
| `MIN_ENTITIES_FOR_THREADING` | 500 | Threshold for multi-threaded processing |
| `MIN_BATCH_SIZE` | 64 | Minimum entities per batch |
| `TIER_UPDATE_INTERVAL` | 120 | Frames between tier recalculation |

## Tier Radius Configuration

### Default Values (1920x1080)

```cpp
float m_activeRadius{1650.0f};      // ~1.5x window half-diagonal
float m_backgroundRadius{2200.0f};  // ~2x window half-diagonal
```

### configureForScreenSize() Formula

```cpp
float halfDiagonal = sqrt((width/2)^2 + (height/2)^2);
activeRadius = halfDiagonal * 1.5f;      // Visible + buffer
backgroundRadius = halfDiagonal * 2.0f;  // Pre-loading zone
```

### Tier Assignment

```
Distance from reference point:
├─ 0 to activeRadius       → Active tier (full processing)
├─ activeRadius to backgroundRadius → Background tier (10Hz)
└─ > backgroundRadius      → Hibernated tier (no processing)
```

## Power Efficiency

BackgroundSimulationManager is designed for minimal CPU usage:

1. **Zero CPU when paused**: Immediate return from `update()`
2. **No work detection**: Skips processing if no background entities
3. **Accumulator-based timing**: Only processes at target rate (10Hz)
4. **Tier caching**: Only recalculates tiers every 60 frames

### Typical CPU Usage

| Scenario | CPU Usage |
|----------|-----------|
| No background entities | ~0% |
| 100 background entities | < 0.1% |
| 1000 background entities | < 0.5% |
| 10K background entities | ~1-2% |

## Performance Stats

```cpp
auto& bsm = BackgroundSimulationManager::Instance();
const auto& stats = bsm.getPerfStats();

std::cout << "Last update: " << stats.lastUpdateMs << "ms\n";
std::cout << "Avg update: " << stats.avgUpdateMs << "ms\n";
std::cout << "Entities: " << stats.lastEntitiesProcessed << "\n";
std::cout << "Tier changes: " << stats.lastTierChanges << "\n";
```

## Related Documentation

- **[EntityDataManager](EntityDataManager.md)** - Central entity data authority
- **[AIManager](../ai/AIManager.md)** - Active tier AI processing
- **[CollisionManager](CollisionManager.md)** - Active tier collision
- **[ThreadSystem](../core/ThreadSystem.md)** - WorkerBudget integration
