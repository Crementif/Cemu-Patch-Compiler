
#ifndef _CEMU_PATCH_H
#define _CEMU_PATCH_H

#include <_cemu_patch_libc.h>

#define _CEMU_PATCH_STRINGIFY(x) #x
#define _CEMU_PATCH_TOSTRING(x) _CEMU_PATCH_STRINGIFY(x)
#define PATCH_NOP(addr) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = nop")
#define PATCH_JUMP_BLA(addr, func) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = bla " #func)
#define PATCH_WRITE(addr, instr) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = " instr)
#define PATCH_INT(addr, value) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = .int " _CEMU_PATCH_TOSTRING(value))
#define PATCH_FLOAT(addr, value) asm("\n# __CEMU_PATCH: " _CEMU_PATCH_TOSTRING(addr) " = .float " _CEMU_PATCH_TOSTRING(value))
#define _CEMU_PATCH_CONCAT_IMPL(a, b) a##b
#define _CEMU_PATCH_CONCAT(a, b) _CEMU_PATCH_CONCAT_IMPL(a, b)
#define _CEMU_PATCH_ENTRY_IMPL(func, callbackName) \
    extern "C" __attribute__((used)) void callbackName() { (void)(func()); } \
    asm("\n# __CEMU_PATCH: .callback entry " _CEMU_PATCH_TOSTRING(callbackName))
#define PATCH_ON_ENTRY(func) \
    _CEMU_PATCH_ENTRY_IMPL(func, _CEMU_PATCH_CONCAT(__cemu_patch_entry, __COUNTER__))
// Compatibility spelling: Cemu invokes this immediately before the title entrypoint.
#define PATCH_ON_LOAD(func) PATCH_ON_ENTRY(func)

_CEMU_PATCH_BEGIN_C

// Fast approximate square roots from the reciprocal square root estimate, without refinement
float cemu_patch_fast_sqrtf(float x);
double cemu_patch_fast_sqrt(double x);

float sqrtf(float x);
double sqrt(double x);
float sinf(float x);
float cosf(float x);
float tanf(float x);
float fabsf(float x);
float copysignf(float magnitude, float sign);
float fminf(float x, float y);
float fmaxf(float x, float y);
float truncf(float x);
float floorf(float x);
float ceilf(float x);
float roundf(float x);

_CEMU_PATCH_END_C

#endif
