# Memory Profiler — Mode-by-Mode Instructions

Detailed command blocks, output parsing, and severity classification for each
profiling mode. Loaded on demand from `SKILL.md`.

**Platform gate:** Mode 1 (memcheck) and Mode 3 (massif) require **valgrind**,
which is **Linux-only**. On macOS use Mode 2 (ASan) and Mode 2b (TSan). The
helper scripts check for `valgrind` and exit with the ASan build command if it
is missing.

**Test selection:** All mode loops below iterate `TEST_EXECUTABLES`. Populate it
from the scope choice. For "All Systems", discover executables at runtime
(never freeze a list):

```bash
mapfile -t TEST_EXECUTABLES < <(find ./bin/debug -maxdepth 1 -name "*tests" -type f -perm -u+x | sort)
```

Scoped lists:

```bash
# Core Tests Only
TEST_EXECUTABLES=(
    "./bin/debug/thread_system_tests"
    "./bin/debug/buffer_utilization_tests"
    "./bin/debug/event_manager_tests"
)

# AI System
TEST_EXECUTABLES=(
    "./bin/debug/thread_safe_ai_manager_tests"
    "./bin/debug/ai_optimization_tests"
    "./bin/debug/behavior_functionality_tests"
)

# Collision/Pathfinding
TEST_EXECUTABLES=(
    "./bin/debug/collision_system_tests"
    "./bin/debug/pathfinder_manager_tests"
    "./bin/debug/collision_pathfinding_integration_tests"
)
```

---

## Mode 1: Quick Leak Check (valgrind memcheck, Linux-only)

### 1a. Ensure Debug Build Exists

```bash
if [ ! -f "./bin/debug/thread_system_tests" ]; then
    echo "Debug build not found. Building..."
    cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
fi
```

### 1b. Run Valgrind Memcheck

```bash
OUTPUT_DIR="test_results/memory_profiles"
mkdir -p "$OUTPUT_DIR"

for TEST_EXEC in "${TEST_EXECUTABLES[@]}"; do
    TEST_NAME=$(basename "$TEST_EXEC")
    echo "Running valgrind on $TEST_NAME..."

    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file="$OUTPUT_DIR/${TEST_NAME}_memcheck.log" \
        "$TEST_EXEC" --log_level=test_suite \
        2>&1 | tee "$OUTPUT_DIR/${TEST_NAME}_output.txt"
done
```

**Flags:**
- `--leak-check=full`: Detailed leak information
- `--show-leak-kinds=all`: Show all leak types (definite, indirect, possible, reachable)
- `--track-origins=yes`: Track origin of uninitialized values
- `--verbose`: Detailed output
- `--log-file`: Save valgrind output to file

### 1c. Parse Valgrind Output

```bash
for LOG in "$OUTPUT_DIR"/*_memcheck.log; do
    TEST_NAME=$(basename "$LOG" _memcheck.log)

    echo "=== $TEST_NAME ==="

    DEFINITE_LEAKS=$(grep "definitely lost:" "$LOG" | tail -1 | awk '{print $4, $5}')
    echo "Definite leaks: $DEFINITE_LEAKS"

    INDIRECT_LEAKS=$(grep "indirectly lost:" "$LOG" | tail -1 | awk '{print $4, $5}')
    echo "Indirect leaks: $INDIRECT_LEAKS"

    POSSIBLE_LEAKS=$(grep "possibly lost:" "$LOG" | tail -1 | awk '{print $4, $5}')
    echo "Possible leaks: $POSSIBLE_LEAKS"

    REACHABLE=$(grep "still reachable:" "$LOG" | tail -1 | awk '{print $4, $5}')
    echo "Still reachable: $REACHABLE"

    TOTAL_HEAP=$(grep "total heap usage:" "$LOG" | tail -1)
    echo "Heap usage: $TOTAL_HEAP"

    INVALID_READ=$(grep -c "Invalid read" "$LOG")
    INVALID_WRITE=$(grep -c "Invalid write" "$LOG")
    echo "Invalid reads: $INVALID_READ"
    echo "Invalid writes: $INVALID_WRITE"

    echo ""
done
```

**Severity Classification:**
- **CRITICAL (Block merge):**
  - Definite leaks > 0 bytes
  - Invalid reads/writes > 0
  - Use after free
- **WARNING (Review required):**
  - Indirect leaks > 100 bytes
  - Possible leaks > 1 KB
  - Uninitialized value usage
