#include "ModuleLinker.h"
#include "EmbeddedFiles.h"
#include "EmbeddedHeader.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/Internalize.h"

static void initializeLlvmOnce() {
  static bool initialized = false;
  if (initialized)
    return;
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Code generation runs in this process instead of through clang, so the backend options are set globally
  const char *fakeArgv[] = {"CemuPatchCompiler", "-ppc-asm-full-reg-names",
                            "-disable-ppc-cmp-opt", "-enable-ppc-branch-coalesce"};
  llvm::cl::ParseCommandLineOptions(4, fakeArgv);
  initialized = true;
}

// The embedded files (clang's freestanding C headers, libc++, the C library headers and the runtime sources) are
// served from memory under this directory
static constexpr std::string_view kResourceDir = "C:/__cemu_patch_resource";
static const std::string kLibcxxIncludeDir = std::format("{}/include/c++/v1", kResourceDir);
static const std::string kLibcIncludeDir = std::format("{}/libc", kResourceDir);
static const std::string kClangIncludeDir = std::format("{}/include", kResourceDir);

struct EmbeddedFile {
  std::string path;
  std::string contents;
};

// The generated table splits files into chunks, joined here once
static const std::vector<EmbeddedFile> &getEmbeddedFiles() {
  static const std::vector<EmbeddedFile> files = [] {
    std::vector<EmbeddedFile> joined;
    for (const auto &[path, chunk] : kEmbeddedFiles) {
      if (joined.empty() || joined.back().path != path)
        joined.push_back({std::string(path), {}});
      joined.back().contents.append(chunk);
    }
    return joined;
  }();
  return files;
}

static llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> createFileSystem() {
  auto memoryFileSystem = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
  for (const auto &file : getEmbeddedFiles()) {
    std::string path = std::format("{}/{}", kResourceDir, file.path);
    memoryFileSystem->addFile(path, 0, llvm::MemoryBuffer::getMemBuffer(file.contents, path));
  }
  auto fileSystem = llvm::makeIntrusiveRefCnt<llvm::vfs::OverlayFileSystem>(llvm::vfs::getRealFileSystem());
  fileSystem->pushOverlay(memoryFileSystem);
  return fileSystem;
}

// A source of the patch runtime: the C library functions, operator new and delete and the parts of libc++ that
// live in its compiled library. It is compiled and linked only when the patch references one of the symbols its
// "// provides:" lines list. A trailing * matches any symbol with that prefix.
struct RuntimeUnit {
  std::string name;
  std::string_view source;
  std::vector<std::string> provides;

  bool providesSymbol(llvm::StringRef symbol) const {
    for (const auto &pattern : provides) {
      if (pattern.ends_with('*') ? symbol.starts_with(std::string_view(pattern).substr(0, pattern.size() - 1))
                                 : symbol == pattern)
        return true;
    }
    return false;
  }
};

static const std::vector<RuntimeUnit> &getRuntimeUnits() {
  static const std::vector<RuntimeUnit> units = [] {
    std::vector<RuntimeUnit> result;
    for (const auto &file : getEmbeddedFiles()) {
      if (!file.path.starts_with("runtime/"))
        continue;
      RuntimeUnit unit{file.path.substr(8), file.contents, {}};
      std::istringstream lines(file.contents);
      for (std::string line; std::getline(lines, line);) {
        if (!line.starts_with("// provides:"))
          continue;
        std::istringstream symbols(line.substr(12));
        for (std::string symbol; symbols >> symbol;)
          unit.provides.push_back(symbol);
      }
      result.push_back(std::move(unit));
    }
    return result;
  }();
  return units;
}

