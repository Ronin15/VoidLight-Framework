/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef SIMD_MATH_HPP
#define SIMD_MATH_HPP

/**
 * @file SIMDMath.hpp
 * @brief Cross-platform SIMD abstraction layer for VoidLight
 *
 * Provides unified SIMD operations that work across:
 * - x86-64: SSE2, AVX2 (Linux, Windows)
 * - ARM64: NEON (Apple Silicon Mac)
 *
 * This abstraction layer allows writing SIMD code once and compiling
 * for multiple platforms without duplicating logic.
 */

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

// ============================================================================
// Platform Detection
// ============================================================================

// SSE2 detection (x86-64)
#if defined(__SSE2__) || \
    (defined(_MSC_VER) && \
     (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#define VOIDLIGHT_SIMD_SSE2 1
#include <emmintrin.h>
#endif

// SSE4.1 detection (x86-64)
#if defined(__SSE4_1__) || (defined(_MSC_VER) && defined(__AVX__))
#define VOIDLIGHT_SIMD_SSE4 1
#include <smmintrin.h>
#endif

// AVX2 detection (x86-64)
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#define VOIDLIGHT_SIMD_AVX2 1
#include <immintrin.h>
#endif

// ARM NEON detection (Apple Silicon)
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define VOIDLIGHT_SIMD_NEON 1
#include <arm_neon.h>
#endif

// ============================================================================
// SIMD Type Definitions
// ============================================================================

namespace VoidLight {
namespace SIMD {

/**
 * @brief 4-wide float vector (cross-platform)
 * Maps to __m128 on x86 or float32x4_t on ARM
 */
#if defined(VOIDLIGHT_SIMD_SSE2)
    using Float4 = __m128;
#elif defined(VOIDLIGHT_SIMD_NEON)
    using Float4 = float32x4_t;
#else
    // Scalar fallback
    struct Float4 {
        float data[4];
    };
#endif

/**
 * @brief 4-wide integer vector (cross-platform)
 * Maps to __m128i on x86 or uint32x4_t on ARM
 */
#if defined(VOIDLIGHT_SIMD_SSE2)
    using Int4 = __m128i;
#elif defined(VOIDLIGHT_SIMD_NEON)
    using Int4 = uint32x4_t;
#else
    // Scalar fallback
    struct Int4 {
        int data[4];
    };
#endif

/**
 * @brief 16-byte vector for byte-level operations
 * Maps to __m128i on x86 or uint8x16_t on ARM
 */
#if defined(VOIDLIGHT_SIMD_SSE2)
    using Byte16 = __m128i;
#elif defined(VOIDLIGHT_SIMD_NEON)
    using Byte16 = uint8x16_t;
#else
    // Scalar fallback
    struct Byte16 {
        uint8_t data[16];
    };
#endif

// ============================================================================
// Load/Store Operations
// ============================================================================

/**
 * @brief Load 4 floats from memory (aligned or unaligned)
 * @param ptr Pointer to 4 consecutive floats
 * @return SIMD vector containing the 4 floats
 */
inline Float4 load4(const float* ptr) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_loadu_ps(ptr);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vld1q_f32(ptr);
#else
    Float4 result;
    result.data[0] = ptr[0];
    result.data[1] = ptr[1];
    result.data[2] = ptr[2];
    result.data[3] = ptr[3];
    return result;
#endif
}

/**
 * @brief Load 4 floats from memory (aligned)
 * @param ptr Pointer to 4 consecutive floats aligned to 16 bytes
 * @return SIMD vector containing the 4 floats
 */
inline Float4 load4_aligned(const float* ptr) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_load_ps(ptr);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vld1q_f32(ptr);
#else
    return load4(ptr);
#endif
}

/**
 * @brief Store 4 floats to memory
 * @param ptr Destination pointer
 * @param v SIMD vector to store
 */
inline void store4(float* ptr, Float4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    _mm_storeu_ps(ptr, v);
#elif defined(VOIDLIGHT_SIMD_NEON)
    vst1q_f32(ptr, v);
#else
    ptr[0] = v.data[0];
    ptr[1] = v.data[1];
    ptr[2] = v.data[2];
    ptr[3] = v.data[3];
#endif
}

/**
 * @brief Store 4 floats to memory (aligned)
 * @param ptr Destination pointer aligned to 16 bytes
 * @param v SIMD vector to store
 */
inline void store4_aligned(float* ptr, Float4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    _mm_store_ps(ptr, v);
#elif defined(VOIDLIGHT_SIMD_NEON)
    vst1q_f32(ptr, v);
#else
    store4(ptr, v);
#endif
}

/**
 * @brief Create SIMD vector by broadcasting a single float to all lanes
 * @param value Float to broadcast
 * @return SIMD vector with all 4 lanes set to value
 */
inline Float4 broadcast(float value) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_set1_ps(value);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vdupq_n_f32(value);
#else
    Float4 result;
    result.data[0] = result.data[1] = result.data[2] = result.data[3] = value;
    return result;
#endif
}

/**
 * @brief Create SIMD vector from 4 individual floats
 * @param x, y, z, w Individual float values
 * @return SIMD vector [x, y, z, w]
 */
inline Float4 set(float x, float y, float z, float w) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_set_ps(w, z, y, x); // Note: SSE uses reverse order
#elif defined(VOIDLIGHT_SIMD_NEON)
    const float data[4] = {x, y, z, w};
    return vld1q_f32(data);
#else
    Float4 result;
    result.data[0] = x;
    result.data[1] = y;
    result.data[2] = z;
    result.data[3] = w;
    return result;
#endif
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

/**
 * @brief Add two SIMD vectors
 */
inline Float4 add(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_add_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vaddq_f32(a, b);
#else
    Float4 result;
    result.data[0] = a.data[0] + b.data[0];
    result.data[1] = a.data[1] + b.data[1];
    result.data[2] = a.data[2] + b.data[2];
    result.data[3] = a.data[3] + b.data[3];
    return result;
#endif
}

/**
 * @brief Subtract two SIMD vectors
 */
inline Float4 sub(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_sub_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vsubq_f32(a, b);
#else
    Float4 result;
    result.data[0] = a.data[0] - b.data[0];
    result.data[1] = a.data[1] - b.data[1];
    result.data[2] = a.data[2] - b.data[2];
    result.data[3] = a.data[3] - b.data[3];
    return result;
#endif
}

/**
 * @brief Multiply two SIMD vectors (component-wise)
 */
inline Float4 mul(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_mul_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vmulq_f32(a, b);
#else
    Float4 result;
    result.data[0] = a.data[0] * b.data[0];
    result.data[1] = a.data[1] * b.data[1];
    result.data[2] = a.data[2] * b.data[2];
    result.data[3] = a.data[3] * b.data[3];
    return result;
#endif
}

/**
 * @brief Fused multiply-add: result = a * b + c
 * More efficient than separate mul + add on modern CPUs
 */
inline Float4 madd(Float4 a, Float4 b, Float4 c) {
#if defined(VOIDLIGHT_SIMD_AVX2) && defined(__FMA__)
    return _mm_fmadd_ps(a, b, c);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vmlaq_f32(c, a, b);
#elif defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_add_ps(_mm_mul_ps(a, b), c);
#else
    Float4 result;
    result.data[0] = a.data[0] * b.data[0] + c.data[0];
    result.data[1] = a.data[1] * b.data[1] + c.data[1];
    result.data[2] = a.data[2] * b.data[2] + c.data[2];
    result.data[3] = a.data[3] * b.data[3] + c.data[3];
    return result;
#endif
}

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * @brief Less-than comparison (returns mask)
 */
inline Float4 cmplt(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_cmplt_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vreinterpretq_f32_u32(vcltq_f32(a, b));
#else
    Float4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? -1.0f : 0.0f;
    }
    return result;
#endif
}

