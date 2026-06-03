# SIMDMath

## Overview

SIMDMath is a cross-platform SIMD (Single Instruction, Multiple Data) abstraction layer for the VoidLight-Framework that provides unified SIMD operations across x86-64 (SSE2/AVX2) and ARM64 (NEON) platforms. This allows writing vectorized code once and compiling for multiple architectures without duplicating logic, achieving 2-4x performance improvements for arithmetic-heavy operations.

## Supported Platforms

### x86-64 Platforms
- **SSE2**: Baseline SIMD support (4-wide float/int operations)
- **SSE4.1**: Enhanced operations (blend, dot product)
- **AVX2**: Advanced operations (FMA, 8-wide operations)
- **Targets**: Intel/AMD CPUs on Linux, Windows, Intel Macs

### ARM64 Platforms
- **NEON**: ARM's SIMD instruction set (4-wide operations)
- **Targets**: Apple Silicon Macs (M1/M2/M3)

### Scalar Fallback
- **Purpose**: Portability and debugging
- **Performance**: No SIMD acceleration, but functionally equivalent
- **Targets**: Platforms without SIMD support

## Platform Detection Macros

### Compile-Time Detection
```cpp
// SSE2 (x86-64 baseline)
#if defined(VOIDLIGHT_SIMD_SSE2)
    // SSE2 code path
#endif

// SSE4.1 (x86-64 enhanced)
#if defined(VOIDLIGHT_SIMD_SSE4)
    // SSE4 code path
#endif

// AVX2 (x86-64 advanced)
#if defined(VOIDLIGHT_SIMD_AVX2)
    // AVX2 code path
#endif

// ARM NEON (Apple Silicon)
#if defined(VOIDLIGHT_SIMD_NEON)
    // NEON code path
#endif
```

### Usage Pattern
```cpp
#include "utils/SIMDMath.hpp"

void processData(float* data, size_t count) {
#if defined(VOIDLIGHT_SIMD_SSE2) || defined(VOIDLIGHT_SIMD_NEON)
    // Unified SIMD path (works on both x86 and ARM)
    using namespace VoidLight::SIMD;

    for (size_t i = 0; i + 3 < count; i += 4) {
        Float4 v = load4(&data[i]);
        v = mul(v, broadcast(2.0f));
        store4(&data[i], v);
    }

    // Scalar tail for remaining elements
    for (size_t i = (count & ~3); i < count; ++i) {
        data[i] *= 2.0f;
    }
#else
    // Scalar fallback
    for (size_t i = 0; i < count; ++i) {
        data[i] *= 2.0f;
    }
#endif
}
```

## Type Definitions

### Float4 - 4-Wide Float Vector
```cpp
using Float4 = __m128;           // x86-64: SSE2 128-bit vector
using Float4 = float32x4_t;      // ARM64: NEON 128-bit vector
struct Float4 { float data[4]; } // Scalar fallback
```

**Usage**: Primary type for 4-wide float operations
- Collision detection (AABB bounds)
- Particle physics (position, velocity)
- AI pathfinding (distance calculations)

### Int4 - 4-Wide Integer Vector
```cpp
using Int4 = __m128i;            // x86-64: SSE2 integer vector
using Int4 = uint32x4_t;         // ARM64: NEON integer vector
struct Int4 { int data[4]; }     // Scalar fallback
```

**Usage**: Layer masks, integer comparisons, indices

### Byte16 - 16-Byte Vector
```cpp
using Byte16 = __m128i;          // x86-64: SSE2 byte vector
using Byte16 = uint8x16_t;       // ARM64: NEON byte vector
struct Byte16 { uint8_t data[16]; } // Scalar fallback
```

**Usage**: Byte-level operations, bit flags, particle active masks

## Core Operations Reference

### Load/Store Operations

#### `Float4 load4(const float* ptr)`
Loads 4 consecutive floats from memory (aligned or unaligned).
```cpp
float positions[4] = {1.0f, 2.0f, 3.0f, 4.0f};
Float4 v = load4(positions); // [1.0, 2.0, 3.0, 4.0]
```

#### `void store4(float* ptr, Float4 v)`
Stores 4 floats to memory.
```cpp
float output[4];
store4(output, v); // output = [1.0, 2.0, 3.0, 4.0]
```

