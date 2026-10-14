// provides: _ZNSt3__114__shared_count* _ZNSt3__119__shared_weak_count* _ZTVNSt3__114__shared_countE
// provides: _ZTVNSt3__119__shared_weak_countE _ZNSt3__112__next_primeEm
// The parts of std::shared_ptr and std::unordered_map that libc++ keeps in its compiled library. libc++ is
// configured without threads, so reference counts are not atomic and a shared_ptr must not be shared between
// threads.

#include <memory>
#include <unordered_map>

_LIBCPP_BEGIN_NAMESPACE_STD

__shared_count::~__shared_count() {}

__shared_weak_count::~__shared_weak_count() {}

void __shared_weak_count::__release_weak() noexcept {
    // the count is -1 based, so the last weak reference brings it from 0 to -1
    if (__shared_weak_owners_ == 0 || --__shared_weak_owners_ == -1)
        __on_zero_shared_weak();
}

__shared_weak_count* __shared_weak_count::lock() noexcept {
    if (__shared_owners_ == -1)
        return nullptr;
    __shared_owners_++;
    return this;
}

const void* __shared_weak_count::__get_deleter(const type_info&) const noexcept {
    return nullptr;
}

// The smallest prime that is at least n
size_t __next_prime(size_t n) {
    if (n <= 2)
        return 2;
    if (n > 4294967291u)
        __libcpp_verbose_abort("__next_prime overflow");
    for (size_t candidate = n | 1;; candidate += 2) {
        bool isPrime = true;
        for (size_t divisor = 3; divisor <= candidate / divisor; divisor += 2) {
            if (candidate % divisor == 0) {
                isPrime = false;
                break;
            }
        }
        if (isPrime)
            return candidate;
    }
}

_LIBCPP_END_NAMESPACE_STD
