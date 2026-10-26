// provides: _ZNSt3__18to_charsEPcS0_*

#include <charconv>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

std::to_chars_result copyOut(char* first, char* last, const char* text, size_t length) {
    if ((size_t)(last - first) < length)
        return {last, std::errc::value_too_large};
    memcpy(first, text, length);
    return {first + length, std::errc{}};
}

// Copies printf's %a without its 0x
std::to_chars_result copyHex(char* first, char* last, char* text, size_t length) {
    char* prefix = strchr(text, 'x');
    if (prefix) {
        memmove(prefix - 1, prefix + 1, length - (prefix + 1 - text));
        length -= 2;
    }
    return copyOut(first, last, text, length);
}

std::to_chars_result withPrecision(char* first, char* last, double value, std::chars_format format, int precision) {
    const char* spec = format == std::chars_format::fixed        ? "%.*f"
                       : format == std::chars_format::scientific ? "%.*e"
                       : format == std::chars_format::hex        ? "%.*a"
                                                                 : "%.*g";
    size_t length = (size_t)snprintf(nullptr, 0, spec, precision, value);
    char small[128];
    char* text = length < sizeof(small) ? small : (char*)malloc(length + 1);
    if (!text)
        return {last, std::errc::value_too_large};
    snprintf(text, length + 1, spec, precision, value);
    std::to_chars_result result = format == std::chars_format::hex ? copyHex(first, last, text, length)
                                                                   : copyOut(first, last, text, length);
    if (text != small)
        free(text);
    return result;
}

// The significant digits of a positive value and the exponent of the first one
struct Digits {
    char digits[18];
    int count = 0;
    int exponent;
};

template <class T>
T readBack(const char* text) {
    if constexpr (sizeof(T) == sizeof(float))
        return strtof(text, nullptr);
    else
        return strtod(text, nullptr);
}

template <class T>
Digits shortestDigits(T value) {
    char text[32];
    // 17 digits always read back the same
    for (int precision = 0;; precision++) {
        snprintf(text, sizeof(text), "%.*e", precision, (double)value);
        if (precision == 16 || readBack<T>(text) == value)
            break;
    }
    Digits d;
    const char* p = text;
    for (; *p != 'e'; p++) {
        if (*p != '.')
            d.digits[d.count++] = *p;
    }
    d.exponent = atoi(p + 1);
    return d;
}

struct Writer {
    char text[400];
    size_t length = 0;

    void put(char c) { text[length++] = c; }
    void put(const char* chars, int count) {
        for (int i = 0; i < count; i++)
            put(chars[i]);
    }
    void zeros(int count) {
        for (; count > 0; count--)
            put('0');
    }
};

// The exact digits of an integer, which can have more than the 17 significant digits printf gets right
void writeInteger(Writer& out, double value) {
    typedef unsigned long long u64;
    if (value == 0) {
        out.put('0');
        return;
    }
    u64 bits = __builtin_bit_cast(u64, value);
    u64 mantissa = (bits & ((1ull << 52) - 1)) | (1ull << 52);
    int exponent = (int)(bits >> 52) - 1075;
    if (exponent <= 0) {
        char* end = std::to_chars(out.text + out.length, out.text + sizeof(out.text), mantissa >> -exponent).ptr;
        out.length = end - out.text;
        return;
    }
    unsigned limbs[34] = {(unsigned)mantissa, (unsigned)(mantissa >> 32)};
    int size = 2;
    for (; exponent > 0; exponent -= 31) {
        int shift = exponent < 31 ? exponent : 31;
        unsigned carry = 0;
        for (int i = 0; i < size; i++) {
            u64 shifted = (u64)limbs[i] << shift | carry;
            limbs[i] = (unsigned)shifted;
            carry = (unsigned)(shifted >> 32);
        }
        if (carry)
            limbs[size++] = carry;
    }
    char digits[320];
    int count = 0;
    while (size) {
        u64 rest = 0;
        for (int i = size - 1; i >= 0; i--) {
            u64 current = rest << 32 | limbs[i];
            limbs[i] = (unsigned)(current / 1000000000);
            rest = current % 1000000000;
        }
        while (size && !limbs[size - 1])
            size--;
        for (int i = 0; i < 9; i++, rest /= 10)
            digits[count++] = (char)('0' + rest % 10);
    }
    while (digits[count - 1] == '0')
        count--;
    while (count)
        out.put(digits[--count]);
}

void writeFixed(Writer& out, const Digits& d, double value) {
    if (d.exponent < 0) {
        out.put("0.", 2);
        out.zeros(-d.exponent - 1);
        out.put(d.digits, d.count);
    } else if (d.exponent + 1 >= d.count) {
        writeInteger(out, value);
    } else {
        out.put(d.digits, d.exponent + 1);
        out.put('.');
        out.put(d.digits + d.exponent + 1, d.count - d.exponent - 1);
    }
}

void writeScientific(Writer& out, const Digits& d) {
    out.put(d.digits[0]);
    if (d.count > 1) {
        out.put('.');
        out.put(d.digits + 1, d.count - 1);
    }
    char exponent[8];
    out.put(exponent, snprintf(exponent, sizeof(exponent), "e%+03d", d.exponent));
}

template <class T>
std::to_chars_result shortest(char* first, char* last, T value, std::chars_format format) {
    // infinity and NaN look the same in every format
    if (!__builtin_isfinite(value) || format == std::chars_format::hex) {
        char text[40];
        int length = snprintf(text, sizeof(text), "%a", (double)value);
        return copyHex(first, last, text, (size_t)length);
    }
    Writer out;
    if (__builtin_signbit(value)) {
        out.put('-');
        value = -value;
    }
    Digits d = shortestDigits(value);
    bool fixed;
    if (format == std::chars_format::fixed) {
        fixed = true;
    } else if (format == std::chars_format::scientific) {
        fixed = false;
    } else if (format == std::chars_format::general) {
        // like %g with its default precision of 6
        fixed = d.exponent >= -4 && d.exponent < 6;
    } else {
        // whichever is shorter, fixed on a tie
        Writer fixedText, scientificText;
        writeFixed(fixedText, d, value);
        writeScientific(scientificText, d);
        fixed = fixedText.length <= scientificText.length;
    }
    if (fixed)
        writeFixed(out, d, value);
    else
        writeScientific(out, d);
    return copyOut(first, last, out.text, out.length);
}

}

_LIBCPP_BEGIN_NAMESPACE_STD

to_chars_result to_chars(char* first, char* last, float value) {
    return shortest(first, last, value, chars_format{});
}

to_chars_result to_chars(char* first, char* last, double value) {
    return shortest(first, last, value, chars_format{});
}

to_chars_result to_chars(char* first, char* last, long double value) {
    return shortest(first, last, (double)value, chars_format{});
}

to_chars_result to_chars(char* first, char* last, float value, chars_format format) {
    return shortest(first, last, value, format);
}

to_chars_result to_chars(char* first, char* last, double value, chars_format format) {
    return shortest(first, last, value, format);
}

to_chars_result to_chars(char* first, char* last, long double value, chars_format format) {
    return shortest(first, last, (double)value, format);
}

to_chars_result to_chars(char* first, char* last, float value, chars_format format, int precision) {
    return withPrecision(first, last, value, format, precision);
}

to_chars_result to_chars(char* first, char* last, double value, chars_format format, int precision) {
    return withPrecision(first, last, value, format, precision);
}

to_chars_result to_chars(char* first, char* last, long double value, chars_format format, int precision) {
    return withPrecision(first, last, (double)value, format, precision);
}

_LIBCPP_END_NAMESPACE_STD
