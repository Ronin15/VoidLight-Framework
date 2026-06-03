# VoidLight Engine Test Framework

This document provides a comprehensive guide to the testing framework used in the VoidLight Engine project. All tests use the Boost Test Framework for consistency and are organized by component.

**Current Test Coverage:** 80 core source-controlled test executables plus 8 GPU-specific test targets covering AI systems, AI behaviors, behavior state transitions, crowd-query runtime behavior, UI functionality, core systems, collision detection, pathfinding, WorkerBudget coordination, event management, particle systems, buffer management, rendering pipeline, SIMD correctness, camera systems, input handling, loading-state helpers, manager runtime behavior, frame profiling, GameTimeManager simulation, controller systems, entity state management, entity data management, NPC memory system, background simulation, EDM integration tests, resource integration coverage, GPU rendering subsystem, GPU frame timing benchmarks, and utility components with both functional validation and performance benchmarking.

## Test Suites Overview

The VoidLight Engine has the following test suites:

1. **AI System Tests**
   - AI Optimization Tests: Verify performance optimizations in the AI system
   - Thread-Safe AI Tests: Validate thread safety of the AI management system
   - Thread-Safe AI Integration Tests: Test integration of AI components with threading
   - AI Benchmark Tests: Measure performance characteristics and scaling capabilities
   - Behavior Functionality Tests: Comprehensive validation of all 8 AI behaviors, modes, and behavior transitions
   - Behavior Transition Tests: Validate state preservation during behavior switches (e.g., Guard→Attack)
   - Crowd Runtime Tests: Validate crowd cache reuse, stat accounting, invalidation, and thread-local buffer behavior
   - ThreadSystem Queue Load Tests: Defensive monitoring to prevent ThreadSystem overload
   - AIManager EDM Integration Tests: Validate AIManager's integration with EntityDataManager (sparse behavior vector, batch processing, state transitions)

2. **UI System Tests**
   - UIManagerFunctionalTests: Comprehensive UI component creation, management, and interaction

3. **Core Systems Tests**
   - Buffer Reuse Tests: Validate memory allocation patterns and buffer reuse across frames
   - Camera Tests: Validate camera world/screen coordinate transformations
   - Frame Profiler Tests: Validate overlay toggling, suppression, hitch attribution, and GPU swapchain-wait exclusion
   - InputManager Tests: Validate input handling and coordinate conversion
   - InputManager Command Tests: Validate action command bindings, rebinding, persistence, and command edge state
   - Loading State Tests: Validate `LoadingState` configuration/reset helpers and non-blocking async-loading primitives
   - Rendering Pipeline Tests: Source-structure validation for render-flow ownership and GPU pass boundaries
   - Save Manager Tests: Validate save/load functionality with directory creation and file operations
   - Thread System Tests: Verify multi-threading capabilities and priority scheduling
   - ThreadSystem Load Monitoring: Defensive tests ensuring AI doesn't overwhelm 4096 task limit
   - Event Manager Tests: Validate event handling and integration with threading
   - Event Types Tests: Test specific event type implementations (Weather, Scene Change, NPC Spawn)
   - Weather Event Tests: Focused tests for weather event functionality
   - Event Manager Scaling Benchmark: Performance testing for event system scalability
   - Event Coordination Integration Tests: Multi-system event coordination and synchronization
   - Particle Manager Tests: Comprehensive particle system validation covering core functionality, weather integration, performance, and threading

4. **Collision System Tests**
   - AABB Tests: Validate axis-aligned bounding box intersection, containment, and closest point calculations
   - SpatialHash Tests: Test spatial hash insertion, removal, updating, and querying with deduplication
   - Collision Performance Tests: Benchmark insertion, query, and update operations with up to 10K entities
   - Collision Stress Tests: High-density collision detection and boundary condition testing
   - AI Collision Integration Tests: Integration tests for AI pathfinding with collision system
   - CollisionManager EDM Integration Tests: Validate CollisionManager's integration with EntityDataManager (active tier filtering, dual index semantics, static/dynamic separation)

5. **Pathfinding System Tests**
   - PathfindingGrid Tests: Validate A* pathfinding algorithm with grid coordinate conversion
   - Pathfinding Configuration Tests: Test diagonal movement, cost configuration, and iteration limits
   - Dynamic Weight Tests: Validate weight-based avoidance areas and path cost modification
   - Pathfinding Performance Tests: Benchmark pathfinding across various grid sizes (50x50 to 200x200)
   - Edge Case Tests: Test blocked start/goal positions, extreme distances, and nearest open cell finding
   - PathfinderManager & AIManager Contention Tests: Integration tests for WorkerBudget coordination under heavy load

6. **SIMD & Performance Tests**
   - SIMD Correctness Tests: Cross-platform SIMD correctness validation (SSE2/AVX2/NEON)
   - SIMD Performance Benchmark: Performance validation of SIMD optimizations in AIManager, CollisionManager, ParticleManager
   - Integrated System Benchmark: Full system performance profiling across all major systems

7. **Utility System Tests**
   - JsonReader Tests: RFC 8259 compliant JSON parser validation with comprehensive error handling and type safety testing

8. **GameTimeManager Tests**
   - GameTimeManager Tests: Validate core time advancement, tick system, and time configuration
   - GameTimeManager Calendar Tests: Test fantasy calendar (months, days, year cycles)
   - GameTimeManager Season Tests: Validate seasonal transitions and weather probability changes

9. **Controller Tests**
   - ControllerRegistry Tests: Type-safe controller registration, lifecycle management, batch operations
   - WeatherController Tests: Weather event coordination and state transitions
   - DayNightController Tests: Time period tracking and visual transitions
   - HarvestController Tests: Resource harvest coordination and guard behavior
   - InventoryController Tests: Inventory UI, item interaction behavior, event wiring, hotbar dragging, and inventory slot drag/drop reordering
   - NPCRenderController Tests: Animation state, row/frame selection, and facing behavior
   - CombatController Tests: Controller basics and null-player guard behavior
   - ResourceRenderController Tests: Resource render-controller lifecycle and update behavior
   - SocialController Tests: Social interaction controller lifecycle behavior

10. **Entity State Machine Tests**
    - EntityStateManager Tests: State registration, transitions, lifecycle callbacks (enter/exit/update)

11. **Entity Data Management Tests**
    - EntityDataManager Tests: Data-oriented entity storage, handle validation, tier management
    - SparseSidecar Tests: Validate sparse/dense transient sidecar storage invariants
    - KnockbackSidecar Tests: Validate knockback sidecar lifecycle during destruction and expiry
    - BackgroundSimulationManager Tests: Background entity simulation, tier-based processing, pause/resume

12. **Utility Runtime Tests**
    - Manager Runtime Tests: Direct runtime coverage for SoundManager, FontManager, and TextureManager
    - ResourcePath Tests: Search-path priority, fallback behavior, and resolution rules

13. **EDM Integration Tests**
    - AIManager EDM Integration Tests: Sparse behavior vector, batch processing with EDM indices, state transitions
    - CollisionManager EDM Integration Tests: Active tier filtering, dual index semantics, static/dynamic separation

14. **NPC Memory System Tests**
    - Memory Structure Tests: MemoryEntry size validation, EmotionalState layout, NPCMemoryData alignment
    - Memory Initialization Tests: Memory data allocation, initialization, validity flags
    - Add Memory Tests: Adding memories, inline vs overflow storage, circular buffer behavior
    - Find Memory Tests: Search by type, search by entity, combined searches
    - Emotional State Tests: Emotional decay over time, emotion modification, clamping
    - Combat Event Tests: Recording attacks, damage tracking, combat statistics
    - Location History Tests: Location tracking, circular buffer, history limits
    - Cleanup Tests: Memory clearing, overflow cleanup, state transition handling

15. **GPU System Tests**
    - GPU Types Tests: Vertex struct layouts (SpriteVertex, ColorVertex), UBO alignment validation
    - GPU Pipeline Config Tests: Pipeline configuration factory methods, blend modes
    - GPU Device Tests: Device lifecycle, shader format queries, swapchain format
    - GPU Shader Manager Tests: Shader loading, caching, and platform-native shader path validation (SPIR-V/Metal/DXIL as applicable)
    - GPU Resource Tests: Buffer, texture, transfer buffer, sampler wrappers
    - GPU Vertex Pool Tests: Triple-buffered vertex pool, frame cycling
    - Sprite Batch Tests: Batch recording, vertex data verification
    - GPU Renderer Tests: Full frame flow, pipeline/pool accessors, composite rendering

16. **Tooling Tests**
    - Atlas Tool Tests: Python unittest coverage for sprite extraction/mapping/packing helpers (`python3 -B tests/tools/test_atlas_tool.py`)

**Test Execution Categories:**
- **Core Tests**: Fast functional validation (~4-8 minutes total)
- **Benchmarks**: Performance and scalability testing (~8-20 minutes total)
- **GPU Tests**: SDL3 GPU rendering validation
- **Tooling Tests**: Python tool validation outside the Boost executable list
- **Total Coverage**: 80 core `ALL_TESTS` executables plus 8 GPU-specific test targets with comprehensive automation scripts

## Running Tests

### Available Test Scripts

Each test suite has dedicated scripts in the `tests/test_scripts/` directory:

