// provides: vsnprintf snprintf vsprintf sprintf vprintf printf puts
// Formatted output in the "C" locale. The conversions are d i u o x X c s p f F e E g G a A n and %, with the flags
// - + space # 0, field width, precision, * for either, and the length modifiers hh h l ll j z t. Floating point is
// correctly rounded to 17 significant digits; digits past those are printed as zeros. printf and puts write a line
// to the log through OSReport.

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef unsigned long long u64;

extern "C" int __cemu_patch_double_to_decimal(double value, int digits, u64* mantissa);
extern "C" void _IMPORT_COREINIT_OSReport(const char* format, ...);

namespace {

struct Output {
    char* buffer;
    size_t capacity;
    size_t length = 0;

    void put(char c) {
        if (length + 1 < capacity)
            buffer[length] = c;
        length++;
    }

    void put(const char* text, size_t count) {
        for (size_t i = 0; i < count; i++)
            put(text[i]);
    }

    void repeat(char c, int count) {
        for (; count > 0; count--)
            put(c);
    }

    void finish() {
        if (capacity)
            buffer[length < capacity ? length : capacity - 1] = '\0';
    }
};

struct Spec {
    bool left = false;
    bool plus = false;
    bool space = false;
    bool alternate = false;
    bool zero = false;
    int width = 0;
    int precision = -1;
    char conversion = 0;
};

// Pads `body` (with `prefix` before it, like a sign or 0x) to the field width. Zero padding goes between the two.
void emitField(Output& out, const Spec& spec, const char* prefix, size_t prefixLength, const char* body,
               size_t bodyLength, int leadingZeros, bool zeroPad) {
    int padding = spec.width - (int)(prefixLength + bodyLength) - leadingZeros;
    if (!spec.left && !zeroPad)
        out.repeat(' ', padding);
    out.put(prefix, prefixLength);
    if (!spec.left && zeroPad)
        out.repeat('0', padding);
    out.repeat('0', leadingZeros);
    out.put(body, bodyLength);
    if (spec.left)
        out.repeat(' ', padding);
}

char signOf(const Spec& spec, bool negative) {
    return negative ? '-' : spec.plus ? '+' : spec.space ? ' ' : 0;
}

void formatInteger(Output& out, const Spec& spec, u64 magnitude, bool negative) {
    unsigned base = 10;
    const char* digitChars = "0123456789abcdef";
    switch (spec.conversion) {
    case 'o':
        base = 8;
        break;
    case 'x':
    case 'p':
        base = 16;
        break;
    case 'X':
        base = 16;
        digitChars = "0123456789ABCDEF";
        break;
    }

    char digits[24];
    int count = 0;
    for (u64 rest = magnitude; rest; count++) {
        // 32-bit division while the value allows it, which avoids the 64-bit division helper
        if (rest >> 32 == 0) {
            unsigned small = (unsigned)rest;
            digits[sizeof(digits) - 1 - count] = digitChars[small % base];
            rest = small / base;
        } else {
            digits[sizeof(digits) - 1 - count] = digitChars[rest % base];
            rest /= base;
        }
    }

    int precision = spec.precision < 0 ? 1 : spec.precision;
    int leadingZeros = precision > count ? precision - count : 0;
    if (spec.conversion == 'o' && spec.alternate && leadingZeros == 0 && (count == 0 || digits[sizeof(digits) - count] != '0'))
        leadingZeros = 1;

    char prefix[3];
    size_t prefixLength = 0;
    bool isSigned = spec.conversion == 'd' || spec.conversion == 'i';
    if (isSigned && signOf(spec, negative))
        prefix[prefixLength++] = signOf(spec, negative);
    if ((spec.conversion == 'x' || spec.conversion == 'X') && spec.alternate && magnitude) {
        prefix[prefixLength++] = '0';
        prefix[prefixLength++] = spec.conversion;
    }
    if (spec.conversion == 'p') {
        prefix[prefixLength++] = '0';
        prefix[prefixLength++] = 'x';
    }
    emitField(out, spec, prefix, prefixLength, digits + sizeof(digits) - count, count, leadingZeros,
              spec.zero && spec.precision < 0);
}

// Digits of a positive finite value, rounded to `count` significant digits (at most 17 are real, the rest zeros)
struct Decimal {
    char digits[18];
    int count;
    int exponent;