- **INFO (Monitor):**
  - Still reachable < 10 KB (static globals, SDL resources)

---

## Mode 2: Allocation Profiling (AddressSanitizer)

### 2a. Build with AddressSanitizer

```bash
echo "Building with AddressSanitizer..."
rm -rf build/

cmake -B build/ -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=address -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
    -DUSE_MOLD_LINKER=OFF

ninja -C build
```

**Why ASan for allocation profiling:** tracks every allocation with stack
traces; detects heap-buffer-overflow, use-after-free, double-free; ~2x slowdown
(acceptable for profiling).

### 2b. Run Tests with ASan

```bash
OUTPUT_DIR="test_results/memory_profiles"
mkdir -p "$OUTPUT_DIR"

export ASAN_OPTIONS="detect_leaks=1:symbolize=1:log_path=$OUTPUT_DIR/asan"

for TEST_EXEC in "${TEST_EXECUTABLES[@]}"; do
    TEST_NAME=$(basename "$TEST_EXEC")
    echo "Running ASan on $TEST_NAME..."

    "$TEST_EXEC" --log_level=test_suite 2>&1 | tee "$OUTPUT_DIR/${TEST_NAME}_asan_output.txt"
done

unset ASAN_OPTIONS
```

### 2c. Parse ASan Output

```bash
for OUTPUT in "$OUTPUT_DIR"/*_asan_output.txt; do
    TEST_NAME=$(basename "$OUTPUT" _asan_output.txt)

    echo "=== $TEST_NAME ASan Analysis ==="

    BUFFER_OVERFLOW=$(grep -c "heap-buffer-overflow" "$OUTPUT")
    if [ "$BUFFER_OVERFLOW" -gt 0 ]; then
        echo "CRITICAL: $BUFFER_OVERFLOW heap buffer overflows detected"
        grep -A 10 "heap-buffer-overflow" "$OUTPUT"
    fi

    USE_AFTER_FREE=$(grep -c "heap-use-after-free" "$OUTPUT")
    if [ "$USE_AFTER_FREE" -gt 0 ]; then
        echo "CRITICAL: $USE_AFTER_FREE use-after-free detected"
        grep -A 10 "heap-use-after-free" "$OUTPUT"
    fi

    DOUBLE_FREE=$(grep -c "attempting double-free" "$OUTPUT")
    if [ "$DOUBLE_FREE" -gt 0 ]; then
        echo "CRITICAL: $DOUBLE_FREE double-free detected"
        grep -A 10 "attempting double-free" "$OUTPUT"
    fi

    grep "alloc-dealloc-mismatch" "$OUTPUT" || echo "No alloc-dealloc mismatches"

    echo ""
done
```

### 2d. Identify Per-Frame Allocation Hotspots

```bash
echo "=== Per-Frame Allocation Hotspot Analysis ==="

# Allocations in update loops
grep -n "std::vector" src/managers/AIManager.cpp | grep -i "update\|process" || echo "AIManager: No obvious vector allocations in update"
grep -n "std::vector" src/managers/CollisionManager.cpp | grep -i "update\|detect" || echo "CollisionManager: No obvious vector allocations in update"
grep -n "std::vector" src/managers/ParticleManager.cpp | grep -i "update\|render" || echo "ParticleManager: No obvious vector allocations in update"

# Allocations inside loops (MAJOR ISSUE)
echo ""
echo "Checking for allocations inside loops..."
grep -A 5 "for\|while" src/managers/*.cpp | grep "std::vector\|std::make" | head -20
```

**Per-frame allocation patterns to flag:**
```cpp
// BAD: Allocates every frame
void update() {
    std::vector<Data> buffer;  // Fresh allocation
    buffer.reserve(entityCount);
}  // Deallocation

// BAD: Allocation in loop
for (size_t i = 0; i < count; ++i) {
    std::vector<Item> items;  // Allocation per iteration!
}

// BAD: No reserve before push_back loop
std::vector<Entity> entities;
for (...) {
    entities.push_back(entity);  // Incremental reallocations
}
```

---

## Mode 2b: Thread Safety Validation (ThreadSanitizer)

Use TSan for data race detection, deadlock detection, and thread
synchronization issues. Best for threading systems (AIManager, EventManager,
ParticleManager threading tests).

**Important:** ThreadSanitizer and AddressSanitizer are **mutually exclusive** —
use one or the other, not both.

