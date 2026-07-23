#pragma once

// Shared definitions for instancing PatchCompiler C++ sources.
// This header is processed by the PatchCompiler to generate
// PPC assembly hooks for the Stereo Instancing graphic pack.

// Base integer typedefs
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef signed int sint32;
typedef signed short sint16;
typedef signed char sint8;
typedef unsigned int size_t;

// Keeps unreferenced symbols alive through compilation.
#define ATTR_KEEP __attribute__((used))

// ----------------------------------------------------------------------------
// Logging
//
// coreinit.OSReport writes to the Cemu log. The PatchCompiler maps the
// _IMPORT_COREINIT_ prefix to `import.coreinit.<name>` in the generated asm.
// IMPORTANT: always end the format string with \n, otherwise the line is not
// flushed and nothing shows up in the log.
// ----------------------------------------------------------------------------
extern "C" {
extern void _IMPORT_COREINIT_OSReport(const char* format, ...);
}

#define OSReport _IMPORT_COREINIT_OSReport
#define DEBUG_LOG(...) OSReport(__VA_ARGS__)

// Instancing constants
#define STEREO_INSTANCE_COUNT  2
#define STEREO_EYE_LEFT        0
#define STEREO_EYE_RIGHT       1

// GX2 register addresses (to be filled in as reverse-engineered)
// PA_SU_SC_MODE_CNTL
#define GX2_PA_SU_SC_MODE_CNTL_OFFSET  0x0000  // TODO: fill actual offset

// Attribute divisor for fetch shader attribute buffer
// Used to set divisor=1 so instanceID increments per eye
// rather than per-vertex
