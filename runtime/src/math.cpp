// provides: fabs copysign fmin fmax fdim fdimf trunc floor ceil round lround llround lroundf llroundf rint rintf
// provides: nearbyint nearbyintf lrint llrint lrintf llrintf fmod fmodf modf modff frexp frexpf ldexp ldexpf scalbn
// provides: scalbnf fma fmaf sin cos tan asin acos atan atan2 asinf acosf atanf atan2f sinh cosh tanh sinhf coshf
// provides: tanhf asinh acosh atanh asinhf acoshf atanhf exp exp2 expm1 expf exp2f expm1f log log2 log10 log1p logf
// provides: log2f log10f log1pf pow powf cbrt cbrtf hypot hypotf scalbln scalblnf ilogb ilogbf logb logbf remainder
// provides: remainderf remquo remquof nextafter nextafterf nexttoward nexttowardf sqrt sqrtf cemu_patch_fast_sqrt
// provides: cemu_patch_fast_sqrtf sinf cosf tanf fabsf copysignf fminf fmaxf truncf floorf ceilf roundf
// The kernels of sinf, cosf and tanf originate from musl's __sindf.c, __cosdf.c, and __tandf.c:
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this software is freely
// granted, provided that this notice is preserved.
//
// Copyright 2004 Sun Microsystems, Inc. All Rights Reserved.
// Permission to use, copy, modify, and distribute this software is freely
// granted, provided that this notice is preserved.

#include <math.h>

// Error compensation below relies on each product being rounded on its own
#pragma clang fp contract(off)

typedef unsigned long long u64;
typedef unsigned u32;

namespace {

u64 bitsOf(double x) {
    return __builtin_bit_cast(u64, x);
}

double fromBits(u64 bits) {
    return __builtin_bit_cast(double, bits);
}

u32 bitsOf(float x) {
    return __builtin_bit_cast(u32, x);
}

float fromBits(u32 bits) {
    return __builtin_bit_cast(float, bits);
}

int exponentOf(double x) {
    return (int)(bitsOf(x) >> 52 & 0x7ff);
}

constexpr u64 kSign = 1ull << 63;
constexpr double kToInt = 0x1p52;
constexpr double kInfinity = __builtin_inf();
constexpr double kPio2Hi = 1.57079632679489655800e+00;
constexpr double kPio2Lo = 6.12323399573676603587e-17;
constexpr double kPi = 3.14159265358979311600e+00;
constexpr double kPiLo = 1.22464679914735317720e-16;
constexpr double kLn2Hi = 6.93147180369123816490e-01;
constexpr double kLn2Lo = 1.90821492927058770002e-10;
constexpr double kLn2 = 6.93147180559945286227e-01;

bool isFinite(double x) {
    return exponentOf(x) != 0x7ff;
}

}

extern "C" __attribute__((weak)) double fabs(double x) {
    return fromBits(bitsOf(x) & ~kSign);
}

extern "C" __attribute__((weak)) double copysign(double magnitude, double sign) {
    return fromBits((bitsOf(magnitude) & ~kSign) | (bitsOf(sign) & kSign));
}

extern "C" __attribute__((weak)) double fmin(double x, double y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    if (x == y)
        return bitsOf(x) & kSign ? x : y;
    return x < y ? x : y;
}

extern "C" __attribute__((weak)) double fmax(double x, double y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    if (x == y)
        return bitsOf(x) & kSign ? y : x;
    return x > y ? x : y;
}

extern "C" __attribute__((weak)) double fdim(double x, double y) {
    if (x != x || y != y)
        return x + y;
    return x > y ? x - y : 0.0;
}

extern "C" __attribute__((weak)) float fdimf(float x, float y) {
    if (x != x || y != y)
        return x + y;
    return x > y ? x - y : 0.0f;
}

extern "C" __attribute__((weak, always_inline)) float fabsf(float x) {
    return __builtin_fabsf(x);
}

extern "C" __attribute__((weak, always_inline)) float copysignf(float magnitude, float sign) {
    return fromBits((bitsOf(magnitude) & 0x7fffffffu) | (bitsOf(sign) & 0x80000000u));
}

extern "C" __attribute__((weak, always_inline)) float fminf(float x, float y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    if (x == y)
        return fromBits(bitsOf(x) | bitsOf(y));
    return x < y ? x : y;
}

extern "C" __attribute__((weak, always_inline)) float fmaxf(float x, float y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    if (x == y)
        return fromBits(bitsOf(x) & bitsOf(y));
    return x > y ? x : y;
}

// Rounding

extern "C" __attribute__((weak)) double trunc(double x) {
    int e = exponentOf(x) - 0x3ff + 12;
    if (e >= 52 + 12)
        return x;
    if (e < 12)
        e = 1;
    u64 mask = ~0ull >> e;
    u64 bits = bitsOf(x);
    if ((bits & mask) == 0)
        return x;
    return fromBits(bits & ~mask);
}

extern "C" __attribute__((weak)) double floor(double x) {
    int e = exponentOf(x);
    if (e >= 0x3ff + 52 || x == 0)
        return x;
    bool negative = bitsOf(x) >> 63;
    if (e < 0x3ff)
        return negative ? -1.0 : 0.0;
    double y = negative ? x - kToInt + kToInt - x : x + kToInt - kToInt - x;
    return y > 0 ? x + y - 1 : x + y;
}

extern "C" __attribute__((weak)) double ceil(double x) {
    int e = exponentOf(x);
    if (e >= 0x3ff + 52 || x == 0)
        return x;
    bool negative = bitsOf(x) >> 63;
    if (e < 0x3ff)
        return negative ? -0.0 : 1.0;
    double y = negative ? x - kToInt + kToInt - x : x + kToInt - kToInt - x;
    return y < 0 ? x + y + 1 : x + y;
}