/**
 * @brief Bitwise OR
 */
inline Float4 bitwise_or(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_or_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
#else
    // Use std::bit_cast for portable type-punning (C++20)
    Float4 result{};
    for (int i = 0; i < 4; ++i) {
        uint32_t a_bits = std::bit_cast<uint32_t>(a.data[i]);
        uint32_t b_bits = std::bit_cast<uint32_t>(b.data[i]);
        result.data[i] = std::bit_cast<float>(a_bits | b_bits);
    }
    return result;
#endif
}

/**
 * @brief Extract movemask (for early-exit tests)
 * @return Bitmask where bit i = sign bit of lane i
 */
inline int movemask(Float4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_movemask_ps(v);
#elif defined(VOIDLIGHT_SIMD_NEON)
    // ARM NEON doesn't have direct movemask - use comparison trick
    uint32x4_t const mask = vreinterpretq_u32_f32(v);
    uint32x4_t const shifted = vshrq_n_u32(mask, 31); // Extract sign bits
    uint32_t const result = vgetq_lane_u32(shifted, 0) |
                     (vgetq_lane_u32(shifted, 1) << 1) |
                     (vgetq_lane_u32(shifted, 2) << 2) |
                     (vgetq_lane_u32(shifted, 3) << 3);
    return static_cast<int>(result);
#else
    // Use std::bit_cast for portable type-punning (C++20)
    int result = 0;
    for (int i = 0; i < 4; ++i) {
        uint32_t bits = std::bit_cast<uint32_t>(v.data[i]);
        if (bits & 0x80000000) {
            result |= (1 << i);
        }
    }
    return result;
#endif
}