#### Linux/macOS
```bash
# Core functionality tests (fast execution)
./tests/test_scripts/run_thread_tests.sh                # Thread system tests
./tests/test_scripts/run_buffer_utilization_tests.sh    # WorkerBudget buffer thread utilization tests
./tests/test_scripts/run_thread_safe_ai_tests.sh        # Thread-safe AI tests
./tests/test_scripts/run_thread_safe_ai_integration_tests.sh  # Thread-safe AI integration tests
./tests/test_scripts/run_ai_optimization_tests.sh       # AI optimization tests
./tests/test_scripts/run_behavior_functionality_tests.sh # Comprehensive AI behavior validation tests
./tests/test_scripts/run_save_tests.sh                  # Save manager and BinarySerializer tests
./tests/test_scripts/run_event_tests.sh                 # Event manager tests
./tests/test_scripts/run_json_reader_tests.sh           # JSON parser validation tests
./tests/test_scripts/run_collision_tests.sh             # Collision system and spatial hash tests
./tests/test_scripts/run_pathfinding_tests.sh           # Pathfinding algorithm and grid tests
./tests/test_scripts/run_pathfinder_ai_contention_tests.sh  # PathfinderManager & AIManager WorkerBudget coordination tests
./tests/test_scripts/run_pathfinder_manager_tests.sh      # PathfinderManager EDM integration and lifecycle tests
./tests/test_scripts/run_game_time_tests.sh               # GameTimeManager tests
./tests/test_scripts/run_controller_tests.sh              # Controller tests (Registry, Weather, DayNight)
./tests/test_scripts/run_projectile_manager_tests.sh      # ProjectileManager EDM integration and lifecycle tests
./tests/test_scripts/run_entity_state_manager_tests.sh    # Entity state machine tests
./tests/test_scripts/run_entity_data_manager_tests.sh     # EntityDataManager and BackgroundSimulationManager tests
./tests/test_scripts/run_ai_manager_edm_integration_tests.sh      # AIManager EDM integration tests
./tests/test_scripts/run_collision_manager_edm_integration_tests.sh  # CollisionManager EDM integration tests
./tests/test_scripts/run_npc_memory_tests.sh                      # NPC memory system tests

# Performance scaling benchmarks (slow execution)
./tests/test_scripts/run_event_scaling_benchmark.sh     # Event manager scaling benchmark
./tests/test_scripts/run_ai_benchmark.sh                # AI scaling benchmark with realistic automatic threading
./tests/test_scripts/run_collision_benchmark.sh         # Collision system performance benchmarks
./tests/test_scripts/run_pathfinder_benchmark.sh        # Pathfinder system performance benchmarks
./tests/test_scripts/run_projectile_benchmark.sh        # Projectile scaling and SIMD throughput benchmarks

# Run all tests
./tests/test_scripts/run_all_tests.sh                   # Run all test scripts sequentially

# AI benchmark test examples with automatic threading
./tests/test_scripts/run_ai_benchmark.sh                                   # Full realistic benchmark suite
./tests/test_scripts/run_ai_benchmark.sh --realistic-only                  # Clean realistic performance tests
./tests/test_scripts/run_ai_benchmark.sh --stress-test                     # 100K entity stress test only
./tests/test_scripts/run_ai_benchmark.sh --threshold-test                  # Threading threshold validation (200 entities)

# Save manager test examples with BinarySerializer
./tests/test_scripts/run_save_tests.sh --save-test                         # Basic save/load operations
./tests/test_scripts/run_save_tests.sh --serialization-test                # New BinarySerializer system tests
./tests/test_scripts/run_save_tests.sh --performance-test                  # Serialization performance benchmarks
./tests/test_scripts/run_save_tests.sh --integration-test                  # BinarySerializer integration tests
./tests/test_scripts/run_save_tests.sh --error-test                        # Error handling tests
./tests/test_scripts/run_save_tests.sh --verbose                           # Detailed test output

# JSON reader test examples
./tests/test_scripts/run_json_reader_tests.sh --parse-test                 # Basic JSON parsing validation
./tests/test_scripts/run_json_reader_tests.sh --error-test                 # Error handling and malformed JSON tests
./tests/test_scripts/run_json_reader_tests.sh --file-test                  # File loading functionality tests
./tests/test_scripts/run_json_reader_tests.sh --game-test                  # Game item data parsing tests
./tests/test_scripts/run_json_reader_tests.sh --verbose                    # Detailed test output with all test cases
```

#### Windows
```
# Core functionality tests (fast execution)
tests/test_scripts/run_thread_tests.bat                 # Thread system tests
tests/test_scripts/run_thread_safe_ai_tests.bat         # Thread-safe AI tests
tests/test_scripts/run_thread_safe_ai_integration_tests.bat  # Thread-safe AI integration tests
tests/test_scripts/run_ai_optimization_tests.bat        # AI optimization tests
tests/test_scripts/run_behavior_functionality_tests.bat # Comprehensive AI behavior validation tests
tests/test_scripts/run_ai_benchmark.bat                 # AI scaling benchmark with realistic automatic threading
tests/test_scripts/run_save_tests.bat                   # Save manager and BinarySerializer tests
tests/test_scripts/run_event_tests.bat                  # Event manager tests
tests/test_scripts/run_collision_tests.bat              # Collision system and spatial hash tests
tests/test_scripts/run_pathfinding_tests.bat            # Pathfinding algorithm and grid tests
tests/test_scripts/run_pathfinder_manager_tests.bat     # PathfinderManager EDM integration and lifecycle tests
tests/test_scripts/run_game_time_tests.bat              # GameTimeManager tests
tests/test_scripts/run_controller_tests.bat             # Controller tests (Registry, Weather, DayNight)
tests/test_scripts/run_projectile_manager_tests.bat     # ProjectileManager EDM integration and lifecycle tests
tests/test_scripts/run_entity_state_manager_tests.bat   # Entity state machine tests
tests/test_scripts/run_entity_data_manager_tests.bat    # EntityDataManager and BackgroundSimulationManager tests
tests/test_scripts/run_ai_manager_edm_integration_tests.bat      # AIManager EDM integration tests
tests/test_scripts/run_collision_manager_edm_integration_tests.bat  # CollisionManager EDM integration tests
tests/test_scripts/run_npc_memory_tests.bat                       # NPC memory system tests

tests/test_scripts/run_json_reader_tests.bat            # JSON parser validation tests

# Performance scaling benchmarks (slow execution)
tests/test_scripts/run_event_scaling_benchmark.bat      # Event manager scaling benchmark
tests/test_scripts/run_ai_benchmark.bat                 # AI scaling benchmark
tests/test_scripts/run_collision_benchmark.bat          # Collision system performance benchmarks
tests/test_scripts/run_pathfinder_benchmark.bat         # Pathfinder system performance benchmarks
tests/test_scripts/run_projectile_benchmark.bat         # Projectile scaling and SIMD throughput benchmarks

# Run all tests
tests/test_scripts/run_all_tests.bat                    # Run all test scripts sequentially

# Save manager test examples with BinarySerializer
tests/test_scripts/run_save_tests.bat --save-test                          # Basic save/load operations
tests/test_scripts/run_save_tests.bat --serialization-test                 # New BinarySerializer system tests
tests/test_scripts/run_save_tests.bat --performance-test                   # Serialization performance benchmarks
tests/test_scripts/run_save_tests.bat --integration-test                   # BinarySerializer integration tests
tests/test_scripts/run_save_tests.bat --error-test                         # Error handling tests
tests/test_scripts/run_save_tests.bat --verbose                            # Detailed test output
```

### Test Execution Control

The `tests/test_scripts/run_all_tests.sh` script provides flexible execution control:

#### Test Category Options
| Option | Description | Duration |
|--------|-------------|----------|
| `--core-only` | Run only core functionality tests | ~2-5 minutes |
| `--benchmarks-only` | Run only performance benchmarks (AI, Event, Collision, Pathfinder, GPU, SIMD, integrated) | ~8-20 minutes |
| `--no-benchmarks` | Run core tests but skip benchmarks | ~2-5 minutes |
| *(default)* | Run all tests sequentially | ~10-25 minutes |

#### Examples
```bash
# Quick validation during development
./tests/test_scripts/run_all_tests.sh --core-only

# Skip slow benchmarks in CI
./tests/test_scripts/run_all_tests.sh --no-benchmarks

# Performance testing only
./tests/test_scripts/run_all_tests.sh --benchmarks-only --verbose

# Complete test suite
./tests/test_scripts/run_all_tests.sh
```

### Common Command-Line Options

Many individual test scripts support these options, but support is script-specific and should be confirmed with `--help`:

| Option | Description |
|--------|-------------|
| `--verbose` | Show detailed test output |
| `--release` | Run tests in release mode when supported by that script |
| `--clean` | Clean test artifacts before building when supported by that script |
| `--help` | Show help message for the script |

Special options in current source-controlled scripts:
- `--extreme` for AI benchmark (runs extended benchmarks)
- `--verbose` for scaling benchmarks (shows detailed performance metrics)

**Test Execution Strategy:**
The script executes tests in optimal order for efficient development workflow:
1. **Core functionality tests** (fast execution, ~5-30 seconds each)
2. **Performance scaling benchmarks** (slow execution, 1-5+ minutes each)

This ordering provides quick feedback on basic functionality before running time-intensive performance tests.

## Test Output

Test results are saved in the `test_results` directory:

- `ai_optimization_tests_output.txt` - Output from AI optimization tests
- `event_scaling_benchmark_output.txt` - Output from EventManager scaling benchmark
- `ai_optimization_tests_performance_metrics.txt` - Performance metrics from optimization tests
- `thread_safe_ai_test_output.txt` - Output from thread-safe AI tests
- `thread_safe_ai_performance_metrics.txt` - Performance metrics from thread-safe AI tests
- `ai_scaling_benchmark_[timestamp].txt` - AI scaling benchmark results
- `save_test_output.txt` - Output from save manager tests
- `thread_test_output.txt` - Output from thread system tests
- `event_test_output.txt` - Output from event manager tests
- `event_types_test_output.txt` - Output from event types tests
- `weather_event_test_output.txt` - Output from weather event tests

When using the `tests/test_scripts/run_all_tests` scripts, combined results are also saved:

- `combined/all_tests_results.txt` - Summary of all test script results

## Test Implementation Details

### AI Optimization Tests

Located in `AIOptimizationTest.cpp`, these tests verify:

