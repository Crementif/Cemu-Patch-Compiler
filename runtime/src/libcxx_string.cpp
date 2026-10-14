// provides: _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE* _ZNSt3__19to_stringE*
// provides: _ZNSt3__14stoiE* _ZNSt3__14stolE* _ZNSt3__15stoulE* _ZNSt3__15stollE* _ZNSt3__16stoullE*
// provides: _ZNSt3__14stofE* _ZNSt3__14stodE*
// libc++ declares std::string's members as instantiated in its compiled library, so they are instantiated here
// the same way libc++'s string.cpp does, with to_string and the sto functions next to them. long double isn't
// supported by either.

#include <charconv>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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

// %f, like libc++
string to_string(float value) { return to_string((double)value); }
string to_string(double value) {
    char buffer[330];
    int length = snprintf(buffer, sizeof(buffer), "%f", value);
    return string(buffer, length);
}

// Parses the number at the start of str with convert(text, end), stopping the game like libc++ would throw when there
// is no number or it is out of range
template <class Convert>
static auto parse(const char* function, const string& str, size_t* index, Convert convert) {
    const char* begin = str.c_str();
    char* end;
    int savedErrno = errno;
    errno = 0;
    auto value = convert(begin, &end);
    int error = errno;
    errno = savedErrno;
    if (end == begin)
        __throw_invalid_argument(function);
    if (error == ERANGE)
        __throw_out_of_range(function);
    if (index)
        *index = (size_t)(end - begin);
    return value;
}

int stoi(const string& str, size_t* index, int base) {
    long value = parse("stoi", str, index, [=](const char* text, char** end) { return strtol(text, end, base); });
    if (value < INT_MIN || value > INT_MAX)
        __throw_out_of_range("stoi");
    return (int)value;
}
long stol(const string& str, size_t* index, int base) {
    return parse("stol", str, index, [=](const char* text, char** end) { return strtol(text, end, base); });
}
unsigned long stoul(const string& str, size_t* index, int base) {
    return parse("stoul", str, index, [=](const char* text, char** end) { return strtoul(text, end, base); });
}
long long stoll(const string& str, size_t* index, int base) {
    return parse("stoll", str, index, [=](const char* text, char** end) { return strtoll(text, end, base); });
}
unsigned long long stoull(const string& str, size_t* index, int base) {
    return parse("stoull", str, index, [=](const char* text, char** end) { return strtoull(text, end, base); });
}
float stof(const string& str, size_t* index) { return parse("stof", str, index, strtof); }
double stod(const string& str, size_t* index) { return parse("stod", str, index, strtod); }

_LIBCPP_END_NAMESPACE_STD
