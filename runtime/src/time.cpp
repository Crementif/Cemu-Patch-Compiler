// provides: time _ZNSt3__16chrono12system_clock* _ZNSt3__16chrono12steady_clock*

#include <chrono>
#include <time.h>

extern "C" long long _IMPORT_COREINIT_OSGetTime();
extern "C" long long _IMPORT_COREINIT_OSGetSystemTime();

namespace {

constexpr long long kTicksPerSecond = 62156250;
// OSGetTime counts from 2000 instead of 1970
constexpr long long kSecondsFrom1970To2000 = 946684800;

// Splits off whole seconds first, so the multiplication doesn't overflow
long long ticksTo(long long ticks, long long perSecond) {
    return ticks / kTicksPerSecond * perSecond + ticks % kTicksPerSecond * perSecond / kTicksPerSecond;
}

}

extern "C" __attribute__((weak)) time_t time(time_t* result) {
    time_t now = _IMPORT_COREINIT_OSGetTime() / kTicksPerSecond + kSecondsFrom1970To2000;
    if (result)
        *result = now;
    return now;
}

_LIBCPP_BEGIN_NAMESPACE_STD

namespace chrono {

system_clock::time_point system_clock::now() noexcept {
    return time_point(seconds(kSecondsFrom1970To2000) + microseconds(ticksTo(_IMPORT_COREINIT_OSGetTime(), 1000000)));
}

time_t system_clock::to_time_t(const time_point& point) noexcept {
    return duration_cast<seconds>(point.time_since_epoch()).count();
}

system_clock::time_point system_clock::from_time_t(time_t t) noexcept {
    return time_point(seconds(t));
}

steady_clock::time_point steady_clock::now() noexcept {
    return time_point(nanoseconds(ticksTo(_IMPORT_COREINIT_OSGetSystemTime(), 1000000000)));
}

}

_LIBCPP_END_NAMESPACE_STD
