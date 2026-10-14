// moduleMatches = 0x6267BFD0

// On-target coverage for libc++ and the patch runtime: containers, std::string, std::sort, shared_ptr, a global
// with a static constructor and a replaced allocator. patchContainersSelfTest() runs from the entry callback and
// reports a bit per failed check.

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern "C" void _IMPORT_COREINIT_OSReport(const char* format, ...);
extern "C" void* _IMPORT_COREINIT_MEMCreateExpHeapEx(void* memory, unsigned size, unsigned short flags);
extern "C" void* _IMPORT_COREINIT_MEMAllocFromExpHeapEx(void* heap, unsigned size, int alignment);
extern "C" void _IMPORT_COREINIT_MEMFreeToExpHeap(void* heap, void* ptr);
extern "C" unsigned _IMPORT_COREINIT_MEMGetTotalFreeSizeForExpHeap(void* heap);

// By default patches allocate from the game's default heap, but the homebrew test title takes all of it before the
// entry callback runs. So this patch replaces the allocator with an expanded heap in its own code cave memory.
alignas(32) static unsigned char gArena[64 * 1024];
static void* gArenaHeap;

static void* arenaHeap() {
    constexpr unsigned short kThreadSafe = 4;
    if (!gArenaHeap)
        gArenaHeap = _IMPORT_COREINIT_MEMCreateExpHeapEx(gArena, sizeof(gArena), kThreadSafe);
    return gArenaHeap;
}

extern "C" void* cemu_patch_alloc(size_t size, size_t alignment) noexcept {
    return _IMPORT_COREINIT_MEMAllocFromExpHeapEx(arenaHeap(), size ? size : 1, alignment < 4 ? 4 : (int)alignment);
}

extern "C" void cemu_patch_free(void* ptr) noexcept {
    if (ptr)
        _IMPORT_COREINIT_MEMFreeToExpHeap(arenaHeap(), ptr);
}

// Built by the static constructor before the entry callback runs
static std::string gGreeting = "hello, world from a heap allocated string";

static unsigned arenaFree() {
    return _IMPORT_COREINIT_MEMGetTotalFreeSizeForExpHeap(arenaHeap());
}

static int sumOf(std::span<const int> values) {
    int sum = 0;
    for (int value : values)
        sum += value;
    return sum;
}

static std::optional<int> findSquare(const std::vector<int>& values, int square) {
    auto it = std::find_if(values.begin(), values.end(), [&](int value) { return value * value == square; });
    if (it == values.end())
        return std::nullopt;
    return *it;
}

static unsigned runChecks() {
    unsigned failures = 0;

    // vector and std::sort with the default comparison, which libc++ keeps in its compiled library
    std::vector<int> values;
    for (int i = 0; i < 100; i++)
        values.push_back((i * 37) % 100);
    std::sort(values.begin(), values.end());
    if (values.size() != 100 || values.front() != 0 || values.back() != 99 || !std::is_sorted(values.begin(), values.end()))
        failures |= 1u << 0;
    if (sumOf(values) != 4950)
        failures |= 1u << 1;
    if (findSquare(values, 49) != 7 || findSquare(values, 50).has_value())
        failures |= 1u << 2;

    // std::string with heap storage, to_string of 32 and 64-bit values and string_view
    std::string text = gGreeting + ", " + std::to_string(-42) + " " + std::to_string(1234567890123ll);
    text += std::string(40, '!');
    if (text.size() != 100 || std::string_view(text).substr(35, 25) != "string, -42 1234567890123" ||
        text.back() != '!')
        failures |= 1u << 3;
    if (gGreeting != "hello, world from a heap allocated string")
        failures |= 1u << 4;

    // map and unordered_map with string keys
    std::map<std::string, int> ordered;
    std::unordered_map<std::string, int> hashed;
    for (int i = 0; i < 50; i++) {
        std::string key = "key" + std::to_string(i);
        ordered[key] = i;
        hashed[key] = i * 2;
    }
    if (ordered.size() != 50 || ordered.begin()->first != "key0" || ordered["key42"] != 42)
        failures |= 1u << 5;
    if (hashed.size() != 50 || hashed.at("key21") != 42 || hashed.count("key50") != 0)
        failures |= 1u << 6;

    // shared_ptr, weak_ptr and unique_ptr
    std::weak_ptr<std::vector<int>> weak;
    {
        auto shared = std::make_shared<std::vector<int>>(10, 3);
        weak = shared;
        auto copy = shared;
        if (shared.use_count() != 2 || weak.lock()->size() != 10)
            failures |= 1u << 7;
    }
    if (!weak.expired())
        failures |= 1u << 8;
    auto owned = std::make_unique<int[]>(16);
    owned[15] = 5;
    if (owned[15] != 5 || owned[0] != 0)
        failures |= 1u << 9;

    // operator new, malloc and the static constructor all went through the replaced allocator
    auto inArena = [](const void* ptr) {
        return ptr >= gArena && ptr < gArena + sizeof(gArena);
    };
    void* block = malloc(24);
    if (!inArena(values.data()) || !inArena(block) || !inArena(gGreeting.data()))
        failures |= 1u << 10;
    free(block);
    return failures;
}

extern "C" __attribute__((used)) unsigned patchContainersSelfTest() {
    _IMPORT_COREINIT_OSReport("[PatchCompiler Containers Test] START\n");
    unsigned freeBefore = arenaFree();
    unsigned failures = runChecks();
    // everything the checks allocated was freed again
    unsigned freeAfter = arenaFree();
    if (freeAfter != freeBefore)
        failures |= 1u << 11;
    if (failures == 0)
        _IMPORT_COREINIT_OSReport("[PatchCompiler Containers Test] PASS\n");
    else
        _IMPORT_COREINIT_OSReport("[PatchCompiler Containers Test] FAIL 0x%08x (arena free %u before, %u after)\n",
                                  failures, freeBefore, freeAfter);
    return failures;
}

PATCH_ON_ENTRY(patchContainersSelfTest);