1. **Entity Component Caching**: Tests caching mechanisms for faster entity-behavior lookups
2. **Batch Processing**: Validates efficient batch processing of entities with similar behaviors
3. **Early Exit Conditions**: Tests optimizations that skip unnecessary updates
4. **Message Queue System**: Verifies batched message processing for efficient AI communication
5. **Priority-Based Scheduling**: Tests the task priority system integration with AI behaviors

### Thread-Safe AI Tests

Located in `ThreadSafeAIManagerTest.cpp`, these tests verify:

1. **Thread-Safe Behavior Registration**: Tests concurrent registration of behaviors
2. **Thread-Safe Entity Assignment**: Validates behavior assignment from multiple threads
3. **Concurrent Behavior Processing**: Tests running AI behaviors across multiple threads
4. **Thread-Safe Cache Invalidation**: Validates optimization cache in multi-threaded context
5. **Thread-Safe Messaging**: Tests message queuing with concurrent access

Special considerations for thread-safety tests:
- Use atomic operations with proper synchronization
- Disable threading before cleanup to prevent segmentation faults
- Allow time between operations for thread synchronization
- Use timeout when waiting for futures to prevent hanging

### Behavior Functionality Tests

Located in `BehaviorFunctionalityTest.cpp`, these tests comprehensively validate all 8 AI behaviors:

1. **Behavior Registration and Assignment**: Tests that all behaviors and variants are registered
2. **Idle Behavior Testing**: Validates stationary, sway, turn, and fidget modes
3. **Movement Behavior Testing**: Tests Wander, Chase, and Flee behaviors with path following
4. **Complex Behavior Testing**: Validates Follow, Guard, and Attack behaviors
5. **Message System Testing**: Tests behavior-specific and broadcast messages
6. **Behavior Mode Testing**: Tests all behavior variants (FollowClose, AttackMelee, etc.)
7. **Behavior Transitions**: Critical tests for state preservation during behavior switches
8. **Performance Testing**: Large-scale entity testing and memory management validation

### Behavior Transition Tests

Located in `BehaviorFunctionalityTest.cpp` (BehaviorTransitionTests suite), these tests specifically validate that behavior state is preserved during transitions. This addresses a critical bug where `clean()` was called after `init()`, causing new behavior state to be wiped.

**Test Cases:**

1. **TestGuardToAttackTransitionStatePreserved**: Tests the specific Guard→Attack transition that was previously buggy. Verifies:
   - BehaviorData remains valid after transition
   - BehaviorData is properly initialized after transition
   - Entity can continue executing behaviors after transition

2. **TestAllBehaviorTransitionsPreserveState**: Tests all behavior pair transitions:
   - Idle→Wander, Wander→Chase, Chase→Attack, Attack→Flee
   - Flee→Guard, Guard→Attack (critical path), Attack→Follow, Follow→Idle
   - Verifies `isValid()` and `isInitialized()` after each transition

3. **TestRapidBehaviorTransitionsStability**: Stress tests rapid behavior switching:
   - Cycles through all 7 behaviors 3 times
   - Single update between each transition
   - Validates no state corruption under rapid transitions

**What These Tests Catch:**
- Init/clean order bugs: When `clean()` is called after `init()`, wiping new behavior state
- BehaviorData initialization failures during transitions
- State corruption from concurrent behavior assignment
- Missing `initBehaviorData()` calls in behavior assignment paths

**Running Transition Tests:**
```bash
# Run all transition tests
./bin/debug/behavior_functionality_tests --run_test="BehaviorTransitionTests/*"

# Run specific transition test
./bin/debug/behavior_functionality_tests --run_test="BehaviorTransitionTests/TestGuardToAttackTransitionStatePreserved"
```

### AI Benchmark Tests

Located in `AIScalingBenchmark.cpp`, these tests measure realistic performance characteristics:

1. **Realistic Performance Testing**: Tests automatic threading behavior across key entity counts
   - 100 entities: Validates single-threaded baseline (~170K updates/sec)
   - 200+ entities: Validates automatic threading activation (~750K updates/sec)
   - 1000+ entities: Confirms high-performance threading (~975K updates/sec)
   - 10K entities: Target performance validation (~995K updates/sec, 5.85x improvement)

2. **Realistic Scalability**: Tests clean automatic threading behavior across entity ranges
   - Tests 100-10K entity range with automatic mode selection
   - Validates 200-entity threading threshold effectiveness (4.41x performance jump)
   - Demonstrates consistent high performance scaling (5.29x-5.85x ratios)

3. **Legacy Comparison**: Forced threading modes for comparison with previous benchmarks
   - Single-threaded forced mode for baseline comparison
   - Multi-threaded forced mode for maximum performance comparison

4. **Stress Testing**: 100K entity extreme load testing
   - Validates system stability under extreme stress
   - Tests WorkerBudget system coordination
   - Confirms queue capacity handling (4096 tasks)

**Key Performance Targets:**
- 100 entities: Single-threaded baseline (~170K updates/sec)
- 200 entities: Automatic threading activation (~750K updates/sec)
- 1000 entities: High threading performance (~975K updates/sec)
- 10K entities: Target performance achieved (~995K updates/sec, 5.85x improvement)
- 100K entities: Stress test validation (2.2M+ updates/sec)

### UI Functional Tests

The source-controlled UI test surface is currently the `ui_manager_functional_tests` executable and its wrapper script.

These tests focus on:
1. **Component Lifecycle**: Creation, lookup, update, and removal
2. **Layout and Positioning**: Alignment helpers and positioning modes
3. **Interaction Behavior**: Visibility, enable/disable, and UI state handling
4. **Rendering Safety**: Functional UI flows without relying on an external stress harness


### Save Manager Tests

Located in `SaveManagerTests.cpp` with supporting `MockPlayer` class, these tests verify the **BinarySerializer integration** that replaces Boost serialization:

1. **BinarySerializer Integration**: Tests the new fast, header-only serialization system:
   - Tests `BinarySerial::Writer` and `BinarySerial::Reader` classes
   - Verifies smart pointer-based memory management
   - Tests convenience functions `saveToFile()` and `loadFromFile()`
   - Validates cross-platform binary compatibility

2. **Data Serialization**: Tests serialization of game objects using the new system:
   - `TestNewSerializationSystem`: Vector2D and custom object serialization
   - `TestBinaryWriterReader`: Primitive types, strings, and direct Writer/Reader usage
   - `TestVectorSerialization`: Container serialization with safety checks
   - `TestBinarySerializerIntegration`: End-to-end serialization testing

3. **Performance Testing**: `TestPerformanceComparison` benchmarks:
   - 100 serialization operations timing (typically ~13-25ms)
   - Memory usage validation
   - Cross-platform performance verification

4. **File Operations**: Tests using BinarySerializer for save file management:
   - `TestSaveAndLoad`: Basic save/load operations with MockPlayer
   - `TestSlotOperations`: Multiple file handling and cleanup
   - File existence validation and proper error handling

5. **Error Handling**: `TestErrorHandling` covers:
   - Non-existent file handling
   - Invalid file path recovery
   - Corrupted data detection through the BinarySerializer safety checks

**Key Features Tested:**
- **Fast Performance**: 10x faster than Boost for primitives, 4x faster for strings
- **Smart Pointer Safety**: Automatic memory management with `std::shared_ptr` and `std::unique_ptr`
- **Type Safety**: Compile-time checks for trivially copyable types
- **Logging Integration**: Proper integration with Forge logging system (`SAVEGAME_*` macros)
- **Cross-Platform**: Header-only solution works on Windows, macOS, and Linux

### Thread System Tests

Located in `ThreadSystemTests.cpp`, these tests verify:

1. **Task Scheduling**: Tests scheduling and execution of tasks
2. **Thread Safety**: Tests synchronization mechanisms
3. **Performance**: Tests scaling with different numbers of threads
4. **Error Handling**: Tests recovery from failed tasks
5. **Priority System**: Tests the task priority levels (Critical, High, Normal, Low, Idle)
6. **Priority Scheduling**: Verifies that higher priority tasks execute before lower priority ones

### WorkerBudget Buffer Utilization Tests

Located in `BufferUtilizationTest.cpp`, these tests verify the intelligent buffer thread utilization system:

1. **Hardware Tier Classification**: Tests allocation strategies across ultra low-end to very high-end systems
2. **Dynamic Buffer Scaling**: Validates workload-based buffer utilization (AI: >1000 entities, Events: >100 events)
3. **Conservative Burst Strategy**: Tests systems using maximum 50% of base allocation from buffer
4. **Resource Allocation Logic**: Verifies no over-allocation and proper fallback behavior
5. **Graceful Degradation**: Tests single-threaded fallback on resource-constrained systems

**Test Coverage:**
- **Ultra Low-End (1 worker)**: GameLoop only, AI/Events/Particles single-threaded
- **Low-End (3-4 workers)**: Conservative allocation with limited buffer
- **Target Minimum (7 workers)**: Optimal allocation (GameLoop: 1, AI: 3, Particles: 1, Buffer: 2)
- **High-End (12+ workers)**: Full buffer utilization for burst capacity

**Validation Examples:**
```bash
# 12-worker system test results:
Base allocations - GameLoop: 2, AI: 6, Events: 3, Buffer: 1
Low workload (500 entities): 6 workers    # Uses base allocation
High workload (5000 entities): 7 workers  # Uses base + buffer

# 16-worker system test results:
Very high workload burst: 10 workers      # AI gets 8 base + 2 buffer
```

The tests ensure the WorkerBudget system provides guaranteed minimum performance while enabling intelligent scaling for high workloads without resource conflicts.

### Event Manager Tests

Located in `events/EventManagerTest.cpp`, `events/EventTypesTest.cpp`, `events/WeatherEventTest.cpp`, and `EventManagerScalingBenchmark.cpp`, these tests verify:

1. **Event Registration**: Tests registering different types of events with the EventManager
2. **Event Conditions**: Tests condition-based event triggering
3. **Event Execution**: Validates proper execution of event sequences
4. **Thread-Safe Processing**: Tests concurrent event processing using ThreadSystem
5. **Message System**: Tests the event messaging system for communication between events
6. **Priority-Based Scheduling**: Tests task priority integration with event processing
7. **NPCSpawnEvent Integration**: Tests NPC spawning functionality with mocked dependencies
8. **Performance Scaling**: Comprehensive benchmarking from small to extreme scales (see EventManager Scaling Benchmark below)

#### Event Manager Test Details

**EventManagerTest.cpp** focuses on EventManager integration:
- Event registration and retrieval
- Event activation/deactivation
- Event messaging system
- Thread-safe event processing
- NPCSpawnEvent integration with EventManager

**EventTypesTest.cpp** tests specific event type implementations:
- WeatherEvent creation and parameter setting
- SceneChangeEvent functionality
- NPCSpawnEvent creation, spawn parameters, conditions, and limits
- EventFactory event creation methods
- Event sequences and cooldown functionality

**WeatherEventTest.cpp** provides focused weather event testing:
- Weather state transitions
- Particle effect integration
- Sound effect integration
- Time-based weather conditions

#### Mock Infrastructure for NPCSpawnEvent Tests

The NPCSpawnEvent tests use a comprehensive mocking infrastructure to avoid dependencies on the full game engine:

**Mock Components:**
- `MockNPC.hpp/cpp`: Mock NPC class implementing Entity interface without game engine dependencies
- `MockGameEngine.hpp/cpp`: Mock GameEngine providing basic window dimensions for testing
- `NPCSpawnEventTest.cpp`: Test-specific NPCSpawnEvent implementation using mocks

**Mock Features:**
- Complete NPCSpawnEvent functionality testing without real game engine
- Proper Vector2D usage with `getX()/getY()` and `setX()/setY()` methods
- Correct Event base class implementation
- Mock entity creation and management
- Spawn parameter validation and condition testing
- Respawn logic and timer functionality
- Proximity and area-based spawning tests

**Test Coverage:**
- Spawn parameters (count, radius, positioning)
- Spawn conditions (proximity, time of day, custom conditions)
- Spawn limits and counting
- Respawn functionality and timing
- Spawn area management (points, rectangles, circles)
- Entity lifecycle and cleanup
- Message-based spawn requests

#### EventManager Scaling Benchmark

Located in `EventManagerScalingBenchmark.cpp`, this comprehensive performance test validates the EventManager's scalability across various workloads:

**Test Scenarios:**
- **Basic Performance**: Small scale validation (100 events, immediate vs batched)
- **Medium Scale**: Moderate load testing (5,000 events, 25,000 handler calls)
- **Scalability Suite**: Progressive testing from minimal to very large scales
- **Concurrency Test**: Multi-threaded event generation and processing
- **Extreme Scale**: Large simulation testing (100,000 events, 5,000,000 handler calls)

**Performance Metrics Measured:**
- Events per second throughput
- Handler calls per second
- Time per event and per handler call
- Memory usage and cache performance
- Thread safety validation
- Batching vs immediate processing comparison

**Key Features Tested:**
- Handler batching system performance
- `boost::flat_map` storage optimization
- Thread-safe concurrent event queuing
- Lock-free handler execution
- Cache locality optimization through event type grouping

**Running the Scaling Benchmark:**
```bash
# Run just the scaling benchmark
ctest -R EventManagerScaling

# Or run the executable directly for detailed output
./bin/debug/event_manager_scaling_benchmark
```

**Expected Performance (on modern hardware):**
- Small scale: ~540K events/sec
- Medium scale: ~78K events/sec
- Large scale: ~39K events/sec
- Extreme scale: ~7.8K events/sec with 5M handler calls

The benchmark validates that the EventManager can handle large-scale simulations (MMOs, city simulations, real-time strategy games) while maintaining consistent performance characteristics.

### Particle Manager Tests

Located in `particle/` directory, these tests provide comprehensive validation of the ParticleManager system covering core functionality, weather integration, performance benchmarks, and threading safety.

#### Test Suites Overview

**1. Core Tests (`ParticleManagerCoreTest.cpp`)**
**14 test cases** covering basic ParticleManager functionality:
- Initialization and cleanup
- Effect registration and management  
- Particle creation and lifecycle
- Global pause/resume functionality
- Performance statistics
- State transition handling

**2. Weather Integration Tests (`ParticleManagerWeatherTest.cpp`)**
**9 test cases** covering weather system integration:
- Weather effect triggering (Rain, Snow, Fog, Cloudy, Stormy, Clear)
- Weather transitions and timing
- Weather-specific particle behavior
- Intensity scaling
- Weather effect cleanup
- Multiple weather effect handling

**3. Performance Tests (`ParticleManagerPerformanceTest.cpp`)**
**8 test cases** covering performance characteristics:
- Large-scale particle simulation (1000+ particles)
- Update performance scaling
- Memory usage efficiency
- Sustained performance benchmarks
- Different effect type performance
- Cleanup performance

**4. Threading Tests (`ParticleManagerThreadingTest.cpp`)**
**7 test cases** covering multi-threading safety:
- Concurrent particle creation
- Thread-safe effect management
- Concurrent weather changes (using `triggerWeatherEffect()` for accurate marking)
- Parallel statistics access
- Mixed concurrent operations
- Enhanced resource cleanup safety with proper weather effect stopping

#### Running Particle Manager Tests

**Quick Commands:**
```bash
# Run all particle manager tests (4-6 minutes)
./tests/test_scripts/run_particle_manager_tests.sh

# Quick core validation (30 seconds)
./tests/test_scripts/run_particle_manager_tests.sh --core

# Weather functionality only (45 seconds)
./tests/test_scripts/run_particle_manager_tests.sh --weather

# Performance benchmarks (2-3 minutes)
./tests/test_scripts/run_particle_manager_tests.sh --performance

# Threading safety tests (1-2 minutes)
./tests/test_scripts/run_particle_manager_tests.sh --threading

# Verbose output
./tests/test_scripts/run_particle_manager_tests.sh --verbose
```

**Cross-Platform Support:**
- **Linux/macOS:** `run_particle_manager_tests.sh`
- **Windows:** `run_particle_manager_tests.bat`

Both scripts support identical command-line options and functionality.

**Test Results:**
Test results are automatically saved to:
- `test_results/particle_manager/` - Individual test outputs
- `test_results/particle_manager/all_particle_tests_results.txt` - Combined summary

#### Integration with Main Test Runner

The particle manager tests are integrated into the main test runner:
```bash
# All tests (includes particle manager tests)
./tests/test_scripts/run_all_tests.sh

# Core functionality only (includes particle manager core tests)
./tests/test_scripts/run_all_tests.sh --core-only
```

### Collision System Tests

Located in `tests/collisions/CollisionSystemTests.cpp`, these tests comprehensively validate the spatial hash collision system:

#### Test Coverage

1. **AABB Tests** (`AABBTests` suite):
   - Basic bounding box properties (left, right, top, bottom)
   - Intersection detection between AABBs
   - Point containment testing
   - Closest point calculation for AABBs

2. **SpatialHash Tests** (`SpatialHashTests` suite):
   - Entity insertion and spatial partitioning
   - Entity removal and cell cleanup
   - Position updates with cell transitions
   - Query operations with result deduplication
   - Clear operations for spatial hash cleanup

3. **Performance Tests** (`CollisionPerformanceTests` suite):
   - Insertion performance with up to 10,000 entities
   - Query performance across various entity densities
   - Update performance during entity movement
   - Scalability benchmarking with performance assertions

4. **Stress Tests** (`CollisionStressTests` suite):
   - High-density collision detection (20 entities per cell)
   - Boundary condition handling
   - Large entity spanning multiple cells
   - Edge case validation

#### Running Collision Tests

```bash
# Linux/macOS
./tests/test_scripts/run_collision_tests.sh [--verbose]

# Windows
tests/test_scripts/run_collision_tests.bat [--verbose]
```

**Performance Targets:**
- SpatialHash insertion: < 50μs per entity
- SpatialHash query: < 100μs per query  
- SpatialHash update: < 75μs per update

### Pathfinding System Tests

Located in `tests/pathfinding/PathfindingSystemTests.cpp`, these tests validate the current grid-based pathfinding implementation:

#### Test Coverage

1. **PathfindingGrid Basic Tests** (`PathfindingGridBasicTests` suite):
   - Pathfinding system configuration (diagonal movement, costs, iteration limits)
   - Explicit blocked-cell layout setup for deterministic path tests
   - Weight-system setup on a concrete test grid

2. **Algorithm Tests** (`PathfindingAlgorithmTests` suite):
   - Deterministic pathfinding across a known obstacle layout
   - Invalid start/goal position handling
   - Same start/goal position edge cases
   - Diagonal vs orthogonal movement comparison
   - Direct vs hierarchical path selection coverage

3. **Weight System Tests** (`PathfindingWeightTests` suite):
   - Dynamic weight area application on successful paths
   - Weight circle placement and overlapping
   - Weighted-path regression coverage

4. **Performance Tests** (`PathfindingPerformanceTests` suite):
   - Repeated pathfinding across the deterministic fixture grid
   - Iteration limit impact on completion time
   - Memory usage validation through stress testing

#### Running Pathfinding Tests

```bash
# Linux/macOS
./tests/test_scripts/run_pathfinding_tests.sh [--verbose]

# Windows
tests/test_scripts/run_pathfinding_tests.bat [--verbose]
```

**Performance Targets:**
- Small grids (≤100x100): < 10ms per pathfinding request
- Large grids (≤200x200): < 50ms per pathfinding request
- Iteration limits prevent indefinite stalls

### PathfinderManager & AIManager Contention Tests

