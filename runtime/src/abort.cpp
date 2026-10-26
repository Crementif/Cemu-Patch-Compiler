// provides: abort __cxa_pure_virtual __cxa_deleted_virtual _ZNSt3__122__libcpp_verbose_abortEPKcz
// provides: __cemu_patch_assert_fail _ZNSt3__117__throw_bad_allocEv _ZNSt3__120__throw_system_errorEiPKc

#include <stdarg.h>
#include <stdlib.h>
#include <__verbose_abort>

extern "C" __attribute__((noreturn)) void _IMPORT_COREINIT_OSFatal(const char* message);

namespace {

struct MessageWriter {
    char text[256];
    unsigned length = 0;

    void put(char c) {
        if (length + 1 < sizeof(text))
            text[length++] = c;
    }

    void putString(const char* str) {
        for (; str && *str; str++)
            put(*str);
    }

    void putUnsigned(unsigned long value, unsigned base) {
        char digits[12];
        unsigned count = 0;
        do {
            unsigned digit = (unsigned)(value % base);
            digits[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
            value /= base;
        } while (value);
        while (count)
            put(digits[--count]);
    }
};

}

// Supports the conversions libc++ uses in its messages: %s, %d, %i, %u, %x, %p, %c and %%, with l, ll and z.
// Numbers are printed with 32 bits, so 64-bit division isn't pulled into every patch.
__attribute__((weak, noreturn)) void std::__libcpp_verbose_abort(const char* format, ...) {
    MessageWriter writer;
    va_list args;
    va_start(args, format);
    for (const char* p = format; *p; p++) {
        if (*p != '%') {
            writer.put(*p);
            continue;
        }
        int longs = 0;
        bool sizeT = false;
        for (p++; *p == 'l' || *p == 'z'; p++) {
            if (*p == 'z')
                sizeT = true;
            else
                longs++;
        }
        switch (*p) {
        case 's':
            writer.putString(va_arg(args, const char*));
            break;
        case 'c':
            writer.put((char)va_arg(args, int));
            break;
        case 'd':
        case 'i': {
            long value = longs >= 2 ? (long)va_arg(args, long long) : longs ? va_arg(args, long) : va_arg(args, int);
            if (value < 0) {
                writer.put('-');
                writer.putUnsigned(0ul - (unsigned long)value, 10);
            } else {
                writer.putUnsigned((unsigned long)value, 10);
            }
            break;
        }
        case 'u':
        case 'x': {
            unsigned long value = longs >= 2        ? (unsigned long)va_arg(args, unsigned long long)
                                  : longs || sizeT ? va_arg(args, unsigned long)
                                                   : va_arg(args, unsigned);
            writer.putUnsigned(value, *p == 'x' ? 16 : 10);
            break;
        }
        case 'p':
            writer.putString("0x");
            writer.putUnsigned((unsigned long)va_arg(args, void*), 16);
            break;
        case '%':
            writer.put('%');
            break;
        default:
            if (!*p)
                p--;
            break;
        }
    }
    va_end(args);
    writer.text[writer.length] = '\0';
    _IMPORT_COREINIT_OSFatal(writer.text);
}

// assert() of <assert.h>
extern "C" __attribute__((weak, noreturn)) void __cemu_patch_assert_fail(const char* expression, const char* file, int line,
                                                                         const char* function) noexcept {
    std::__libcpp_verbose_abort("%s:%d: %s: Assertion '%s' failed", file, line, function, expression);
}

extern "C" __attribute__((weak, noreturn)) void abort() noexcept {
    _IMPORT_COREINIT_OSFatal("Patch called abort()");
}

_LIBCPP_BEGIN_NAMESPACE_STD

__attribute__((weak, noreturn)) void __throw_bad_alloc() {
    __libcpp_verbose_abort("bad_alloc was thrown in -fno-exceptions mode");
}

__attribute__((weak, noreturn)) void __throw_system_error(int error, const char* message) {
    __libcpp_verbose_abort("system_error was thrown in -fno-exceptions mode with error %i and message \"%s\"", error,
                           message);
}

_LIBCPP_END_NAMESPACE_STD

// vtables of abstract classes point their pure and deleted virtual functions here
extern "C" __attribute__((weak, noreturn)) void __cxa_pure_virtual() {
    _IMPORT_COREINIT_OSFatal("Patch called a pure virtual function");
}

extern "C" __attribute__((weak, noreturn)) void __cxa_deleted_virtual() {
    _IMPORT_COREINIT_OSFatal("Patch called a deleted virtual function");
}
