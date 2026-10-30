
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

#define FP_ILOGB0 (-2147483647 - 1)
#define FP_ILOGBNAN (-2147483647 - 1)

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
float fabsf(float x);
double copysign(double magnitude, double sign);
float copysignf(float magnitude, float sign);
double fmin(double x, double y);
float fminf(float x, float y);
double fmax(double x, double y);
float fmaxf(float x, float y);
double fdim(double x, double y);
float fdimf(float x, float y);
double fma(double x, double y, double z);
float fmaf(float x, float y, float z);
double sqrt(double x);
float sqrtf(float x);

double trunc(double x);
float truncf(float x);
double floor(double x);
float floorf(float x);
double ceil(double x);
float ceilf(float x);
double round(double x);
float roundf(float x);
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
double scalbln(double x, long exponent);
float scalblnf(float x, long exponent);
int ilogb(double x);
int ilogbf(float x);
double logb(double x);
float logbf(float x);

double remainder(double x, double y);
float remainderf(float x, float y);
double remquo(double x, double y, int* quotient);
float remquof(float x, float y, int* quotient);
double nextafter(double x, double y);
float nextafterf(float x, float y);
double nexttoward(double x, long double y);
float nexttowardf(float x, long double y);

_CEMU_PATCH_INLINE double nan(const char* tag) { (void)tag; return __builtin_nan(""); }
_CEMU_PATCH_INLINE float nanf(const char* tag) { (void)tag; return __builtin_nanf(""); }

double sin(double x);
double cos(double x);
double tan(double x);
float sinf(float x);
float cosf(float x);
float tanf(float x);
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

_CEMU_PATCH_INLINE long double fabsl(long double x) { return fabs(x); }
_CEMU_PATCH_INLINE long double copysignl(long double magnitude, long double sign) { return copysign(magnitude, sign); }
_CEMU_PATCH_INLINE long double fminl(long double x, long double y) { return fmin(x, y); }
_CEMU_PATCH_INLINE long double fmaxl(long double x, long double y) { return fmax(x, y); }
_CEMU_PATCH_INLINE long double fdiml(long double x, long double y) { return fdim(x, y); }
_CEMU_PATCH_INLINE long double fmal(long double x, long double y, long double z) { return fma(x, y, z); }
_CEMU_PATCH_INLINE long double sqrtl(long double x) { return sqrt(x); }

_CEMU_PATCH_INLINE long double truncl(long double x) { return trunc(x); }
_CEMU_PATCH_INLINE long double floorl(long double x) { return floor(x); }
_CEMU_PATCH_INLINE long double ceill(long double x) { return ceil(x); }
_CEMU_PATCH_INLINE long double roundl(long double x) { return round(x); }
_CEMU_PATCH_INLINE long double rintl(long double x) { return rint(x); }
_CEMU_PATCH_INLINE long double nearbyintl(long double x) { return nearbyint(x); }
_CEMU_PATCH_INLINE long lroundl(long double x) { return lround(x); }
_CEMU_PATCH_INLINE long long llroundl(long double x) { return llround(x); }
_CEMU_PATCH_INLINE long lrintl(long double x) { return lrint(x); }
_CEMU_PATCH_INLINE long long llrintl(long double x) { return llrint(x); }

_CEMU_PATCH_INLINE long double fmodl(long double x, long double y) { return fmod(x, y); }
_CEMU_PATCH_INLINE long double remainderl(long double x, long double y) { return remainder(x, y); }
_CEMU_PATCH_INLINE long double remquol(long double x, long double y, int* quotient) { return remquo(x, y, quotient); }
_CEMU_PATCH_INLINE long double modfl(long double x, long double* integral) { return modf(x, (double*)integral); }
_CEMU_PATCH_INLINE long double frexpl(long double x, int* exponent) { return frexp(x, exponent); }
_CEMU_PATCH_INLINE long double ldexpl(long double x, int exponent) { return ldexp(x, exponent); }
_CEMU_PATCH_INLINE long double scalbnl(long double x, int exponent) { return scalbn(x, exponent); }
_CEMU_PATCH_INLINE long double scalblnl(long double x, long exponent) { return scalbln(x, exponent); }
_CEMU_PATCH_INLINE int ilogbl(long double x) { return ilogb(x); }
_CEMU_PATCH_INLINE long double logbl(long double x) { return logb(x); }
_CEMU_PATCH_INLINE long double nextafterl(long double x, long double y) { return nextafter(x, y); }
_CEMU_PATCH_INLINE long double nexttowardl(long double x, long double y) { return nextafter(x, y); }
_CEMU_PATCH_INLINE long double nanl(const char* tag) { return nan(tag); }

_CEMU_PATCH_INLINE long double sinl(long double x) { return sin(x); }
_CEMU_PATCH_INLINE long double cosl(long double x) { return cos(x); }
_CEMU_PATCH_INLINE long double tanl(long double x) { return tan(x); }
_CEMU_PATCH_INLINE long double asinl(long double x) { return asin(x); }
_CEMU_PATCH_INLINE long double acosl(long double x) { return acos(x); }
_CEMU_PATCH_INLINE long double atanl(long double x) { return atan(x); }
_CEMU_PATCH_INLINE long double atan2l(long double y, long double x) { return atan2(y, x); }
_CEMU_PATCH_INLINE long double sinhl(long double x) { return sinh(x); }
_CEMU_PATCH_INLINE long double coshl(long double x) { return cosh(x); }
_CEMU_PATCH_INLINE long double tanhl(long double x) { return tanh(x); }
_CEMU_PATCH_INLINE long double asinhl(long double x) { return asinh(x); }
_CEMU_PATCH_INLINE long double acoshl(long double x) { return acosh(x); }
_CEMU_PATCH_INLINE long double atanhl(long double x) { return atanh(x); }

_CEMU_PATCH_INLINE long double expl(long double x) { return exp(x); }
_CEMU_PATCH_INLINE long double exp2l(long double x) { return exp2(x); }
_CEMU_PATCH_INLINE long double expm1l(long double x) { return expm1(x); }
_CEMU_PATCH_INLINE long double logl(long double x) { return log(x); }
_CEMU_PATCH_INLINE long double log2l(long double x) { return log2(x); }
_CEMU_PATCH_INLINE long double log10l(long double x) { return log10(x); }
_CEMU_PATCH_INLINE long double log1pl(long double x) { return log1p(x); }
_CEMU_PATCH_INLINE long double powl(long double x, long double y) { return pow(x, y); }
_CEMU_PATCH_INLINE long double cbrtl(long double x) { return cbrt(x); }
_CEMU_PATCH_INLINE long double hypotl(long double x, long double y) { return hypot(x, y); }

_CEMU_PATCH_END_C

#endif