// ============================================================================
// Min/Max Operations
// ============================================================================

/**
 * @brief Component-wise minimum
 */
inline Float4 min(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_min_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vminq_f32(a, b);
#else
    Float4 result;
    result.data[0] = std::min(a.data[0], b.data[0]);
    result.data[1] = std::min(a.data[1], b.data[1]);
    result.data[2] = std::min(a.data[2], b.data[2]);
    result.data[3] = std::min(a.data[3], b.data[3]);
    return result;
#endif
}

/**
 * @brief Component-wise maximum
 */
inline Float4 max(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_max_ps(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vmaxq_f32(a, b);
#else
    Float4 result;
    result.data[0] = std::max(a.data[0], b.data[0]);
    result.data[1] = std::max(a.data[1], b.data[1]);
    result.data[2] = std::max(a.data[2], b.data[2]);
    result.data[3] = std::max(a.data[3], b.data[3]);
    return result;
#endif
}

/**
 * @brief Component-wise clamp: min(max(v, minVal), maxVal)
 */
inline Float4 clamp(Float4 v, Float4 minVal, Float4 maxVal) {
    return min(max(v, minVal), maxVal);
}

// ============================================================================
// Integer Operations (for layer masks, etc.)
// ============================================================================

/**
 * @brief Broadcast integer to all lanes
 */
inline Int4 broadcast_int(int32_t value) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_set1_epi32(value);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vdupq_n_u32(static_cast<uint32_t>(value));
#else
    Int4 result;
    result.data[0] = result.data[1] = result.data[2] = result.data[3] = value;
    return result;
#endif
}

/**
 * @brief Bitwise AND (integer)
 */
inline Int4 bitwise_and(Int4 a, Int4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_and_si128(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vandq_u32(a, b);
#else
    Int4 result;
    result.data[0] = a.data[0] & b.data[0];
    result.data[1] = a.data[1] & b.data[1];
    result.data[2] = a.data[2] & b.data[2];
    result.data[3] = a.data[3] & b.data[3];
    return result;
#endif
}

/**
 * @brief Compare integers for equality (returns mask)
 */
inline Int4 cmpeq_int(Int4 a, Int4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_cmpeq_epi32(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vceqq_u32(a, b);
#else
    Int4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] == b.data[i]) ? -1 : 0;
    }
    return result;
#endif
}

/**
 * @brief Extract movemask from integer vector
 */
inline int movemask_int(Int4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_movemask_ps(_mm_castsi128_ps(v));
#elif defined(VOIDLIGHT_SIMD_NEON)
    // Similar to float movemask
    uint32x4_t const shifted = vshrq_n_u32(v, 31);
    uint32_t const result = vgetq_lane_u32(shifted, 0) |
                     (vgetq_lane_u32(shifted, 1) << 1) |
                     (vgetq_lane_u32(shifted, 2) << 2) |
                     (vgetq_lane_u32(shifted, 3) << 3);
    return static_cast<int>(result);
#else
    int result = 0;
    for (int i = 0; i < 4; ++i) {
        if (v.data[i] & 0x80000000) {
            result |= (1 << i);
        }
    }
    return result;
#endif
}

/**
 * @brief Create Int4 from 4 individual integers
 */
inline Int4 set_int4(int32_t x, int32_t y, int32_t z, int32_t w) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_set_epi32(w, z, y, x); // Note: SSE uses reverse order
#elif defined(VOIDLIGHT_SIMD_NEON)
    const uint32_t data[4] = {static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                              static_cast<uint32_t>(z), static_cast<uint32_t>(w)};
    return vld1q_u32(data);
#else
    Int4 result;
    result.data[0] = x;
    result.data[1] = y;
    result.data[2] = z;
    result.data[3] = w;
    return result;
#endif
}

/**
 * @brief Load 4 integers from memory
 */
