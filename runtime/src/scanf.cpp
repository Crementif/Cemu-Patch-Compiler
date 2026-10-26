// provides: vsscanf sscanf
// Formatted input from strings in the "C" locale. The conversions are d i u o x X c s [ p f F e E g G a A n and %,
// with assignment suppression (*), a maximum field width and the length modifiers hh h l ll j z t L.

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

// Copies at most `width` characters of the input into a terminated buffer, since strtol and strtod have no limit
struct Field {
    char text[128];

    const char* take(const char* input, int width) {
        int limit = width > 0 && width < (int)sizeof(text) - 1 ? width : (int)sizeof(text) - 1;
        int i = 0;
        for (; i < limit && input[i]; i++)
            text[i] = input[i];
        text[i] = '\0';
        return text;
    }
};

void storeInteger(void* target, int size, unsigned long long value) {
    switch (size) {
    case 1:
        *(unsigned char*)target = (unsigned char)value;
        break;
    case 2:
        *(unsigned short*)target = (unsigned short)value;
        break;
    case 8:
        *(unsigned long long*)target = value;
        break;
    default:
        *(unsigned*)target = (unsigned)value;
        break;
    }
}

}

extern "C" __attribute__((weak)) int vsscanf(const char* input, const char* fmt, va_list args) noexcept {
    const char* in = input;
    int assigned = 0;
    bool converted = false;
    for (const char* p = fmt; *p; p++) {
        if (isspace((unsigned char)*p)) {
            while (isspace((unsigned char)*in))
                in++;
            continue;
        }
        if (*p != '%' || p[1] == '%') {
            if (*p == '%') {
                p++;
                while (isspace((unsigned char)*in))
                    in++;
            }
            if (*in != *p)
                break;
            in++;
            continue;
        }

        p++;
        bool suppress = *p == '*';
        if (suppress)
            p++;
        int width = 0;
        for (; *p >= '0' && *p <= '9'; p++)
            width = width * 10 + (*p - '0');
        int size = 4;
        bool wide = false;
        switch (*p) {
        case 'h':
            size = p[1] == 'h' ? 1 : 2;
            p += size == 1 ? 2 : 1;
            break;
        case 'l':
            size = p[1] == 'l' ? 8 : (int)sizeof(long);
            wide = true;
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
        case 'L':
            wide = true;
            p++;
            break;
        }

        char conversion = *p;
        if (conversion == 'n') {
            if (!suppress)
                storeInteger(va_arg(args, void*), size, (unsigned long long)(in - input));
            continue;
        }
        if (conversion != 'c' && conversion != '[') {
            while (isspace((unsigned char)*in))
                in++;
        }
        if (!*in)
            return converted || assigned ? assigned : EOF;

        Field field;
        switch (conversion) {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
        case 'p': {
            int base = conversion == 'd' || conversion == 'u' ? 10 : conversion == 'i' ? 0 : conversion == 'o' ? 8 : 16;
            const char* text = field.take(in, width);
            char* end;
            unsigned long long value = conversion == 'd' || conversion == 'i' ? (unsigned long long)strtoll(text, &end, base)
                                                                               : strtoull(text, &end, base);
            if (end == text)
                return assigned;
            in += end - text;
            if (!suppress) {
                if (conversion == 'p')
                    *va_arg(args, void**) = (void*)(size_t)value;
                else
                    storeInteger(va_arg(args, void*), size, value);
            }
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            const char* text = field.take(in, width);
            char* end;
            double value = strtod(text, &end);
            if (end == text)
                return assigned;
            in += end - text;
            if (!suppress) {
                if (wide)
                    *va_arg(args, double*) = value;
                else
                    *va_arg(args, float*) = (float)value;
            }
            break;
        }
        case 's': {
            char* out = suppress ? nullptr : va_arg(args, char*);
            int count = 0;
            for (; *in && !isspace((unsigned char)*in) && (width == 0 || count < width); in++, count++) {
                if (out)
                    *out++ = *in;
            }
            if (out)
                *out = '\0';
            break;
        }
        case 'c': {
            char* out = suppress ? nullptr : va_arg(args, char*);
            int count = width ? width : 1;
            for (int i = 0; i < count; i++, in++) {
                if (!*in)
                    return assigned;
                if (out)
                    *out++ = *in;
            }
            break;
        }
        case '[': {
            // the set ends at the first ] that isn't its first character
            const char* set = ++p;
            bool invert = *set == '^';
            if (invert)
                set++;
            const char* setEnd = *set == ']' ? set + 1 : set;
            while (*setEnd && *setEnd != ']')
                setEnd++;
            p = *setEnd ? setEnd : setEnd - 1;
            auto inSet = [&](char c) {
                for (const char* s = set; s < setEnd; s++) {
                    if (s[1] == '-' && s + 2 < setEnd) {
                        if ((unsigned char)c >= (unsigned char)s[0] && (unsigned char)c <= (unsigned char)s[2])
                            return true;
                        s += 2;
                    } else if (*s == c) {
                        return true;
                    }
                }
                return false;
            };
            char* out = suppress ? nullptr : va_arg(args, char*);
            int count = 0;
            for (; *in && inSet(*in) != invert && (width == 0 || count < width); in++, count++) {
                if (out)
                    *out++ = *in;
            }
            if (count == 0)
                return assigned;
            if (out)
                *out = '\0';
            break;
        }
        default:
            return assigned;
        }
        converted = true;
        if (!suppress)
            assigned++;
    }
    return assigned;
}

extern "C" __attribute__((weak)) int sscanf(const char* input, const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    int count = vsscanf(input, fmt, args);
    va_end(args);
    return count;
}
