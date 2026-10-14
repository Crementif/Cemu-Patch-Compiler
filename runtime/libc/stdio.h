// Patches have no file streams. This provides formatting to and from strings, printf and puts, which write a line to
// the log through OSReport, and the types and constants libc++ refers to.

#ifndef _CEMU_PATCH_STDIO_H
#define _CEMU_PATCH_STDIO_H

#include <_cemu_patch_libc.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

_CEMU_PATCH_BEGIN_C

typedef struct _CemuPatchFile FILE;
typedef long long fpos_t;

int snprintf(char* __restrict buffer, size_t size, const char* __restrict format, ...) _CEMU_PATCH_NOEXCEPT
    __attribute__((format(printf, 3, 4)));
int vsnprintf(char* __restrict buffer, size_t size, const char* __restrict format, va_list args) _CEMU_PATCH_NOEXCEPT
    __attribute__((format(printf, 3, 0)));
int sprintf(char* __restrict buffer, const char* __restrict format, ...) _CEMU_PATCH_NOEXCEPT
    __attribute__((format(printf, 2, 3)));
int vsprintf(char* __restrict buffer, const char* __restrict format, va_list args) _CEMU_PATCH_NOEXCEPT
    __attribute__((format(printf, 2, 0)));
int sscanf(const char* __restrict input, const char* __restrict format, ...) _CEMU_PATCH_NOEXCEPT
    __attribute__((format(scanf, 2, 3)));
int vsscanf(const char* __restrict input, const char* __restrict format, va_list args) _CEMU_PATCH_NOEXCEPT
    __attribute__((format(scanf, 2, 0)));

int printf(const char* __restrict format, ...) __attribute__((format(printf, 1, 2)));
int vprintf(const char* __restrict format, va_list args) __attribute__((format(printf, 1, 0)));
int puts(const char* text);

// Declared because libc++'s <cstdio> needs std::remove and std::rename to exist next to the algorithms of the same name
int remove(const char* path) _CEMU_PATCH_NOEXCEPT;
int rename(const char* from, const char* to) _CEMU_PATCH_NOEXCEPT;

_CEMU_PATCH_END_C

#endif