inline Int4 load_int4(const uint32_t* ptr) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vld1q_u32(ptr);
#else
    Int4 result;
    result.data[0] = ptr[0];
    result.data[1] = ptr[1];
    result.data[2] = ptr[2];
    result.data[3] = ptr[3];
    return result;
#endif
}

/**
 * @brief Create zero integer vector
 */
inline Int4 setzero_int() {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_setzero_si128();
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vdupq_n_u32(0);
#else
    Int4 result;
    result.data[0] = result.data[1] = result.data[2] = result.data[3] = 0;
    return result;
#endif
}

/**
 * @brief Bitwise OR (integer)
 */
inline Int4 bitwise_or_int(Int4 a, Int4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_or_si128(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vorrq_u32(a, b);
#else
    Int4 result;
    result.data[0] = a.data[0] | b.data[0];
    result.data[1] = a.data[1] | b.data[1];
    result.data[2] = a.data[2] | b.data[2];
    result.data[3] = a.data[3] | b.data[3];
    return result;
#endif
}

/**
 * @brief Right shift (logical) integer vector by N bits
 */
template<int N>
inline Int4 shift_right_int(Int4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_srli_epi32(v, N);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vshrq_n_u32(v, N);
#else
    Int4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = static_cast<uint32_t>(v.data[i]) >> N;
    }
    return result;
#endif
}

// ============================================================================
// Byte-Level Operations
// ============================================================================

/**
 * @brief Load 16 bytes from memory
 */
inline Byte16 load_byte16(const uint8_t* ptr) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vld1q_u8(ptr);
#else
    Byte16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
#endif
}

/**
 * @brief Broadcast byte to all 16 lanes
 */
inline Byte16 broadcast_byte(uint8_t value) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_set1_epi8(static_cast<char>(value));
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vdupq_n_u8(value);
#else
    Byte16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = value;
    }
    return result;
#endif
}

/**
 * @brief Bitwise AND (byte-level)
 */
inline Byte16 bitwise_and_byte(Byte16 a, Byte16 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_and_si128(a, b);
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vandq_u8(a, b);
#else
    Byte16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
    return result;
#endif
}

/**
 * @brief Greater-than comparison (byte-level, UNSIGNED)
 * Returns 0xFF for true, 0x00 for false per byte.
 * This performs UNSIGNED comparison on all platforms.
 * Used by ParticleManager for unsigned lifetime values.
 */
inline Byte16 cmpgt_byte(Byte16 a, Byte16 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    // SSE only has signed byte comparison (_mm_cmpgt_epi8).
    // To emulate unsigned comparison, XOR both operands with 0x80
    // which flips the sign bit, converting unsigned [0,255] range
    // to signed [-128,127] range while preserving comparison order.
    __m128i sign_flip = _mm_set1_epi8(static_cast<char>(0x80));
    return _mm_cmpgt_epi8(_mm_xor_si128(a, sign_flip), _mm_xor_si128(b, sign_flip));
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vcgtq_u8(a, b);
#else
    Byte16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? 0xFF : 0x00;
    }
    return result;
#endif
}

/**
 * @brief Extract movemask from byte vector (returns 16-bit mask)
 * Each bit represents the sign bit of corresponding byte
 */
inline int movemask_byte(Byte16 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_movemask_epi8(v);
#elif defined(VOIDLIGHT_SIMD_NEON)
    // Correct NEON implementation for movemask_byte.
    // NEON lacks a direct equivalent to SSE's _mm_movemask_epi8,
    // so we use a sequence of shifts and horizontal adds (packed add).

    // 1. Shift the high bit of each byte to the low bit. Result is 0x01 or 0x00.
    uint8x16_t const msbs = vshrq_n_u8(v, 7);

    // 2. Create a vector of powers of 2 to scale the bits.
    const uint8_t p[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    uint8x8_t const powers = vld1_u8(p);

    // 3. Multiply the low and high 8 bytes of MSBs with the powers vector.
    // This places each bit in its correct position within a byte.
    uint8x8_t const low_scaled = vmul_u8(vget_low_u8(msbs), powers);
    uint8x8_t const high_scaled = vmul_u8(vget_high_u8(msbs), powers);

    // 4. Horizontally add the bytes in each 8-byte vector to get the final masks.
    // We use a cascade of pairwise-add-and-widen operations.
    uint16x4_t const low_sum1 = vpaddl_u8(low_scaled);
    uint32x2_t const low_sum2 = vpaddl_u16(low_sum1);
    uint64x1_t const low_sum3 = vpaddl_u32(low_sum2);

    uint16x4_t const high_sum1 = vpaddl_u8(high_scaled);
    uint32x2_t const high_sum2 = vpaddl_u16(high_sum1);
    uint64x1_t const high_sum3 = vpaddl_u32(high_sum2);

    uint8_t const low_mask = vget_lane_u64(low_sum3, 0);
    uint8_t const high_mask = vget_lane_u64(high_sum3, 0);

    return static_cast<int>(low_mask | (high_mask << 8));
#else
    int result = 0;
    for (int i = 0; i < 16; ++i) {
        if (v.data[i] & 0x80) {
            result |= (1 << i);
        }
    }
    return result;
#endif
}

/**
 * @brief Create zero byte vector
 */
inline Byte16 setzero_byte() {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_setzero_si128();
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vdupq_n_u8(0);
#else
    Byte16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = 0;
    }
    return result;
#endif
}