extern "C" __attribute__((weak)) double round(double x) {
    int e = exponentOf(x);
    if (e >= 0x3ff + 52)
        return x;
    bool negative = bitsOf(x) >> 63;
    double a = negative ? -x : x;
    if (e < 0x3ff - 1)
        return negative ? -0.0 : 0.0;
    double y = a + kToInt - kToInt - a;
    if (y > 0.5)
        y = y + a - 1;
    else if (y <= -0.5)
        y = y + a + 1;
    else
        y = y + a;
    return negative ? -y : y;
}

extern "C" __attribute__((weak, always_inline)) float truncf(float x) {
    float magnitude = fabsf(x);
    if (magnitude >= 0x1p23f || x != x)
        return x;
    if (magnitude < 1.0f)
        return x * 0.0f;
    return (float)(int)x;
}

extern "C" __attribute__((weak, always_inline)) float floorf(float x) {
    float result = truncf(x);
    return result > x ? result - 1.0f : result;
}

extern "C" __attribute__((weak, always_inline)) float ceilf(float x) {
    float result = truncf(x);
    return result < x ? result + 1.0f : result;
}

extern "C" __attribute__((weak, always_inline)) float roundf(float x) {
    float magnitude = fabsf(x);
    if (magnitude >= 0x1p23f || x != x)
        return x;
    if (magnitude < 0.5f)
        return x * 0.0f;
    return (float)(int)(x < 0.0f ? x - 0.5f : x + 0.5f);
}

// Rounds to the nearest integer, ties to even
extern "C" __attribute__((weak)) double rint(double x) {
    int e = exponentOf(x);
    if (e >= 0x3ff + 52)
        return x;
    bool negative = bitsOf(x) >> 63;
    double y = negative ? x - kToInt + kToInt : x + kToInt - kToInt;
    if (y == 0)
        return negative ? -0.0 : 0.0;
    return y;
}

extern "C" __attribute__((weak)) float rintf(float x) {
    int e = (int)(bitsOf(x) >> 23 & 0xff);
    if (e >= 0x7f + 23)
        return x;
    bool negative = bitsOf(x) >> 31;
    float y = negative ? x - 0x1p23f + 0x1p23f : x + 0x1p23f - 0x1p23f;
    if (y == 0)
        return negative ? -0.0f : 0.0f;
    return y;
}

extern "C" __attribute__((weak)) double nearbyint(double x) {
    return rint(x);
}

extern "C" __attribute__((weak)) float nearbyintf(float x) {
    return rintf(x);
}

extern "C" __attribute__((weak)) long lround(double x) {
    return (long)round(x);
}

extern "C" __attribute__((weak)) long long llround(double x) {
    return (long long)round(x);
}

extern "C" __attribute__((weak)) long lroundf(float x) {
    return (long)round(x);
}

extern "C" __attribute__((weak)) long long llroundf(float x) {
    return (long long)round(x);
}

extern "C" __attribute__((weak)) long lrint(double x) {
    return (long)rint(x);
}

extern "C" __attribute__((weak)) long long llrint(double x) {
    return (long long)rint(x);
}

extern "C" __attribute__((weak)) long lrintf(float x) {
    return (long)rintf(x);
}

extern "C" __attribute__((weak)) long long llrintf(float x) {
    return (long long)rintf(x);
}

// Decomposition and scaling

extern "C" __attribute__((weak)) double modf(double x, double* integral) {
    int e = exponentOf(x) - 0x3ff;
    u64 bits = bitsOf(x);
    if (e >= 52) {
        *integral = x;
        if (x != x)
            return x;
        return fromBits(bits & kSign);
    }
    if (e < 0) {
        *integral = fromBits(bits & kSign);
        return x;
    }
    u64 mask = ~0ull >> 12 >> e;
    if ((bits & mask) == 0) {
        *integral = x;
        return fromBits(bits & kSign);
    }
    *integral = fromBits(bits & ~mask);
    return x - *integral;
}

extern "C" __attribute__((weak)) float modff(float x, float* integral) {
    double whole;
    float fraction = (float)modf(x, &whole);
    *integral = (float)whole;
    return fraction;
}

extern "C" __attribute__((weak)) double frexp(double x, int* exponent) {
    int e = exponentOf(x);
    if (e == 0) {
        if (x != 0) {
            x = frexp(x * 0x1p64, exponent);
            *exponent -= 64;
        } else {
            *exponent = 0;
        }
        return x;
    }
    if (e == 0x7ff) {
        *exponent = 0;
        return x;
    }
    *exponent = e - 0x3fe;
    return fromBits((bitsOf(x) & 0x800fffffffffffffull) | 0x3fe0000000000000ull);
}

extern "C" __attribute__((weak)) float frexpf(float x, int* exponent) {
    return (float)frexp((double)x, exponent);
}

extern "C" __attribute__((weak)) double scalbn(double x, int n) {
    double y = x;
    if (n > 1023) {
        y *= 0x1p1023;
        n -= 1023;
        if (n > 1023) {
            y *= 0x1p1023;
            n -= 1023;
            if (n > 1023)
                n = 1023;
        }
    } else if (n < -1022) {
        // scaling by 2^-969 at a time keeps the intermediate normal, so only the last step rounds
        y *= 0x1p-1022 * 0x1p53;
        n += 1022 - 53;
        if (n < -1022) {
            y *= 0x1p-1022 * 0x1p53;
            n += 1022 - 53;
            if (n < -1022)
                n = -1022;
        }
    }
    return y * fromBits((u64)(0x3ff + n) << 52);
}

