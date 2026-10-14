// Memory allocation and integer helpers for Cemu patches. malloc, free and operator new allocate through
// cemu_patch_alloc and cemu_patch_free, which the patch runtime implements with the game's default heap (MEM2).
// A patch can define both functions itself to allocate from somewhere else.

#ifndef _CEMU_PATCH_STDLIB_H
#define _CEMU_PATCH_STDLIB_H

#include <_cemu_patch_libc.h>
#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

_CEMU_PATCH_BEGIN_C

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

void* cemu_patch_alloc(size_t size, size_t alignment) _CEMU_PATCH_NOEXCEPT;
void cemu_patch_free(void* ptr) _CEMU_PATCH_NOEXCEPT;

void* malloc(size_t size) _CEMU_PATCH_NOEXCEPT;
void* calloc(size_t count, size_t size) _CEMU_PATCH_NOEXCEPT;
void* aligned_alloc(size_t alignment, size_t size) _CEMU_PATCH_NOEXCEPT;
void free(void* ptr) _CEMU_PATCH_NOEXCEPT;
__attribute__((noreturn)) void abort(void) _CEMU_PATCH_NOEXCEPT;

_CEMU_PATCH_INLINE int abs(int x) _CEMU_PATCH_NOEXCEPT { return x < 0 ? -x : x; }
_CEMU_PATCH_INLINE long labs(long x) _CEMU_PATCH_NOEXCEPT { return x < 0 ? -x : x; }
_CEMU_PATCH_INLINE long long llabs(long long x) _CEMU_PATCH_NOEXCEPT { return x < 0 ? -x : x; }
_CEMU_PATCH_INLINE div_t div(int x, int y) _CEMU_PATCH_NOEXCEPT { div_t r = {x / y, x % y}; return r; }
_CEMU_PATCH_INLINE ldiv_t ldiv(long x, long y) _CEMU_PATCH_NOEXCEPT { ldiv_t r = {x / y, x % y}; return r; }
_CEMU_PATCH_INLINE lldiv_t lldiv(long long x, long long y) _CEMU_PATCH_NOEXCEPT { lldiv_t r = {x / y, x % y}; return r; }

_CEMU_PATCH_END_C

#endif