static std::unique_ptr<llvm::Module> compileToModule(llvm::LLVMContext &context,
                                                     const SourceUnit &source,
                                                     const CompileOptions &options,
                                                     bool isRuntime = false) {
  if (!isRuntime) {
    printf("Compiling %s...\n", source.path.filename().string().c_str());
    fflush(stdout);
  }

  clang::CompilerInstance clang;
  clang::DiagnosticOptions *diagOpts = new clang::DiagnosticOptions();
  clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diags =
      clang::CompilerInstance::createDiagnostics(
          diagOpts, new clang::TextDiagnosticPrinter(llvm::errs(), diagOpts));
  clang.setDiagnostics(diags.get());

  std::string sourcePath = source.path.string();
  std::string ext = source.path.extension().string();
  std::string optFlag = "-O" + options.optLevel;

  std::vector<const char *> args = {
      "clang",
      "-target", "powerpc-eabi",
      "-mcpu=750",
      "-m32",
      "-mbig-endian",
      "-mno-crbits",
      "-ffreestanding",
      "-fno-math-errno",
      "-fno-autolink",
      "-fno-exceptions",
      "-fno-rtti",
      // Static constructors run once from an entry callback and destructors never run
      "-fno-threadsafe-statics",
      "-fno-c++-static-destructors",
      "-fvisibility=default",
      "-std=c++20",
      // libc++ wraps the C library headers, which wrap clang's freestanding headers
      "-nostdinc",
      "-isystem", kLibcxxIncludeDir.c_str(),
      "-isystem", kLibcIncludeDir.c_str(),
      "-isystem", kClangIncludeDir.c_str(),
      optFlag.c_str(),
      // Optimization happens once on the linked module
      "-Xclang", "-disable-llvm-passes",
  };
  if (!isRuntime) {
    // Functions only referenced from patch directives or asm still have to reach the linker
    args.push_back("-femit-all-decls");
  }
  if (ext == ".c" || ext == ".h") {
    args.push_back("-x");
    args.push_back("c++");
  }
  if (!isRuntime) {
    for (const auto &arg : options.clangArgs)
      args.push_back(arg.c_str());
  }
  args.push_back("-c");
  args.push_back(sourcePath.c_str());

  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fileSystem = createFileSystem();
  clang::CreateInvocationOptions invocationOptions;
  invocationOptions.Diags = diags;
  invocationOptions.VFS = fileSystem;
  std::shared_ptr<clang::CompilerInvocation> invocation =
      clang::createInvocation(args, invocationOptions);
  if (!invocation)
    return nullptr;

  // Inject the PATCH_* macros and math helpers as an implicit header
  invocation->getPreprocessorOpts().Includes.push_back("C:/__cemu_patch_macros.h");
  invocation->getPreprocessorOpts().addRemappedFile(
      "C:/__cemu_patch_macros.h",
      llvm::MemoryBuffer::getMemBufferCopy(kCemuPatchImplicitHeader, "C:/__cemu_patch_macros.h")
          .release());
  invocation->getPreprocessorOpts().addRemappedFile(
      sourcePath, llvm::MemoryBuffer::getMemBufferCopy(source.contents, sourcePath).release());
  invocation->getPreprocessorOpts().RetainRemappedFileBuffers = true;
  clang.setInvocation(invocation);
  clang.createFileManager(fileSystem);

  clang::EmitLLVMOnlyAction action(&context);
  if (!clang.ExecuteAction(action))
    return nullptr;
  return action.takeModule();
}

static bool isSymbolStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '$';
}

static bool isSymbolChar(char c) {
  return isSymbolStart(c) || std::isdigit(static_cast<unsigned char>(c));
}

static void collectAsmIdentifiers(llvm::StringRef text, std::set<std::string> &out) {
  size_t i = 0;
  while (i < text.size()) {
    if (!isSymbolStart(text[i])) {
      i++;
      continue;
    }
    size_t start = i;
    while (i < text.size() && isSymbolChar(text[i]))
      i++;
    out.emplace(text.substr(start, i - start).str());
  }
}

// Symbols named in module level asm (patch directives, hand-written stubs) and in inline asm of
// functions are invisible to LLVM, so they are collected and kept external.
static std::set<std::string> collectAsmReferencedSymbols(llvm::Module &module) {
  std::set<std::string> identifiers;
  collectAsmIdentifiers(module.getModuleInlineAsm(), identifiers);
  for (auto &function : module) {
    for (auto &block : function) {
      for (auto &instruction : block) {
        auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
        if (!call)
          continue;
        if (auto *inlineAsm = llvm::dyn_cast<llvm::InlineAsm>(call->getCalledOperand()))
          collectAsmIdentifiers(inlineAsm->getAsmString(), identifiers);
      }
    }
  }

  std::set<std::string> referenced;
  for (auto &global : module.global_values()) {
    if (global.hasName() && identifiers.contains(global.getName().str()))
      referenced.emplace(global.getName().str());
  }
  return referenced;
}

