// moduleMatches = 0x6267BFD0

#include "../separate_patch.h"

extern "C" __attribute__((used)) void patchSeparateSecond() {
    runSeparatePatch("second");
}

PATCH_ON_ENTRY(patchSeparateSecond);