### 2b-a. Build with ThreadSanitizer

```bash
echo "Building with ThreadSanitizer..."
rm -rf build/

cmake -B build/ -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=thread -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
    -DUSE_MOLD_LINKER=OFF

ninja -C build
```

Remember to export suppressions (per CLAUDE.md):
```bash
export TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"
```

**Why TSan:** detects data races (reads/writes without synchronization), finds
deadlocks and lock-order violations, validates thread-safe container usage;
~5-15x slowdown (acceptable for thread safety validation).

### 2b-b. Run Threading Tests with TSan

```bash
OUTPUT_DIR="test_results/memory_profiles"
mkdir -p "$OUTPUT_DIR"

THREAD_TESTS=(
    "./bin/debug/thread_system_tests"
    "./bin/debug/thread_safe_ai_manager_tests"
    "./bin/debug/thread_safe_ai_integration_tests"
    "./bin/debug/particle_manager_threading_tests"
    "./bin/debug/event_coordination_integration_tests"
)

for TEST_EXEC in "${THREAD_TESTS[@]}"; do
    TEST_NAME=$(basename "$TEST_EXEC")
    echo "Running TSan on $TEST_NAME..."

    "$TEST_EXEC" --log_level=test_suite 2>&1 | tee "$OUTPUT_DIR/${TEST_NAME}_tsan_output.txt"
done
```

### 2b-c. Parse TSan Output

```bash
for OUTPUT in "$OUTPUT_DIR"/*_tsan_output.txt; do
    TEST_NAME=$(basename "$OUTPUT" _tsan_output.txt)

    echo "=== $TEST_NAME TSan Analysis ==="

    DATA_RACES=$(grep -c "WARNING: ThreadSanitizer: data race" "$OUTPUT")
    if [ "$DATA_RACES" -gt 0 ]; then
        echo "CRITICAL: $DATA_RACES data race(s) detected"
        grep -A 15 "WARNING: ThreadSanitizer: data race" "$OUTPUT"
    fi

    DEADLOCKS=$(grep -c "WARNING: ThreadSanitizer: lock-order-inversion" "$OUTPUT")
    if [ "$DEADLOCKS" -gt 0 ]; then
        echo "CRITICAL: $DEADLOCKS potential deadlock(s) detected"
        grep -A 15 "WARNING: ThreadSanitizer: lock-order-inversion" "$OUTPUT"
    fi

    THREAD_LEAKS=$(grep -c "WARNING: ThreadSanitizer: thread leak" "$OUTPUT")
    if [ "$THREAD_LEAKS" -gt 0 ]; then
        echo "WARNING: $THREAD_LEAKS thread leak(s) detected"
    fi

    if [ "$DATA_RACES" -eq 0 ] && [ "$DEADLOCKS" -eq 0 ] && [ "$THREAD_LEAKS" -eq 0 ]; then
        echo "No thread safety issues detected"
    fi

    echo ""
done
```

**Severity Classification:**
- **CRITICAL (Block merge):** data races, deadlocks/lock-order inversions
- **WARNING (Review required):** thread leaks, signal-unsafe function calls

---

## Mode 3: Full Memory Profile (valgrind massif, Linux-only)

### 3a. Run Valgrind Massif

```bash
OUTPUT_DIR="test_results/memory_profiles"
mkdir -p "$OUTPUT_DIR"

for TEST_EXEC in "${TEST_EXECUTABLES[@]}"; do
    TEST_NAME=$(basename "$TEST_EXEC")
    echo "Running massif on $TEST_NAME..."

    valgrind \
        --tool=massif \
        --massif-out-file="$OUTPUT_DIR/${TEST_NAME}_massif.out" \
        --time-unit=ms \
        --detailed-freq=1 \
        --max-snapshots=100 \
        --threshold=0.1 \
        "$TEST_EXEC" --log_level=test_suite
done
```

**Flags:** `--tool=massif` (heap profiler), `--time-unit=ms`,
`--detailed-freq=1` (detailed snapshot frequency), `--max-snapshots=100`,
`--threshold=0.1` (capture 0.1% heap changes).

> The `scripts/run_massif_all_tests.sh` helper automates this loop, and
> `scripts/parse_massif.py` produces the full analysis. Prefer them.

### 3b. Parse Massif Output

