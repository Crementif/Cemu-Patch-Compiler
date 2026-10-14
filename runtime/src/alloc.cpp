// provides: cemu_patch_alloc cemu_patch_free malloc calloc aligned_alloc free _ZSt7nothrow
// provides: _Znwm _Znam _ZnwmRKSt9nothrow_t _ZnamRKSt9nothrow_t _ZnwmSt11align_val_t _ZnamSt11align_val_t
// provides: _ZnwmSt11align_val_tRKSt9nothrow_t _ZnamSt11align_val_tRKSt9nothrow_t
// provides: _ZdlPv _ZdaPv _ZdlPvm _ZdaPvm _ZdlPvSt11align_val_t _ZdaPvSt11align_val_t _ZdlPvmSt11align_val_t
// provides: _ZdaPvmSt11align_val_t _ZdlPvRKSt9nothrow_t _ZdaPvRKSt9nothrow_t _ZdlPvSt11align_val_tRKSt9nothrow_t
// provides: _ZdaPvSt11align_val_tRKSt9nothrow_t
// All allocations of a patch go through cemu_patch_alloc and cemu_patch_free. By default they use the game's default
// heap, the MEM2 expanded heap that MEMAllocFromDefaultHeap allocates from. Patches share it with the game, which
// creates its own heaps from it during boot, so a patch should keep its allocations small. Blocks are taken from
// the top of the heap, which leaves the large free space at the bottom in one piece for the game.
// A patch replaces the allocator by defining cemu_patch_alloc and cemu_patch_free itself.

#include <new>
#include <stdlib.h>
#include <string.h>

extern "C" void* _IMPORT_COREINIT_MEMGetBaseHeapHandle(int arena);
extern "C" void* _IMPORT_COREINIT_MEMAllocFromExpHeapEx(void* heap, unsigned size, int alignment);
extern "C" void _IMPORT_COREINIT_MEMFreeToExpHeap(void* heap, void* ptr);

static constexpr int kMem2Arena = 1;

extern "C" __attribute__((weak)) void* cemu_patch_alloc(size_t size, size_t alignment) noexcept {
    if (alignment < 4)
        alignment = 4;
    // a negative alignment allocates from the top of the heap
    return _IMPORT_COREINIT_MEMAllocFromExpHeapEx(_IMPORT_COREINIT_MEMGetBaseHeapHandle(kMem2Arena), size ? size : 1,
                                                  -(int)alignment);
}

extern "C" __attribute__((weak)) void cemu_patch_free(void* ptr) noexcept {
    if (ptr)
        _IMPORT_COREINIT_MEMFreeToExpHeap(_IMPORT_COREINIT_MEMGetBaseHeapHandle(kMem2Arena), ptr);
}

extern "C" __attribute__((weak)) void* malloc(size_t size) noexcept {
    return cemu_patch_alloc(size, 8);
}

extern "C" __attribute__((weak)) void* calloc(size_t count, size_t size) noexcept {
    if (size && count > (size_t)-1 / size)
        return nullptr;
    void* ptr = cemu_patch_alloc(count * size, 8);
    if (ptr)
        memset(ptr, 0, count * size);
    return ptr;
}

extern "C" __attribute__((weak)) void* aligned_alloc(size_t alignment, size_t size) noexcept {
    return cemu_patch_alloc(size, alignment);
}

extern "C" __attribute__((weak)) void free(void* ptr) noexcept {
    cemu_patch_free(ptr);
}

const std::nothrow_t std::nothrow{};

// new without nothrow can't report failure without exceptions, so running out of memory is fatal
static void* allocateOrDie(size_t size, size_t alignment) {
    void* ptr = cemu_patch_alloc(size, alignment);
    if (!ptr)
        std::__libcpp_verbose_abort("Patch out of memory: allocating %u bytes failed", (unsigned)size);
    return ptr;
}

static constexpr size_t kDefaultAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

__attribute__((weak)) void* operator new(size_t size) { return allocateOrDie(size, kDefaultAlignment); }
__attribute__((weak)) void* operator new[](size_t size) { return allocateOrDie(size, kDefaultAlignment); }
__attribute__((weak)) void* operator new(size_t size, std::align_val_t alignment) { return allocateOrDie(size, (size_t)alignment); }
__attribute__((weak)) void* operator new[](size_t size, std::align_val_t alignment) { return allocateOrDie(size, (size_t)alignment); }
__attribute__((weak)) void* operator new(size_t size, const std::nothrow_t&) noexcept { return cemu_patch_alloc(size, kDefaultAlignment); }
__attribute__((weak)) void* operator new[](size_t size, const std::nothrow_t&) noexcept { return cemu_patch_alloc(size, kDefaultAlignment); }
__attribute__((weak)) void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return cemu_patch_alloc(size, (size_t)alignment); }
__attribute__((weak)) void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return cemu_patch_alloc(size, (size_t)alignment); }

__attribute__((weak)) void operator delete(void* ptr) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete[](void* ptr) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete(void* ptr, size_t) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete[](void* ptr, size_t) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete(void* ptr, std::align_val_t) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete[](void* ptr, std::align_val_t) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete(void* ptr, size_t, std::align_val_t) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete[](void* ptr, size_t, std::align_val_t) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete(void* ptr, const std::nothrow_t&) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete[](void* ptr, const std::nothrow_t&) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { cemu_patch_free(ptr); }
__attribute__((weak)) void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { cemu_patch_free(ptr); }