extern "C" __attribute__((weak)) float scalbnf(float x, int n) {
    if (n > 300)
        n = 300;
    if (n < -300)
        n = -300;
    return (float)scalbn((double)x, n);
}

extern "C" __attribute__((weak)) double scalbln(double x, long n) {
    return scalbn(x, (int)n);
}

extern "C" __attribute__((weak)) float scalblnf(float x, long n) {
    return scalbnf(x, (int)n);
}

extern "C" __attribute__((weak)) double ldexp(double x, int n) {
    return scalbn(x, n);
}

extern "C" __attribute__((weak)) float ldexpf(float x, int n) {
    return scalbnf(x, n);
}

// fmadd and fmadds round once
extern "C" __attribute__((weak)) double fma(double x, double y, double z) {
    return __builtin_fma(x, y, z);
}

extern "C" __attribute__((weak)) float fmaf(float x, float y, float z) {
    return __builtin_fmaf(x, y, z);
}

// Exact remainder by shift and subtract on the mantissas
extern "C" __attribute__((weak)) double fmod(double x, double y) {
    u64 ux = bitsOf(x);
    u64 uy = bitsOf(y);
    int ex = (int)(ux >> 52 & 0x7ff);
    int ey = (int)(uy >> 52 & 0x7ff);
    u64 sign = ux & kSign;
    if (uy << 1 == 0 || y != y || ex == 0x7ff)
        return (x * y) / (x * y);
    if (ux << 1 <= uy << 1)
        return ux << 1 == uy << 1 ? 0 * x : x;

    u64 i;
    if (!ex) {
        for (i = ux << 12; i >> 63 == 0; ex--, i <<= 1) {
        }
        ux <<= -ex + 1;
    } else {
        ux &= ~0ull >> 12;
        ux |= 1ull << 52;
    }
    if (!ey) {
        for (i = uy << 12; i >> 63 == 0; ey--, i <<= 1) {
        }
        uy <<= -ey + 1;
    } else {
        uy &= ~0ull >> 12;
        uy |= 1ull << 52;
    }

    for (; ex > ey; ex--) {
        i = ux - uy;
        if (i >> 63 == 0) {
            if (i == 0)
                return 0 * x;
            ux = i;
        }
        ux <<= 1;
    }
    i = ux - uy;
    if (i >> 63 == 0) {
        if (i == 0)
            return 0 * x;
        ux = i;
    }
    for (; ux >> 52 == 0; ux <<= 1, ex--) {
    }

    if (ex > 0) {
        ux -= 1ull << 52;
        ux |= (u64)ex << 52;
    } else {
        ux >>= -ex + 1;
    }
    return fromBits(ux | sign);
}

extern "C" __attribute__((weak)) float fmodf(float x, float y) {
    // exact in double, and the remainder fits in float
    return (float)fmod((double)x, (double)y);
}

extern "C" __attribute__((weak)) int ilogb(double x) {
    if (x == 0)
        return FP_ILOGB0;
    if (x != x)
        return FP_ILOGBNAN;
    if (!isFinite(x))
        return __INT_MAX__;
    int exponent;
    frexp(x, &exponent);
    return exponent - 1;
}

extern "C" __attribute__((weak)) int ilogbf(float x) {
    return ilogb((double)x);
}

extern "C" __attribute__((weak)) double logb(double x) {
    if (x == 0)
        return -kInfinity;
    if (!isFinite(x))
        return x * x;
    return ilogb(x);
}

extern "C" __attribute__((weak)) float logbf(float x) {
    return (float)logb((double)x);
}

// x minus y times the nearest integer to x / y, and the low three bits of that integer
extern "C" __attribute__((weak)) double remquo(double x, double y, int* quotient) {
    *quotient = 0;
    if (y == 0 || y != y || x != x || !isFinite(x))
        return (x * y) / (x * y);
    if (!isFinite(y))
        return x;
    double ay = fabs(y);
    // fmod by 8y keeps the three bits, and every subtraction below is exact
    double r = ay < 0x1p1021 ? fmod(fabs(x), 8 * ay) : fabs(x);
    int bits = 0;
    for (int step = 4; step; step >>= 1) {
        if (r >= step * ay) {
            r -= step * ay;
            bits += step;
        }
    }
    if (2 * r > ay || (2 * r == ay && (bits & 1))) {
        r -= ay;
        bits++;
    }
    bits &= 7;
    *quotient = (bitsOf(x) ^ bitsOf(y)) & kSign ? -bits : bits;
    return bitsOf(x) & kSign ? -r : r;
}

extern "C" __attribute__((weak)) double remainder(double x, double y) {
    int quotient;
    return remquo(x, y, &quotient);
}

extern "C" __attribute__((weak)) float remquof(float x, float y, int* quotient) {
    return (float)remquo((double)x, (double)y, quotient);
}

extern "C" __attribute__((weak)) float remainderf(float x, float y) {
    return (float)remainder((double)x, (double)y);
}

extern "C" __attribute__((weak)) double nextafter(double x, double y) {
    if (x != x || y != y)
        return x + y;
    if (x == y)
        return y;
    if (x == 0)
        return copysign(fromBits(1ull), y);
    u64 bits = bitsOf(x);
    return fromBits((y > x) == (x > 0) ? bits + 1 : bits - 1);
}

extern "C" __attribute__((weak)) double nexttoward(double x, long double y) {
    return nextafter(x, (double)y);
}

extern "C" __attribute__((weak)) float nexttowardf(float x, long double y) {
    if (x != x || y != y)
        return (float)(x + y);
    if (x == y)
        return (float)y;
    if (x == 0)
        return fromBits(y > 0 ? 1u : 0x80000001u);
    u32 bits = bitsOf(x);
    return fromBits((y > x) == (x > 0) ? bits + 1 : bits - 1);
}

