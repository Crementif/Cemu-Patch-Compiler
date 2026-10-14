// Shared definitions of the C library headers for Cemu patches

#ifndef _CEMU_PATCH_LIBC_H
#define _CEMU_PATCH_LIBC_H

#ifdef __cplusplus
#  define _CEMU_PATCH_BEGIN_C extern "C" {
#  define _CEMU_PATCH_END_C }
#  define _CEMU_PATCH_INLINE inline
#  define _CEMU_PATCH_NOEXCEPT noexcept
#else
#  define _CEMU_PATCH_BEGIN_C
#  define _CEMU_PATCH_END_C
#  define _CEMU_PATCH_INLINE static inline
#  define _CEMU_PATCH_NOEXCEPT
#endif

#endif
