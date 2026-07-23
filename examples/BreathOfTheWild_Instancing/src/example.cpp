// moduleMatches = 0x6267BFD0

// ============================================================================
// Stereo Instancing - basic C++ patch example
//
// This file demonstrates the C++ patch capabilities of the Cemu PatchCompiler.
// It is intentionally "safe": nothing here modifies game memory on its own.
// Everything compiles into a codecave; the address-patch macros are shown but
// left commented out so the pack can be built and launched without risk.
//
// Once real instancing hook targets are wired up in patch_Instancing.asm, the
// ATTR_KEEP hook stub below can be called from handwritten PPC assembly.
// ============================================================================

#include "shared.h"

// ----------------------------------------------------------------------------
// Pointer / read helpers
//
// These compile to tiny PPC routines and show how C++ reads structured game
// memory. Mirrors the helpers used by the Better VR example.
// ----------------------------------------------------------------------------

uint8* ptrAdd(void* p, uint32 offset) {
    return ((uint8*)p) + offset;
}

uint32 readU32(void* base, uint32 offset) {
    return *(uint32*)ptrAdd(base, offset);
}

uint16 readU16(void* base, uint32 offset) {
    return *(uint16*)ptrAdd(base, offset);
}

uint8 readU8(void* base, uint32 offset) {
    return *(uint8*)ptrAdd(base, offset);
}

float readF32(void* base, uint32 offset) {
    return *(float*)ptrAdd(base, offset);
}

// ----------------------------------------------------------------------------
// Self-contained codecave data + function
//
// ATTR_KEEP ensures these symbols survive even though nothing references them
// yet. This guarantees the compiler always produces a valid codecave, so the
// pack builds even before any hooks are wired up.
// ----------------------------------------------------------------------------

extern "C" {

ATTR_KEEP const char gInstancingExampleName[] = "stereo-instancing";
ATTR_KEEP uint32 gInstancingInstanceCount = STEREO_INSTANCE_COUNT;

// Simple pure computation to demonstrate C++ -> PPC codegen.
ATTR_KEEP uint32 instancingComputeSeed(uint32 seed) {
    uint32 result = 0;
    for (uint32 i = 0; i < STEREO_INSTANCE_COUNT; ++i) {
        result += (seed + i) * (i + 1);
    }
    DEBUG_LOG("[Instancing] instancingComputeSeed seed=%d result=%d\n", seed, result);
    return result;
}

// ----------------------------------------------------------------------------
// Hook stub for the instanced draw seam.
//
// Intended target (see re-notes/instancing_function_outline.md):
//   0x399ED84 sub_399ED84   -> single-instance choke point
//   0x3C0B094 sub_3C0B094   -> GX2DrawIndexedEx(..., numInstances)
//
// Wire this up from patch_Instancing.asm later, e.g.:
//   mr r3, <ModelRenderContext*>
//   li r4, STEREO_INSTANCE_COUNT
//   bl hook_modelDraw
//
// For now it is inert: it reads nothing and returns the requested instance
// count unchanged, so it cannot destabilize the game.
// ----------------------------------------------------------------------------
ATTR_KEEP uint32 hook_modelDraw(void* modelRenderContext, uint32 instanceCount) {
    (void)modelRenderContext;
    DEBUG_LOG("[Instancing] hook_modelDraw ctx=%08X instanceCount=%d\n", (uint32)modelRenderContext, instanceCount);
    if (instanceCount == 0) {
        return STEREO_INSTANCE_COUNT;
    }
    return instanceCount;
}

} // extern "C"

// ----------------------------------------------------------------------------
// Address-patch API examples (INTENTIONALLY DISABLED)
//
// The PatchCompiler exposes these builtins to overwrite specific game
// addresses. They are left commented out so this example never modifies game
// code. Uncomment and supply verified addresses when ready.
//
//   PATCH_NOP(0x023F90E0);                  // replace an instruction with nop
//   PATCH_INT(0x10416BF0, 0x00000000);      // overwrite a 32-bit value
//   PATCH_FLOAT(0x101E55F8, 3.0);           // overwrite a float constant
//   PATCH_WRITE(0x02E1905C, "li r3, 0");    // assemble one instruction
// ----------------------------------------------------------------------------