Located in `tests/integration/PathfinderAIContentionTests.cpp`, these integration tests validate WorkerBudget coordination between PathfinderManager and AIManager under heavy load:

#### Test Coverage

1. **WorkerBudget Allocation Tests**:
   - Verify each manager gets all workers during its update window (sequential execution model)
   - Validate WorkerBudget returns full worker count for any active workload
   - Confirm adaptive batch tuning adjusts based on timing feedback
   - Check queue pressure monitoring prevents ThreadSystem overload

2. **Simultaneous Load Tests**:
   - Submit 100+ simultaneous pathfinding requests alongside AI updates
   - Validate both managers process work without starvation
   - Confirm rate limiting (50 requests/frame) prevents queue flooding
   - Verify queue pressure stays below critical threshold (< 95%)

3. **Worker Starvation Prevention**:
   - Heavy load testing with 200+ pathfinding requests
   - Concurrent AIManager updates to simulate realistic game conditions
   - Validate neither manager monopolizes ThreadSystem workers
   - Confirm graceful degradation under extreme stress

4. **Queue Pressure Coordination**:
   - Test 150+ requests triggering rate limiting
   - Monitor queue pressure across multiple update frames
   - Verify adaptive batching responds to queue pressure
   - Validate Normal priority prevents AIManager contention

#### Running Contention Tests

```bash
# Linux/macOS
./tests/test_scripts/run_pathfinder_ai_contention_tests.sh [--verbose]

# Windows
tests/test_scripts/run_pathfinder_ai_contention_tests.bat [--verbose]
```

**Key Validations:**
- WorkerBudget integration: Pathfinding uses 19% allocation properly
- Request batching: Adaptive batch sizes based on available workers
- Rate limiting: Maximum 50 requests processed per frame
- Queue pressure: Monitoring triggers graceful degradation
- No worker starvation: Both managers make progress under heavy load

**Estimated Runtime:** ~2 seconds (4 integration tests)

### Collision System Performance Benchmark

Located in `tests/performance/CollisionScalingBenchmark.cpp`, this dedicated benchmark suite measures collision system performance:

#### Collision Benchmark Coverage

1. **CollisionManager SOA Storage**: Structure-of-Arrays performance testing
2. **SpatialHash Performance**: Insertion, query, and update scaling (100 to 10,000 entities)
3. **Broadphase/Narrowphase Detection**: Collision detection pipeline performance
4. **Threading Comparison**: Single-threaded vs multi-threaded performance analysis

#### Running Collision Benchmarks

```bash
# Linux/macOS
./tests/test_scripts/run_collision_benchmark.sh [--verbose]

# Windows
tests/test_scripts/run_collision_benchmark.bat [--verbose]
```

**Estimated Runtime:** 2-3 minutes

**Performance Targets:**
- SpatialHash insertion: < 50μs per entity
- SpatialHash query: < 100μs per query
- SpatialHash update: < 75μs per update
- Collision detection: < 5ms for 1000 entities

### Pathfinder System Performance Benchmark

Located in `tests/performance/PathfinderBenchmark.cpp`, this dedicated benchmark suite measures pathfinding system performance:

#### Pathfinder Benchmark Coverage

1. **Immediate Pathfinding**: Performance across grid sizes (50x50 to 200x200)
2. **Async Pathfinding**: Request throughput and latency analysis
3. **Cache Performance**: Hit rate analysis and speedup measurements
4. **Path Length Scaling**: Computation time vs path complexity
5. **Threading Analysis**: Overhead vs benefits comparison

#### Running Pathfinder Benchmarks

```bash
# Linux/macOS
./tests/test_scripts/run_pathfinder_benchmark.sh [--verbose]

# Windows
tests/test_scripts/run_pathfinder_benchmark.bat [--verbose]
```

**Estimated Runtime:** 3-5 minutes

**Performance Targets:**
- Immediate pathfinding: < 20ms per request
- Async throughput: > 100 paths/second
- Cache speedup: > 2x for repeated paths
- Success rate: > 90% for reasonable requests

**Output Files:**
- `test_results/collision_benchmark.csv` - Collision system detailed metrics
- `test_results/pathfinder_benchmark.csv` - Pathfinder system detailed metrics
- `test_results/collision_benchmark_results.txt` - Human-readable collision results
- `test_results/pathfinder_benchmark_results.txt` - Human-readable pathfinder results

#### Test Status Summary

**✅ Passing Test Suites:**
- **Core Tests:** 14/14 passing - Basic functionality verified
- **Weather Tests:** 9/9 passing - Weather integration working correctly

**⚠️ Known Issues:**
- **Performance Tests:** Some tests may fail on slower systems or under load
- **Threading Tests:** Concurrency tests may be sensitive to system timing

These issues don't affect core functionality but may require system-specific tuning.

#### Performance Characteristics

**Expected Performance (Debug builds):**
- **Core tests:** Complete in ~30 seconds
- **Weather tests:** Complete in ~45 seconds
- **Performance tests:** Complete in 2-3 minutes
- **Threading tests:** Complete in 1-2 minutes

**Scaling Behavior:**
The ParticleManager is designed to handle:
- 1000+ active particles efficiently
- Multiple concurrent weather effects
- Thread-safe operations across multiple cores
- Sustained 60 FPS performance with realistic particle loads

Performance tests validate these capabilities and benchmark actual system performance.

#### Weather Test Fixes Applied

Recent fixes to weather tests addressed:
1. **Timing issues:** Tests now use multiple update cycles for particle emission
2. **Weather type mapping:** Consistent use of weather type names (e.g., "Rainy" vs "Rain")
3. **Low emission rates:** Special handling for effects like "Cloudy" with very low emission rates
4. **Particle lifecycle:** Proper handling of particle fade and cleanup behavior

These fixes ensure reliable weather functionality testing across different systems.
5. **Test Adjustments:** Weather effects are now created using `triggerWeatherEffect()` to ensure proper marking and cleanup consistent with expected ParticleManager behavior.

#### Development Guidelines

**Adding New Tests:**
1. **Core functionality:** Add to `ParticleManagerCoreTest.cpp`
2. **Weather features:** Add to `ParticleManagerWeatherTest.cpp`
3. **Performance cases:** Add to `ParticleManagerPerformanceTest.cpp`
4. **Threading scenarios:** Add to `ParticleManagerThreadingTest.cpp`

**Test Fixture Pattern:**
All test files use the same fixture pattern:
```cpp
struct ParticleManagerTestFixture {
    ParticleManagerTestFixture() {
        manager = &ParticleManager::Instance();
        if (manager->isInitialized()) {
            manager->clean();
        }
    }
    
    ~ParticleManagerTestFixture() {
        if (manager->isInitialized()) {
            manager->clean();
        }
    }
    
    ParticleManager* manager;
};
```

**Best Practices:**
1. **Always clean up:** Use RAII pattern in test fixtures
2. **Multiple update cycles:** Weather and effect tests need multiple `update()` calls
3. **Timing considerations:** Performance tests should account for system variations
4. **Resource cleanup:** Add delays between resource-intensive tests

### GameTime System Tests

Located in `tests/core/`, these tests validate the fantasy calendar and time simulation:

#### Test Coverage

1. **GameTime Core Tests** (`GameTimeTest.cpp`):
   - Time advancement and tick system
   - Configuration and initialization
   - Time scale modifications

2. **Calendar Tests** (`GameTimeCalendarTest.cpp`):
   - Fantasy month system (Bloomtide, Sunpeak, Harvestmoon, Frosthold)
   - Day advancement (30 days per month)
   - Year cycle transitions

3. **Season Tests** (`GameTimeSeasonTest.cpp`):
   - Season transitions (Spring→Summer→Fall→Winter)
   - Weather probability changes per season
   - Event dispatching on season change

#### Running GameTimeManager Tests

```bash
# Linux/macOS
./tests/test_scripts/run_game_time_tests.sh [--verbose]

# Windows
tests/test_scripts/run_game_time_tests.bat [--verbose]
```

### Controller Tests

Located in `tests/controllers/`, these tests validate the state-scoped controller pattern. Tests use a template-based infrastructure (`tests/controllers/common/`) that provides reusable test suites for common ControllerBase behaviors, reducing duplication across controller test files.

#### Test Coverage

1. **ControllerRegistry Tests** (`ControllerRegistryTests.cpp`):
   - Type-safe controller registration and retrieval
   - Batch operations (subscribeAll, unsubscribeAll, suspendAll, resumeAll, updateAll)
   - Lifecycle management (clear, move semantics)
   - Empty registry edge cases

2. **WeatherController Tests** (`WeatherControllerTests.cpp`):
   - Weather event coordination and state transitions
   - All weather types (Clear, Cloudy, Rainy, Stormy, Foggy, Snowy, Windy)
   - Weather descriptions and event filtering

3. **DayNightController Tests** (`DayNightControllerTests.cpp`):
   - Time period tracking (Morning/Day/Evening/Night)
   - Hour-to-period mapping and transitions
   - Visual state management and descriptions

4. **Additional Controller Targets**:
   - `HarvestControllerTests.cpp`
   - `InventoryControllerTests.cpp` - inventory UI, equipment clicks, hotbar assignment/dragging, and inventory slot drag/drop reordering
   - `NPCRenderControllerTests.cpp`
   - `CombatControllerTests.cpp`
   - `ResourceRenderControllerTests.cpp`
   - `SocialControllerTests.cpp`

5. **Related Input/Inventory Targets**:
   - `InputManagerCommandTests.cpp`
   - `InventoryControllerTests.cpp`

#### Common Test Infrastructure

The `tests/controllers/common/` directory provides reusable test templates:
- `ControllerTestFixture.hpp`: Base fixture with EventManager/GameTimeManager setup
- `ControllerOwnershipTests.hpp`: Instantiation, move semantics, RAII destruction
- `ControllerSubscriptionTests.hpp`: Subscribe/unsubscribe lifecycle
- `ControllerSuspendResumeTests.hpp`: Suspend/resume behavior
- `ControllerGetNameTests.hpp`: getName validation

