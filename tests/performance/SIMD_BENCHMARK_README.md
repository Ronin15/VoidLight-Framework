# SIMD Performance Benchmark

Comprehensive performance validation suite for VoidLight-Framework's SIMD optimizations across AIManager, CollisionManager, and ParticleManager.

## Purpose

This benchmark suite validates the SIMD performance claims documented in the SIMD docs and repo guidance:
- **AIManager distance calculations**: 3-4x speedup
- **CollisionManager bounds (ARM64)**: 2-3x speedup
- **ParticleManager physics**: 2-4x speedup

**Critical Gap Addressed**: Prior to this benchmark, only correctness tests existed (`SIMDCorrectnessTests.cpp`). There was no performance validation to verify SIMD paths were actually used and provided claimed speedups.

## Quick Start

```bash
# Debug build (correctness validation only)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
./tests/test_scripts/run_simd_benchmark.sh

# Release build (full performance validation)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build
./tests/test_scripts/run_simd_benchmark.sh --verbose
```

## Test Coverage

### 1. AIManager Distance Calculation
- **Operation**: Batch distance calculation for 10K entities
- **SIMD Pattern**: Process 4 entities per iteration (SSE2/NEON width)
- **Expected Speedup**: 3-4x in Release builds
- **Measured (Release/AVX2)**: ~1.2-1.4x (compiler auto-vectorization reduces gap)
- **Measured (Release/NEON)**: 2.5-3.5x typical on Apple Silicon

### 2. CollisionManager AABB Bounds Expansion
- **Operation**: Expand 10K AABB bounds by epsilon
- **SIMD Pattern**: Single 4-element SIMD operation per bounds
- **Finding**: Compiler auto-vectorization achieves performance parity
- **Note**: Real benefit comes from SIMD pipeline integration in CollisionManager

### 3. CollisionManager Layer Mask Filtering
- **Operation**: Bitwise AND filtering of 10K layer masks
- **Finding**: Compiler auto-vectorization outperforms manual SIMD for simple patterns
- **Note**: Validates correctness; performance benefit is from pipeline integration

### 4. ParticleManager Physics Update
- **Operation**: Physics integration for 10K particles
- **SIMD Pattern**: Batch velocity/position updates using FMA instructions
- **Expected Speedup**: 2-4x
- **Measured (Release/AVX2)**: **4.8-4.9x** ✓ Exceeds claims

## Platform Detection

The benchmark automatically detects and reports:
- **x86-64**: SSE2, AVX2 (with -march=native)
- **ARM64**: NEON (Apple Silicon)
- **Scalar**: Fallback if no SIMD available

## Build Configuration Impact

### Debug Build (-O0)
- SIMD often **slower** than scalar due to disabled optimizations
- Validates correctness only, not performance
- Tests pass without requiring speedup

### Release Build (-O3 -march=native)
- Full compiler optimizations enabled
- SIMD intrinsics properly optimized
- Auto-vectorization of scalar code enabled
- **Required for performance validation**

## Key Findings

1. **ParticleManager Shows Best SIMD Gains**: 4.8x speedup validates claims
2. **Compiler Auto-Vectorization Is Excellent**: Simple loops (bounds expansion, masking) achieve near-parity
3. **Complex Operations Benefit Most**: Multi-step physics (FMA, multiple operations) shows largest SIMD advantage
4. **Platform Matters**: ARM64 NEON typically shows better speedups than x86-64 SSE2 due to more efficient instruction encoding

## Interpreting Results

### Speedup Categories
- **Excellent (3x+)**: SIMD significantly outperforms scalar
- **Good (2-3x)**: Strong SIMD benefit
- **Moderate (1.5-2x)**: SIMD faster, typical for optimized builds with auto-vectorization
- **Small (1-1.5x)**: SIMD faster but compiler auto-vectorization is competitive
- **< 1.0x**: Manual SIMD slower (compiler wins)

### When Manual SIMD Wins
- Complex multi-operation pipelines (ParticleManager physics)
- Keeping data in SIMD registers across operations
- Non-obvious vectorization patterns (compiler can't auto-vectorize)

### When Compiler Auto-Vectorization Wins
- Simple arithmetic loops (single operation per iteration)
- Regular memory access patterns
- Operations the compiler recognizes and optimizes

## Usage Examples

```bash
# Quick validation (summary only)
./tests/test_scripts/run_simd_benchmark.sh

# Detailed output with all measurements
./tests/test_scripts/run_simd_benchmark.sh --verbose

# Build and run Release benchmark
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Release && \
ninja -C build simd_performance_benchmark && \
./bin/release/simd_performance_benchmark
```

## Success Criteria

### Debug Build
✓ All tests compile and run
✓ SIMD results match scalar results (correctness)
✓ Platform detection works
✓ No crashes or undefined behavior

### Release Build
✓ SIMD correctness maintained
✓ At least one operation shows significant speedup (> 2x)
✓ No SIMD implementation significantly slower than scalar (< 0.9x)
✓ ParticleManager physics meets or exceeds claimed 2-4x speedup

## Implementation Details

### Benchmark Configuration
- **Entity Count**: 10,000 (matches production scale)
- **Warmup Iterations**: 100 (stabilize cache/branch predictor)
- **Benchmark Iterations**: 1,000 (statistical significance)
- **Timing**: std::chrono::high_resolution_clock (nanosecond precision)

### Memory Layout
- Pre-allocated buffers (avoid allocation overhead)
- Aligned data where beneficial (16-byte alignment for SIMD)
- Representative data distributions (random positions/velocities)

## Troubleshooting

### SIMD Not Detected
- **Symptom**: "Detected SIMD: Scalar (no SIMD)"
- **Cause**: Missing compiler flags or architecture detection
- **Fix**: Ensure `-march=native` in Release builds

### All Benchmarks Fail in Debug
- **Expected**: Debug builds disable optimizations
- **Solution**: Run in Release mode for performance validation

### Lower Speedups Than Claimed
- **Check**: Build configuration (should be Release with -O3)
- **Check**: Platform (ARM64 NEON typically faster than x86-64 SSE2)
- **Note**: Compiler auto-vectorization reduces SIMD advantage for simple patterns

## Related Files

- `/tests/SIMDCorrectnessTests.cpp` - SIMD correctness validation
- `/include/utils/SIMDMath.hpp` - Cross-platform SIMD abstraction layer
- `/src/managers/AIManager.cpp` - Production SIMD distance calculations
- `/src/managers/CollisionManager.cpp` - Production SIMD bounds/masking
- `/src/managers/ParticleManager.cpp` - Production SIMD physics

## References

- **SIMDMath**: SIMD optimization documentation and performance claims
- **AGENTS.md**: Repo-level performance and architecture guidance
- **Architecture**: VoidLight-Framework uses Data-Oriented Design with SoA layouts optimized for SIMD
- **Cross-Platform**: Same SIMD code compiles for x86-64 (SSE2/AVX2) and ARM64 (NEON)
