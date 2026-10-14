// provides: memcpy memmove memset memcmp memchr strlen strnlen strcmp strncmp strchr strrchr strstr strcpy strncpy
// provides: strcat strncat strspn strcspn strpbrk strtok strtok_r strcoll strxfrm strerror strcasecmp strncasecmp
// Memory and string functions. Code generation also calls memcpy, memmove and memset for copies of structs and
// arrays. Patches can define any of them themselves.

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

typedef uint32_t __attribute__((may_alias)) AliasedWord;

static bool isWordAligned(const void* a, const void* b) {
    return (((uintptr_t)a | (uintptr_t)b) & 3) == 0;
}

extern "C" __attribute__((weak)) void* memcpy(void* __restrict dst, const void* __restrict src, size_t size) noexcept {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (isWordAligned(d, s)) {
        for (; size >= 4; size -= 4, d += 4, s += 4)
            *(AliasedWord*)d = *(const AliasedWord*)s;
    }
    while (size--)
        *d++ = *s++;
    return dst;
}

extern "C" __attribute__((weak)) void* memmove(void* dst, const void* src, size_t size) noexcept {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d <= s || d >= s + size)
        return memcpy(dst, src, size);
    d += size;
    s += size;
    if (isWordAligned(d, s)) {
        for (; size >= 4; size -= 4) {
            d -= 4;
            s -= 4;
            *(AliasedWord*)d = *(const AliasedWord*)s;
        }
    }
    while (size--)
        *--d = *--s;
    return dst;
}

extern "C" __attribute__((weak)) void* memset(void* dst, int value, size_t size) noexcept {
    unsigned char* d = (unsigned char*)dst;
    for (; size && ((uintptr_t)d & 3); size--)
        *d++ = (unsigned char)value;
    uint32_t word = (unsigned char)value * 0x01010101u;
    for (; size >= 4; size -= 4, d += 4)
        *(AliasedWord*)d = word;
    while (size--)
        *d++ = (unsigned char)value;
    return dst;
}

extern "C" __attribute__((weak)) int memcmp(const void* a, const void* b, size_t size) noexcept {
    const unsigned char* x = (const unsigned char*)a;
    const unsigned char* y = (const unsigned char*)b;
    for (; size; size--, x++, y++) {
        if (*x != *y)
            return *x < *y ? -1 : 1;
    }
    return 0;
}

extern "C" __attribute__((weak)) void* memchr(const void* ptr, int value, size_t size) noexcept {
    const unsigned char* p = (const unsigned char*)ptr;
    for (; size; size--, p++) {
        if (*p == (unsigned char)value)
            return (void*)p;
    }
    return nullptr;
}

extern "C" __attribute__((weak)) size_t strlen(const char* str) noexcept {
    const char* end = str;
    while (*end)
        end++;
    return (size_t)(end - str);
}

extern "C" __attribute__((weak)) size_t strnlen(const char* str, size_t maxSize) noexcept {
    size_t length = 0;
    while (length < maxSize && str[length])
        length++;
    return length;
}

extern "C" __attribute__((weak)) int strcmp(const char* a, const char* b) noexcept {
    for (; *a && *a == *b; a++, b++) {
    }
    return (unsigned char)*a - (unsigned char)*b;
}

extern "C" __attribute__((weak)) int strncmp(const char* a, const char* b, size_t size) noexcept {
    for (; size; size--, a++, b++) {
        if (*a != *b || !*a)
            return (unsigned char)*a - (unsigned char)*b;
    }
    return 0;
}

extern "C" __attribute__((weak)) char* strchr(const char* str, int c) noexcept {
    for (;; str++) {
        if (*str == (char)c)
            return (char*)str;
        if (!*str)
            return nullptr;
    }
}

extern "C" __attribute__((weak)) char* strrchr(const char* str, int c) noexcept {
    const char* found = nullptr;
    for (;; str++) {
        if (*str == (char)c)
            found = str;
        if (!*str)
            return (char*)found;
    }
}

