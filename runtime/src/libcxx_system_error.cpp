// provides: _ZNSt3__116generic_categoryEv _ZNSt3__115system_categoryEv _ZNSt3__114error_category*
// provides: _ZNKSt3__114error_category* _ZTVNSt3__114error_categoryE _ZNKSt3__110error_code7messageEv
// provides: _ZNKSt3__115error_condition7messageEv
// The categories of std::error_code. The system category has the same errors as the generic one.

#include <string.h>
#include <system_error>

namespace {

class GenericCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "generic"; }
    std::string message(int error) const override { return strerror(error); }
};

class SystemCategory : public GenericCategory {
public:
    const char* name() const noexcept override { return "system"; }
    std::error_condition default_error_condition(int error) const noexcept override {
        return std::error_condition(error, std::generic_category());
    }
};

constinit const GenericCategory kGenericCategory;
constinit const SystemCategory kSystemCategory;

}

_LIBCPP_BEGIN_NAMESPACE_STD

error_category::~error_category() noexcept {}

error_condition error_category::default_error_condition(int error) const noexcept {
    return error_condition(error, *this);
}

bool error_category::equivalent(int error, const error_condition& condition) const noexcept {
    return default_error_condition(error) == condition;
}

bool error_category::equivalent(const error_code& code, int condition) const noexcept {
    return *this == code.category() && code.value() == condition;
}

const error_category& generic_category() noexcept {
    return kGenericCategory;
}

const error_category& system_category() noexcept {
    return kSystemCategory;
}

string error_code::message() const {
    return category().message(value());
}

string error_condition::message() const {
    return category().message(value());
}

_LIBCPP_END_NAMESPACE_STD