#### `Float4 broadcast(float value)`
Creates vector with same value in all lanes.
```cpp
Float4 v = broadcast(5.0f); // [5.0, 5.0, 5.0, 5.0]
```

#### `Float4 set(float x, float y, float z, float w)`
Creates vector from 4 individual values.
```cpp
Float4 v = set(1.0f, 2.0f, 3.0f, 4.0f); // [1.0, 2.0, 3.0, 4.0]
```

### Arithmetic Operations

#### `Float4 add(Float4 a, Float4 b)`
Component-wise addition.
```cpp
Float4 a = set(1.0f, 2.0f, 3.0f, 4.0f);
Float4 b = set(5.0f, 6.0f, 7.0f, 8.0f);
Float4 c = add(a, b); // [6.0, 8.0, 10.0, 12.0]
```

#### `Float4 sub(Float4 a, Float4 b)`
Component-wise subtraction.
```cpp
Float4 c = sub(b, a); // [4.0, 4.0, 4.0, 4.0]
```

#### `Float4 mul(Float4 a, Float4 b)`
Component-wise multiplication.
```cpp
Float4 c = mul(a, b); // [5.0, 12.0, 21.0, 32.0]
```

#### `Float4 madd(Float4 a, Float4 b, Float4 c)`
Fused multiply-add: `result = a * b + c`.
- **Performance**: Single instruction on AVX2/NEON (more efficient than separate mul + add)
```cpp
Float4 result = madd(a, b, c); // a * b + c
```

### Comparison Operations

#### `Float4 cmplt(Float4 a, Float4 b)`
Less-than comparison (returns mask).
- **Result**: All-bits-set (0xFFFFFFFF) for true, zero for false
```cpp
Float4 a = set(1.0f, 5.0f, 3.0f, 7.0f);
Float4 b = set(2.0f, 4.0f, 3.0f, 6.0f);
Float4 mask = cmplt(a, b); // [true, false, false, false]
```

#### `int movemask(Float4 v)`
Extracts sign bits from vector (for early-exit tests).
- **Returns**: 4-bit integer where bit i = sign bit of lane i
```cpp
int mask = movemask(cmplt(a, b)); // 0x1 (only first lane is true)
if (mask == 0) {
    // No lanes are true, early exit
}
```

### Min/Max Operations

#### `Float4 min(Float4 a, Float4 b)`
Component-wise minimum.
```cpp
Float4 a = set(1.0f, 5.0f, 3.0f, 7.0f);
Float4 b = set(2.0f, 4.0f, 6.0f, 1.0f);
Float4 c = min(a, b); // [1.0, 4.0, 3.0, 1.0]
```

#### `Float4 max(Float4 a, Float4 b)`
Component-wise maximum.
```cpp
Float4 c = max(a, b); // [2.0, 5.0, 6.0, 7.0]
```

#### `Float4 clamp(Float4 v, Float4 minVal, Float4 maxVal)`
Component-wise clamp: `min(max(v, minVal), maxVal)`.
```cpp
Float4 v = set(-1.0f, 0.5f, 1.5f, 2.0f);
Float4 clamped = clamp(v, broadcast(0.0f), broadcast(1.0f)); // [0.0, 0.5, 1.0, 1.0]
```

### Integer Operations

#### `Int4 broadcast_int(int32_t value)`
Broadcasts integer to all lanes.
```cpp
Int4 layer = broadcast_int(0x01); // [0x01, 0x01, 0x01, 0x01]
```

#### `Int4 bitwise_and_int(Int4 a, Int4 b)`
Bitwise AND for integers.
```cpp
Int4 result = bitwise_and_int(layerMask, collideMask);
```

#### `int movemask_int(Int4 v)`
Extracts sign bits from integer vector.

### Vector Math Utilities

#### `float dot2D(Float4 a, Float4 b)`
Computes 2D dot product (returns scalar).
- **Uses**: First 2 lanes (x, y)
```cpp
Float4 a = set(3.0f, 4.0f, 0.0f, 0.0f);
Float4 b = set(1.0f, 2.0f, 0.0f, 0.0f);
float dot = dot2D(a, b); // 3*1 + 4*2 = 11.0
```