// ============================================================================
// Shuffle and Horizontal Operations
// ============================================================================

/**
 * @brief Shuffle float lanes (SSE-style)
 * For cross-platform code, prefer using higher-level operations
 */
template<int i0, int i1, int i2, int i3>
inline Float4 shuffle(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
#elif defined(VOIDLIGHT_SIMD_NEON)
    // NEON doesn't have direct shuffle, implement common cases
    // This is a simplified version - full implementation would be complex
    alignas(16) float dataA[4], dataB[4], result[4];
    vst1q_f32(dataA, a);
    vst1q_f32(dataB, b);
    result[0] = dataA[i0];
    result[1] = dataA[i1];
    result[2] = dataB[i2];
    result[3] = dataB[i3];
    return vld1q_f32(result);
#else
    Float4 result;
    result.data[0] = (i0 < 4) ? a.data[i0] : b.data[i0 - 4];
    result.data[1] = (i1 < 4) ? a.data[i1] : b.data[i1 - 4];
    result.data[2] = (i2 < 4) ? a.data[i2] : b.data[i2 - 4];
    result.data[3] = (i3 < 4) ? a.data[i3] : b.data[i3 - 4];
    return result;
#endif
}

/**
 * @brief Extract single float from vector
 */
template<int lane>
inline float extract_lane(Float4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(lane, lane, lane, lane)));
#elif defined(VOIDLIGHT_SIMD_NEON)
    return vgetq_lane_f32(v, lane);
#else
    return v.data[lane];
#endif
}

/**
 * @brief Horizontal add - sum all 4 lanes (returns scalar)
 */
inline float horizontal_add(Float4 v) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    Float4 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    Float4 sums = _mm_add_ps(v, shuf);
    shuf = _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(1, 0, 3, 2));
    Float4 result = _mm_add_ps(sums, shuf);
    return _mm_cvtss_f32(result);
#elif defined(VOIDLIGHT_SIMD_NEON)
    float32x2_t const low = vget_low_f32(v);
    float32x2_t const high = vget_high_f32(v);
    float32x2_t const sum = vpadd_f32(low, high);
    float32x2_t const final_sum = vpadd_f32(sum, sum);
    return vget_lane_f32(final_sum, 0);
#else
    return v.data[0] + v.data[1] + v.data[2] + v.data[3];
#endif
}

// ============================================================================
// Vector Math Utilities
// ============================================================================

/**
 * @brief Compute dot product of two 2D vectors (returns scalar)
 * Used for: velocity damping, projection
 */
inline float dot2D(Float4 a, Float4 b) {
#if defined(VOIDLIGHT_SIMD_SSE2)
    Float4 prod = _mm_mul_ps(a, b);
    Float4 shuf = _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1));
    Float4 sum = _mm_add_ps(prod, shuf);
    return _mm_cvtss_f32(sum);
#elif defined(VOIDLIGHT_SIMD_NEON)
    Float4 const prod = vmulq_f32(a, b);
    float32x2_t const sum = vpadd_f32(vget_low_f32(prod), vget_high_f32(prod));
    return vget_lane_f32(sum, 0);
#else
    return a.data[0] * b.data[0] + a.data[1] * b.data[1];
#endif
}

/**
 * @brief Compute squared length of 2D vector (returns scalar)
 * Used for: distance calculations
 */
inline float lengthSquared2D(Float4 v) {
    return dot2D(v, v);
}

/**
 * @brief Compute length of 2D vector (returns scalar)
 */
inline float length2D(Float4 v) {
    return std::sqrt(lengthSquared2D(v));
}

} // namespace SIMD
} // namespace VoidLight

#endif // SIMD_MATH_HPP
