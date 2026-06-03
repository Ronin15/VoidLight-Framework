# ThreadSystem

**Code:** `include/core/ThreadSystem.hpp`, `include/core/WorkerBudget.hpp`, `src/core/WorkerBudget.cpp`

**Singleton Access:** Use `VoidLight::ThreadSystem::Instance()` to access the thread system.

## Overview

The VoidLight-Framework ThreadSystem is a robust, production-ready thread pool implementation designed for high-performance game development. It provides efficient task-based concurrency with priority-based scheduling, WorkerBudget batch optimization, and comprehensive performance monitoring.

## Architecture Overview

### Core Components Hierarchy

```
ThreadSystem (Singleton)
├── ThreadPool (Worker thread management)
│   ├── TaskQueue (Global priority-based queue)
│   │   ├── Priority Queues [0-4] (Critical → Idle)
│   │   ├── Per-Priority Mutexes (Reduced contention)
│   │   ├── Statistics Tracking
│   │   └── Profiling System
│   └── Worker Threads [N]
│       ├── Priority-Based Task Processing
│       ├── Exponential Backoff
│       └── Exception Handling
└── WorkerBudgetManager (Batch optimization)
    ├── Sequential Execution Model
    ├── Threshold-Based Threading Decisions
    └── Queue Pressure Monitoring
```

### Class Responsibilities

| Class | Responsibility | Key Features |
|-------|---------------|--------------|
| **ThreadSystem** | Singleton API manager | Initialization, cleanup, public interface |
| **ThreadPool** | Worker thread lifecycle | Thread creation, task distribution, graceful shutdown |
| **TaskQueue** | Priority-based queuing | 5 priority levels, per-priority mutexes, capacity management |
| **WorkerBudgetManager** | Batch optimization | Adaptive batch sizing, learned threading thresholds, queue pressure |
| **PrioritizedTask** | Task wrapper | Priority, timing, description, FIFO within priority |

### Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Memory Overhead** | <0.5KB | Minimal static system overhead |
| **CPU Overhead** | <0.05% | Per task processing cost |
| **Throughput** | 15,000-25,000 tasks/sec | Small tasks (100-1000 ops) |
| **Load Balance Efficiency** | 85%+ | Priority-based distribution |
| **Scalability** | 75-95% efficiency | Scales with logical cores (SMT/hyperthreading aware) |

---

## WorkerBudget Integration

### Overview

ThreadSystem integrates with the **WorkerBudgetManager** for adaptive batch optimization. WorkerBudget now combines learned threading thresholds, adaptive batch sizing, and queue-pressure monitoring so managers can scale work without hard-coded thread cutovers.

For detailed WorkerBudget documentation, see [WorkerBudget.md](WorkerBudget.md).

### WorkerBudgetManager API

```cpp
class WorkerBudgetManager {
public:
    static WorkerBudgetManager& Instance();

    // Get cached worker count
    const WorkerBudget& getBudget() const;

    // Get optimal workers for a system (returns all workers if workload > 0)
    size_t getOptimalWorkers(SystemType system, size_t workloadSize);

    // Get optimal batch strategy based on workload and throughput history
    std::pair<size_t, size_t> getBatchStrategy(
        SystemType system,
        size_t workloadSize,
        size_t availableWorkers) const;

    // Report execution result for adaptive tuning
    void reportExecution(
        SystemType system,
        size_t workloadSize,
        bool wasThreaded,
        size_t batchCount,
        double elapsedMs);
};

enum class SystemType {
    AI,
    Particle,
    Pathfinding,
    Event,
    Collision,
    BackgroundSim
};
```

### Threading Decision and Batch Tuning

WorkerBudgetManager now makes threading decisions from learned single-thread timing thresholds and then tunes multi-threaded batch counts with the hill-climb path.