// Replaces llvm.global_ctors with one function that calls the constructors in priority order.
// It is registered as the first entry callback, so it runs before any PATCH_ON_ENTRY function.
static bool lowerGlobalConstructors(llvm::Module &module) {
  if (auto *dtors = module.getNamedGlobal("llvm.global_dtors"))
    dtors->eraseFromParent();

  auto *ctors = module.getNamedGlobal("llvm.global_ctors");
  if (!ctors)
    return false;

  std::vector<std::pair<uint64_t, llvm::Function *>> entries;
  if (auto *array = llvm::dyn_cast_or_null<llvm::ConstantArray>(
          ctors->hasInitializer() ? ctors->getInitializer() : nullptr)) {
    for (auto &operand : array->operands()) {
      auto *entry = llvm::dyn_cast<llvm::ConstantStruct>(operand);
      if (!entry)
        continue;
      auto *priority = llvm::dyn_cast<llvm::ConstantInt>(entry->getOperand(0));
      auto *function =
          llvm::dyn_cast<llvm::Function>(entry->getOperand(1)->stripPointerCasts());
      if (priority && function)
        entries.emplace_back(priority->getZExtValue(), function);
    }
  }
  ctors->eraseFromParent();
  if (entries.empty())
    return false;
  std::stable_sort(entries.begin(), entries.end(),
                   [](const auto &a, const auto &b) { return a.first < b.first; });

  auto &context = module.getContext();
  auto *runner = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), false),
      llvm::GlobalValue::ExternalLinkage, kGlobalCtorsSymbol, module);
  // Code generation reads the CPU and features from each function, so take them from a constructor
  runner->copyAttributesFrom(entries.front().second);
  llvm::IRBuilder<> builder(llvm::BasicBlock::Create(context, "entry", runner));
  for (auto &[priority, function] : entries)
    builder.CreateCall(function);
  builder.CreateRetVoid();

  module.setModuleInlineAsm(std::format("\n# __CEMU_PATCH: .callback entry {}\n{}",
                                        kGlobalCtorsSymbol,
                                        module.getModuleInlineAsm()));
  printf("Static constructors: %d, run from %s\n", (int)entries.size(),
         kGlobalCtorsSymbol.data());
  return true;
}

// Linking and optimization add suffixes like .1 or .cold to local names, which Cemu's assembler
// can't parse as symbols.
static void sanitizeLocalSymbolNames(llvm::Module &module) {
  for (auto &global : module.global_values()) {
    if (!global.hasLocalLinkage() || !global.hasName())
      continue;
    llvm::StringRef name = global.getName();
    if (name.find_first_of(".$") == llvm::StringRef::npos)
      continue;
    std::string sanitized = name.str();
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');
    std::replace(sanitized.begin(), sanitized.end(), '$', '_');
    global.setName(sanitized);
  }
}

static llvm::OptimizationLevel getOptimizationLevel(std::string_view level) {
  if (level == "0")
    return llvm::OptimizationLevel::O0;
  if (level == "1")
    return llvm::OptimizationLevel::O1;
  if (level == "3")
    return llvm::OptimizationLevel::O3;
  if (level == "s")
    return llvm::OptimizationLevel::Os;
  if (level == "z")
    return llvm::OptimizationLevel::Oz;
  return llvm::OptimizationLevel::O2;
}

static void optimizeModule(llvm::Module &module, llvm::TargetMachine &targetMachine,
                           llvm::OptimizationLevel level) {
  llvm::LoopAnalysisManager loopAnalysis;
  llvm::FunctionAnalysisManager functionAnalysis;
  llvm::CGSCCAnalysisManager cgsccAnalysis;
  llvm::ModuleAnalysisManager moduleAnalysis;
  llvm::PassBuilder passBuilder(&targetMachine);

  // Freestanding: optimizations may not assume any C library function exists
  llvm::TargetLibraryInfoImpl libraryInfo(llvm::Triple(module.getTargetTriple()));
  libraryInfo.disableAllFunctions();
  functionAnalysis.registerPass([&] { return llvm::TargetLibraryAnalysis(libraryInfo); });

  passBuilder.registerModuleAnalyses(moduleAnalysis);
  passBuilder.registerCGSCCAnalyses(cgsccAnalysis);
  passBuilder.registerFunctionAnalyses(functionAnalysis);
  passBuilder.registerLoopAnalyses(loopAnalysis);
  passBuilder.crossRegisterProxies(loopAnalysis, functionAnalysis, cgsccAnalysis, moduleAnalysis);

  llvm::ModulePassManager passes = level == llvm::OptimizationLevel::O0
                                       ? passBuilder.buildO0DefaultPipeline(level)
                                       : passBuilder.buildPerModuleDefaultPipeline(level);
  passes.addPass(llvm::GlobalDCEPass());
  passes.run(module, moduleAnalysis);
}

// Code generation calls these for memory intrinsics it doesn't expand inline
static constexpr std::array<std::string_view, 3> kCodegenLibcalls = {"memmove", "memset", "memcpy"};

