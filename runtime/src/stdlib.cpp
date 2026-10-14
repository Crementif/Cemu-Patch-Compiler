// provides: strtol strtoul strtoll strtoull atoi atol atoll strtod strtof atof qsort bsearch rand srand
// Number parsing in the "C" locale, sorting, searching and the C library's pseudo-random numbers

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long u64;

extern "C" double __cemu_patch_decimal_to_double(u64 mantissa, double tail, int exponent);

namespace {

int digitValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 99;
}

// The magnitude of an integer, saturated at ULLONG_MAX
struct ParsedInteger {
    u64 magnitude = 0;
    bool negative = false;
    bool overflow = false;
};

ParsedInteger parseInteger(const char* str, char** end, int base) {
    ParsedInteger result;
    const char* p = str;
    while (isspace((unsigned char)*p))
        p++;
    if (*p == '+' || *p == '-')
        result.negative = *p++ == '-';
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 32) == 'x' && isxdigit((unsigned char)p[2])) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = p[0] == '0' ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        errno = EINVAL;
        if (end)
            *end = (char*)str;
        return result;
    }
    const char* digits = p;
    u64 limit = ~0ull / (unsigned)base;
    int lastDigit = (int)(~0ull % (unsigned)base);
    for (int digit; (digit = digitValue(*p)) < base; p++) {
        if (result.magnitude > limit || (result.magnitude == limit && digit > lastDigit))
            result.overflow = true;
        else
            result.magnitude = result.magnitude * (unsigned)base + (unsigned)digit;
    }
    if (p == digits) {
        result.negative = false;
        p = str;
    }
    if (end)
        *end = (char*)p;
    return result;
}

// Parses a signed integer that saturates at [min, max]
long long parseSigned(const char* str, char** end, int base, long long min, long long max) {
    ParsedInteger parsed = parseInteger(str, end, base);
    u64 limit = parsed.negative ? 0 - (u64)min : (u64)max;
    if (parsed.overflow || parsed.magnitude > limit) {
        errno = ERANGE;
        return parsed.negative ? min : max;
    }
    return parsed.negative ? (long long)(0 - parsed.magnitude) : (long long)parsed.magnitude;
}

// Parses an unsigned integer that saturates at max. A minus sign negates the value as unsigned.
u64 parseUnsigned(const char* str, char** end, int base, u64 max) {
    ParsedInteger parsed = parseInteger(str, end, base);
    if (parsed.overflow || parsed.magnitude > max) {
        errno = ERANGE;
        return max;
    }
    return parsed.negative ? (0 - parsed.magnitude) & max : parsed.magnitude;
}

bool startsWithWord(const char* p, const char* word) {
    for (; *word; p++, word++) {
        if ((*p | 32) != *word)
            return false;
    }
    return true;
}

double parseHexFloat(const char*& p) {
    u64 mantissa = 0;
    int exponent = 0;
    bool sticky = false;
    bool any = false;
    for (bool fraction = false;; p++) {
        if (*p == '.' && !fraction) {
            fraction = true;
            continue;
        }
        int digit = digitValue(*p);
        if (digit >= 16)
            break;
        any = true;
        if (mantissa >> 60 == 0) {
            mantissa = mantissa << 4 | (unsigned)digit;
            exponent -= fraction ? 4 : 0;
        } else {
            sticky |= digit != 0;
            exponent += fraction ? 0 : 4;
        }
    }
    if (!any)
        return -1.0;
    if ((*p | 32) == 'p') {
        const char* q = p + 1;
        bool negative = false;
        if (*q == '+' || *q == '-')
            negative = *q++ == '-';
        if (isdigit((unsigned char)*q)) {
            int value = 0;
            for (; isdigit((unsigned char)*q); q++)
                value = value < 100000 ? value * 10 + (*q - '0') : value;
            exponent += negative ? -value : value;
            p = q;
        }
    }
    // the sticky bit sits below double precision, so it only breaks ties
    return scalbn((double)(mantissa | (sticky ? 1 : 0)), exponent);
}

}

extern "C" __attribute__((weak)) long strtol(const char* str, char** end, int base) noexcept {
    return (long)parseSigned(str, end, base, LONG_MIN, LONG_MAX);
}

extern "C" __attribute__((weak)) long long strtoll(const char* str, char** end, int base) noexcept {
    return parseSigned(str, end, base, LLONG_MIN, LLONG_MAX);
}

extern "C" __attribute__((weak)) unsigned long strtoul(const char* str, char** end, int base) noexcept {
    return (unsigned long)parseUnsigned(str, end, base, ULONG_MAX);
}

extern "C" __attribute__((weak)) unsigned long long strtoull(const char* str, char** end, int base) noexcept {
    return parseUnsigned(str, end, base, ULLONG_MAX);
}

extern "C" __attribute__((weak)) int atoi(const char* str) noexcept {
    return (int)parseSigned(str, nullptr, 10, LLONG_MIN, LLONG_MAX);
}

extern "C" __attribute__((weak)) long atol(const char* str) noexcept {
    return (long)parseSigned(str, nullptr, 10, LLONG_MIN, LLONG_MAX);
}

extern "C" __attribute__((weak)) long long atoll(const char* str) noexcept {
    return parseSigned(str, nullptr, 10, LLONG_MIN, LLONG_MAX);
}

