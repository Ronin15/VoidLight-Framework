---
paths:
  - "include/utils/SIMDMath*"
  - "src/utils/SIMDMath*"
  - "**/*SIMD*"
---

# SIMD Rules

`include/utils/SIMDMath.hpp` provides SSE2/AVX2 (x86-64) and NEON (ARM64)
implementations. Use it for all SIMD work; do not add raw intrinsics
elsewhere.

- Process 4 elements per iteration, plus a scalar tail loop for the
  remainder.
- Always provide a scalar fallback path.
- Non-Apple Release builds are AVX2-minimum — no runtime CPU dispatch.
