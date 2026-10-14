// provides: __floatundidf __floatdidf __floatundisf __floatdisf __fixunsdfdi __fixdfdi __fixunssfdi __fixsfdi
// Conversions between 64-bit integers and floating point, which the 750 has no instructions for, so code generation
// calls these helpers. Like C casts, float to integer truncates toward zero; out of range values saturate and NaN
// gives 0.

typedef unsigned long long u64;
typedef long long s64;

extern "C" __attribute__((weak)) double __floatundidf(u64 value) {
    // the high half times 2^32 is exact, so the sum rounds only once
    return (double)(unsigned)(value >> 32) * 0x1p32 + (double)(unsigned)value;
}

extern "C" __attribute__((weak)) double __floatdidf(s64 value) {
    return value < 0 ? -__floatundidf(0 - (u64)value) : __floatundidf((u64)value);
}

// Through double, with the bits below double precision folded into a sticky bit so the result rounds only once
extern "C" __attribute__((weak)) float __floatundisf(u64 value) {
    if (value >> 53 == 0)
        return (float)__floatundidf(value);
    int shift = 11 - __builtin_clzll(value);
    u64 kept = (value >> shift) | ((value & ((1ull << shift) - 1)) != 0);
    return (float)(__floatundidf(kept) * (double)(1u << shift));
}

extern "C" __attribute__((weak)) float __floatdisf(s64 value) {
    return value < 0 ? -__floatundisf(0 - (u64)value) : __floatundisf((u64)value);
}

extern "C" __attribute__((weak)) u64 __fixunsdfdi(double value) {
    if (!(value >= 1.0))
        return 0;
    if (value >= 0x1p64)
        return ~0ull;
    unsigned high = (unsigned)(value * 0x1p-32);
    double low = value - (double)high * 0x1p32;
    return ((u64)high << 32) | (unsigned)low;
}

extern "C" __attribute__((weak)) s64 __fixdfdi(double value) {
    if (value != value)
        return 0;
    if (value >= 0x1p63)
        return 0x7fffffffffffffffll;
    if (value <= -0x1p63)
        return -0x7fffffffffffffffll - 1;
    return value < 0 ? (s64)(0 - __fixunsdfdi(-value)) : (s64)__fixunsdfdi(value);
}

extern "C" __attribute__((weak)) u64 __fixunssfdi(float value) {
    return __fixunsdfdi(value);
}

extern "C" __attribute__((weak)) s64 __fixsfdi(float value) {
    return __fixdfdi(value);
}