#### Running Controller Tests

```bash
# Linux/macOS
./tests/test_scripts/run_controller_tests.sh              # Run all controller tests
./tests/test_scripts/run_controller_tests.sh --registry   # ControllerRegistry tests only
./tests/test_scripts/run_controller_tests.sh --weather    # WeatherController tests only
./tests/test_scripts/run_controller_tests.sh --daynight   # DayNightController tests only
./tests/test_scripts/run_controller_tests.sh --verbose    # Verbose output

# Windows
tests/test_scripts/run_controller_tests.bat              # Run all controller tests
tests/test_scripts/run_controller_tests.bat --registry   # ControllerRegistry tests only
tests/test_scripts/run_controller_tests.bat --weather    # WeatherController tests only
tests/test_scripts/run_controller_tests.bat --daynight   # DayNightController tests only
tests/test_scripts/run_controller_tests.bat --verbose    # Verbose output
```

### Entity State Machine Tests

Located in `tests/EntityStateManagerTests.cpp`, these tests validate the EntityStateManager class that manages entity state machines:

#### Test Coverage

1. **Basic State Management** (5 tests):
   - AddState: Verify state added and retrievable
   - AddMultipleStates: Multiple state registration
   - AddDuplicateStateThrows: Duplicate name throws exception
   - HasStateReturnsFalseForNonExistent: Non-existing state checks
   - GetCurrentStateNameEmptyWhenNoState: Empty state handling

2. **State Transitions** (4 tests):
   - SetStateCallsEnter: Setting state triggers enter() callback
   - SetStateTransitionCallsExitThenEnter: Proper exit/enter sequence
   - SetSameStateTriggersCycle: Re-entering same state triggers cycle
   - SetNonExistentStateResetsCurrentState: Invalid state handling

3. **Update Propagation** (4 tests):
   - UpdateCallsCurrentStateUpdate: update() propagates to state
   - UpdatePassesDeltaTimeCorrectly: DeltaTime passed accurately
   - UpdateWithNoCurrentStateIsNoOp: Safe with no current state
   - UpdateOnlyAffectsCurrentState: Only active state updated

4. **Remove State** (3 tests):
   - RemoveState: Basic state removal
   - RemoveCurrentStateResetsIt: Removing current state resets it
   - RemoveNonExistentStateIsNoOp: Safe removal of non-existing state

5. **Edge Cases** (2 tests):
   - EmptyManagerIsValid: Empty manager operations safe
   - MultipleTransitions: Complex transition sequences

#### Running Entity State Machine Tests

```bash
# Linux/macOS
./tests/test_scripts/run_entity_state_manager_tests.sh [--verbose]
./tests/test_scripts/run_entity_state_manager_tests.sh --basic-test       # Basic state management only
./tests/test_scripts/run_entity_state_manager_tests.sh --transition-test  # State transitions only
./tests/test_scripts/run_entity_state_manager_tests.sh --update-test      # Update propagation only

# Windows
tests/test_scripts/run_entity_state_manager_tests.bat [--verbose]
tests/test_scripts/run_entity_state_manager_tests.bat --basic-test
tests/test_scripts/run_entity_state_manager_tests.bat --transition-test
tests/test_scripts/run_entity_state_manager_tests.bat --update-test
```

**Estimated Runtime:** ~1 second (18 tests)

### Entity Data Management Tests

Located in `tests/managers/`, these tests validate the Data-Oriented Design (DoD) entity management system:

#### Test Coverage

1. **EntityDataManager Tests** (`EntityDataManagerTests.cpp`) - **65 test cases**:

   **Singleton & Lifecycle** (6 tests):
   - Singleton pattern validation
   - Init/clean lifecycle
   - State transition handling
   - Double init prevention

   **Entity Creation** (10 tests):
   - NPC, Player, DroppedItem, Projectile, AreaEffect, StaticBody creation
   - Handle validity after creation
   - Entity kind assignment
   - Initial tier placement

   **Handle Validation** (8 tests):
   - Valid/invalid handle detection
   - Stale handle detection after destruction
   - Generation increment verification
   - Index extraction from handles

   **Data Access** (12 tests):
   - Transform data access (position, velocity, rotation)
   - HotData access (flags, kind, tier)
   - Type-specific data (CharacterData, ItemData, ProjectileData, AreaEffectData)
   - Static vs dynamic entity separation

   **Destruction Queue** (8 tests):
   - Entity destruction queuing
   - Batch destruction processing
   - Slot reuse after destruction
   - Generation increment on reuse

   **Simulation Tier System** (12 tests):
   - Tier assignment (Active, Background, Hibernated)
   - Distance-based tier updates
   - Active/Background index retrieval
   - Tier transitions based on reference point

   **Queries & Lookups** (9 tests):
   - Radius-based entity queries
   - Entity count by kind/tier
   - EntityId lookup
   - Handle-to-index mapping

2. **BackgroundSimulationManager Tests** (`BackgroundSimulationManagerTests.cpp`) - **32 test cases**:

   **Singleton & Lifecycle** (6 tests):
   - Singleton pattern validation
   - Init/clean lifecycle (including state reset)
   - Dependency verification (requires EntityDataManager)
   - State transition preparation

   **Pause/Resume** (5 tests):
   - Global pause stops all processing
   - Resume continues processing
   - No frame counter updates when paused
   - No tier calculations when paused

   **Reference Point** (5 tests):
   - Reference point setting
   - Movement threshold (32-unit) triggering tier recalc
   - Initial reference point always sets dirty flag

   **Tier Management** (6 tests):
   - Tier update interval (120 frames)
   - Manual tier invalidation
   - hasWork() reflects background entity presence
   - Tier update delegates to EntityDataManager

   **Configuration** (5 tests):
   - Active/Background radius configuration
   - Update rate configuration
   - Screen-size based configuration
   - Default values validation

   **Update Processing** (3 tests):
   - Accumulator pattern (10Hz updates)
   - Background entity processing
   - Performance statistics tracking

   **Performance Stats** (2 tests):
   - Stats collection and retrieval
   - Stats reset functionality

#### Running Entity Data Management Tests

```bash
# Linux/macOS
./tests/test_scripts/run_entity_data_manager_tests.sh              # Run all tests
./tests/test_scripts/run_entity_data_manager_tests.sh --edm        # EntityDataManager tests only
./tests/test_scripts/run_entity_data_manager_tests.sh --bgsm       # BackgroundSimulationManager tests only
./tests/test_scripts/run_entity_data_manager_tests.sh --verbose    # Verbose output

# Windows
tests/test_scripts/run_entity_data_manager_tests.bat              # Run all tests
tests/test_scripts/run_entity_data_manager_tests.bat --edm        # EntityDataManager tests only
tests/test_scripts/run_entity_data_manager_tests.bat --bgsm       # BackgroundSimulationManager tests only
tests/test_scripts/run_entity_data_manager_tests.bat --verbose    # Verbose output
```

**Estimated Runtime:** ~2-3 seconds (97 tests total)

### NPC Memory System Tests

Located in `tests/managers/NPCMemoryTests.cpp`, these tests validate the NPC memory system that enables NPCs to remember combat encounters, social interactions, witnessed events, locations visited, and emotional states.

#### Test Coverage

1. **Memory Structure Tests** (4 tests):
   - MemoryEntry size validation (≤40 bytes with padding)
   - EmotionalState layout (16 bytes, 4 floats)
   - NPCMemoryData cache-line alignment (alignas(64))
   - MemoryOverflow vector storage

2. **Memory Initialization Tests** (3 tests):
   - Memory data allocation with entity creation
   - Initialization sets validity flag
   - Default emotional state values

3. **Add Memory Tests** (4 tests):
   - Adding memories to inline storage (6 slots)
   - Overflow to vector storage when inline full
   - Memory count tracking
   - Circular buffer behavior for inline slots

4. **Find Memory Tests** (4 tests):
   - Search memories by type (AttackedBy, Attacked, etc.)
   - Search memories by entity handle
   - Combined type and entity search
   - Empty result handling

5. **Emotional State Tests** (3 tests):
   - Emotional decay over time (configurable rate)
   - Emotion modification (add/subtract)
   - Clamping to [0.0, 1.0] range

6. **Combat Event Tests** (3 tests):
   - Recording attack events (attacker/target)
   - Damage tracking (dealt/received totals)
   - Combat statistics (encounter count, last combat time)

7. **Location History Tests** (2 tests):
   - Location tracking with circular buffer
   - History limit enforcement (4 locations)

8. **Cleanup Tests** (3 tests):
   - Memory clearing on entity destruction
   - Overflow cleanup
   - State transition handling (prepareForStateTransition)

#### Running NPC Memory Tests

```bash
# Linux/macOS
./tests/test_scripts/run_npc_memory_tests.sh              # Run all tests
./tests/test_scripts/run_npc_memory_tests.sh --memory     # NPC memory tests only
./tests/test_scripts/run_npc_memory_tests.sh --ai-integration  # AI manager integration only
./tests/test_scripts/run_npc_memory_tests.sh --verbose    # Verbose output

# Windows
tests/test_scripts/run_npc_memory_tests.bat              # Run all tests
tests/test_scripts/run_npc_memory_tests.bat --memory     # NPC memory tests only
tests/test_scripts/run_npc_memory_tests.bat --ai-integration  # AI manager integration only
tests/test_scripts/run_npc_memory_tests.bat --verbose    # Verbose output

# Direct execution (fastest for development)
./bin/debug/npc_memory_tests                              # Run all memory tests
./bin/debug/npc_memory_tests --list_content               # List available tests
./bin/debug/npc_memory_tests --run_test="AddMemory*"      # Run specific tests
```

**Estimated Runtime:** ~1-2 seconds (24 tests)

