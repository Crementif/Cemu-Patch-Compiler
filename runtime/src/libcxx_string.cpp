// provides: _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE* _ZNSt3__19to_stringE*
// libc++ declares std::string's members as instantiated in its compiled library, so they are instantiated here
// the same way libc++'s string.cpp does. to_string supports the integer types only.

#include <charconv>
#include <string>

_LIBCPP_BEGIN_NAMESPACE_STD

#define _LIBCPP_DEFINE(...) template __VA_ARGS__;
_LIBCPP_STRING_V1_EXTERN_TEMPLATE_LIST(_LIBCPP_DEFINE, char)
#undef _LIBCPP_DEFINE

template <class T>
static string integerToString(T value) {
    char buffer[24];
    to_chars_result result = to_chars(buffer, buffer + sizeof(buffer), value);
    return string(buffer, result.ptr);
}

string to_string(int value) { return integerToString(value); }
string to_string(unsigned value) { return integerToString(value); }
string to_string(long value) { return integerToString(value); }
string to_string(unsigned long value) { return integerToString(value); }
string to_string(long long value) { return integerToString(value); }
string to_string(unsigned long long value) { return integerToString(value); }

_LIBCPP_END_NAMESPACE_STD
