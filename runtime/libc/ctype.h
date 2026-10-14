// Character classes of the "C" locale, for ASCII

#ifndef _CEMU_PATCH_CTYPE_H
#define _CEMU_PATCH_CTYPE_H

#include <_cemu_patch_libc.h>

_CEMU_PATCH_BEGIN_C

_CEMU_PATCH_INLINE int isdigit(int c) _CEMU_PATCH_NOEXCEPT { return c >= '0' && c <= '9'; }
_CEMU_PATCH_INLINE int isupper(int c) _CEMU_PATCH_NOEXCEPT { return c >= 'A' && c <= 'Z'; }
_CEMU_PATCH_INLINE int islower(int c) _CEMU_PATCH_NOEXCEPT { return c >= 'a' && c <= 'z'; }
_CEMU_PATCH_INLINE int isalpha(int c) _CEMU_PATCH_NOEXCEPT { return isupper(c) || islower(c); }
_CEMU_PATCH_INLINE int isalnum(int c) _CEMU_PATCH_NOEXCEPT { return isalpha(c) || isdigit(c); }
_CEMU_PATCH_INLINE int isxdigit(int c) _CEMU_PATCH_NOEXCEPT { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
_CEMU_PATCH_INLINE int isspace(int c) _CEMU_PATCH_NOEXCEPT { return c == ' ' || (c >= '\t' && c <= '\r'); }
_CEMU_PATCH_INLINE int isblank(int c) _CEMU_PATCH_NOEXCEPT { return c == ' ' || c == '\t'; }
_CEMU_PATCH_INLINE int iscntrl(int c) _CEMU_PATCH_NOEXCEPT { return (c >= 0 && c < 32) || c == 127; }
_CEMU_PATCH_INLINE int isprint(int c) _CEMU_PATCH_NOEXCEPT { return c >= 32 && c < 127; }
_CEMU_PATCH_INLINE int isgraph(int c) _CEMU_PATCH_NOEXCEPT { return c > 32 && c < 127; }
_CEMU_PATCH_INLINE int ispunct(int c) _CEMU_PATCH_NOEXCEPT { return isgraph(c) && !isalnum(c); }
_CEMU_PATCH_INLINE int tolower(int c) _CEMU_PATCH_NOEXCEPT { return isupper(c) ? c + ('a' - 'A') : c; }
_CEMU_PATCH_INLINE int toupper(int c) _CEMU_PATCH_NOEXCEPT { return islower(c) ? c - ('a' - 'A') : c; }

_CEMU_PATCH_END_C

#endif