```
┌─────────────────────────────────────────────────────────────────┐
│            Threshold Learning + Batch Tuning                     │
├─────────────────────────────────────────────────────────────────┤
│  1. Learn when single-threaded time crosses the switch-over     │
│  2. Use hysteresis so modes do not flap near the threshold      │
│  3. If threaded, tune batch count from multi-thread throughput  │
│  4. Converge to stable batch sizing for current hardware        │
│                                                                 │
│  Queue pressure can still scale work back when required         │
└─────────────────────────────────────────────────────────────────┘
```

**Example:**
```cpp
// AI stays single-threaded until the learned threshold is crossed.
// Once threaded, WorkerBudget tunes batchCount around the current workload.
```

### Queue Pressure Handling

WorkerBudgetManager monitors ThreadSystem queue utilization and adapts batch strategy:

```cpp
static constexpr float QUEUE_PRESSURE_CRITICAL = 0.90f;  // 90% threshold

// When queue pressure > 90%:
// - Reduce batch count to prevent overflow
// - Scale back workers to reduce contention
// - Log warning for diagnostics
```

### Usage Example

```cpp
void AIManager::update(float deltaTime) {
    size_t entityCount = getActiveEntityCount();
    if (entityCount == 0) return;

    // Get optimal batch configuration from WorkerBudget
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    size_t optimalWorkers = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::AI, entityCount);

    auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::AI,
        entityCount,
        optimalWorkers);

    // Submit batches
    auto startTime = std::chrono::steady_clock::now();

    for (size_t i = 0; i < batchCount; ++i) {
        size_t start = i * batchSize;
        size_t end = std::min(start + batchSize, entityCount);

        ThreadSystem::Instance().enqueueTask([this, start, end, deltaTime]() {
            processBatch(start, end, deltaTime);
        }, TaskPriority::High, "AI_Batch");
    }

    // Wait for completion and report metrics
    // ... wait for futures ...

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    budgetMgr.reportExecution(
        VoidLight::SystemType::AI,
        entityCount,
        true,
        batchCount,
        std::chrono::duration<double, std::milli>(elapsed).count());
}
```

---

## Task Priority System

### Priority Levels

```cpp
enum class TaskPriority : uint8_t {
    Critical = 0,   // Must execute ASAP (rendering, input handling)
    High = 1,       // Important tasks (physics, animation)
    Normal = 2,     // Default priority for most tasks
    Low = 3,        // Background tasks (asset loading)
    Idle = 4        // Only execute when nothing else is pending
};
```

### Priority Guidelines

| Priority | Use Case | Examples | Queue Behavior |
|----------|----------|----------|----------------|
| **Critical** | Mission-critical operations | Input handling, rendering pipeline | Immediate processing, notify all threads |
| **High** | Important game operations | Physics updates, combat AI | High priority processing, notify all threads |
| **Normal** | Standard game logic | Entity updates, standard AI | Default processing, single notification |
| **Low** | Background operations | Asset loading, cleanup | Lower priority, single notification |
| **Idle** | Maintenance tasks | Memory cleanup, cache optimization | Lowest priority, execute when idle |

---

## Quick Start

### Basic Initialization

```cpp
#include "core/ThreadSystem.hpp"
using namespace VoidLight-Framework;

// Initialize with default settings (recommended)
if (!ThreadSystem::Instance().init()) {
    THREADSYSTEM_ERROR("Failed to initialize ThreadSystem!");
    return false;
}

// Verify initialization
unsigned int threads = ThreadSystem::Instance().getThreadCount();
THREADSYSTEM_INFO("ThreadSystem initialized with " + std::to_string(threads) + " threads");
```

### Basic Task Submission

```cpp
// Simple fire-and-forget task
ThreadSystem::Instance().enqueueTask([]() {
    processGameLogic();
}, TaskPriority::Normal, "Game Logic Update");

// Task with return value
auto future = ThreadSystem::Instance().enqueueTaskWithResult([](int value) -> int {
    return calculateComplexValue(value);
}, TaskPriority::High, "Complex Calculation", 42);

// Retrieve result (blocks until complete)
int result = future.get();
```

### Batch Processing with WorkerBudget