#### Key Data Structures

```cpp
// Memory entry for storing individual memories (≤40 bytes)
struct MemoryEntry {
    EntityHandle subject;   // Who/what is remembered
    Vector2D location;      // Where it happened
    float timestamp;        // Game time when occurred
    float value;            // Context-dependent (damage, etc.)
    MemoryType type;        // Type of memory (AttackedBy, Interaction, etc.)
    uint8_t importance;     // 0-255 importance score
    uint8_t flags;          // State flags (FLAG_VALID, FLAG_FADING)
};

// Emotional state affecting NPC behavior (16 bytes)
struct EmotionalState {
    float aggression;   // Combat readiness
    float fear;         // Flee threshold
    float curiosity;    // Investigation tendency
    float suspicion;    // Alertness to threats
};

// Per-entity memory data (cache-line aligned, ≤512 bytes)
struct alignas(64) NPCMemoryData {
    MemoryEntry memories[6];        // Inline storage
    Vector2D locationHistory[4];    // Recent locations
    EmotionalState emotions;
    EntityHandle lastAttacker, lastTarget;
    float totalDamageReceived, totalDamageDealt;
    // ... additional tracking fields
};
```

#### Integration with BehaviorContext

Memory data is accessible in AI behaviors via `BehaviorContext`:

```cpp
void AttackBehavior::executeLogic(BehaviorContext& ctx) {
    if (ctx.memoryData && ctx.memoryData->isValid()) {
        auto& memory = *ctx.memoryData;

        // Check grudge against attacker
        if (memory.lastAttacker == ctx.playerHandle) {
            memory.emotions.aggression += 0.2f * ctx.deltaTime;
        }

        // Fear response from damage
        if (memory.totalDamageReceived > 50.0f) {
            memory.emotions.fear += 0.1f * ctx.deltaTime;
        }
    }
}
```

### EDM Integration Tests

Located in `tests/managers/` and `tests/collisions/`, these tests validate manager-specific integration with the EntityDataManager:

#### AIManager EDM Integration Tests

**File:** `tests/managers/AIManagerEDMIntegrationTests.cpp` (~400 lines, 12 test cases)

**Test Coverage:**

1. **Sparse Behavior Vector Tests** (4 tests):
   - Behavior assignment creates EDM index mapping
   - Sparse vector handles gaps correctly
   - Behavior unassignment clears sparse behavior
   - Behavior reassignment updates sparse behavior

2. **Batch Processing EDM Tests** (2 tests):
   - Batch processing writes to EDM transform
   - Multiple entities processed via batch

3. **State Transition Tests** (3 tests):
   - prepareForStateTransition clears AI data
   - State transition while batch processing
   - AIManager reinit after state transition

4. **EDM Index Caching Tests** (2 tests):
   - EDM index cached on behavior assignment
   - Entity destruction doesn't affect other entities

5. **Behavior Cloning Tests** (1 test):
   - Each entity gets separate behavior instance

#### CollisionManager EDM Integration Tests

**File:** `tests/collisions/CollisionManagerEDMIntegrationTests.cpp` (~480 lines, 17 test cases)

**Test Coverage:**

1. **Active Tier Filtering Tests** (3 tests):
   - Only Active tier entities participate in collision
   - Entities with collision disabled not in active list
   - Background tier entities not in collision

2. **Static vs Dynamic Separation Tests** (3 tests):
   - Static body added to storage (not EDM)
   - Dynamic entity in EDM not in static storage
   - Static bodies always checked for collision

3. **Position Reading Tests** (2 tests):
   - Collision uses EDM position
   - AABB computed from EDM half-size

4. **Index Semantics Tests** (2 tests):
   - Movable-movable pair indices are EDM indices
   - Movable-static pair uses mixed indices

5. **State Transition Tests** (2 tests):
   - prepareForStateTransition clears dynamic data
   - Static bodies preserved after dynamic clear

6. **Layer Filtering Tests** (2 tests):
   - Collision layers read from EDM
   - Trigger flag read from EDM

#### Running EDM Integration Tests

```bash
# Linux/macOS
./tests/test_scripts/run_ai_manager_edm_integration_tests.sh [--verbose]
./tests/test_scripts/run_collision_manager_edm_integration_tests.sh [--verbose]

# Windows
tests/test_scripts/run_ai_manager_edm_integration_tests.bat [--verbose]
tests/test_scripts/run_collision_manager_edm_integration_tests.bat [--verbose]
```

**Estimated Runtime:** ~1-2 seconds per test suite

#### Key Testing Patterns

**Explicit Radii for Deterministic Testing:**
Tests use explicit radii rather than screen-size calculations for deterministic results:
```cpp
bgsm->setActiveRadius(500.0f);
bgsm->setBackgroundRadius(1000.0f);
// Entity at 750 units → Background tier
// Entity at 100 units → Active tier
```

**Singleton Lifecycle in Test Fixtures:**
```cpp
class EntityDataManagerTestFixture {
public:
    EntityDataManagerTestFixture() {
        edm = &EntityDataManager::Instance();
        edm->init();
    }

    ~EntityDataManagerTestFixture() {
        edm->clean();
    }

protected:
    EntityDataManager* edm;
};
```

**Handle Validation Pattern:**
```cpp
auto handle = edm->createNPC(Vector2D(100, 100), 32, 32);
BOOST_CHECK(edm->isValidHandle(handle));

edm->destroyEntity(handle);
edm->processDestructionQueue();
BOOST_CHECK(!edm->isValidHandle(handle));  // Now stale
```

## Adding New Tests

1. Choose the appropriate test file for your component
2. Add test cases using the `BOOST_AUTO_TEST_CASE` macro
3. Follow the existing pattern for setup, execution, and verification
4. Run tests with `--clean` to ensure your changes are compiled
5. For directory or file operation tests, always clean up created resources when tests complete
6. For tests requiring game engine dependencies, consider creating mock implementations

### Creating Mock Dependencies

When testing components that depend on complex systems (like NPCSpawnEvent depending on NPC and GameEngine):

1. **Create Mock Headers**: Define mock classes that implement the same interface as real dependencies
2. **Mock Implementation**: Provide minimal functionality needed for testing
3. **Test-Specific Compilation**: Create test-specific source files that use mocks instead of real implementations
4. **CMake Integration**: Update `tests/CMakeLists.txt` to include mock files and dependencies

**Example Mock Structure:**
```cpp
// MockNPC.hpp - Mock implementation of NPC for testing
class MockNPC : public Entity {
public:
    MockNPC(const std::string& textureID, const Vector2D& position, int width, int height);

    // Mock the required interface methods
    void setWanderArea(float x1, float y1, float x2, float y2);
    void setBoundsCheckEnabled(bool enabled);

    // Factory method like real class
    static std::shared_ptr<MockNPC> create(const std::string& textureID, const Vector2D& position, int width, int height);
};

// Define alias for testing
using NPC = MockNPC;
```

**Benefits of Mocking:**
- Tests run faster without full game engine initialization
- Tests are more focused and isolated
- Easier to reproduce specific conditions
- No external dependencies like graphics, audio, or file systems
- Better test reliability and maintainability

For directory management tests (like in SaveManagerTests):
- Use a dedicated test directory that's different from production directories
- Implement proper cleanup in test class destructors or at the end of test cases
- Add detailed logging to identify filesystem operation failures
- Test both successful cases and error cases (e.g., permission denied, disk full)

### Basic Test Structure

```cpp
// Define module name
#define BOOST_TEST_MODULE YourTestName
#include <boost/test/included/unit_test.hpp>

// For thread-safe tests, disable signal handling
#define BOOST_TEST_NO_SIGNAL_HANDLING

// Global fixture for setup/teardown
struct TestFixture {
    TestFixture() {
        // Setup code
    }

    ~TestFixture() {
        // Cleanup code
    }
};

BOOST_GLOBAL_FIXTURE(TestFixture);

// Test cases
BOOST_AUTO_TEST_CASE(TestSomething) {
    // Test code
    BOOST_CHECK(condition);  // Continues test if failed
    BOOST_REQUIRE(condition);  // Stops test if failed
}
```

## Singleton and Factory Lifecycle Management

When working with singleton factories (like ResourceFactory) in tests, follow these important guidelines:

### Factory Lifecycle Best Practices

1. **Test Isolation**: Use `clear()` methods in test fixtures to ensure test isolation
2. **Production Safety**: **NEVER** call factory `clear()` methods from production code or other singleton destructors
3. **Static Object Destruction**: Let static factories clean themselves up at program exit

**Safe Test Pattern:**
```cpp
class ResourceFactoryTestFixture {
public:
    ResourceFactoryTestFixture() {
        ResourceFactory::initialize();  // Safe in tests
    }
    
    ~ResourceFactoryTestFixture() {
        ResourceFactory::clear();      // Safe in tests for isolation
    }
};
```

**Unsafe Production Pattern (DO NOT DO):**
```cpp
// WRONG: Don't call factory clear() from other singletons
void SomeManager::clean() {
    ResourceFactory::clear();  // ❌ Can cause crashes due to undefined destruction order
}
```

**Why This Matters:**
- Static object destruction order is undefined between translation units
- Calling `clear()` from other destructors can cause double-free or use-after-free errors
- Test crashes like "Abort trap: 6" often indicate this anti-pattern

## Thread Safety Considerations

For thread-safety tests, follow these guidelines:

1. **Test Initialization Order**
   - Initialize ThreadSystem first, then other systems
   - Enable threading only after initialization is complete
   - Configure task priorities during initialization if needed

2. **Test Cleanup Order**
   - Disable threading before cleanup
   - Wait for threads to complete with appropriate timeouts
   - Clean up managers before cleaning up ThreadSystem