static std::string_view getIntrinsicLibcall(const llvm::Function &function) {
  switch (function.getIntrinsicID()) {
  case llvm::Intrinsic::memcpy:
    return "memcpy";
  case llvm::Intrinsic::memmove:
    return "memmove";
  case llvm::Intrinsic::memset:
    return "memset";
  default:
    return {};
  }
}

// Symbols the module uses but doesn't define, including the library functions of memory intrinsics
static std::set<std::string> collectUndefinedSymbols(llvm::Module &module) {
  std::set<std::string> undefined;
  for (auto &global : module.global_values()) {
    if (!global.isDeclaration() || global.use_empty() || !global.hasName())
      continue;
    if (auto *function = llvm::dyn_cast<llvm::Function>(&global); function && function->isIntrinsic()) {
      std::string_view libcall = getIntrinsicLibcall(*function);
      if (auto *defined = libcall.empty() ? nullptr : module.getFunction(libcall); defined && !defined->isDeclaration())
        continue;
      if (!libcall.empty())
        undefined.emplace(libcall);
      continue;
    }
    undefined.emplace(global.getName().str());
  }
  return undefined;
}

static std::set<std::string> collectDefinedSymbols(llvm::Module &module) {
  std::set<std::string> defined;
  for (auto &global : module.global_values()) {
    if (!global.isDeclaration() && global.hasName())
      defined.emplace(global.getName().str());
  }
  return defined;
}

// Links the runtime units that define symbols the patch uses, until nothing more is needed. Returns the symbols the
// runtime added, or nullopt if a unit failed to compile or link.
static std::optional<std::set<std::string>> linkRuntimeUnits(llvm::LLVMContext &context, llvm::Module &linked,
                                                             llvm::Linker &linker, const CompileOptions &options) {
  const auto &units = getRuntimeUnits();
  std::set<std::string> definedByPatch = collectDefinedSymbols(linked);
  std::vector<bool> isLinked(units.size(), false);
  for (bool linkedUnit = true; linkedUnit;) {
    linkedUnit = false;
    std::set<std::string> undefined = collectUndefinedSymbols(linked);
    for (size_t i = 0; i < units.size(); i++) {
      const RuntimeUnit &unit = units[i];
      if (isLinked[i])
        continue;
      auto needed = std::find_if(undefined.begin(), undefined.end(),
                                 [&](const std::string &symbol) { return unit.providesSymbol(symbol); });
      if (needed == undefined.end())
        continue;
      printf("Compiling runtime %s for %s...\n", unit.name.c_str(), needed->c_str());
      fflush(stdout);
      SourceUnit source{std::format("{}/runtime/{}", kResourceDir, unit.name), std::string(unit.source)};
      auto module = compileToModule(context, source, options, true);
      if (!module) {
        printf("Error: Compilation failed for runtime '%s'.\n", unit.name.c_str());
        return std::nullopt;
      }
      std::set<std::string> unitDefines = collectDefinedSymbols(*module);
      for (const auto &pattern : unit.provides) {
        bool defined = pattern.ends_with('*')
                           ? std::any_of(unitDefines.begin(), unitDefines.end(),
                                         [&](const std::string &symbol) {
                                           return symbol.starts_with(std::string_view(pattern).substr(0, pattern.size() - 1));
                                         })
                           : unitDefines.contains(pattern);
        if (!defined)
          fatalError(std::format("Runtime '{}' lists '{}' but doesn't define it", unit.name, pattern));
      }
      if (linker.linkInModule(std::move(module))) {
        printf("Error: Linking runtime '%s' failed.\n", unit.name.c_str());
        return std::nullopt;
      }
      isLinked[i] = true;
      linkedUnit = true;
    }
  }

  std::set<std::string> definedByRuntime;
  for (const auto &symbol : collectDefinedSymbols(linked)) {
    if (!definedByPatch.contains(symbol))
      definedByRuntime.insert(symbol);
  }
  // Runtime definitions are weak so the patch can replace them. The ones that stay external are final now.
  for (std::string_view libcall : kCodegenLibcalls) {
    if (auto *function = linked.getFunction(libcall); function && definedByRuntime.contains(std::string(libcall)))
      function->setLinkage(llvm::GlobalValue::ExternalLinkage);
  }
  return definedByRuntime;
}