#### `float lengthSquared2D(Float4 v)`
Computes squared length of 2D vector (returns scalar).
```cpp
Float4 v = set(3.0f, 4.0f, 0.0f, 0.0f);
float lenSq = lengthSquared2D(v); // 3*3 + 4*4 = 25.0
```

#### `float length2D(Float4 v)`
Computes length of 2D vector (returns scalar).
```cpp
float len = length2D(v); // sqrt(25.0) = 5.0
```

#### `float horizontal_add(Float4 v)`
Sums all 4 lanes (returns scalar).
```cpp
Float4 v = set(1.0f, 2.0f, 3.0f, 4.0f);
float sum = horizontal_add(v); // 1 + 2 + 3 + 4 = 10.0
```

## Practical Examples

### Example 1: Distance Calculations (AIManager)
```cpp
// Compute distances from AI entity to 4 targets simultaneously
void computeDistances(const Vector2D& aiPos, const Vector2D targets[4], float distances[4]) {
    using namespace VoidLight::SIMD;

    // Load AI position into all lanes
    Float4 aiX = broadcast(aiPos.x);
    Float4 aiY = broadcast(aiPos.y);

    // Load target positions (structure-of-arrays layout)
    float targetX[4] = {targets[0].x, targets[1].x, targets[2].x, targets[3].x};
    float targetY[4] = {targets[0].y, targets[1].y, targets[2].y, targets[3].y};

    Float4 tX = load4(targetX);
    Float4 tY = load4(targetY);

    // Compute delta vectors
    Float4 dx = sub(tX, aiX);
    Float4 dy = sub(tY, aiY);

    // Compute squared distances
    Float4 dxSq = mul(dx, dx);
    Float4 dySq = mul(dy, dy);
    Float4 distSq = add(dxSq, dySq);

    // Store results
    store4(distances, distSq);
}
```

### Example 2: AABB Bounds Calculation (CollisionManager)
```cpp
// Compute AABB bounds for 4 collision bodies simultaneously
struct AABB {
    float minX, minY, maxX, maxY;
};

void computeBounds(const Vector2D centers[4], const Vector2D halfsizes[4], AABB bounds[4]) {
    using namespace VoidLight::SIMD;

    // Load centers
    float centerX[4] = {centers[0].x, centers[1].x, centers[2].x, centers[3].x};
    float centerY[4] = {centers[0].y, centers[1].y, centers[2].y, centers[3].y};
    Float4 cx = load4(centerX);
    Float4 cy = load4(centerY);

    // Load halfsizes
    float halfsizeX[4] = {halfsizes[0].x, halfsizes[1].x, halfsizes[2].x, halfsizes[3].x};
    float halfsizeY[4] = {halfsizes[0].y, halfsizes[1].y, halfsizes[2].y, halfsizes[3].y};
    Float4 hx = load4(halfsizeX);
    Float4 hy = load4(halfsizeY);

    // Compute bounds
    Float4 minX = sub(cx, hx);
    Float4 minY = sub(cy, hy);
    Float4 maxX = add(cx, hx);
    Float4 maxY = add(cy, hy);

    // Store results (would need proper AABB storage layout)
    float minXArray[4], minYArray[4], maxXArray[4], maxYArray[4];
    store4(minXArray, minX);
    store4(minYArray, minY);
    store4(maxXArray, maxX);
    store4(maxYArray, maxY);

    for (int i = 0; i < 4; ++i) {
        bounds[i].minX = minXArray[i];
        bounds[i].minY = minYArray[i];
        bounds[i].maxX = maxXArray[i];
        bounds[i].maxY = maxYArray[i];
    }
}
```

