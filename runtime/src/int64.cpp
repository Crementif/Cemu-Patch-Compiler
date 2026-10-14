// provides: __udivdi3 __umoddi3 __divdi3 __moddi3
// 64-bit division, which the 750 has no instruction for, so code generation calls these helpers

typedef unsigned long long u64;
typedef long long s64;

static u64 divideUnsigned(u64 n, u64 d, u64* remainder) {
    if (d == 0)
        __builtin_trap();
    if ((n >> 32) == 0 && (d >> 32) == 0) {
        *remainder = (unsigned)n % (unsigned)d;
        return (unsigned)n / (unsigned)d;
    }
    if (n < d) {
        *remainder = n;
        return 0;
    }
    // shift and subtract, one quotient bit per step
    int shift = __builtin_clzll(d) - __builtin_clzll(n);
    d <<= shift;
    u64 quotient = 0;
    for (int i = 0; i <= shift; i++) {
        quotient <<= 1;
        if (n >= d) {
            n -= d;
            quotient |= 1;
        }
        d >>= 1;
    }
    *remainder = n;
    return quotient;
}

static u64 magnitude(s64 value) {
    return value < 0 ? 0 - (u64)value : (u64)value;
}

extern "C" __attribute__((weak)) u64 __udivdi3(u64 n, u64 d) {
    u64 remainder;
    return divideUnsigned(n, d, &remainder);
}

extern "C" __attribute__((weak)) u64 __umoddi3(u64 n, u64 d) {
    u64 remainder;
    divideUnsigned(n, d, &remainder);
    return remainder;
}

extern "C" __attribute__((weak)) s64 __divdi3(s64 n, s64 d) {
    u64 remainder;
    u64 quotient = divideUnsigned(magnitude(n), magnitude(d), &remainder);
    return (n < 0) != (d < 0) ? (s64)(0 - quotient) : (s64)quotient;
}

extern "C" __attribute__((weak)) s64 __moddi3(s64 n, s64 d) {
    u64 remainder;
    divideUnsigned(magnitude(n), magnitude(d), &remainder);
    return n < 0 ? (s64)(0 - remainder) : (s64)remainder;
}