// Decimal and hexadecimal floating point, inf, infinity and nan. Overflow gives inf and ERANGE.
extern "C" __attribute__((weak)) double strtod(const char* str, char** end) noexcept {
    const char* p = str;
    while (isspace((unsigned char)*p))
        p++;
    bool negative = false;
    if (*p == '+' || *p == '-')
        negative = *p++ == '-';

    double value;
    if (startsWithWord(p, "inf")) {
        p += startsWithWord(p, "infinity") ? 8 : 3;
        value = __builtin_inf();
    } else if (startsWithWord(p, "nan")) {
        p += 3;
        if (*p == '(') {
            const char* q = p + 1;
            while (isalnum((unsigned char)*q) || *q == '_')
                q++;
            if (*q == ')')
                p = q + 1;
        }
        value = __builtin_nan("");
    } else if (p[0] == '0' && (p[1] | 32) == 'x') {
        const char* q = p + 2;
        value = parseHexFloat(q);
        if (value < 0) {
            // just the 0 of "0x"
            value = 0.0;
            p++;
        } else {
            p = q;
        }
    } else {
        // up to 19 significant digits, which fit in 64 bits, and the value of the digits after them
        u64 mantissa = 0;
        double tail = 0;
        double tailUnit = 0.1;
        int digits = 0;
        int exponent = 0;
        bool any = false;
        for (bool fraction = false;; p++) {
            if (*p == '.' && !fraction) {
                fraction = true;
                continue;
            }
            if (!isdigit((unsigned char)*p))
                break;
            any = true;
            if (digits < 19) {
                if (mantissa || *p != '0') {
                    mantissa = mantissa * 10 + (unsigned)(*p - '0');
                    digits++;
                }
                exponent -= fraction ? 1 : 0;
            } else {
                tail += (*p - '0') * tailUnit;
                tailUnit *= 0.1;
                exponent += fraction ? 0 : 1;
            }
        }
        if (!any) {
            if (end)
                *end = (char*)str;
            return 0.0;
        }
        if ((*p | 32) == 'e') {
            const char* q = p + 1;
            bool exponentNegative = false;
            if (*q == '+' || *q == '-')
                exponentNegative = *q++ == '-';
            if (isdigit((unsigned char)*q)) {
                int written = 0;
                for (; isdigit((unsigned char)*q); q++)
                    written = written < 100000 ? written * 10 + (*q - '0') : written;
                exponent += exponentNegative ? -written : written;
                p = q;
            }
        }
        value = __cemu_patch_decimal_to_double(mantissa, tail, exponent);
        if (value == __builtin_inf() || (value == 0 && mantissa))
            errno = ERANGE;
    }
    if (end)
        *end = (char*)p;
    return negative ? -value : value;
}

extern "C" __attribute__((weak)) float strtof(const char* str, char** end) noexcept {
    double value = strtod(str, end);
    float result = (float)value;
    if ((result == __builtin_inff() || result == -__builtin_inff()) && value == value && value != __builtin_inf() &&
        value != -__builtin_inf())
        errno = ERANGE;
    return result;
}

extern "C" __attribute__((weak)) double atof(const char* str) noexcept {
    return strtod(str, nullptr);
}

// Heapsort: no recursion and no extra memory
extern "C" __attribute__((weak)) void qsort(void* base, size_t count, size_t size,
                                            int (*compare)(const void*, const void*)) {
    unsigned char* items = (unsigned char*)base;
    auto at = [&](size_t i) { return items + i * size; };
    auto swap = [&](size_t i, size_t j) {
        unsigned char* a = at(i);
        unsigned char* b = at(j);
        for (size_t k = 0; k < size; k++) {
            unsigned char t = a[k];
            a[k] = b[k];
            b[k] = t;
        }
    };
    auto siftDown = [&](size_t root, size_t end) {
        for (size_t child; (child = 2 * root + 1) < end; root = child) {
            if (child + 1 < end && compare(at(child), at(child + 1)) < 0)
                child++;
            if (compare(at(root), at(child)) >= 0)
                return;
            swap(root, child);
        }
    };
    if (count < 2 || size == 0)
        return;
    for (size_t i = count / 2; i-- > 0;)
        siftDown(i, count);
    for (size_t end = count - 1; end > 0; end--) {
        swap(0, end);
        siftDown(0, end);
    }
}

extern "C" __attribute__((weak)) void* bsearch(const void* key, const void* base, size_t count, size_t size,
                                               int (*compare)(const void*, const void*)) {
    const unsigned char* items = (const unsigned char*)base;
    while (count) {
        const unsigned char* middle = items + (count / 2) * size;
        int order = compare(key, middle);
        if (order == 0)
            return (void*)middle;
        if (order > 0) {
            items = middle + size;
            count -= count / 2 + 1;
        } else {
            count /= 2;
        }
    }
    return nullptr;
}

namespace {
unsigned long gRandomState = 1;
}

// The generator the C standard gives as an example
extern "C" __attribute__((weak)) int rand() noexcept {
    gRandomState = gRandomState * 1103515245u + 12345u;
    return (int)((gRandomState / 65536u) % 32768u);
}

extern "C" __attribute__((weak)) void srand(unsigned seed) noexcept {
    gRandomState = seed;
}