### Example 3: Layer Mask Filtering (CollisionManager)
```cpp
// Check collision layer masks for 4 bodies simultaneously
bool canCollide(uint32_t layerMasks[4], uint32_t collideMasks[4], bool results[4]) {
    using namespace VoidLight::SIMD;

    // Load masks
    Int4 layers = _mm_loadu_si128(reinterpret_cast<const __m128i*>(layerMasks));
    Int4 collides = _mm_loadu_si128(reinterpret_cast<const __m128i*>(collideMasks));

    // Bitwise AND to test overlap
    Int4 overlap = bitwise_and_int(layers, collides);

    // Check if any bits set (non-zero means can collide)
    Int4 zero = broadcast_int(0);
    Int4 mask = cmpgt_int(overlap, zero);

    // Extract results
    int movemask = movemask_int(mask);
    results[0] = (movemask & 0x1) != 0;
    results[1] = (movemask & 0x2) != 0;
    results[2] = (movemask & 0x4) != 0;
    results[3] = (movemask & 0x8) != 0;

    return movemask != 0; // Returns true if any can collide
}
```

### Example 4: Particle Position Update (ParticleManager)
```cpp
// Update 4 particle positions simultaneously
void updateParticles(float posX[4], float posY[4],
                     const float velX[4], const float velY[4],
                     float deltaTime) {
    using namespace VoidLight::SIMD;

    // Load positions and velocities
    Float4 px = load4(posX);
    Float4 py = load4(posY);
    Float4 vx = load4(velX);
    Float4 vy = load4(velY);

    // Broadcast delta time
    Float4 dt = broadcast(deltaTime);

    // Update positions: pos += vel * dt (using FMA)
    px = madd(vx, dt, px);
    py = madd(vy, dt, py);

    // Store updated positions
    store4(posX, px);
    store4(posY, py);
}
```

## Performance Considerations

### Speedup Expectations
- **Arithmetic Operations**: 3-4x speedup (4 values per instruction)
- **Memory-Bound Operations**: 1.5-2x speedup (cache/bandwidth limited)
- **Scalar Tail Processing**: Minimal overhead when handling non-multiple-of-4 counts

### Best Practices

#### 1. Structure of Arrays (SoA) Layout
```cpp
// GOOD: SoA layout (SIMD-friendly)
struct Entities {
    float posX[1000];
    float posY[1000];
    float velX[1000];
    float velY[1000];
};

// BAD: Array of Structures (AoS) layout
struct Entity {
    float posX, posY, velX, velY;
};
Entity entities[1000]; // Requires gather/scatter (slow)
```

#### 2. Alignment
```cpp
// Prefer aligned loads when possible
alignas(16) float data[8];
Float4 v = load4(data); // Aligned load (faster on some platforms)
```

#### 3. Process 4 Elements at a Time
```cpp
void processData(float* data, size_t count) {
    using namespace VoidLight::SIMD;

    size_t i = 0;

    // SIMD loop (process 4 at once)
    for (; i + 3 < count; i += 4) {
        Float4 v = load4(&data[i]);
        v = mul(v, broadcast(2.0f));
        store4(&data[i], v);
    }

    // Scalar tail (remaining 0-3 elements)
    for (; i < count; ++i) {
        data[i] *= 2.0f;
    }
}
```

#### 4. Avoid Branching in SIMD Loops
```cpp
// BAD: Branches in SIMD loop
for (size_t i = 0; i + 3 < count; i += 4) {
    Float4 v = load4(&data[i]);
    if (some_condition) { // Avoid!
        v = mul(v, broadcast(2.0f));
    }
    store4(&data[i], v);
}

// GOOD: Use masking instead
for (size_t i = 0; i + 3 < count; i += 4) {
    Float4 v = load4(&data[i]);
    Float4 mask = some_simd_condition(v);
    Float4 scaled = mul(v, broadcast(2.0f));
    v = blend(v, scaled, mask); // Conditional move, no branch
    store4(&data[i], v);
}
```

#### 5. Profile and Measure
```cpp
// Always profile SIMD code vs scalar baseline
// SIMD isn't always faster (e.g., for small datasets)
if (count < 16) {
    // Use scalar path for small counts
    for (size_t i = 0; i < count; ++i) {
        processScalar(data[i]);
    }
} else {
    // Use SIMD path for large counts
    processSIMD(data, count);
}
```

### Platform-Specific Notes

#### x86-64 SSE2
- **Availability**: All x86-64 CPUs
- **Width**: 4 floats or 4 ints
- **Performance**: Baseline SIMD, good for most operations

#### x86-64 AVX2
- **Availability**: Intel Haswell+ (2013), AMD Excavator+ (2015)
- **Width**: 8 floats or 8 ints (not exposed by SIMDMath yet)
- **Performance**: FMA support for madd(), 2x throughput