3. **Preventing Deadlocks and Race Conditions**
   - Use atomic operations with proper synchronization
   - Add sleep between operations to allow threads to complete
   - Use timeouts when waiting for futures instead of blocking calls
   - Use `compare_exchange_strong` instead of simple `exchange` for atomics
   - Be careful about task priority assignments to avoid priority inversion

4. **Boost Test Options**
   - Add `#define BOOST_TEST_NO_SIGNAL_HANDLING` before including Boost.Test
   - Use `--catch_system_errors=no --no_result_code --detect_memory_leak=0` test options

## Troubleshooting Common Issues

### Boost.Test Configuration Issues

**Problem**: Errors related to Boost.Test initialization or missing symbols.

**Solution**:
- Add `#define BOOST_TEST_MODULE YourModuleName` before including Boost.Test
- Use `#include <boost/test/included/unit_test.hpp>` for header-only approach
- Check CMakeLists.txt has correct Boost linking
- Use correct test macros (`BOOST_AUTO_TEST_CASE`, `BOOST_CHECK`, etc.)

### Thread-Related Segmentation Faults

**Problem**: Tests crash with signal 11 (segmentation fault) during cleanup.

**Solution**:
- Add `#define BOOST_TEST_NO_SIGNAL_HANDLING` before including Boost.Test
- Run with `--catch_system_errors=no --no_result_code --detect_memory_leak=0`
- Disable threading before cleanup: `AIManager::Instance().enableThreading(false)`
- Add sleep between operations: `std::this_thread::sleep_for(std::chrono::milliseconds(100))`
- Use timeout for futures: `future.wait_for(std::chrono::seconds(1))`
- Don't register SIGSEGV handler in test code

### Filesystem Operation Issues

**Problem**: Directory creation, file operations, or save/load functions fail during tests.

**Solution**:
- Check working directory: Tests might run from a different directory than expected
- Print and verify absolute paths: `std::filesystem::absolute(path).string()`
- Ensure parent directories exist before creating files
- Verify write permissions on directories with a small test file
- Use detailed logging to identify exactly which operation is failing
- Add proper error handling with try/catch blocks around all filesystem operations
- Always clean up test files/directories in both success and failure scenarios
- On Windows, ensure paths don't exceed MAX_PATH (260 characters)

### Build Issues

**Problem**: Tests won't build or can't find dependencies.

**Solution**:
- Run with `--clean` to ensure clean rebuilding
- Check that all required libraries are installed and linked in CMakeLists.txt
- Check proper include paths are set in CMakeLists.txt

## Running All Tests

The `tests/test_scripts/run_all_tests` scripts provide a convenient way to run all test suites sequentially:

### Linux/macOS
```bash
./tests/test_scripts/run_all_tests.sh [options]
```

### Windows
```
tests/test_scripts/run_all_tests.bat [options]
```

These scripts:
1. Run each test script one by one, giving them time to complete
2. Pass along the `--verbose` option to individual test scripts if specified
3. Generate a summary showing which tests passed or failed
4. Save combined results to `test_results/combined/all_tests_results.txt`
5. Return a non-zero exit code if any tests fail

## CMake Configuration

Tests are configured in `tests/CMakeLists.txt` with the following structure:

1. Define test executables with source files
2. Set compiler definitions for Boost.Test
3. Link necessary libraries
4. Register tests with CTest

For thread-safe tests, ensure `BOOST_TEST_NO_SIGNAL_HANDLING` is defined.

## 13. GPU System Tests

Located in `tests/gpu/`, these tests validate the SDL3 GPU rendering subsystem. Tests are organized into three categories based on GPU requirements.

### Test Categories

#### Unit Tests (No GPU Required)

**GPUTypesTests.cpp** - Vertex and UBO layout validation:
- SpriteVertex size (20 bytes) and member offsets
- ColorVertex size (12 bytes) and member offsets
- ViewProjectionUBO layout (64 bytes)
- CompositeUBO std140 alignment (32 bytes)
- Trivially copyable and standard layout verification

**GPUPipelineConfigTests.cpp** - Pipeline configuration creation:
- PipelineConfig default values
- PipelineType enum values
- Sprite config factory (opaque/alpha)
- Particle config factory (additive blending)
- Primitive and composite config factories

#### Integration Tests (GPU Required)

**GPUDeviceTests.cpp** - GPU device lifecycle:
- Singleton pattern validation
- Init/shutdown with valid window
- Double init/shutdown safety
- Shader format queries
- Swapchain format queries
- Driver name retrieval
- Format support queries

**GPUShaderManagerTests.cpp** - Shader loading and caching:
- Load all 6 shaders (sprite, color, composite × vert/frag)
- Shader caching and retrieval
- Platform-native shader path verification (`.spv`, `.metal`, or `.dxil` depending on target platform)
- Nonexistent shader handling

**GPUResourceTests.cpp** - Buffer, texture, sampler wrappers:
- GPUBuffer creation (vertex, index)
- GPUTexture creation (sampler, render target, combined)
- GPUTransferBuffer map/unmap operations
- GPUSampler factory methods (nearest, linear, mipmapped)
- Move semantics for all resource types

**GPUVertexPoolTests.cpp** - Triple-buffered vertex pool:
- Initialization with SpriteVertex/ColorVertex sizes
- Custom capacity configuration
- beginFrame/endFrame cycle
- Frame index advancement
- No GPU stall verification

**SpriteBatchTests.cpp** - Sprite batch recording:
- begin/draw/drawUV/end workflow
- Vertex count tracking
- Color tint application
- Capacity and hasSprites flag
- Vertex position and UV verification

#### System Tests (Full Frame Flow)

**GPURendererTests.cpp** - Complete rendering pipeline:
- Singleton and lifecycle management
- Frame cycle (beginFrame, beginScenePass, beginSwapchainPass, endFrame)
- Pipeline accessor validation (7 pipelines)
- Vertex pool accessor validation (5 pools)
- Sampler and scene texture validation
- Composite params and day/night params
- Viewport management
- Orthographic matrix creation

### GPU Benchmark Utility

**GPUFrameTimingBenchmark.cpp** - Focused GPU frame timing benchmark:
- Uses the real `GPURenderer` path with synthetic workload uploads and draws
- Supports workload modes: `particle`, `primitive`, `sprite`, `ui`, and `mixed`
- Reports average total frame time plus `GPUSwapchainWait`, `GPUUpload`, and `GPUSubmit`
- Intended for before/after renderer comparisons on the same machine
- Exercises the frame lifecycle, upload path, and present timing without needing a full game state

Run it directly:

```bash
ninja -C build gpu_frame_timing_benchmark

# Typical run
./bin/debug/gpu_frame_timing_benchmark --mode particle --warmup 120 --frames 300 --quads 2000

# Compare different workload paths
./bin/debug/gpu_frame_timing_benchmark --mode primitive --frames 600 --quads 4000
./bin/debug/gpu_frame_timing_benchmark --mode sprite --frames 600 --quads 4000
./bin/debug/gpu_frame_timing_benchmark --mode ui --frames 600 --quads 4000
./bin/debug/gpu_frame_timing_benchmark --mode mixed --frames 600 --quads 4000

# Script wrapper
./tests/test_scripts/run_gpu_frame_benchmark.sh --mode mixed
./tests/test_scripts/run_gpu_frame_benchmark.sh --mode sprite --frames 600 --quads 4000
```

Interpretation:
- `Avg frame time` is the end-to-end CPU frame duration for the benchmark loop
- `Avg GPUSwapchainWait` is where frame pacing / swapchain wait shows up
- `Avg GPUUpload` shows CPU-side time spent finalizing uploads into the frame
- `Avg GPUSubmit` shows command buffer submission overhead

Notes:
- Run this on a normal desktop session for meaningful numbers
- Headless or `SDL_VIDEODRIVER=offscreen` runs are useful only to validate the executable path, not to judge real rendering performance
- Compare runs on the same machine, driver, display mode, and window state
- `--quads` is applied per active workload; in `mixed` mode it is distributed across the active pools
- Windows wrapper: `tests/test_scripts/run_gpu_frame_benchmark.bat`

### Running GPU Tests

```bash
# Run all GPU tests (requires GPU)
./tests/test_scripts/run_gpu_tests.sh

# Run only unit tests (no GPU required, works in CI)
./tests/test_scripts/run_gpu_tests.sh --unit-only

# Run only integration tests
./tests/test_scripts/run_gpu_tests.sh --integration-only

# Run only system tests
./tests/test_scripts/run_gpu_tests.sh --system-only

# Skip GPU-requiring tests (for headless CI)
./tests/test_scripts/run_gpu_tests.sh --skip-gpu

# Verbose output
./tests/test_scripts/run_gpu_tests.sh --verbose

# Run via CTest with GPU label
ctest -L GPU --output-on-failure

# Run unit tests only via CTest
ctest -L "GPU;Unit" --output-on-failure
```

### Build Requirements

GPU tests are built as part of the standard debug build:

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
```

### Test Fixture Pattern

GPU tests use a common fixture that handles GPU availability detection:

```cpp
struct GPUTestFixture {
    GPUTestFixture() {
        // Initialize SDL and detect GPU availability
    }
    static bool isGPUAvailable();
    static SDL_Window* getTestWindow();
};

// Use SKIP_IF_NO_GPU() macro for graceful skip in CI
BOOST_FIXTURE_TEST_CASE(TestName, GPUTestFixture) {
    SKIP_IF_NO_GPU();
    // Test code that requires GPU
}
```

### Test Count

- Unit tests: ~20 test cases
- Integration tests: ~45 test cases
- System tests: ~25 test cases
- GPU benchmark utilities: 1 standalone executable
- **Total: ~90 test cases across 8 GPU test executables plus 1 GPU benchmark utility**

## Additional Documentation

For specific testing scenarios and troubleshooting, refer to these additional documents:

- `BEHAVIOR_TESTING.md` - Specific guidance for AI thread testing scenarios
- `TROUBLESHOOTING.md` - Common issues and their solutions across all components
