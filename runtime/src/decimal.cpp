// provides: __cemu_patch_decimal_to_double __cemu_patch_double_to_decimal
// Conversion between doubles and decimal digits for strtod, printf and scanf. Scaling by powers of ten is done in
// double-double arithmetic with fmadd, which is precise to about 2^-100, so results are correctly rounded except
// when the exact value lies extremely close to halfway between two neighbors.

typedef unsigned long long u64;
typedef long long s64;

namespace {

constexpr double kPowersOfTen[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                   1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

// An unevaluated sum hi + lo with |lo| at most half an ulp of hi
struct DoubleDouble {
    double hi, lo;

    void normalize(double sum, double error) {
        hi = sum + error;
        lo = error - (hi - sum);
    }

    // multiplication and division by an exact power of ten
    void multiply(double factor) {
        double product = hi * factor;
        double error = __builtin_fma(hi, factor, -product) + lo * factor;
        normalize(product, error);
    }

    void divide(double divisor) {
        double quotient = hi / divisor;
        double rest = __builtin_fma(-quotient, divisor, hi) + lo;
        normalize(quotient, rest / divisor);
    }

    void scaleByPowerOfTen(int exponent) {
        for (; exponent >= 22; exponent -= 22)
            multiply(1e22);
        for (; exponent <= -22; exponent += 22)
            divide(1e22);
        if (exponent > 0)
            multiply(kPowersOfTen[exponent]);
        else if (exponent < 0)
            divide(kPowersOfTen[-exponent]);
    }
};

DoubleDouble fromInteger(u64 value) {
    DoubleDouble result;
    result.hi = (double)value;
    // the difference is below 2^11, so it converts exactly
    result.lo = (double)(s64)(value - (u64)result.hi);
    return result;
}

}

// (mantissa + tail) * 10^exponent, correctly rounded, where the tail in [0, 1) holds digits past the 19 that fit in
// the mantissa
extern "C" __attribute__((weak)) double __cemu_patch_decimal_to_double(u64 mantissa, double tail, int exponent) {
    if (mantissa == 0)
        return 0.0;
    // the decimal exponent of the leading digit, give or take one
    int leading = exponent + (63 - __builtin_clzll(mantissa)) * 30103 / 100000;
    if (leading < -330)
        return 0.0;
    if (leading > 310)
        return __builtin_inf();
    if (mantissa >> 53 == 0 && tail == 0 && exponent >= -22 && exponent <= 22) {
        // one rounding of exact operands
        double value = (double)mantissa;
        return exponent >= 0 ? value * kPowersOfTen[exponent] : value / kPowersOfTen[-exponent];
    }
    // keep the value normal and finite while scaling, then scale back exactly
    int scale = leading < -280 ? 256 : leading > 280 ? -256 : 0;
    DoubleDouble value = fromInteger(mantissa);
    value.normalize(value.hi, value.lo + tail);
    // powers of two, which scale exactly
    double up = scale > 0 ? 0x1p256 : scale < 0 ? 0x1p-256 : 1.0;
    double down = 1.0 / up;
    value.hi *= up;
    value.lo *= up;
    value.scaleByPowerOfTen(exponent);
    if (scale <= 0 || value.hi >= 0x1p-766)
        return (value.hi + value.lo) * down;
    // A subnormal result: round to its spacing, 2^-1074 scaled up by 2^256, while still scaled. Adding a constant
    // rounds hi to a multiple of it, and lo with the rest of hi decides any rounding that took the wrong way.
    constexpr double kSpacing = 0x1p-818;
    constexpr double kRounder = 0x1.8p-766;
    double rounded = (value.hi + kRounder) - kRounder;
    double rest = (value.hi - rounded) + value.lo;
    // the ulp of rounded + kRounder is the spacing
    bool odd = __builtin_bit_cast(u64, rounded + kRounder) & 1;
    if (rest > kSpacing / 2 || (rest == kSpacing / 2 && odd))
        rounded += kSpacing;
    else if (rest < -kSpacing / 2 || (rest == -kSpacing / 2 && odd))
        rounded -= kSpacing;
    return rounded * down;
}

// Rounds a positive finite value to `digits` significant decimal digits (1 to 17), ties to even. Stores them as an
// integer in *mantissa and returns the decimal exponent of the first digit.
extern "C" __attribute__((weak)) int __cemu_patch_double_to_decimal(double value, int digits, u64* mantissa) {
    u64 bits = __builtin_bit_cast(u64, value);
    int binaryExponent = (int)(bits >> 52 & 0x7ff);
    if (binaryExponent == 0)
        binaryExponent = -1074 + 63 - __builtin_clzll(bits & ((1ull << 52) - 1));
    else
        binaryExponent -= 1023;
    // floor(binaryExponent * log10(2)), which can be one too low; the loop corrects it
    int exponent = (binaryExponent * 78913) >> 18;

    u64 low = (u64)kPowersOfTen[digits - 1];
    u64 high = (u64)kPowersOfTen[digits];
    for (int attempt = 0;; attempt++) {
        DoubleDouble scaled = {value, 0.0};
        scaled.scaleByPowerOfTen(digits - 1 - exponent);
        u64 whole = (u64)scaled.hi;
        double fraction = (scaled.hi - (double)whole) + scaled.lo;
        for (; fraction > 0.5; fraction -= 1.0)
            whole++;
        for (; fraction < -0.5; fraction += 1.0)
            whole--;
        if ((fraction == 0.5 || fraction == -0.5) && (whole & 1))
            whole += fraction > 0 ? 1 : -1;
        if (whole >= high && attempt < 2) {
            exponent++;
            continue;
        }
        if (whole < low && attempt < 2) {
            exponent--;
            continue;
        }
        *mantissa = whole;
        return exponent;
    }
}