    char at(int i) const { return i < count ? digits[i] : '0'; }
};

Decimal toDecimal(double value, int significant) {
    Decimal d;
    d.count = significant < 17 ? significant : 17;
    if (value == 0) {
        d.exponent = 0;
        memset(d.digits, '0', sizeof(d.digits));
        return d;
    }
    u64 mantissa;
    d.exponent = __cemu_patch_double_to_decimal(value, d.count, &mantissa);
    for (int i = d.count - 1; i >= 0; i--) {
        d.digits[i] = (char)('0' + mantissa % 10);
        mantissa /= 10;
    }
    return d;
}

// Writes the number into `text`, which has room for any field this produces before width padding
class FloatText {
public:
    char* text;
    size_t length = 0;

    void put(char c) { text[length++] = c; }
    void putExponent(char letter, int exponent, int minDigits) {
        put(letter);
        put(exponent < 0 ? '-' : '+');
        unsigned magnitude = exponent < 0 ? -exponent : exponent;
        char digits[6];
        int count = 0;
        do {
            digits[count++] = (char)('0' + magnitude % 10);
            magnitude /= 10;
        } while (magnitude || count < minDigits);
        while (count)
            put(digits[--count]);
    }
};

// %f: `precision` digits after the point
void fixedText(FloatText& t, double value, int precision, bool alternate, bool stripZeros) {
    Decimal d = toDecimal(0, 1);
    if (value != 0) {
        // the exponent before rounding to the precision, so rounding up to a power of ten adds a digit in front
        u64 full;
        int exponent = __cemu_patch_double_to_decimal(value, 17, &full);
        int significant = exponent + 1 + precision;
        if (significant > 0) {
            d = toDecimal(value, significant);
        } else if (significant == 0 && full > 50000000000000000ull) {
            // just below the last digit, more than half of it: rounds up to one unit of it
            d.digits[0] = '1';
            d.exponent = -precision;
        }
    }
    int intDigits = d.exponent >= 0 ? d.exponent + 1 : 1;
    for (int i = 0; i < intDigits; i++)
        t.put(d.exponent >= 0 ? d.at(i) : '0');
    int fractionDigits = precision;
    if (stripZeros) {
        while (fractionDigits > 0) {
            int index = d.exponent + fractionDigits;
            if (index >= 0 && d.at(index) != '0')
                break;
            fractionDigits--;
        }
    }
    if (fractionDigits > 0 || alternate)
        t.put('.');
    for (int i = 1; i <= fractionDigits; i++) {
        int index = d.exponent + i;
        t.put(index >= 0 ? d.at(index) : '0');
    }
}

// %e: one digit before the point and `precision` after
void exponentText(FloatText& t, const Decimal& d, int precision, bool alternate, bool stripZeros, bool upper) {
    t.put(d.at(0));
    int fractionDigits = precision;
    if (stripZeros) {
        while (fractionDigits > 0 && d.at(fractionDigits) == '0')
            fractionDigits--;
    }
    if (fractionDigits > 0 || alternate)
        t.put('.');
    for (int i = 1; i <= fractionDigits; i++)
        t.put(d.at(i));
    t.putExponent(upper ? 'E' : 'e', d.exponent, 2);
}

// %a: hexadecimal mantissa and binary exponent
void hexText(FloatText& t, double value, int precision, bool alternate, bool upper) {
    const char* hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    u64 bits = __builtin_bit_cast(u64, value);
    int exponent = (int)(bits >> 52 & 0x7ff);
    u64 fraction = bits & ((1ull << 52) - 1);
    int lead = 1;
    if (exponent == 0) {
        lead = 0;
        exponent = fraction ? -1022 : 0;
    } else {
        exponent -= 1023;
    }
    if (precision >= 0 && precision < 13) {
        // round the 52 fraction bits to precision hex digits, ties to even
        int dropped = 52 - 4 * precision;
        u64 half = 1ull << (dropped - 1);
        u64 rest = fraction & ((1ull << dropped) - 1);
        fraction >>= dropped;
        if (rest > half || (rest == half && (fraction & 1)))
            fraction++;
        if (fraction >> (4 * precision)) {
            fraction &= (1ull << (4 * precision)) - 1;
            lead++;
        }
        fraction <<= dropped;
    }
    int digits = precision;
    if (digits < 0) {
        digits = 13;
        while (digits > 0 && ((fraction >> (52 - 4 * digits)) & 15) == 0)
            digits--;
    }
    t.put('0');
    t.put(upper ? 'X' : 'x');
    t.put((char)('0' + lead));
    if (digits > 0 || alternate)
        t.put('.');
    for (int i = 1; i <= digits; i++)
        t.put(i <= 13 ? hex[(fraction >> (52 - 4 * i)) & 15] : '0');
    t.putExponent(upper ? 'P' : 'p', exponent, 1);
}

void formatFloat(Output& out, Spec spec, double value) {
    bool upper = spec.conversion >= 'A' && spec.conversion <= 'Z';
    char lower = (char)(spec.conversion | 32);
    bool negative = __builtin_signbit(value);
    if (negative)
        value = -value;
    char prefix[1];
    size_t prefixLength = 0;
    if (signOf(spec, negative))
        prefix[prefixLength++] = signOf(spec, negative);

    if (value != value || value == __builtin_inf()) {
        const char* word = value != value ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        emitField(out, spec, prefix, prefixLength, word, 3, 0, false);
        return;
    }

    // the longest field is %f of 1e308 with the largest precision
    char text[1100];
    FloatText t{text};
    int precision = spec.precision;
    if (precision > 700)
        precision = 700;
    switch (lower) {
    case 'f':
        fixedText(t, value, precision < 0 ? 6 : precision, spec.alternate, false);
        break;
    case 'e':
        if (precision < 0)
            precision = 6;
        exponentText(t, toDecimal(value, precision + 1), precision, spec.alternate, false, upper);
        break;
    case 'g': {
        int significant = precision < 0 ? 6 : precision == 0 ? 1 : precision;
        Decimal d = toDecimal(value, significant);
        if (d.exponent < significant && d.exponent >= -4)
            fixedText(t, value, significant - 1 - d.exponent, spec.alternate, !spec.alternate);
        else
            exponentText(t, d, significant - 1, spec.alternate, !spec.alternate, upper);
        break;
    }
    default:
        hexText(t, value, precision, spec.alternate, upper);
        break;
    }
    emitField(out, spec, prefix, prefixLength, text, t.length, 0, spec.zero);
}

void format(Output& out, const char* fmt, va_list args) {
    for (const char* p = fmt; *p; p++) {
        if (*p != '%') {
            out.put(*p);
            continue;
        }
        const char* start = p;
        Spec spec;
        for (p++;; p++) {
            if (*p == '-')
                spec.left = true;
            else if (*p == '+')
                spec.plus = true;
            else if (*p == ' ')
                spec.space = true;
            else if (*p == '#')
                spec.alternate = true;
            else if (*p == '0')
                spec.zero = true;
            else
                break;
        }
        if (*p == '*') {
            spec.width = va_arg(args, int);
            if (spec.width < 0) {
                spec.left = true;
                spec.width = -spec.width;
            }
            p++;
        } else {
            for (; *p >= '0' && *p <= '9'; p++)
                spec.width = spec.width * 10 + (*p - '0');
        }
        if (*p == '.') {
            p++;
            spec.precision = 0;
            if (*p == '*') {
                spec.precision = va_arg(args, int);
                p++;
            } else {
                for (; *p >= '0' && *p <= '9'; p++)
                    spec.precision = spec.precision * 10 + (*p - '0');
            }
        }
        // the size of the argument in bytes, 0 for int
        int size = 0;
        switch (*p) {
        case 'h':
            size = p[1] == 'h' ? 1 : 2;
            p += size == 1 ? 2 : 1;
            break;
        case 'l':
            size = p[1] == 'l' ? 8 : (int)sizeof(long);
            p += p[1] == 'l' ? 2 : 1;
            break;
        case 'j':
        case 'q':
            size = 8;
            p++;
            break;
        case 'z':
            size = (int)sizeof(size_t);
            p++;
            break;
        case 't':
            size = (int)sizeof(ptrdiff_t);
            p++;
            break;
        }
        spec.conversion = *p;
        switch (*p) {
        case 'd':
        case 'i': {
            long long value = size == 8 ? va_arg(args, long long) : (long long)va_arg(args, int);
            if (size == 1)
                value = (signed char)value;
            else if (size == 2)
                value = (short)value;
            formatInteger(out, spec, value < 0 ? 0 - (u64)value : (u64)value, value < 0);
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            u64 value = size == 8 ? va_arg(args, unsigned long long) : (u64)va_arg(args, unsigned);
            if (size == 1)
                value = (unsigned char)value;
            else if (size == 2)
                value = (unsigned short)value;
            formatInteger(out, spec, value, false);
            break;
        }
        case 'p':
            spec.alternate = false;
            formatInteger(out, spec, (size_t)va_arg(args, void*), false);
            break;
        case 'c': {
            char c = (char)va_arg(args, int);
            spec.zero = false;
            emitField(out, spec, nullptr, 0, &c, 1, 0, false);
            break;
        }
        case 's': {
            const char* text = va_arg(args, const char*);
            if (!text)
                text = "(null)";
            size_t length = spec.precision >= 0 ? strnlen(text, (size_t)spec.precision) : strlen(text);
            emitField(out, spec, nullptr, 0, text, length, 0, false);
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            formatFloat(out, spec, va_arg(args, double));
            break;
        case 'n': {
            void* target = va_arg(args, void*);
            if (size == 1)
                *(signed char*)target = (signed char)out.length;
            else if (size == 2)
                *(short*)target = (short)out.length;
            else if (size == 8)
                *(long long*)target = (long long)out.length;
            else
                *(int*)target = (int)out.length;
            break;
        }
        case '%':
            out.put('%');
            break;
        default:
            // an unknown conversion is printed as written
            out.put(start, (size_t)(p - start) + (*p ? 1 : 0));
            if (!*p)
                p--;
            break;
        }
    }
}

}

extern "C" __attribute__((weak)) int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args) noexcept {
    Output out{buffer, buffer ? size : 0};
    format(out, fmt, args);
    out.finish();
    return (int)out.length;
}

extern "C" __attribute__((weak)) int snprintf(char* buffer, size_t size, const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    int length = vsnprintf(buffer, size, fmt, args);
    va_end(args);
    return length;
}

extern "C" __attribute__((weak)) int vsprintf(char* buffer, const char* fmt, va_list args) noexcept {
    return vsnprintf(buffer, (size_t)-1 / 2, fmt, args);
}

extern "C" __attribute__((weak)) int sprintf(char* buffer, const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    int length = vsprintf(buffer, fmt, args);
    va_end(args);
    return length;
}

// Each call is one OSReport, cut at 1023 characters
extern "C" __attribute__((weak)) int vprintf(const char* fmt, va_list args) {
    char line[1024];
    int length = vsnprintf(line, sizeof(line), fmt, args);
    _IMPORT_COREINIT_OSReport("%s", line);
    return length;
}

extern "C" __attribute__((weak)) int printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int length = vprintf(fmt, args);
    va_end(args);
    return length;
}

extern "C" __attribute__((weak)) int puts(const char* text) {
    _IMPORT_COREINIT_OSReport("%s\n", text);
    return 1;
}
