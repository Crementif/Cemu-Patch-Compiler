// Shared by the two patches of this graphic pack, which are compiled separately into patch_first.asm and
// patch_second.asm. Both define the same runtime and allocator functions, which only works because these get the
// label prefix of their patch. Cemu stops applying the graphic pack if a label is defined twice.

#include <algorithm>
#include <string>
#include <vector>

extern "C" void _IMPORT_COREINIT_OSReport(const char* format, ...);
extern "C" void* _IMPORT_COREINIT_MEMCreateExpHeapEx(void* memory, unsigned size, unsigned short flags);
extern "C" void* _IMPORT_COREINIT_MEMAllocFromExpHeapEx(void* heap, unsigned size, int alignment);
extern "C" void _IMPORT_COREINIT_MEMFreeToExpHeap(void* heap, void* ptr);

// The homebrew test title leaves the default heap empty, so each patch allocates from its own arena
alignas(32) static unsigned char gArena[16 * 1024];
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

// Builds strings of run-time length, so the copies call memcpy and memmove
static void runSeparatePatch(const char* name) {
    std::vector<std::string> words;
    for (int i = 0; i < 20; i++)
        words.push_back(std::string(name) + " word " + std::to_string(19 - i));
    std::sort(words.begin(), words.end());
    std::string joined;
    for (const auto& word : words)
        joined += word;
    bool inArena = (const unsigned char*)joined.data() >= gArena &&
                   (const unsigned char*)joined.data() < gArena + sizeof(gArena);
    bool sorted = words.front() == std::string(name) + " word 0" && words.back() == std::string(name) + " word 9";
    _IMPORT_COREINIT_OSReport("[PatchCompiler Separate Patches] %s %s\n", name,
                              inArena && sorted && joined.find(" word 19") != std::string::npos ? "PASS" : "FAIL");
}