extern "C" __attribute__((weak)) float nextafterf(float x, float y) {
    return nexttowardf(x, y);
}

// Square roots

namespace {

__attribute__((always_inline)) double rsqrtEstimate(double x) {
    double estimate;
    asm("frsqrte %0, %1" : "=f"(estimate) : "f"(x));
    return estimate;
}

}

extern "C" __attribute__((weak, always_inline)) float cemu_patch_fast_sqrtf(float x) {
    if (x == 0.0f || x != x)
        return x;
    if (x < 0.0f)
        return (x - x) / (x - x);
    if (x > 0x1.fffffep+127f)
        return x;
    return (float)((double)x * rsqrtEstimate((double)x));
}

extern "C" __attribute__((weak, always_inline)) double cemu_patch_fast_sqrt(double x) {
    if (x == 0.0 || x != x)
        return x;
    if (x < 0.0)
        return (x - x) / (x - x);
    if (x > 0x1.fffffffffffffp+1023)
        return x;
    return x * rsqrtEstimate(x);
}

// Three Newton steps in double take the 1/32 estimate past float precision
extern "C" __attribute__((weak, always_inline)) float sqrtf(float x) {
    if (x == 0.0f || x != x)
        return x;
    if (x < 0.0f)
        return (x - x) / (x - x);
    if (x > 0x1.fffffep+127f)
        return x;
    double value = (double)x;
    double half = 0.5 * value;
    double estimate = rsqrtEstimate(value);
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    return (float)(value * estimate);
}

extern "C" __attribute__((weak, always_inline)) double sqrt(double x) {
    if (x == 0.0 || x != x)
        return x;
    if (x < 0.0)
        return (x - x) / (x - x);
    if (x > 0x1.fffffffffffffp+1023)
        return x;
    double value = x;
    double resultScale = 1.0;
    if (value < 0x1p-1022) {
        value *= 0x1p+512;
        resultScale = 0x1p-256;
    }
    double half = 0.5 * value;
    double estimate = rsqrtEstimate(value);
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    estimate *= 1.5 - half * estimate * estimate;
    return value * estimate * resultScale;
}

// Trigonometry

