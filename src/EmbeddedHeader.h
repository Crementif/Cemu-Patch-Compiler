#pragma once

// The trigonometric kernels below originate from musl's __sindf.c,
// __cosdf.c, and __tandf.c:
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this software is freely
// granted, provided that this notice is preserved.
//
// Copyright 2004 Sun Microsystems, Inc. All Rights Reserved.
// Permission to use, copy, modify, and distribute this software is freely
// granted, provided that this notice is preserved.

inline constexpr std::string_view kCemuPatchImplicitHeader = R"CEMU_HEADER(
#pragma once
#define _CEMU_PATCH_STRINGIFY(x) #x
#define _CEMU_PATCH_TOSTRING(x) _CEMU_PATCH_STRINGIFY(x)
#define PATCH_NOP(addr) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = nop")
#define PATCH_JUMP_BLA(addr, func) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = bla " #func)
#define PATCH_WRITE(addr, instr) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = " instr)
#define PATCH_INT(addr, value) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = .int " _CEMU_PATCH_TOSTRING(value))
#define PATCH_FLOAT(addr, value) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = .float " _CEMU_PATCH_TOSTRING(value))
#define _CEMU_PATCH_CONCAT_IMPL(a, b) a##b
#define _CEMU_PATCH_CONCAT(a, b) _CEMU_PATCH_CONCAT_IMPL(a, b)
#define _CEMU_PATCH_ENTRY_IMPL(func, callbackName) \
    extern "C" __attribute__((used)) void callbackName() { (void)(func()); } \
    asm("\n# __CEMU_PATCH: .callback entry " _CEMU_PATCH_TOSTRING(callbackName))
#define PATCH_ON_ENTRY(func) \
    _CEMU_PATCH_ENTRY_IMPL(func, _CEMU_PATCH_CONCAT(cemuPatchEntry, __COUNTER__))
// Compatibility spelling: Cemu invokes this immediately before the title entrypoint.
#define PATCH_ON_LOAD(func) PATCH_ON_ENTRY(func)

#define CEMU_PATCH_ALWAYS_INLINE __attribute__((always_inline)) inline

