// The POSIX case insensitive comparisons, for ASCII

#ifndef _CEMU_PATCH_STRINGS_H
#define _CEMU_PATCH_STRINGS_H

#include <_cemu_patch_libc.h>
#include <stddef.h>

_CEMU_PATCH_BEGIN_C

int strcasecmp(const char* a, const char* b) _CEMU_PATCH_NOEXCEPT;
int strncasecmp(const char* a, const char* b, size_t size) _CEMU_PATCH_NOEXCEPT;

_CEMU_PATCH_END_C

#endif