#### ARM NEON
- **Availability**: All ARM64 CPUs (Apple Silicon M1/M2/M3)
- **Width**: 4 floats or 4 ints
- **Performance**: Competitive with SSE2, excellent FMA support

### Performance Metrics
Based on VoidLight-Framework benchmarks:
- **AIManager Distance Calculations**: 3.2x speedup on 10,000+ entities
- **CollisionManager AABB Operations**: 2.8x speedup on 10,000+ bodies
- **ParticleManager Updates**: 3.5x speedup on 50,000+ particles

## Migration Guide

### Migrating from Old SIMD Code

#### Before (Platform-Specific)
```cpp
#ifdef __AVX2__
    // x86 AVX2 code
    __m256 v = _mm256_loadu_ps(data);
    v = _mm256_mul_ps(v, _mm256_set1_ps(2.0f));
    _mm256_storeu_ps(data, v);
#elif __ARM_NEON
    // ARM NEON code
    float32x4_t v = vld1q_f32(data);
    v = vmulq_f32(v, vdupq_n_f32(2.0f));
    vst1q_f32(data, v);
#endif
```

#### After (Cross-Platform)
```cpp
using namespace VoidLight::SIMD;

Float4 v = load4(data);
v = mul(v, broadcast(2.0f));
store4(data, v);
```

### Adding SIMD to Existing Code

#### Step 1: Identify Hot Loops
```bash
# Profile to find hotspots
./bin/debug/AIManager_Benchmark

# Look for arithmetic-heavy loops
```

#### Step 2: Convert to SoA Layout
```cpp
// Before: AoS
struct Particle {
    Vector2D pos, vel;
};
std::vector<Particle> particles;

// After: SoA
struct ParticleSystem {
    std::vector<float> posX, posY;
    std::vector<float> velX, velY;
};
```

#### Step 3: Implement SIMD Path
```cpp
#include "utils/SIMDMath.hpp"

void updateParticles(ParticleSystem& ps, float dt) {
    using namespace VoidLight::SIMD;

    size_t count = ps.posX.size();
    size_t i = 0;

    // SIMD loop
    for (; i + 3 < count; i += 4) {
        Float4 px = load4(&ps.posX[i]);
        Float4 vx = load4(&ps.velX[i]);
        px = madd(vx, broadcast(dt), px);
        store4(&ps.posX[i], px);
        // ... similar for Y
    }

    // Scalar tail
    for (; i < count; ++i) {
        ps.posX[i] += ps.velX[i] * dt;
        ps.posY[i] += ps.velY[i] * dt;
    }
}
```

## Debugging Tips

### Enable Scalar Fallback
```bash
# Compile without SIMD flags to test scalar path
cmake -B build/ -DCMAKE_CXX_FLAGS="" # No -march=native
```

### Print SIMD Vectors
```cpp
void printFloat4(const char* name, Float4 v) {
    alignas(16) float data[4];
    store4(data, v);
    printf("%s: [%.2f, %.2f, %.2f, %.2f]\n", name, data[0], data[1], data[2], data[3]);
}

// Usage
Float4 v = broadcast(5.0f);
printFloat4("v", v); // Prints: v: [5.00, 5.00, 5.00, 5.00]
```

### Verify SIMD Path Selection
```cpp
#if defined(VOIDLIGHT_SIMD_AVX2)
    LOGGER_INFO("Using AVX2 SIMD path");
#elif defined(VOIDLIGHT_SIMD_SSE2)
    LOGGER_INFO("Using SSE2 SIMD path");
#elif defined(VOIDLIGHT_SIMD_NEON)
    LOGGER_INFO("Using NEON SIMD path");
#else
    LOGGER_INFO("Using scalar fallback path");
#endif
```

## See Also

- [AIManager Documentation](../ai/AIManager.md) - SIMD distance calculations
- [CollisionManager Documentation](../managers/CollisionManager.md) - SIMD AABB operations
- [ParticleManager Documentation](../managers/ParticleManager.md) - SIMD particle updates
- [AGENTS.md](../../AGENTS.md) - Repo build and architecture guidance for SIMD-related work