extern "C" __attribute__((weak)) char* strstr(const char* str, const char* find) noexcept {
    size_t findLength = strlen(find);
    for (; *str; str++) {
        if (strncmp(str, find, findLength) == 0)
            return (char*)str;
    }
    return findLength ? nullptr : (char*)str;
}

extern "C" __attribute__((weak)) char* strcpy(char* __restrict dst, const char* __restrict src) noexcept {
    char* d = dst;
    while ((*d++ = *src++)) {
    }
    return dst;
}

extern "C" __attribute__((weak)) char* strncpy(char* __restrict dst, const char* __restrict src, size_t size) noexcept {
    size_t i = 0;
    for (; i < size && src[i]; i++)
        dst[i] = src[i];
    for (; i < size; i++)
        dst[i] = '\0';
    return dst;
}

extern "C" __attribute__((weak)) char* strcat(char* __restrict dst, const char* __restrict src) noexcept {
    strcpy(dst + strlen(dst), src);
    return dst;
}

extern "C" __attribute__((weak)) char* strncat(char* __restrict dst, const char* __restrict src, size_t size) noexcept {
    char* d = dst + strlen(dst);
    for (; size && *src; size--)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

extern "C" __attribute__((weak)) size_t strspn(const char* str, const char* accept) noexcept {
    size_t length = 0;
    while (str[length] && strchr(accept, str[length]))
        length++;
    return length;
}

extern "C" __attribute__((weak)) size_t strcspn(const char* str, const char* reject) noexcept {
    size_t length = 0;
    while (str[length] && !strchr(reject, str[length]))
        length++;
    return length;
}

extern "C" __attribute__((weak)) char* strpbrk(const char* str, const char* accept) noexcept {
    str += strcspn(str, accept);
    return *str ? (char*)str : nullptr;
}

extern "C" __attribute__((weak)) char* strtok_r(char* __restrict str, const char* __restrict separators,
                                                 char** __restrict state) noexcept {
    if (!str)
        str = *state;
    str += strspn(str, separators);
    if (!*str) {
        *state = str;
        return nullptr;
    }
    char* end = str + strcspn(str, separators);
    if (*end)
        *end++ = '\0';
    *state = end;
    return str;
}

extern "C" __attribute__((weak)) char* strtok(char* __restrict str, const char* __restrict separators) noexcept {
    static char* state = nullptr;
    return strtok_r(str, separators, &state);
}

// The "C" locale collates by byte value
extern "C" __attribute__((weak)) int strcoll(const char* a, const char* b) noexcept {
    return strcmp(a, b);
}

extern "C" __attribute__((weak)) size_t strxfrm(char* __restrict dst, const char* __restrict src, size_t size) noexcept {
    size_t length = strlen(src);
    if (length < size)
        memcpy(dst, src, length + 1);
    return length;
}

extern "C" __attribute__((weak)) char* strerror(int error) noexcept {
    switch (error) {
    case 0:
        return (char*)"Success";
    case EDOM:
        return (char*)"Numerical argument out of domain";
    case ERANGE:
        return (char*)"Numerical result out of range";
    case EINVAL:
        return (char*)"Invalid argument";
    case ENOMEM:
        return (char*)"Cannot allocate memory";
    default:
        return (char*)"Unknown error";
    }
}

static int lowerAscii(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

extern "C" __attribute__((weak)) int strcasecmp(const char* a, const char* b) noexcept {
    for (; *a && lowerAscii(*a) == lowerAscii(*b); a++, b++) {
    }
    return lowerAscii(*a) - lowerAscii(*b);
}

extern "C" __attribute__((weak)) int strncasecmp(const char* a, const char* b, size_t size) noexcept {
    for (; size; size--, a++, b++) {
        if (lowerAscii(*a) != lowerAscii(*b) || !*a)
            return lowerAscii(*a) - lowerAscii(*b);
    }
    return 0;
}
