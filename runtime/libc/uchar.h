// mbstate_t for libc++. Patches have no multibyte conversion functions.

#ifndef _CEMU_PATCH_UCHAR_H
#define _CEMU_PATCH_UCHAR_H

#include <_cemu_patch_libc.h>

_CEMU_PATCH_BEGIN_C

typedef struct { unsigned int __count; unsigned int __value; } mbstate_t;

_CEMU_PATCH_END_C

#endif
