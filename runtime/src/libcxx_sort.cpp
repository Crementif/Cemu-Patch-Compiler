// provides: _ZNSt3__16__sortIRNS_6__lessI*
// libc++ declares std::sort of arithmetic types with the default comparison as instantiated in its compiled
// library, so it is instantiated here.

#include <algorithm>

_LIBCPP_BEGIN_NAMESPACE_STD

// Declared by <algorithm> and defined in libc++'s algorithm.cpp
template <class _Comp, class _RandomAccessIterator>
void __sort(_RandomAccessIterator __first, _RandomAccessIterator __last, _Comp) {
    // __less<T> only names the type, the comparison is ranges::less, which uses the branchless partitioning
    std::__introsort<_ClassicAlgPolicy, ranges::less, _RandomAccessIterator,
                     __use_branchless_sort<ranges::less, _RandomAccessIterator>::value>(
        __first, __last, ranges::less{}, 2 * std::__log2i(__last - __first));
}

template void __sort<__less<char>&, char*>(char*, char*, __less<char>&);
template void __sort<__less<signed char>&, signed char*>(signed char*, signed char*, __less<signed char>&);
template void __sort<__less<unsigned char>&, unsigned char*>(unsigned char*, unsigned char*, __less<unsigned char>&);
template void __sort<__less<short>&, short*>(short*, short*, __less<short>&);
template void __sort<__less<unsigned short>&, unsigned short*>(unsigned short*, unsigned short*, __less<unsigned short>&);
template void __sort<__less<int>&, int*>(int*, int*, __less<int>&);
template void __sort<__less<unsigned>&, unsigned*>(unsigned*, unsigned*, __less<unsigned>&);
template void __sort<__less<long>&, long*>(long*, long*, __less<long>&);
template void __sort<__less<unsigned long>&, unsigned long*>(unsigned long*, unsigned long*, __less<unsigned long>&);
template void __sort<__less<long long>&, long long*>(long long*, long long*, __less<long long>&);
template void __sort<__less<unsigned long long>&, unsigned long long*>(unsigned long long*, unsigned long long*,
                                                                         __less<unsigned long long>&);
template void __sort<__less<float>&, float*>(float*, float*, __less<float>&);
template void __sort<__less<double>&, double*>(double*, double*, __less<double>&);
template void __sort<__less<long double>&, long double*>(long double*, long double*, __less<long double>&);

_LIBCPP_END_NAMESPACE_STD
