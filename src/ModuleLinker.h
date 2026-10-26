#pragma once

#include "AssemblyParser.h"

struct CompileOptions {
  // Optimization level passed to clang and the link time pipeline: 0, 1, 2, 3, s or z. Os keeps the patch small, which Cemu assembles on every boot
  std::string optLevel = "s";
  // Extra arguments for clang, appended after the built-in ones so they can override them
  std::vector<std::string> clangArgs;
  // Source lines as comments in the patch (-s)
  bool sourceComments = false;
};

struct SourceUnit {
  std::filesystem::path path;
  std::string contents;
};

// Name of the generated function that runs the static constructors, registered as the first entry callback
inline constexpr std::string_view kGlobalCtorsSymbol = "__cemu_patch_global_ctors";

// Compiles every source to LLVM IR, links them into one module, internalizes everything that
// isn't reachable from outside (patch directives, asm, used attributes, C symbols), optimizes the
// whole program once and returns the PowerPC assembly for it.
std::unique_ptr<TextFile> compileAndLinkSources(const std::vector<SourceUnit> &sources,
                                                const CompileOptions &options);
