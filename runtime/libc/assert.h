// assert stops the game with OSFatal and the failed expression unless NDEBUG is defined. Like every <assert.h>, this
// has no include guard: each inclusion defines assert anew for the NDEBUG of that point.

#include <_cemu_patch_libc.h>

#undef assert

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
_CEMU_PATCH_BEGIN_C
__attribute__((noreturn)) void __cemu_patch_assert_fail(const char* expression, const char* file, int line,
                                                        const char* function) _CEMU_PATCH_NOEXCEPT;
_CEMU_PATCH_END_C
#define assert(expression) \
    ((expression) ? (void)0 : __cemu_patch_assert_fail(#expression, __FILE__, __LINE__, __func__))
#endif

#if !defined(__cplusplus) && !defined(static_assert)
#define static_assert _Static_assert
#endif
