// <math.h> for Cemu patches. The implicit Cemu patch header defines sqrtf, sqrt, sinf, cosf, tanf, fabsf, copysignf,
// fminf, fmaxf, truncf, floorf, ceilf and roundf inline, so they exist without including anything. The other
// functions are implemented by the patch runtime. long double has no math functions.

#ifndef _CEMU_PATCH_MATH_H
#define _CEMU_PATCH_MATH_H

#include <_cemu_patch_libc.h>

#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4

#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")

#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2
#define math_errhandling 0

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isfinite(x) __builtin_isfinite(x)
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define isnormal(x) __builtin_isnormal(x)
#define signbit(x) __builtin_signbit(x)
#define isgreater(x, y) __builtin_isgreater(x, y)
#define isgreaterequal(x, y) __builtin_isgreaterequal(x, y)
#define isless(x, y) __builtin_isless(x, y)
#define islessequal(x, y) __builtin_islessequal(x, y)
#define islessgreater(x, y) __builtin_islessgreater(x, y)
#define isunordered(x, y) __builtin_isunordered(x, y)

// The POSIX constants
#define M_E 2.7182818284590452354
#define M_LOG2E 1.4426950408889634074
#define M_LOG10E 0.43429448190325182765
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_1_PI 0.31830988618379067154
#define M_2_PI 0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2 1.41421356237309504880
#define M_SQRT1_2 0.70710678118654752440

_CEMU_PATCH_BEGIN_C

typedef float float_t;
typedef double double_t;

double fabs(double x);
double copysign(double magnitude, double sign);
double fmin(double x, double y);
double fmax(double x, double y);
double fdim(double x, double y);
float fdimf(float x, float y);
double fma(double x, double y, double z);
float fmaf(float x, float y, float z);

double trunc(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double rint(double x);
float rintf(float x);
double nearbyint(double x);
float nearbyintf(float x);
long lround(double x);
long long llround(double x);
long lroundf(float x);
long long llroundf(float x);
long lrint(double x);
long long llrint(double x);
long lrintf(float x);
long long llrintf(float x);

double fmod(double x, double y);
float fmodf(float x, float y);
double modf(double x, double* integral);
float modff(float x, float* integral);
double frexp(double x, int* exponent);
float frexpf(float x, int* exponent);
double ldexp(double x, int exponent);
float ldexpf(float x, int exponent);
double scalbn(double x, int exponent);
float scalbnf(float x, int exponent);

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double asinh(double x);
double acosh(double x);
double atanh(double x);
float sinhf(float x);
float coshf(float x);
float tanhf(float x);
float asinhf(float x);
float acoshf(float x);
float atanhf(float x);

double exp(double x);
double exp2(double x);
double expm1(double x);
double log(double x);
double log2(double x);
double log10(double x);
double log1p(double x);
double pow(double x, double y);
double cbrt(double x);
double hypot(double x, double y);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float logf(float x);
float log2f(float x);
float log10f(float x);
float log1pf(float x);
float powf(float x, float y);
float cbrtf(float x);
float hypotf(float x, float y);

_CEMU_PATCH_END_C

#endif