```cpp
void processEntitiesWithBudget(std::vector<Entity*>& entities, float deltaTime) {
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::AI,
        entities.size(),
        budgetMgr.getOptimalWorkers(VoidLight::SystemType::AI, entities.size()));

    for (size_t i = 0; i < batchCount; ++i) {
        size_t start = i * batchSize;
        size_t end = std::min(start + batchSize, entities.size());

        ThreadSystem::Instance().enqueueTask([&entities, start, end, deltaTime]() {
            for (size_t j = start; j < end; ++j) {
                entities[j]->update(deltaTime);
            }
        }, TaskPriority::Normal, "Entity Batch");
    }
}
```

---

## API Reference

### Core Initialization Methods

```cpp
class ThreadSystem {
public:
    // Singleton access
    static ThreadSystem& Instance();
    static bool Exists();  // Returns false if shutdown

    // Initialization and cleanup
    bool init(size_t queueCapacity = DEFAULT_QUEUE_CAPACITY,
              unsigned int customThreadCount = 0,
              bool enableProfiling = false);
    void clean();  // Graceful shutdown with pending task logging

    // System status
    bool isShutdown() const;
    unsigned int getThreadCount() const;
    int64_t getTimeSinceLastEnqueue() const;  // Low-activity detection (ms)
};
```

### Task Management Methods

```cpp
// Basic task submission
void enqueueTask(std::function<void()> task,
                 TaskPriority priority = TaskPriority::Normal,
                 const std::string& description = "");

// Task with result
template<class F, class... Args>
auto enqueueTaskWithResult(F&& f,
                          TaskPriority priority = TaskPriority::Normal,
                          const std::string& description = "",
                          Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>;
```

### Queue Management Methods

```cpp
// Queue status
bool isBusy() const;
size_t getQueueSize() const;
size_t getQueueCapacity() const;
bool reserveQueueCapacity(size_t capacity);

// Statistics
size_t getTotalTasksProcessed() const;
size_t getTotalTasksEnqueued() const;

// Debug and profiling
void setDebugLogging(bool enable);
bool isDebugLoggingEnabled() const;
```

---

## Engine Integration

### Manager Integration Pattern

All managers follow the same pattern for WorkerBudget integration:

```cpp
void Manager::update(float deltaTime) {
    size_t workloadSize = getWorkloadSize();
    if (workloadSize == 0) return;

    // 1. Get optimal configuration from WorkerBudget
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    size_t optimalWorkers = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::MyType, workloadSize);

    auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::MyType,
        workloadSize,
        optimalWorkers);

    // 2. Submit batches
    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::future<void>> futures;

    for (size_t i = 0; i < batchCount; ++i) {
        // ... submit batch tasks ...
    }

    // 3. Wait for completion
    for (auto& f : futures) f.wait();

    // 4. Report metrics for adaptive tuning
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    budgetMgr.reportExecution(
        VoidLight::SystemType::MyType,
        workloadSize,
        true,
        batchCount,
        std::chrono::duration<double, std::milli>(elapsed).count());
}
```

### Current Manager Integrations

| Manager | SystemType | Threading Threshold |
|---------|------------|---------------------|
| AIManager | AI | 100+ entities |
| CollisionManager | Collision | 500+ bodies (broadphase), 100+ pairs (narrowphase) |
| ParticleManager | Particle | 500+ particles |
| EventManager | Event | Deferred-queue workload dependent |
| PathfinderManager | Pathfinding | 10+ requests |
| BackgroundSimulationManager | BackgroundSim | Tier workload dependent |

---

## Performance Optimization

### Best Practices

#### Optimal Usage Patterns

```cpp
// 1. Use WorkerBudget for adaptive batching
auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(...);

// 2. Use appropriate priorities
ThreadSystem::Instance().enqueueTask(criticalTask, TaskPriority::Critical);
ThreadSystem::Instance().enqueueTask(backgroundTask, TaskPriority::Low);

// 3. Report metrics for adaptive tuning
budgetMgr.reportExecution(systemType, itemCount, true, batchCount, elapsedMs);
```

#### Anti-Patterns to Avoid

