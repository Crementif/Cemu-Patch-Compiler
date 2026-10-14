// provides: errno
// The error number the C library functions set: one global shared by all threads, like on a single threaded libc

#include <errno.h>

extern "C" __attribute__((weak)) int errno = 0;