```bash
for MASSIF in "$OUTPUT_DIR"/*_massif.out; do
    TEST_NAME=$(basename "$MASSIF" _massif.out)

    echo "=== $TEST_NAME Massif Analysis ==="

    ms_print "$MASSIF" > "$OUTPUT_DIR/${TEST_NAME}_massif_report.txt"

    PEAK_MEM=$(grep "peak" "$MASSIF" | head -1)
    echo "Peak memory: $PEAK_MEM"

    echo ""
    echo "Top 10 allocation sites:"
    ms_print "$MASSIF" | grep -A 1 "->.*%" | head -20

    echo ""
done
```

### 3c. System-by-System Breakdown

```bash
echo "=== Memory Usage by System ==="

for REPORT in "$OUTPUT_DIR"/*_massif_report.txt; do
    echo ""
    echo "Report: $(basename "$REPORT")"

    echo "  AIManager allocations: $(grep -c "AIManager" "$REPORT")"
    echo "  CollisionManager allocations: $(grep -c "CollisionManager" "$REPORT")"
    echo "  PathfinderManager allocations: $(grep -c "PathfinderManager" "$REPORT")"
    echo "  EventManager allocations: $(grep -c "EventManager" "$REPORT")"
    echo "  ParticleManager allocations: $(grep -c "ParticleManager" "$REPORT")"
done
```

---

## Mode 4: Buffer Reuse Audit (static scan, cross-platform)

### 4a. Scan for Buffer Reuse Patterns

```bash
echo "=== Buffer Reuse Pattern Audit ==="

MANAGERS=$(find include/managers -name "*.hpp" -type f)

for MANAGER in $MANAGERS; do
    MANAGER_NAME=$(basename "$MANAGER" .hpp)
    echo ""
    echo "=== $MANAGER_NAME ==="

    echo "Member vectors (should be reused):"
    grep "std::vector" "$MANAGER" | grep "m_" | head -10

    CPP_FILE="src/managers/${MANAGER_NAME}.cpp"
    if [ -f "$CPP_FILE" ]; then
        CLEAR_COUNT=$(grep -c "\.clear()" "$CPP_FILE")
        echo "clear() calls: $CLEAR_COUNT (good - reuses capacity)"

        RESERVE_COUNT=$(grep -c "\.reserve(" "$CPP_FILE")
        echo "reserve() calls: $RESERVE_COUNT"

        if [ "$RESERVE_COUNT" -eq 0 ]; then
            echo "WARNING: No reserve() calls found - check for incremental reallocations"
        fi
    fi
done
```

### 4b. Check for Buffer Reuse Anti-Patterns

```bash
echo ""
echo "=== Checking for Anti-Patterns ==="

# Anti-pattern 1: Local vectors in update functions (should be members)
echo "1. Local vectors in update functions:"
grep -n "void.*update\|void.*process" src/managers/*.cpp | while read -r line; do
    FILE=$(echo "$line" | cut -d: -f1)
    LINE_NUM=$(echo "$line" | cut -d: -f2)
    sed -n "${LINE_NUM},$((LINE_NUM+20))p" "$FILE" | grep -n "std::vector" | while read -r vec_line; do
        echo "  $FILE:$((LINE_NUM + $(echo "$vec_line" | cut -d: -f1))) - Local vector in update"
    done
done

# Anti-pattern 2: Vector reconstruction instead of clear()
echo ""
echo "2. Vector reconstruction (use clear() instead):"
for CPP in src/managers/*.cpp; do
    grep -n "= std::vector<" "$CPP" | head -5
done

# Anti-pattern 3: push_back without reserve
echo ""
echo "3. push_back loops without prior reserve():"
for CPP in src/managers/*.cpp; do
    echo "  Checking $CPP..."
    grep -B 5 "push_back" "$CPP" | grep "for\|while" | head -3
done
```

### 4c. Verify CLAUDE.md Buffer Patterns