```cpp
// Don't create too many tiny tasks
for (int i = 0; i < 100000; ++i) {
    ThreadSystem::Instance().enqueueTask([=]() {
        simpleOperation(i);  // Overhead > benefit
    });
}

// Don't block worker threads
ThreadSystem::Instance().enqueueTask([]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Wastes worker
});
```

### Performance Guidelines

| Workload Size | Recommendation | Expected Efficiency |
|---------------|----------------|-------------------|
| **1-100 tasks** | Single batch or sequential | 70-85% |
| **100-1,000 tasks** | Worker-count batches | 85-90% |
| **1,000+ tasks** | WorkerBudget adaptive batching | 90%+ |
| **10,000+ tasks** | WorkerBudget with threshold learning + batch tuning | 95%+ |

---

## Thread Safety

### Safe Patterns

```cpp
// Thread-safe singleton access
auto& threadSystem = ThreadSystem::Instance();

// Atomic operations for shared state
std::atomic<int> sharedCounter{0};
ThreadSystem::Instance().enqueueTask([&]() {
    sharedCounter.fetch_add(1, std::memory_order_relaxed);
});

// Mutex-protected critical sections
std::mutex dataMutex;
std::vector<int> sharedData;

ThreadSystem::Instance().enqueueTask([&]() {
    std::lock_guard<std::mutex> lock(dataMutex);
    sharedData.push_back(42);
});
```

### Thread-Safe Operations

| Operation | Thread Safety | Notes |
|-----------|---------------|-------|
| `enqueueTask()` | Fully thread-safe | Can be called from any thread |
| `enqueueTaskWithResult()` | Fully thread-safe | Returns thread-safe future |
| `getQueueSize()` | Thread-safe read | May be slightly outdated |
| `isBusy()` | Thread-safe read | Atomic operation |
| `clean()` | Single thread only | Call only during shutdown |

---

## ThreadSanitizer (TSAN) Support

### Build Configuration

```bash
# ThreadSanitizer build (mutually exclusive with AddressSanitizer)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
  -DUSE_MOLD_LINKER=OFF && \
ninja -C build
```

### Running with TSAN Suppressions

```bash
# Run with TSAN suppression file
export TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"
./bin/debug/VoidLight_Template
```

ThreadSystem has documented benign race patterns (lock-free statistics, performance counters) suppressed via `tests/tsan_suppressions.txt`.

---

## Shutdown and Low-Activity Detection

### Graceful Shutdown

```cpp
void GameEngine::cleanup() {
    // Wait for critical tasks to complete
    while (ThreadSystem::Instance().isBusy()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Clean shutdown
    ThreadSystem::Instance().clean();
}
```

### Low-Activity Detection

```cpp
// Query time since last activity
int64_t idleTime = ThreadSystem::Instance().getTimeSinceLastEnqueue();

if (idleTime > 5000) {  // 5 seconds idle
    // Reduce update frequency or enter low-power mode
}
```

---

## Troubleshooting

### Common Issues

| Issue | Symptoms | Solution |
|-------|----------|----------|
| **High queue utilization** | Tasks queuing up | Check batch sizes, reduce task granularity |
| **Poor throughput** | Low items/ms | Ensure `reportExecution()` is called with correct threaded/batch data |
| **Memory growth** | Increasing memory | Reserve queue capacity, use move semantics |
| **Deadlocks** | System hanging | Avoid blocking operations in tasks |

---

## See Also

**Core Systems:**
- [GameEngine](GameEngine.md) - Central engine coordination
- [WorkerBudget](WorkerBudget.md) - Detailed WorkerBudget documentation
- [TimestepManager](TimestepManager.md) - Fixed timestep timing

**Manager Integration:**
- [AIManager](../ai/AIManager.md) - AI system threading
- [ParticleManager](../managers/ParticleManager.md) - Particle system integration
- [CollisionManager](../managers/CollisionManager.md) - Collision detection threading

**Performance:**
- [SIMDMath](../utils/SIMDMath.md) - SIMD optimizations
