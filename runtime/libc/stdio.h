// Patches have no file streams. This only provides the types and constants libc++ refers to.

#ifndef _CEMU_PATCH_STDIO_H
#define _CEMU_PATCH_STDIO_H

#include <_cemu_patch_libc.h>
#include <stddef.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

_CEMU_PATCH_BEGIN_C

typedef struct _CemuPatchFile FILE;
typedef long long fpos_t;

// Declared because libc++'s <cstdio> needs std::remove and std::rename to exist next to the algorithms of the same name
int remove(const char* path) _CEMU_PATCH_NOEXCEPT;
int rename(const char* from, const char* to) _CEMU_PATCH_NOEXCEPT;

_CEMU_PATCH_END_C

#endif