namespace {

double sinKernel(double x, double y, bool hasTail) {
    constexpr double S1 = -1.66666666666666324348e-01;
    constexpr double S2 = 8.33333333332248946124e-03;
    constexpr double S3 = -1.98412698298579493134e-04;
    constexpr double S4 = 2.75573137070700676789e-06;
    constexpr double S5 = -2.50507602534068634195e-08;
    constexpr double S6 = 1.58969099521155010221e-10;
    double z = x * x;
    double w = z * z;
    double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    double v = z * x;
    if (!hasTail)
        return x + v * (S1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

double cosKernel(double x, double y) {
    constexpr double C1 = 4.16666666666666019037e-02;
    constexpr double C2 = -1.38888888888741095749e-03;
    constexpr double C3 = 2.48015872894767294178e-05;
    constexpr double C4 = -2.75573143513906633035e-07;
    constexpr double C5 = 2.08757232129817482790e-09;
    constexpr double C6 = -1.13596475577881948265e-11;
    double z = x * x;
    double w = z * z;
    double r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    double hz = 0.5 * z;
    w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

// x - n*pi/2 as y0 + y1, for finite |x| > pi/4. Returns n.
int reducePio2(double x, double& y0, double& y1) {
    constexpr double kInvPio2 = 6.36619772367581382433e-01;
    constexpr double kPio2_1 = 1.57079632673412561417e+00;
    constexpr double kPio2_1t = 6.07710050650619224932e-11;
    constexpr double kPio2_2 = 6.07710050630396597660e-11;
    constexpr double kPio2_2t = 2.02226624879595063154e-21;
    constexpr double kPio2_3 = 2.02226624871116645580e-21;
    constexpr double kPio2_3t = 8.47842766036889956997e-32;
    constexpr double kTwoPi = 6.28318530717958623200e+00;

    // fn*kPio2_1 is exact while fn < 2^20. Beyond that, take the exact remainder modulo the double nearest 2*pi
    // first, which is off by about x * 2.4e-16.
    if (fabs(x) >= 0x1p20 * kPio2Hi) {
        x = fmod(x, kTwoPi);
        if (fabs(x) <= kPio2Hi * 0.5) {
            y0 = x;
            y1 = 0;
            return 0;
        }
    }
    double fn = x * kInvPio2 + 0x1.8p52 - 0x1.8p52;
    int n = (int)fn;
    double r = x - fn * kPio2_1;
    double w = fn * kPio2_1t;
    y0 = r - w;
    int ex = exponentOf(x);
    if (ex - exponentOf(y0) > 16) {
        double t = r;
        w = fn * kPio2_2;
        r = t - w;
        w = fn * kPio2_2t - ((t - r) - w);
        y0 = r - w;
        if (ex - exponentOf(y0) > 49) {
            t = r;
            w = fn * kPio2_3;
            r = t - w;
            w = fn * kPio2_3t - ((t - r) - w);
            y0 = r - w;
        }
    }
    y1 = (r - y0) - w;
    return n;
}

}

extern "C" __attribute__((weak)) double sin(double x) {
    if (fabs(x) <= kPio2Hi * 0.5)
        return fabs(x) < 0x1p-26 ? x : sinKernel(x, 0, false);
    if (!isFinite(x))
        return x - x;
    double y0, y1;
    switch (reducePio2(x, y0, y1) & 3) {
    case 0:
        return sinKernel(y0, y1, true);
    case 1:
        return cosKernel(y0, y1);
    case 2:
        return -sinKernel(y0, y1, true);
    default:
        return -cosKernel(y0, y1);
    }
}

extern "C" __attribute__((weak)) double cos(double x) {
    if (fabs(x) <= kPio2Hi * 0.5)
        return fabs(x) < 0x1p-27 ? 1.0 : cosKernel(x, 0);
    if (!isFinite(x))
        return x - x;
    double y0, y1;
    switch (reducePio2(x, y0, y1) & 3) {
    case 0:
        return cosKernel(y0, y1);
    case 1:
        return -sinKernel(y0, y1, true);
    case 2:
        return -cosKernel(y0, y1);
    default:
        return sinKernel(y0, y1, true);
    }
}

extern "C" __attribute__((weak)) double tan(double x) {
    if (fabs(x) <= kPio2Hi * 0.5)
        return fabs(x) < 0x1p-27 ? x : sinKernel(x, 0, false) / cosKernel(x, 0);
    if (!isFinite(x))
        return x - x;
    double y0, y1;
    int n = reducePio2(x, y0, y1);
    double s = sinKernel(y0, y1, true);
    double c = cosKernel(y0, y1);
    return n & 1 ? -c / s : s / c;
}

namespace {

// Reduces x to [-pi/4, pi/4] with a two part pi/2, exact enough for float up to 2^28 * pi/2. Larger values return 0.
__attribute__((always_inline)) int reducePio2f(float x, double& reduced) {
    double a = x < 0.0f ? -(double)x : (double)x;
    if (a != a || a > 0x1.921fb6p+28) {
        reduced = (double)(x - x);
        return 0;
    }
    if (a <= 0x1.921fb6p-1) {
        reduced = (double)x;
        return 0;
    }
    double scaled = (double)x * 6.36619772367581382433e-1;
    int quadrant = (int)(scaled + (scaled < 0.0 ? -0.5 : 0.5));
    double result = (double)x;
    result -= quadrant * 1.57079631090164184570e+0;
    result -= quadrant * 1.58932547735281966916e-8;
    reduced = result;
    return quadrant;
}

__attribute__((always_inline)) double sinKernelf(double x) {
    double z = x * x;
    double z2 = z * z;
    double tail = -1.98393348360966317347e-4 + z * 2.7183114939898219064e-6;
    return (x + (z * x) * (-1.66666666416265235595e-1 + z * 8.3333293858894631756e-3)) + (z * x) * z2 * tail;
}

__attribute__((always_inline)) double cosKernelf(double x) {
    double z = x * x;
    double z2 = z * z;
    double tail = -1.38867637746099294692e-3 + z * 2.43904487962774090654e-5;
    return ((1.0 + z * -4.99999997251031003120e-1) + z2 * 4.16666233237390631894e-2) + (z2 * z) * tail;
}

__attribute__((always_inline)) double tanKernelf(double x) {
    double z = x * x;
    double z2 = z * z;
    double cubic = z * x;
    double high = 2.97435743359967304927e-3 + z * 9.46564784943673166728e-3;
    double middle = 5.33812378445670393523e-2 + z * 2.45283181166547278873e-2;
    double low = 3.33331395030791399758e-1 + z * 1.33392002712976742718e-1;
    return (x + cubic * low) + (cubic * z2) * (middle + z2 * high);
}

}

extern "C" __attribute__((weak, always_inline)) float sinf(float x) {
    double reduced;
    switch (reducePio2f(x, reduced) & 3) {
    case 0:
        return (float)sinKernelf(reduced);
    case 1:
        return (float)cosKernelf(reduced);
    case 2:
        return (float)-sinKernelf(reduced);
    default:
        return (float)-cosKernelf(reduced);
    }
}

extern "C" __attribute__((weak, always_inline)) float cosf(float x) {
    double reduced;
    switch (reducePio2f(x, reduced) & 3) {
    case 0:
        return (float)cosKernelf(reduced);
    case 1:
        return (float)-sinKernelf(reduced);
    case 2:
        return (float)-cosKernelf(reduced);
    default:
        return (float)sinKernelf(reduced);
    }
}

extern "C" __attribute__((weak, always_inline)) float tanf(float x) {
    double reduced;
    int quadrant = reducePio2f(x, reduced);
    double result = tanKernelf(reduced);
    return (float)(quadrant & 1 ? -1.0 / result : result);
}

extern "C" __attribute__((weak)) double atan(double x) {
    constexpr double kAtanHi[] = {4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01,
                                  1.57079632679489655800e+00};
    constexpr double kAtanLo[] = {2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17,
                                  6.12323399573676603587e-17};
    constexpr double kAT[] = {3.33333333333329318027e-01,  -1.99999999998764832476e-01, 1.42857142725034663711e-01,
                              -1.11111104054623557880e-01, 9.09088713343650656196e-02,  -7.69187620504482999495e-02,
                              6.66107313738753120669e-02,  -5.83357013379057348645e-02, 4.97687799461593236017e-02,
                              -3.65315727442169155270e-02, 1.62858201153657823623e-02};
    double a = fabs(x);
    if (a >= 0x1p66)
        return x != x ? x : copysign(kAtanHi[3] + 0x1p-120, x);
    int id;
    if (a < 0.4375) {
        if (a < 0x1p-27)
            return x;
        id = -1;
    } else if (a < 1.1875) {
        if (a < 0.6875) {
            id = 0;
            a = (2.0 * a - 1.0) / (2.0 + a);
        } else {
            id = 1;
            a = (a - 1.0) / (a + 1.0);
        }
    } else if (a < 2.4375) {
        id = 2;
        a = (a - 1.5) / (1.0 + 1.5 * a);
    } else {
        id = 3;
        a = -1.0 / a;
    }
    double z = a * a;
    double w = z * z;
    double s1 = z * (kAT[0] + w * (kAT[2] + w * (kAT[4] + w * (kAT[6] + w * (kAT[8] + w * kAT[10])))));
    double s2 = w * (kAT[1] + w * (kAT[3] + w * (kAT[5] + w * (kAT[7] + w * kAT[9]))));
    if (id < 0)
        return x - x * (s1 + s2);
    z = kAtanHi[id] - ((a * (s1 + s2) - kAtanLo[id]) - a);
    return x < 0 ? -z : z;
}

extern "C" __attribute__((weak)) double atan2(double y, double x) {
    if (x != x || y != y)
        return x + y;
    bool xNegative = bitsOf(x) >> 63;
    if (y == 0)
        return xNegative ? copysign(kPi, y) : y;
    if (x == 0)
        return copysign(kPio2Hi, y);
    if (!isFinite(x)) {
        if (!isFinite(y))
            return copysign(xNegative ? 3 * kPio2Hi * 0.5 : kPio2Hi * 0.5, y);
        return copysign(xNegative ? kPi : 0.0, y);
    }
    if (!isFinite(y))
        return copysign(kPio2Hi, y);
    double z;
    int scale = exponentOf(y) - exponentOf(x);
    if (scale > 60)
        z = kPio2Hi + 0.5 * kPio2Lo;
    else if (xNegative && scale < -60)
        z = 0.0;
    else
        z = atan(fabs(y / x));
    if (!xNegative)
        return copysign(z, y);
    return copysign(kPi - (z - kPiLo), y);
}

extern "C" __attribute__((weak)) double asin(double x) {
    double a = fabs(x);
    if (a >= 1.0)
        return a == 1.0 ? copysign(kPio2Hi, x) : (x - x) / (x - x);
    // 1 - a is exact, so this stays accurate next to 1
    return atan2(x, sqrt((1.0 - a) * (1.0 + a)));
}

extern "C" __attribute__((weak)) double acos(double x) {
    double a = fabs(x);
    if (a >= 1.0) {
        if (a == 1.0)
            return x > 0 ? 0.0 : kPi;
        return (x - x) / (x - x);
    }
    return atan2(sqrt((1.0 - a) * (1.0 + a)), x);
}

extern "C" __attribute__((weak)) float asinf(float x) {
    return (float)asin((double)x);
}

extern "C" __attribute__((weak)) float acosf(float x) {
    return (float)acos((double)x);
}

extern "C" __attribute__((weak)) float atanf(float x) {
    return (float)atan((double)x);
}

extern "C" __attribute__((weak)) float atan2f(float y, float x) {
    return (float)atan2((double)y, (double)x);
}

// Exponentials and logarithms

extern "C" __attribute__((weak)) double exp(double x) {
    constexpr double kInvLn2 = 1.44269504088896338700e+00;
    constexpr double P1 = 1.66666666666666019037e-01;
    constexpr double P2 = -2.77777777770155933842e-03;
    constexpr double P3 = 6.61375632143793436117e-05;
    constexpr double P4 = -1.65339022054652515390e-06;
    constexpr double P5 = 4.13813679705723846039e-08;
    if (x != x)
        return x;
    if (x > 709.782712893383973096)
        return x * 0x1p1023;
    if (x < -745.13321910194110842)
        return 0.0;
    if (fabs(x) < 0x1p-28)
        return 1.0 + x;
    double hi = x;
    double lo = 0;
    int k = 0;
    if (fabs(x) > 0.5 * kLn2) {
        k = (int)(kInvLn2 * x + (x < 0 ? -0.5 : 0.5));
        hi = x - k * kLn2Hi;
        lo = k * kLn2Lo;
    }
    double r = hi - lo;
    double rr = r * r;
    double c = r - rr * (P1 + rr * (P2 + rr * (P3 + rr * (P4 + rr * P5))));
    double y = 1.0 + (r * c / (2.0 - c) - lo + hi);
    return k ? scalbn(y, k) : y;
}

// 2^x = 2^k * e^(r*ln2) with k the nearest integer, so integer x is exact
extern "C" __attribute__((weak)) double exp2(double x) {
    if (x != x)
        return x;
    if (x > 1024.0)
        return x * 0x1p1023;
    if (x < -1080.0)
        return 0.0;
    double k = rint(x);
    return scalbn(exp((x - k) * kLn2), (int)k);
}

// Kahan's correction makes e^x - 1 accurate next to 0
extern "C" __attribute__((weak)) double expm1(double x) {
    if (fabs(x) > 0.5 || x != x)
        return exp(x) - 1.0;
    double u = exp(x);
    if (u == 1.0)
        return x;
    return (u - 1.0) * x / log(u);
}

namespace {

// x = 2^k * (1 + f) with 1 + f in [sqrt(2)/2, sqrt(2)), and the parts of log(1 + f) = f - hfsq + s*(hfsq + r).
// x has to be positive and finite.
struct LogParts {
    double f, hfsq, s, r;
    int k;
};

LogParts splitLog(double x) {
    constexpr double Lg1 = 6.666666666666735130e-01;
    constexpr double Lg2 = 3.999999999940941908e-01;
    constexpr double Lg3 = 2.857142874366239149e-01;
    constexpr double Lg4 = 2.222219843214978396e-01;
    constexpr double Lg5 = 1.818357216161805012e-01;
    constexpr double Lg6 = 1.531383769920937332e-01;
    constexpr double Lg7 = 1.479819860511658591e-01;
    LogParts p;
    p.k = 0;
    if (exponentOf(x) == 0) {
        x *= 0x1p54;
        p.k = -54;
    }
    u64 bits = bitsOf(x);
    u32 high = (u32)(bits >> 32) + (0x3ff00000 - 0x3fe6a09e);
    p.k += (int)(high >> 20) - 0x3ff;
    high = (high & 0x000fffff) + 0x3fe6a09e;
    x = fromBits((u64)high << 32 | (bits & 0xffffffff));
    p.f = x - 1.0;
    p.hfsq = 0.5 * p.f * p.f;
    p.s = p.f / (2.0 + p.f);
    double z = p.s * p.s;
    double w = z * z;
    double t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    double t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    p.r = t2 + t1;
    return p;
}

// NaN for negative x, -inf for zero, x itself for +inf and NaN, or 0 when x is in range
bool logSpecial(double x, double& result) {
    if (x != x || x == kInfinity) {
        result = x;
        return true;
    }
    if (x == 0) {
        result = -kInfinity;
        return true;
    }
    if (x < 0) {
        result = (x - x) / (x - x);
        return true;
    }
    return false;
}

}

extern "C" __attribute__((weak)) double log(double x) {
    double special;
    if (logSpecial(x, special))
        return special;
    LogParts p = splitLog(x);
    double dk = p.k;
    return p.s * (p.hfsq + p.r) + dk * kLn2Lo - p.hfsq + p.f + dk * kLn2Hi;
}

extern "C" __attribute__((weak)) double log2(double x) {
    constexpr double kInvLn2Hi = 1.44269504072144627571e+00;
    constexpr double kInvLn2Lo = 1.67517131648865118353e-10;
    double special;
    if (logSpecial(x, special))
        return special;
    LogParts p = splitLog(x);
    double hi = fromBits(bitsOf(p.f - p.hfsq) & 0xffffffff00000000ull);
    double lo = p.f - hi - p.hfsq + p.s * (p.hfsq + p.r);
    double valueHi = hi * kInvLn2Hi;
    double valueLo = (lo + hi) * kInvLn2Lo + lo * kInvLn2Hi;
    double y = p.k;
    double w = y + valueHi;
    valueLo += (y - w) + valueHi;
    return valueLo + w;
}

extern "C" __attribute__((weak)) double log10(double x) {
    constexpr double kInvLn10Hi = 4.34294481878168880939e-01;
    constexpr double kInvLn10Lo = 2.50829467116452752298e-11;
    constexpr double kLog10_2Hi = 3.01029995663611771306e-01;
    constexpr double kLog10_2Lo = 3.69423907715893078616e-13;
    double special;
    if (logSpecial(x, special))
        return special;
    LogParts p = splitLog(x);
    double hi = fromBits(bitsOf(p.f - p.hfsq) & 0xffffffff00000000ull);
    double lo = p.f - hi - p.hfsq + p.s * (p.hfsq + p.r);
    double valueHi = hi * kInvLn10Hi;
    double y = p.k;
    double y2 = y * kLog10_2Hi;
    double valueLo = y * kLog10_2Lo + (lo + hi) * kInvLn10Lo + lo * kInvLn10Hi;
    double w = y2 + valueHi;
    valueLo += (y2 - w) + valueHi;
    return valueLo + w;
}

// Kahan's correction makes log(1 + x) accurate next to 0
extern "C" __attribute__((weak)) double log1p(double x) {
    if (x > 0x1p53)
        return log(x);
    double u = 1.0 + x;
    if (u == 1.0)
        return x;
    if (u <= 0.0 || x != x || x == kInfinity)
        return log(u);
    return log(u) * x / (u - 1.0);
}

extern "C" __attribute__((weak)) double pow(double x, double y) {
    if (y == 0 || x == 1.0)
        return 1.0;
    if (x != x || y != y)
        return x + y;

    // whether y is an odd integer, for the sign of negative x
    bool yInteger = rint(y) == y;
    bool yOdd = yInteger && fabs(y) < 0x1p53 && fmod(y, 2.0) != 0;

    if (!isFinite(y)) {
        double a = fabs(x);
        if (a == 1.0)
            return 1.0;
        return (a < 1.0) == (y > 0) ? 0.0 : kInfinity;
    }
    if (x == 0) {
        if (y < 0)
            return yOdd ? copysign(kInfinity, x) : kInfinity;
        return yOdd ? x : 0.0;
    }
    if (!isFinite(x)) {
        double r = y > 0 ? kInfinity : 0.0;
        return x < 0 && yOdd ? -r : r;
    }
    bool negate = false;
    if (x < 0) {
        if (!yInteger)
            return (x - x) / (x - x);
        negate = yOdd;
        x = -x;
    }

    double result;
    if (yInteger && fabs(y) <= 64) {
        // repeated squaring: exact for small results, like x*x for y = 2
        unsigned n = (unsigned)fabs(y);
        double base = x;
        result = 1.0;
        for (; n; n >>= 1, base *= base) {
            if (n & 1)
                result *= base;
        }
        if (y < 0)
            result = 1.0 / result;
    } else {
        // x = 2^k * m: k*y is split into its integer part and an exactly known rest, so that only y*log2(m) with
        // |log2(m)| <= 0.5 contributes rounding error that grows with y
        LogParts p = splitLog(x);
        double m = 1.0 + p.f;
        double logM = log(m) * 1.44269504088896338700e+00;
        double ky = (double)p.k * y;
        double kyError = __builtin_fma((double)p.k, y, -ky);
        double z = ky + y * logM;
        if (z > 1100)
            return negate ? -kInfinity : kInfinity;
        if (z < -1100)
            return negate ? -0.0 : 0.0;
        double whole = rint(ky);
        double rest = (ky - whole) + kyError + y * logM;
        result = scalbn(exp(rest * kLn2), (int)whole);
    }
    return negate ? -result : result;
}

extern "C" __attribute__((weak)) double cbrt(double x) {
    if (x == 0 || !isFinite(x))
        return x;
    double a = fabs(x);
    double scale = 1.0;
    if (exponentOf(a) == 0) {
        a *= 0x1p54;
        scale = 0x1p-18;
    }
    // a third of the exponent bits as the first guess, then Newton's method
    u32 high = (u32)(bitsOf(a) >> 32);
    double t = fromBits((u64)(high / 3 + 715094163) << 32);
    for (int i = 0; i < 5; i++)
        t = t - (t * t * t - a) / (3.0 * t * t);
    return copysign(t * scale, x);
}

extern "C" __attribute__((weak)) double hypot(double x, double y) {
    double a = fabs(x);
    double b = fabs(y);
    if (a == kInfinity || b == kInfinity)
        return kInfinity;
    if (a != a || b != b)
        return a + b;
    if (a < b) {
        double t = a;
        a = b;
        b = t;
    }
    if (b == 0)
        return a;
    double r = b / a;
    return a * sqrt(1.0 + r * r);
}

// Hyperbolic functions

extern "C" __attribute__((weak)) double sinh(double x) {
    double a = fabs(x);
    if (a < 0x1p-28 || x != x)
        return x;
    double r;
    if (a < 22) {
        double t = expm1(a);
        r = a < 1 ? 0.5 * (2.0 * t - t * t / (t + 1.0)) : 0.5 * (t + t / (t + 1.0));
    } else {
        double h = exp(0.5 * a);
        r = 0.5 * h * h;
    }
    return copysign(r, x);
}

extern "C" __attribute__((weak)) double cosh(double x) {
    double a = fabs(x);
    if (x != x)
        return x;
    if (a < 0.5 * kLn2) {
        double t = expm1(a);
        double w = 1.0 + t;
        return 1.0 + (t * t) / (w + w);
    }
    if (a < 22) {
        double t = exp(a);
        return 0.5 * t + 0.5 / t;
    }
    double h = exp(0.5 * a);
    return 0.5 * h * h;
}

extern "C" __attribute__((weak)) double tanh(double x) {
    double a = fabs(x);
    if (x != x)
        return x;
    if (a > 22)
        return copysign(1.0, x);
    if (a < 0x1p-28)
        return x;
    double r;
    if (a >= 1) {
        double t = expm1(2.0 * a);
        r = 1.0 - 2.0 / (t + 2.0);
    } else {
        double t = expm1(-2.0 * a);
        r = -t / (t + 2.0);
    }
    return copysign(r, x);
}

extern "C" __attribute__((weak)) double asinh(double x) {
    double a = fabs(x);
    if (a < 0x1p-28 || !isFinite(x))
        return x;
    double r;
    if (a > 0x1p28) {
        r = log(a) + kLn2;
    } else if (a > 2) {
        r = log(2.0 * a + 1.0 / (sqrt(a * a + 1.0) + a));
    } else {
        double t = a * a;
        r = log1p(a + t / (1.0 + sqrt(1.0 + t)));
    }
    return copysign(r, x);
}

extern "C" __attribute__((weak)) double acosh(double x) {
    if (x < 1)
        return (x - x) / (x - x);
    if (x > 0x1p28)
        return x == kInfinity ? x : log(x) + kLn2;
    if (x > 2)
        return log(2.0 * x - 1.0 / (x + sqrt(x * x - 1.0)));
    double t = x - 1.0;
    return log1p(t + sqrt(2.0 * t + t * t));
}

extern "C" __attribute__((weak)) double atanh(double x) {
    double a = fabs(x);
    if (a > 1 || x != x)
        return (x - x) / (x - x);
    if (a == 1)
        return copysign(kInfinity, x);
    if (a < 0x1p-28)
        return x;
    double r = a < 0.5 ? 0.5 * log1p(2.0 * a + 2.0 * a * a / (1.0 - a)) : 0.5 * log1p((a + a) / (1.0 - a));
    return copysign(r, x);
}

// float versions through double

extern "C" __attribute__((weak)) float sinhf(float x) {
    return (float)sinh((double)x);
}

extern "C" __attribute__((weak)) float coshf(float x) {
    return (float)cosh((double)x);
}

extern "C" __attribute__((weak)) float tanhf(float x) {
    return (float)tanh((double)x);
}

extern "C" __attribute__((weak)) float asinhf(float x) {
    return (float)asinh((double)x);
}

extern "C" __attribute__((weak)) float acoshf(float x) {
    return (float)acosh((double)x);
}

extern "C" __attribute__((weak)) float atanhf(float x) {
    return (float)atanh((double)x);
}

extern "C" __attribute__((weak)) float expf(float x) {
    return (float)exp((double)x);
}

extern "C" __attribute__((weak)) float exp2f(float x) {
    return (float)exp2((double)x);
}

extern "C" __attribute__((weak)) float expm1f(float x) {
    return (float)expm1((double)x);
}

extern "C" __attribute__((weak)) float logf(float x) {
    return (float)log((double)x);
}

extern "C" __attribute__((weak)) float log2f(float x) {
    return (float)log2((double)x);
}

extern "C" __attribute__((weak)) float log10f(float x) {
    return (float)log10((double)x);
}

extern "C" __attribute__((weak)) float log1pf(float x) {
    return (float)log1p((double)x);
}

extern "C" __attribute__((weak)) float powf(float x, float y) {
    return (float)pow((double)x, (double)y);
}

extern "C" __attribute__((weak)) float cbrtf(float x) {
    return (float)cbrt((double)x);
}

extern "C" __attribute__((weak)) float hypotf(float x, float y) {
    double a = x;
    double b = y;
    if (a != a || b != b)
        return (a == kInfinity || a == -kInfinity || b == kInfinity || b == -kInfinity) ? (float)kInfinity : x + y;
    // exact in double without overflow
    return (float)sqrt(a * a + b * b);
}
