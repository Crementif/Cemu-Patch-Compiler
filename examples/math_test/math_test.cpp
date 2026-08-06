// moduleMatches = 0x6267BFD0

// Compile-time and on-target smoke coverage for every math helper injected by
// PatchCompiler. The exported wrappers make the generated PPC easy to inspect,
// while patchMathSelfTest() can be called from a hook or debugger at runtime.

#define ATTR_KEEP __attribute__((used))

union MathFloatBits {
    float f;
    unsigned int u;
};

extern "C" {

extern void _IMPORT_COREINIT_OSReport(const char* format, ...);

ATTR_KEEP volatile float gMathTestFloatResults[13];
ATTR_KEEP volatile float gMathTestBuiltinResults[11];
ATTR_KEEP volatile double gMathTestDoubleResults[2];
ATTR_KEEP volatile unsigned int gMathTestFailures = 0;

ATTR_KEEP float patchMathSqrtf(float x) { return sqrtf(x); }
ATTR_KEEP float patchMathBuiltinSqrtf(float x) { return __builtin_sqrtf(x); }
ATTR_KEEP double patchMathSqrt(double x) { return sqrt(x); }
ATTR_KEEP double patchMathBuiltinSqrt(double x) { return __builtin_sqrt(x); }
ATTR_KEEP float patchMathSinf(float x) { return sinf(x); }
ATTR_KEEP float patchMathCosf(float x) { return cosf(x); }
ATTR_KEEP float patchMathTanf(float x) { return tanf(x); }
ATTR_KEEP float patchMathFabsf(float x) { return fabsf(x); }
ATTR_KEEP float patchMathCopysignf(float x, float y) {
    return copysignf(x, y);
}
ATTR_KEEP float patchMathFminf(float x, float y) { return fminf(x, y); }
ATTR_KEEP float patchMathFmaxf(float x, float y) { return fmaxf(x, y); }
ATTR_KEEP float patchMathTruncf(float x) { return truncf(x); }
ATTR_KEEP float patchMathFloorf(float x) { return floorf(x); }
ATTR_KEEP float patchMathCeilf(float x) { return ceilf(x); }
ATTR_KEEP float patchMathRoundf(float x) { return roundf(x); }

ATTR_KEEP void patchMathGenerate(float x, float y) {
    gMathTestFloatResults[0] = sqrtf(x);
    gMathTestFloatResults[1] = sinf(x);
    gMathTestFloatResults[2] = cosf(x);
    gMathTestFloatResults[3] = tanf(x);
    gMathTestFloatResults[4] = fabsf(y);
    gMathTestFloatResults[5] = copysignf(x, y);
    gMathTestFloatResults[6] = fminf(x, y);
    gMathTestFloatResults[7] = fmaxf(x, y);
    gMathTestFloatResults[8] = truncf(y);
    gMathTestFloatResults[9] = floorf(y);
    gMathTestFloatResults[10] = ceilf(y);
    gMathTestFloatResults[11] = roundf(y);
    gMathTestFloatResults[12] = __builtin_sqrtf(x);

    gMathTestBuiltinResults[0] = __builtin_fabsf(y);
    gMathTestBuiltinResults[1] = __builtin_copysignf(x, y);
    gMathTestBuiltinResults[2] = __builtin_fminf(x, y);
    gMathTestBuiltinResults[3] = __builtin_fmaxf(x, y);
    gMathTestBuiltinResults[4] = __builtin_truncf(y);
    gMathTestBuiltinResults[5] = __builtin_floorf(y);
    gMathTestBuiltinResults[6] = __builtin_ceilf(y);
    gMathTestBuiltinResults[7] = __builtin_roundf(y);
    gMathTestBuiltinResults[8] = __builtin_sinf(x);
    gMathTestBuiltinResults[9] = __builtin_cosf(x);
    gMathTestBuiltinResults[10] = __builtin_tanf(x);

    gMathTestDoubleResults[0] = sqrt((double)x);
    gMathTestDoubleResults[1] = __builtin_sqrt((double)x);
}

ATTR_KEEP unsigned int patchMathSelfTest() {
    _IMPORT_COREINIT_OSReport("[PatchCompiler Math Test] START\n");
    unsigned int failures = 0;

    MathFloatBits accurateSqrt = {sqrtf(2.0f)};
    if (accurateSqrt.u != 0x3fb504f3u)
        failures |= 1u << 0;

    const float fastSqrt = __builtin_sqrtf(2.0f);
    if (fabsf(fastSqrt - 0x1.6a09e6p+0f) > 0.05f)
        failures |= 1u << 1;

    const double accurateDoubleSqrt = sqrt(9.0);
    if (accurateDoubleSqrt < 2.999999999999999 ||
        accurateDoubleSqrt > 3.000000000000001)
        failures |= 1u << 2;

    if (sinf(0.0f) != 0.0f)
        failures |= 1u << 3;
    if (cosf(0.0f) != 1.0f)
        failures |= 1u << 4;
    if (tanf(0.0f) != 0.0f)
        failures |= 1u << 5;
    if (fabsf(-3.0f) != 3.0f)
        failures |= 1u << 6;
    if (copysignf(3.0f, -1.0f) != -3.0f)
        failures |= 1u << 7;
    if (fminf(2.0f, -1.0f) != -1.0f)
        failures |= 1u << 8;
    if (fmaxf(2.0f, -1.0f) != 2.0f)
        failures |= 1u << 9;
    if (truncf(-1.75f) != -1.0f)
        failures |= 1u << 10;
    if (floorf(-1.75f) != -2.0f)
        failures |= 1u << 11;
    if (ceilf(-1.75f) != -1.0f)
        failures |= 1u << 12;
    if (roundf(1.5f) != 2.0f || roundf(-1.5f) != -2.0f)
        failures |= 1u << 13;

    MathFloatBits negativeZero;
    negativeZero.u = 0x80000000u;
    MathFloatBits minZero = {fminf(0.0f, negativeZero.f)};
    MathFloatBits maxZero = {fmaxf(0.0f, negativeZero.f)};
    if (minZero.u != 0x80000000u || maxZero.u != 0x00000000u)
        failures |= 1u << 14;

    gMathTestFailures = failures;
    if (failures == 0)
        _IMPORT_COREINIT_OSReport("[PatchCompiler Math Test] PASS\n");
    else
        _IMPORT_COREINIT_OSReport(
            "[PatchCompiler Math Test] FAIL: 0x%08X\n", failures);
    return failures;
}

}

PATCH_ON_LOAD(patchMathSelfTest);
