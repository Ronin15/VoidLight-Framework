---
paths:
  - "include/utils/SIMDMath*"
  - "src/utils/SIMDMath*"
  - "**/*SIMD*"
---

# SIMD Rules

`include/utils/SIMDMath.hpp` provides SSE2/NEON/AVX2 implementations.

- Process 4 elements per iteration, plus a scalar tail loop for the remainder.
- Always provide a scalar fallback path.