#ifdef __cplusplus
extern "C" {
#endif

typedef union CemuPatchFloatBits {
    float f;
    unsigned int u;
} CemuPatchFloatBits;

static CEMU_PATCH_ALWAYS_INLINE float cemu_patch_invalidf(float x) {
    const float zero = x - x;
    return zero / zero;
}

static CEMU_PATCH_ALWAYS_INLINE double cemu_patch_invalid(double x) {
    const double zero = x - x;
    return zero / zero;
}

static CEMU_PATCH_ALWAYS_INLINE double cemu_patch_rsqrt_estimate(double x) {
    double estimate;
    asm("frsqrte %0, %1" : "=f"(estimate) : "f"(x));
    return estimate;
}

// Fast approximate square root. This deliberately performs no refinement.
CEMU_PATCH_ALWAYS_INLINE float cemu_patch_fast_sqrtf(float x) {
    if (x == 0.0f || x != x)
        return x;
    if (x < 0.0f)
        return cemu_patch_invalidf(x);
    if (x > 0x1.fffffep+127f)
        return x;
    return (float)((double)x * cemu_patch_rsqrt_estimate((double)x));
}

CEMU_PATCH_ALWAYS_INLINE double cemu_patch_fast_sqrt(double x) {
    if (x == 0.0 || x != x)
        return x;
    if (x < 0.0)
        return cemu_patch_invalid(x);
    if (x > 0x1.fffffffffffffp+1023)
        return x;
    return x * cemu_patch_rsqrt_estimate(x);
}

// Refine the reciprocal-square-root estimate in double precision. Three
// Newton steps take the architectural 1/32 estimate past float precision.
CEMU_PATCH_ALWAYS_INLINE float sqrtf(float x) {
    if (x == 0.0f || x != x)
        return x;
    if (x < 0.0f)
        return cemu_patch_invalidf(x);
    if (x > 0x1.fffffep+127f)
        return x;

    const double value = (double)x;
    const double half = 0.5 * value;
    double estimate = cemu_patch_rsqrt_estimate(value);
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    return (float)(value * estimate);
}

CEMU_PATCH_ALWAYS_INLINE double sqrt(double x) {
    if (x == 0.0 || x != x)
        return x;
    if (x < 0.0)
        return cemu_patch_invalid(x);
    if (x > 0x1.fffffffffffffp+1023)
        return x;

    double value = x;
    double resultScale = 1.0;
    if (value < 0x1p-1022) {
        value *= 0x1p+512;
        resultScale = 0x1p-256;
    }

    const double half = 0.5 * value;
    double estimate = cemu_patch_rsqrt_estimate(value);
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    return value * estimate * resultScale;
}

CEMU_PATCH_ALWAYS_INLINE float fabsf(float x) {
    float result;
    asm("fabs %0, %1" : "=f"(result) : "f"(x));
    return result;
}

CEMU_PATCH_ALWAYS_INLINE float copysignf(float magnitude, float sign) {
    CemuPatchFloatBits magnitudeBits = {magnitude};
    CemuPatchFloatBits signBits = {sign};
    magnitudeBits.u = (magnitudeBits.u & 0x7fffffffu) |
                      (signBits.u & 0x80000000u);
    return magnitudeBits.f;
}

CEMU_PATCH_ALWAYS_INLINE float fminf(float x, float y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    if (x == y) {
        CemuPatchFloatBits xBits = {x};
        CemuPatchFloatBits yBits = {y};
        xBits.u |= yBits.u;
        return xBits.f;
    }
    return x < y ? x : y;
}

CEMU_PATCH_ALWAYS_INLINE float fmaxf(float x, float y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    if (x == y) {
        CemuPatchFloatBits xBits = {x};
        CemuPatchFloatBits yBits = {y};
        xBits.u &= yBits.u;
        return xBits.f;
    }
    return x > y ? x : y;
}

CEMU_PATCH_ALWAYS_INLINE float truncf(float x) {
    const float magnitude = fabsf(x);
    if (magnitude >= 0x1p+23f || x != x)
        return x;
    if (magnitude < 1.0f)
        return x * 0.0f;
    return (float)(int)x;
}

CEMU_PATCH_ALWAYS_INLINE float floorf(float x) {
    float result = truncf(x);
    if (result > x)
        result -= 1.0f;
    return result;
}

CEMU_PATCH_ALWAYS_INLINE float ceilf(float x) {
    float result = truncf(x);
    if (result < x)
        result += 1.0f;
    return result;
}

CEMU_PATCH_ALWAYS_INLINE float roundf(float x) {
    const float magnitude = fabsf(x);
    if (magnitude >= 0x1p+23f || x != x)
        return x;
    if (magnitude < 0.5f)
        return x * 0.0f;
    return (float)(int)(x < 0.0f ? x - 0.5f : x + 0.5f);
}

static CEMU_PATCH_ALWAYS_INLINE int cemu_patch_reduce_pio2f(
    float x, double* reduced) {
    const double absX = x < 0.0f ? -(double)x : (double)x;
    if (absX != absX || absX > 0x1.921fb6p+28) {
        *reduced = (double)(x - x);
        return 0;
    }
    if (absX <= 0x1.921fb6p-1) {
        *reduced = (double)x;
        return 0;
    }

    const double scaled = (double)x * 6.36619772367581382433e-1;
    const int quadrant = (int)(scaled + (scaled < 0.0 ? -0.5 : 0.5));
    double result = (double)x;
    result -= quadrant * 1.57079631090164184570e+0;
    result -= quadrant * 1.58932547735281966916e-8;
    *reduced = result;
    return quadrant;
}

static CEMU_PATCH_ALWAYS_INLINE double cemu_patch_sin_kernel(double x) {
    const double z = x * x;
    const double z2 = z * z;
    const double tail = -1.98393348360966317347e-4 +
                        z * 2.7183114939898219064e-6;
    return (x + (z * x) * (-1.66666666416265235595e-1 +
                           z * 8.3333293858894631756e-3)) +
           (z * x) * z2 * tail;
}

static CEMU_PATCH_ALWAYS_INLINE double cemu_patch_cos_kernel(double x) {
    const double z = x * x;
    const double z2 = z * z;
    const double tail = -1.38867637746099294692e-3 +
                        z * 2.43904487962774090654e-5;
    return ((1.0 + z * -4.99999997251031003120e-1) +
            z2 * 4.16666233237390631894e-2) +
           (z2 * z) * tail;
}

static CEMU_PATCH_ALWAYS_INLINE double cemu_patch_tan_kernel(double x) {
    const double z = x * x;
    const double z2 = z * z;
    const double cubic = z * x;
    const double high = 2.97435743359967304927e-3 +
                        z * 9.46564784943673166728e-3;
    const double middle = 5.33812378445670393523e-2 +
                          z * 2.45283181166547278873e-2;
    const double low = 3.33331395030791399758e-1 +
                       z * 1.33392002712976742718e-1;
    return (x + cubic * low) + (cubic * z2) * (middle + z2 * high);
}

CEMU_PATCH_ALWAYS_INLINE float sinf(float x) {
    double reduced;
    const unsigned int quadrant =
        (unsigned int)cemu_patch_reduce_pio2f(x, &reduced) & 3u;
    if (quadrant == 0u)
        return (float)cemu_patch_sin_kernel(reduced);
    if (quadrant == 1u)
        return (float)cemu_patch_cos_kernel(reduced);
    if (quadrant == 2u)
        return (float)-cemu_patch_sin_kernel(reduced);
    return (float)-cemu_patch_cos_kernel(reduced);
}

CEMU_PATCH_ALWAYS_INLINE float cosf(float x) {
    double reduced;
    const unsigned int quadrant =
        (unsigned int)cemu_patch_reduce_pio2f(x, &reduced) & 3u;
    if (quadrant == 0u)
        return (float)cemu_patch_cos_kernel(reduced);
    if (quadrant == 1u)
        return (float)-cemu_patch_sin_kernel(reduced);
    if (quadrant == 2u)
        return (float)-cemu_patch_cos_kernel(reduced);
    return (float)cemu_patch_sin_kernel(reduced);
}

CEMU_PATCH_ALWAYS_INLINE float tanf(float x) {
    double reduced;
    const int quadrant = cemu_patch_reduce_pio2f(x, &reduced);
    const double result = cemu_patch_tan_kernel(reduced);
    return (float)((quadrant & 1) ? -1.0 / result : result);
}

#ifdef __cplusplus
}
#endif

// Explicit builtins select the matching local helper, since the 750 has no fsqrt and no math library to call.
// cemu_patch_fast_sqrtf and cemu_patch_fast_sqrt give the unrefined estimate instead.
#define __builtin_sqrtf(value) sqrtf(value)
#define __builtin_sqrt(value) sqrt(value)
#define __builtin_fabsf(value) fabsf(value)
#define __builtin_copysignf(magnitude, sign) copysignf(magnitude, sign)
#define __builtin_fminf(x, y) fminf(x, y)
#define __builtin_fmaxf(x, y) fmaxf(x, y)
#define __builtin_truncf(value) truncf(value)
#define __builtin_floorf(value) floorf(value)
#define __builtin_ceilf(value) ceilf(value)
#define __builtin_roundf(value) roundf(value)
#define __builtin_sinf(value) sinf(value)
#define __builtin_cosf(value) cosf(value)
#define __builtin_tanf(value) tanf(value)

#undef CEMU_PATCH_ALWAYS_INLINE
)CEMU_HEADER";
