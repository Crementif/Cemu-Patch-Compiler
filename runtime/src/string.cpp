// provides: memcpy memmove memset memcmp memchr strlen strnlen strcmp strncmp strchr strrchr strstr strcpy strncpy
// provides: strcat
// Memory and string functions. Code generation also calls memcpy, memmove and memset for copies of structs and
// arrays. Patches can define any of them themselves.

#include <stdint.h>
#include <string.h>

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
