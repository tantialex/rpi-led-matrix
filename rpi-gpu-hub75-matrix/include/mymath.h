
#if defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif



static inline int32_t saturating_add(int32_t a, int32_t b) {
#if defined(__arm__) || defined(__aarch64__)
    return __builtin_arm_qadd(a, b);  // GCC/Clang built-in for ARM saturated add


#elif defined(__SSE2__)  // x86 with SSE2
    __m128i va = _mm_set1_epi32(a);
    __m128i vb = _mm_set1_epi32(b);
    __m128i result = _mm_adds_epi32(va, vb);  // SSE2 saturated add
    return _mm_cvtsi128_si32(result);

#else  // Portable software-based saturated arithmetic
    int32_t result = a + b;
    if (((b > 0) && (result < a)) || ((b < 0) && (result > a))) {
        result = (b > 0) ? INT32_MAX : INT32_MIN;  // Saturate on overflow or underflow
    }
    return result;
#endif
}