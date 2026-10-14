// Time types for libc++'s <chrono>. Patches have no C clock functions.

#ifndef _CEMU_PATCH_TIME_H
#define _CEMU_PATCH_TIME_H

#include <_cemu_patch_libc.h>
#include <stddef.h>

#define CLOCKS_PER_SEC 1000000

_CEMU_PATCH_BEGIN_C

typedef long long time_t;
typedef long clock_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

_CEMU_PATCH_END_C

#endif