```bash
echo ""
echo "=== Verifying CLAUDE.md Buffer Patterns ==="

# Pattern 1: Member variables for hot-path buffers
echo "1. Checking for member buffer variables..."
for MANAGER in include/managers/*.hpp; do
    MANAGER_NAME=$(basename "$MANAGER" .hpp)
    MEMBER_BUFFERS=$(grep "m_.*Buffer\|m_.*Cache\|m_.*Results" "$MANAGER" | wc -l)
    echo "  $MANAGER_NAME: $MEMBER_BUFFERS reusable buffers"
done

# Pattern 2: clear() over reconstruction
echo ""
echo "2. Checking clear() usage (capacity preservation)..."
for CPP in src/managers/*.cpp; do
    CLEAR_COUNT=$(grep -c "\.clear()" "$CPP")
    RECONSTRUCT_COUNT=$(grep -c "= std::vector" "$CPP")
    echo "  $(basename "$CPP"): clear() = $CLEAR_COUNT, reconstruct = $RECONSTRUCT_COUNT"
    if [ "$RECONSTRUCT_COUNT" -gt "$CLEAR_COUNT" ]; then
        echo "    WARNING: More reconstructions than clears - check for capacity loss"
    fi
done

# Pattern 3: reserve() before loops
echo ""
echo "3. Checking reserve() before insertion loops..."
for CPP in src/managers/*.cpp; do
    echo "  $(basename "$CPP"):"
    grep -B 3 "for.*push_back\|while.*push_back" "$CPP" | grep "reserve(" || echo "    WARNING: No reserve() found before push_back loops"
done
```

---

## Baseline Comparison (optional, any mode)

### Load Baseline Metrics

```bash
BASELINE_DIR="test_results/memory_profiles/baseline"

if [ -d "$BASELINE_DIR" ] && [ "$COMPARE_BASELINE" = "Yes" ]; then
    echo "=== Baseline Comparison ==="

    for LOG in "$OUTPUT_DIR"/*_memcheck.log; do
        TEST_NAME=$(basename "$LOG" _memcheck.log)
        BASELINE_LOG="$BASELINE_DIR/${TEST_NAME}_memcheck.log"

        if [ -f "$BASELINE_LOG" ]; then
            echo ""
            echo "Test: $TEST_NAME"

            CURRENT_LEAKS=$(grep "definitely lost:" "$LOG" | tail -1 | awk '{print $4}')
            CURRENT_LEAKS=${CURRENT_LEAKS:-0}
            BASELINE_LEAKS=$(grep "definitely lost:" "$BASELINE_LOG" | tail -1 | awk '{print $4}')
            BASELINE_LEAKS=${BASELINE_LEAKS:-0}

            if [ "$CURRENT_LEAKS" -gt "$BASELINE_LEAKS" ]; then
                DELTA=$((CURRENT_LEAKS - BASELINE_LEAKS))
                echo "  REGRESSION: +$DELTA bytes leaked (was $BASELINE_LEAKS, now $CURRENT_LEAKS)"
            elif [ "$CURRENT_LEAKS" -lt "$BASELINE_LEAKS" ]; then
                DELTA=$((BASELINE_LEAKS - CURRENT_LEAKS))
                echo "  IMPROVEMENT: -$DELTA bytes leaked (was $BASELINE_LEAKS, now $CURRENT_LEAKS)"
            else
                echo "  STABLE: $CURRENT_LEAKS bytes leaked (unchanged)"
            fi

            CURRENT_HEAP=$(grep "total heap usage:" "$LOG" | tail -1 | awk '{print $5}')
            BASELINE_HEAP=$(grep "total heap usage:" "$BASELINE_LOG" | tail -1 | awk '{print $5}')
            if [ ! -z "$CURRENT_HEAP" ] && [ ! -z "$BASELINE_HEAP" ]; then
                HEAP_DELTA=$((CURRENT_HEAP - BASELINE_HEAP))
                echo "  Total allocs: $CURRENT_HEAP (baseline: $BASELINE_HEAP, delta: $HEAP_DELTA)"
            fi
        fi
    done
fi
```

### Save as New Baseline

```bash
if [ "$BASELINE_MODE" = "Create new baseline" ]; then
    echo ""
    echo "=== Saving New Baseline ==="

    mkdir -p "$BASELINE_DIR"
    cp "$OUTPUT_DIR"/*_memcheck.log "$BASELINE_DIR/" 2>/dev/null || true
    cp "$OUTPUT_DIR"/*_massif.out "$BASELINE_DIR/" 2>/dev/null || true

    cat > "$BASELINE_DIR/baseline_metadata.txt" <<EOF
Baseline created: $(date)
Branch: $(git rev-parse --abbrev-ref HEAD)
Commit: $(git rev-parse HEAD)
Tests included: $(ls "$OUTPUT_DIR"/*_memcheck.log | wc -l)
EOF

    echo "Baseline saved to $BASELINE_DIR"
fi
```
