# WorkerBudget

**Code:** `include/core/WorkerBudget.hpp`, `src/core/WorkerBudget.cpp`

## Overview

`WorkerBudgetManager` is the engine's central threading decision system. It combines:

- learned single-thread switch-over thresholds
- hysteresis for mode stability
- batch tuning for multi-threaded work
- per-system reset during state transitions

The threading decision model is more than a simple throughput-only single-vs-multi cutoff.

## Core API

```cpp
ThreadingDecision shouldUseThreading(SystemType system, size_t workloadSize);
std::pair<size_t, size_t> getBatchStrategy(SystemType system,
                                           size_t workloadSize,
                                           size_t optimalWorkers);
void reportExecution(SystemType system, size_t workloadSize,
                     bool wasThreaded, size_t batchCount, double totalTimeMs);
void prepareForStateTransition();
```

## System Types

- `AI`
- `Particle`
- `Pathfinding`
- `Event`
- `Collision`
- `BackgroundSim`

## Current Model

### Threshold learning

Single-threaded samples teach the manager where the switch-over point should be for each subsystem. A minimum of `MIN_LEARNING_SAMPLES` (10) single-threaded measurements must accumulate before a threshold is written, preventing cold-start cache spikes from triggering premature threading decisions on the first few frames.

### Hysteresis

Once threading becomes active, workload must drop below the low band before the system flips back to single-threaded mode.

### Batch tuning

Multi-threaded runs feed the batch multiplier hill-climb through `reportExecution()`.

## Usage Pattern

```cpp
auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
auto decision = budgetMgr.shouldUseThreading(SystemType::AI, count);

size_t batchCount = 1;
if (decision.shouldThread) {
    auto workers = budgetMgr.getOptimalWorkers(SystemType::AI, count);
    auto [recommendedBatchCount, batchSize] =
        budgetMgr.getBatchStrategy(SystemType::AI, count, workers);
    batchCount = recommendedBatchCount;
    // dispatch worker batches...
}

budgetMgr.reportExecution(SystemType::AI, count,
                          decision.shouldThread, batchCount, elapsedMs);
```

## State Transition

Call `prepareForStateTransition()` during GameState teardown so learned thresholds and smoothing data do not leak across states with unrelated workloads.
