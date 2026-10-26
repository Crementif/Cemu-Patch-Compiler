// provides: _ZNSt3__13pmr15memory_resource* _ZTVNSt3__13pmr15memory_resourceE _ZNSt3__13pmr20get_default_resourceEv
// provides: _ZNSt3__13pmr20set_default_resource* _ZNSt3__13pmr19new_delete_resourceEv
// provides: _ZNSt3__13pmr20null_memory_resourceEv _ZNSt3__13pmr25monotonic_buffer_resource*
// provides: _ZTVNSt3__13pmr25monotonic_buffer_resourceE
// std::pmr's global resources and monotonic_buffer_resource. The pool resources aren't supported.

#include <memory_resource>
#include <stdint.h>

namespace {

class NewDeleteResource : public std::pmr::memory_resource {
    void* do_allocate(size_t bytes, size_t alignment) override {
        return ::operator new(bytes, std::align_val_t(alignment));
    }
    void do_deallocate(void* ptr, size_t bytes, size_t alignment) override {
        ::operator delete(ptr, bytes, std::align_val_t(alignment));
    }
    bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};

class NullResource : public std::pmr::memory_resource {
    void* do_allocate(size_t, size_t) override { std::__throw_bad_alloc(); }
    void do_deallocate(void*, size_t, size_t) override {}
    bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};

constinit NewDeleteResource gNewDeleteResource;
constinit NullResource gNullResource;
std::pmr::memory_resource* gDefaultResource = &gNewDeleteResource;

size_t roundUp(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Takes `bytes` from the end of the free space [start, cur), which shrinks to below them
void* allocateDownwards(char* start, char*& cur, size_t bytes, size_t alignment) {
    if ((size_t)(cur - start) < bytes)
        return nullptr;
    char* ptr = (char*)((uintptr_t)(cur - bytes) & ~(uintptr_t)(alignment - 1));
    if (ptr < start)
        return nullptr;
    cur = ptr;
    return ptr;
}

}

_LIBCPP_BEGIN_NAMESPACE_STD

namespace pmr {

memory_resource::~memory_resource() = default;

memory_resource* new_delete_resource() noexcept {
    return &gNewDeleteResource;
}

memory_resource* null_memory_resource() noexcept {
    return &gNullResource;
}

memory_resource* get_default_resource() noexcept {
    return gDefaultResource;
}

memory_resource* set_default_resource(memory_resource* resource) noexcept {
    memory_resource* previous = gDefaultResource;
    gDefaultResource = resource ? resource : &gNewDeleteResource;
    return previous;
}

void* monotonic_buffer_resource::__initial_descriptor::__try_allocate_from_chunk(size_t bytes, size_t alignment) {
    return __cur_ ? allocateDownwards(__start_, __cur_, bytes, alignment) : nullptr;
}

void* monotonic_buffer_resource::__chunk_footer::__try_allocate_from_chunk(size_t bytes, size_t alignment) {
    return allocateDownwards(__start_, __cur_, bytes, alignment);
}

// Allocations come from the initial buffer, then from the newest chunk. Every new chunk is at least twice as big as
// the one before it, with its footer at the end.
void* monotonic_buffer_resource::do_allocate(size_t bytes, size_t alignment) {
    if (void* ptr = __initial_.__try_allocate_from_chunk(bytes, alignment))
        return ptr;
    if (__chunks_) {
        if (void* ptr = __chunks_->__try_allocate_from_chunk(bytes, alignment))
            return ptr;
    }

    const size_t footerSize = sizeof(__chunk_footer);
    const size_t footerAlignment = alignof(__chunk_footer);
    if (alignment < footerAlignment)
        alignment = footerAlignment;
    size_t previousSize;
    if (__chunks_)
        previousSize = __chunks_->__allocation_size();
    else if (__initial_.__start_)
        previousSize = roundUp((size_t)(__initial_.__end_ - __initial_.__start_), footerAlignment) + footerSize;
    else
        previousSize = roundUp(__initial_.__size_, footerAlignment) + footerSize;
    size_t size = roundUp(bytes, footerAlignment) + footerSize;
    if (size <= previousSize)
        size = roundUp(2 * (previousSize - footerSize), footerAlignment) + footerSize;

    char* start = (char*)__res_->allocate(size, alignment);
    __chunk_footer* footer = (__chunk_footer*)(start + size - footerSize);
    footer->__next_ = __chunks_;
    footer->__start_ = start;
    footer->__cur_ = (char*)footer;
    footer->__align_ = alignment;
    __chunks_ = footer;
    return footer->__try_allocate_from_chunk(bytes, alignment);
}

}

_LIBCPP_END_NAMESPACE_STD