// The memory functions stay external until code generation in case it calls them, but if the optimized patch
// doesn't use them at all they are dropped
static void removeUnusedLibcalls(llvm::Module &module, const std::set<std::string> &definedByRuntime) {
  std::set<std::string_view> intrinsicLibcalls;
  for (auto &function : module) {
    if (!function.use_empty()) {
      if (std::string_view libcall = getIntrinsicLibcall(function); !libcall.empty())
        intrinsicLibcalls.insert(libcall);
    }
  }
  // memmove calls memcpy, so it goes first
  for (std::string_view libcall : kCodegenLibcalls) {
    auto *function = module.getFunction(libcall);
    if (function && definedByRuntime.contains(std::string(libcall)) && function->use_empty() &&
        !intrinsicLibcalls.contains(libcall))
      function->eraseFromParent();
  }
}

std::unique_ptr<TextFile> compileAndLinkSources(const std::vector<SourceUnit> &sources,
                                                const CompileOptions &options) {
  initializeLlvmOnce();
  auto stageStart = std::chrono::steady_clock::now();
  auto printStage = [&](const char *stage) {
    auto now = std::chrono::steady_clock::now();
    printf("%s in %.1f s.\n", stage, std::chrono::duration<double>(now - stageStart).count());
    fflush(stdout);
    stageStart = now;
  };

  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> linked;
  std::unique_ptr<llvm::Linker> linker;
  for (const auto &source : sources) {
    auto module = compileToModule(context, source, options);
    if (!module) {
      printf("Error: Compilation failed for '%s'.\n", source.path.generic_string().c_str());
      return nullptr;
    }
    if (!linked) {
      linked = std::move(module);
      linker = std::make_unique<llvm::Linker>(*linked);
      continue;
    }
    if (linker->linkInModule(std::move(module))) {
      printf("Error: Linking '%s' failed.\n", source.path.generic_string().c_str());
      return nullptr;
    }
  }
  if (!linked)
    return nullptr;
  printStage(std::format("Compiled and linked {} translation unit(s)", sources.size()).c_str());

  auto definedByRuntime = linkRuntimeUnits(context, *linked, *linker, options);
  if (!definedByRuntime)
    return nullptr;
  if (!definedByRuntime->empty())
    printStage("Linked the runtime");

  bool hasConstructors = lowerGlobalConstructors(*linked);
  std::set<std::string> asmReferenced = collectAsmReferencedSymbols(*linked);
  // C++ symbols are only reachable from this patch, so they can be inlined and removed. C symbols
  // stay external: hooks, symbols named in asm, and runtime functions code generation calls.
  // The runtime's C functions are only called from this patch, except the memory functions.
  llvm::internalizeModule(*linked, [&](const llvm::GlobalValue &global) {
    llvm::StringRef name = global.getName();
    if (hasConstructors && name == llvm::StringRef(kGlobalCtorsSymbol))
      return true;
    if (asmReferenced.contains(name.str()))
      return true;
    if (definedByRuntime->contains(name.str()))
      return std::find(kCodegenLibcalls.begin(), kCodegenLibcalls.end(), std::string_view(name)) !=
             kCodegenLibcalls.end();
    return !name.starts_with("_Z");
  });

  std::string error;
  std::string triple = linked->getTargetTriple();
  const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, error);
  if (!target) {
    printf("Error: %s\n", error.c_str());
    return nullptr;
  }
  llvm::TargetOptions targetOptions;
  targetOptions.MCOptions.AsmVerbose = false;
  llvm::OptimizationLevel level = getOptimizationLevel(options.optLevel);
  std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(
      triple, "750", "", targetOptions, llvm::Reloc::Static, std::nullopt,
      level == llvm::OptimizationLevel::O0 ? llvm::CodeGenOptLevel::None
                                           : llvm::CodeGenOptLevel::Default));

  optimizeModule(*linked, *targetMachine, level);
  removeUnusedLibcalls(*linked, *definedByRuntime);
  printStage("Optimized");
  sanitizeLocalSymbolNames(*linked);
  if (llvm::verifyModule(*linked, &llvm::errs())) {
    printf("Error: The linked module is invalid.\n");
    return nullptr;
  }

  llvm::SmallVector<char, 0> assembly;
  llvm::raw_svector_ostream stream(assembly);
  llvm::legacy::PassManager codegen;
  if (targetMachine->addPassesToEmitFile(codegen, stream, nullptr,
                                         llvm::CodeGenFileType::AssemblyFile)) {
    printf("Error: The PowerPC target can't emit assembly.\n");
    return nullptr;
  }
  codegen.run(*linked);
  printStage("Generated code");
  return parseAssemblyFromMemory(std::string_view(assembly.data(), assembly.size()));
}
