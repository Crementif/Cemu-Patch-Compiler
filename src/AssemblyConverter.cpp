#include "AssemblyConverter.h"
#include "StringParser.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "clang/Frontend/Utils.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/Support/MemoryBuffer.h"
#include "AsmUpdater.h"

std::unique_ptr<TextFile> compileCppWithEmbeddedClang(
    const std::string &sourceFilePath, std::string_view sourceFileContents) {
  static bool targetsInitialized = false;
  if (!targetsInitialized) {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    int fake_argc = 4;
    const char* fake_argv[] = {
        "CemuPatchCompiler",
        "-ppc-asm-full-reg-names",
        "-disable-ppc-cmp-opt",
        "-enable-ppc-branch-coalesce"
    };
    llvm::cl::ParseCommandLineOptions(fake_argc, fake_argv);

    targetsInitialized = true;
  }

  clang::CompilerInstance clang;
  clang::DiagnosticOptions *diagOpts = new clang::DiagnosticOptions();
  clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diags =
      clang::CompilerInstance::createDiagnostics(diagOpts, new clang::TextDiagnosticPrinter(llvm::errs(), diagOpts));
  clang.setDiagnostics(diags.get());

  std::filesystem::path sourcePath(sourceFilePath);
  std::string ext = sourcePath.extension().string();
  bool forceCpp = (ext == ".c" || ext == ".h");

  std::vector<const char*> args = {
      "clang",
      "-target", "powerpc-eabi",
      "-mcpu=750",
      "-mllvm", "-mattr=+fsqrt",
      "-m32",
      "-mbig-endian",
      "-mno-crbits",
      "-ffreestanding",
      "-fno-math-errno",
      "-fno-autolink",
      "-fno-data-sections",
      "-fno-function-sections",
      "-fno-exceptions",
      "-fno-verbose-asm",
      "-fno-rtti",
      "-fbasic-block-sections=none",
      "-O1",
      "-fvisibility=default",
      "-femit-all-decls",
      "-fno-inline",
      "-std=c++17",
      "-mllvm", "-ppc-asm-full-reg-names",
      "-mllvm", "-disable-ppc-cmp-opt",
      "-mllvm", "-enable-ppc-branch-coalesce"
  };

  if (forceCpp) {
    args.push_back("-x");
    args.push_back("c++");
  }

  args.push_back("-S");
  args.push_back(sourceFilePath.c_str());
  args.push_back("-o");
  args.push_back("-");

  clang::CreateInvocationOptions opts;
  opts.Diags = diags;
  std::shared_ptr<clang::CompilerInvocation> invocation =
      clang::createInvocation(args, opts);
  if (!invocation) {
    return nullptr;
  }
  // Inject PATCH_* macros as an implicit virtual header included in all compiles.
  std::string macroDefinitions = 
      "#pragma once\n"
      "#define _CEMU_PATCH_STRINGIFY(x) #x\n"
      "#define _CEMU_PATCH_TOSTRING(x) _CEMU_PATCH_STRINGIFY(x)\n"
      "#define PATCH_NOP(addr) asm(\"\\n# __CEMU_PATCH: \" _CEMU_PATCH_TOSTRING(addr) \" = nop\")\n"
      "#define PATCH_JUMP_BLA(addr, func) asm(\"\\n# __CEMU_PATCH: \" _CEMU_PATCH_TOSTRING(addr) \" = bla \" #func)\n"
      "#define PATCH_WRITE(addr, instr) asm(\"\\n# __CEMU_PATCH: \" _CEMU_PATCH_TOSTRING(addr) \" = \" instr)\n"
      "#define PATCH_INT(addr, value) asm(\"\\n# __CEMU_PATCH: \" _CEMU_PATCH_TOSTRING(addr) \" = .int \" _CEMU_PATCH_TOSTRING(value))\n"
      "#define PATCH_FLOAT(addr, value) asm(\"\\n# __CEMU_PATCH: \" _CEMU_PATCH_TOSTRING(addr) \" = .float \" _CEMU_PATCH_TOSTRING(value))\n"
      "#ifdef __cplusplus\n"
      "extern \"C\" {\n"
      "#endif\n"
      "__attribute__((always_inline)) inline float sqrtf(float n) {\n"
      "    float result;\n"
      "    asm(\"fsqrts %0, %1\" : \"=f\"(result) : \"f\"(n));\n"
      "    return result;\n"
      "}\n"
      "__attribute__((always_inline)) inline double sqrt(double n) {\n"
      "    double result;\n"
      "    asm(\"fsqrt %0, %1\" : \"=f\"(result) : \"f\"(n));\n"
      "    return result;\n"
      "}\n"
      "#ifdef __cplusplus\n"
      "}\n"
      "#endif\n";

  invocation->getPreprocessorOpts().Includes.push_back("C:/__cemu_patch_macros.h");
  auto buf = llvm::MemoryBuffer::getMemBufferCopy(macroDefinitions, "C:/__cemu_patch_macros.h");
  invocation->getPreprocessorOpts().addRemappedFile("C:/__cemu_patch_macros.h", buf.release());
  auto sourceBuffer =
      llvm::MemoryBuffer::getMemBufferCopy(sourceFileContents, sourceFilePath);
  invocation->getPreprocessorOpts().addRemappedFile(sourceFilePath,
                                                     sourceBuffer.release());
  invocation->getPreprocessorOpts().RetainRemappedFileBuffers = true;

  invocation->getFrontendOpts().OutputFile = "-";
  clang.setInvocation(invocation);

  llvm::SmallVector<char, 0> byteVector;
  clang.setOutputStream(std::make_unique<llvm::raw_svector_ostream>(byteVector));

  std::unique_ptr<clang::FrontendAction> action = std::make_unique<clang::EmitAssemblyAction>();
  if (!clang.ExecuteAction(*action)) {
    return nullptr;
  }

  (void)clang.takeOutputStream();

  return parseAssemblyFromMemory(std::string_view(byteVector.data(), byteVector.size()));
}

bool AssemblyConverter::prepareCppForLinking(std::string_view sourceFilePath,
                                             std::string_view moduleName,
                                             std::string_view sourceContents) {
  std::filesystem::path sourcePath(sourceFilePath);
  printf("Compiling %s...\n", sourcePath.filename().string().c_str());
  fflush(stdout);

  m_inputTextFile =
      compileCppWithEmbeddedClang(std::string(sourceFilePath), sourceContents);
  if (!m_inputTextFile) {
    printf("Error: Compilation failed using embedded Clang.\n");
    return false;
  }

  m_moduleName = moduleName;
  m_unitHash = calculateUnitHash(moduleName);
  analyzeSymbols(*m_inputTextFile);
  return true;
}

std::unique_ptr<TextFile>
AssemblyConverter::emitPatch(std::string_view moduleGroupName, uint32_t moduleMatches,
                             const std::set<std::string> &discardedDefinitions) {
  cemu_assert_debug(m_inputTextFile != nullptr);

  auto outputTextFile = std::make_unique<TextFile>();
  outputTextFile->lines.emplace_back(std::format("[{}]", moduleGroupName));
  outputTextFile->lines.emplace_back(
      std::format("moduleMatches = 0x{:08X}", moduleMatches));
  outputTextFile->lines.emplace_back(".origin = codecave");
  outputTextFile->lines.emplace_back("");

  m_patchDirectives.clear();
  convertClangToCemu(*m_inputTextFile, *outputTextFile, discardedDefinitions);
  finalize(*outputTextFile);
  return outputTextFile;
}

std::string AssemblyConverter::getEmittedSymbolName(std::string_view symbol) const {
  auto renameIt = m_privateSymbolRenames.find(std::string(symbol));
  if (renameIt != m_privateSymbolRenames.end()) {
    return renameIt->second;
  }
  return std::string(symbol);
}

void AssemblyConverter::convertClangToCemu(
    const TextFile &inputFile, TextFile &outputTextFile,
    const std::set<std::string> &discardedDefinitions) {
  printf("Starting convertClangToCemu for module %s...\n",
         m_moduleName.c_str());
  fflush(stdout);

  bool discardingDefinition = false;
  for (auto inputLine : inputFile.lines) {
    // Skip Clang-generated inline assembly markers to keep output clean
    {
      std::string_view trimmed = inputLine;
      while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
        trimmed.remove_prefix(1);
      while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
        trimmed.remove_suffix(1);
      if (trimmed == "#APP" || trimmed == "#NO_APP") {
        continue;
      }
    }

    inputLine = AsmUpdater_fixClangLocalLabels(m_unitHash, inputLine);
    inputLine = rewritePrivateSymbols(m_privateSymbolRenames, inputLine);
    std::string_view normalizedInputLine = normalizeAssemblyLine(inputLine);

    if (discardingDefinition) {
      auto nextDefinition = parseTopLevelSymbolDefinition(normalizedInputLine);
      if (!nextDefinition ||
          !m_topLevelDefinedSymbols.contains(*nextDefinition)) {
        continue;
      }
      discardingDefinition = false;
    }

    auto definitionSymbol = parseTopLevelSymbolDefinition(normalizedInputLine);
    if (definitionSymbol && m_topLevelDefinedSymbols.contains(*definitionSymbol) &&
        discardedDefinitions.contains(*definitionSymbol)) {
      std::string commonSymbol;
      uint32_t sizeVal = 0;
      uint32_t alignVal = 1;
      if (!parseCommonSymbolDirective(normalizedInputLine, commonSymbol,
                                      sizeVal, alignVal)) {
        discardingDefinition = true;
      }
      continue;
    }

    // check for patch directive markers emitted by PATCH_* macros
    {
      size_t markerPos = inputLine.find("# __CEMU_PATCH:");
      if (markerPos != std::string::npos) {
        std::string directive = inputLine.substr(markerPos + 15);
        // trim leading whitespace
        while (!directive.empty() &&
               (directive.front() == ' ' || directive.front() == '\t'))
          directive.erase(directive.begin());
        if (!directive.empty())
          m_patchDirectives.push_back(directive);
        continue;
      }
    }

    StringTokenParser parser(inputLine.data(), (int32_t)inputLine.size());
    // check for directives that we can skip
    parser.skipWhitespaces();
    if (parser.matchWordI(".text") || parser.matchWordI(".file") ||
        parser.matchWordI(".globl") || parser.matchWordI(".type") ||
        parser.matchWordI(".size") || parser.matchWordI(".section") ||
        parser.matchWordI(".weak") || parser.matchWordI(".data") ||
        parser.matchWordI(".ident") || parser.matchWordI(".addrsig") ||
        parser.matchWordI(".addrsig_sym") || parser.matchWordI(".machine") ||
        parser.matchWordI(".set") || parser.matchWordI(".local")) {
      continue;
    }
    parser = StringTokenParser(inputLine.data(), (int32_t)inputLine.size());

    // some commands have to be translated
    if (parser.matchWordI(".p2align")) {
      uint32_t val;
      if (!parser.parseU32(val))
        __debugbreak();
      outputTextFile.lines.emplace_back(std::format("\t.align {}", 1 << val));
      continue;
    }
    std::string commonSymbol;
    uint32_t commonSizeVal = 0;
    uint32_t commonAlignVal = 1;
    if (parseCommonSymbolDirective(normalizedInputLine, commonSymbol,
                                   commonSizeVal, commonAlignVal)) {
      emitZeroInitializedData(outputTextFile, commonSymbol, commonSizeVal,
                              commonAlignVal);
      continue;
    }
    if (parser.matchWordI(".ascii")) {
      parser.skipWhitespaces();
      std::string_view sv = parser.getView();
      if (!sv.empty() && sv.front() == '"') {
        sv.remove_prefix(1);
        size_t endQuote = sv.rfind('"');
        if (endQuote != std::string_view::npos)
          sv = sv.substr(0, endQuote);
        if (!sv.empty()) {
          std::string line = ".byte ";
          for (size_t i = 0; i < sv.size(); i++) {
            if (i > 0)
              line.append(",");
            line.append(std::to_string((unsigned char)sv[i]));
          }
          outputTextFile.lines.emplace_back(std::move(line));
        }
      }
      continue;
    }
    if (parser.matchWordI(".string")) {
      // append an extra .align 4 after the string
      outputTextFile.lines.emplace_back(inputLine);
      outputTextFile.lines.emplace_back("\t.align 4");
      continue;
    }
    if (parser.matchWordI(".zero") || parser.matchWordI(".space")) {
      uint32_t zeroSize;
      if (!parser.parseU32(zeroSize))
        __debugbreak();
      uint32_t fillVal = 0;
      parser.skipWhitespaces();
      if (parser.compareCharacter(0, ',')) {
        parser.skipCharacters(1);
        if (!parser.parseU32(fillVal)) {
          __debugbreak();
        }
      }
      if (zeroSize > 0) {
        uint32_t numInts = zeroSize / 4;
        uint32_t remainder = zeroSize % 4;
        if (numInts > 0) {
          std::string line = std::format("\t.int {}", fillVal);
          for (uint32_t i = 1; i < numInts; i++)
            line.append(std::format(",{}", fillVal));
          outputTextFile.lines.emplace_back(std::move(line));
        }
        if (remainder >= 2) {
          outputTextFile.lines.emplace_back(std::format("\t.short {}", fillVal));
          remainder -= 2;
        }
        if (remainder == 1) {
          outputTextFile.lines.emplace_back(std::format("\t.byte {}", fillVal));
        }
      }
      continue;
    }
    // and some instructions also need their syntax adjusted
    parser.skipWhitespaces();
    if (!parser.compareCharacter(0, '.')) {
      // dissect instructions into
      inputLine = AsmUpdater_fixInstruction(inputLine);
    } else {
      // .asciz -> .string
      util_stringReplace(inputLine, ".asciz", ".string");
      // .long -> .int
      util_stringReplace(inputLine, ".long", ".int");
    }

    // import directives
    util_stringReplace(inputLine, "_IMPORT_GX2_", "import.gx2.");
    util_stringReplace(inputLine, "_IMPORT_COREINIT_", "import.coreinit.");

    outputTextFile.lines.emplace_back(inputLine);
  }
}

void AssemblyConverter::analyzeSymbols(const TextFile &inputFile) {
  printf("Starting findLabels for %d lines...\n",
         (int)inputFile.lines.size());
  fflush(stdout);
  m_privateSymbolRenames.clear();
  m_exportedSymbols.clear();
  m_weakExportedSymbols.clear();
  m_topLevelDefinedSymbols.clear();
  m_definedExportedSymbols.clear();

  for (const auto &line : inputFile.lines) {
    std::string_view p = normalizeAssemblyLine(line);
    if (p.empty())
      continue;

    StringTokenParser parser(p);
    bool isWeak = parser.matchWordI(".weak");
    if (!isWeak && !parser.matchWordI(".globl")) {
      continue;
    }

    const char *symbolStr;
    int32_t symbolLength;
    if (parser.parseSymbolName(symbolStr, symbolLength)) {
      std::string symbol(symbolStr, symbolLength);
      m_exportedSymbols.emplace(symbol);
      if (isWeak) {
        m_weakExportedSymbols.emplace(std::move(symbol));
      }
    }
  }

  auto registerPrivateSymbol = [&](std::string_view symbol) {
    if (!shouldUniquifyPrivateSymbol(symbol, m_exportedSymbols)) {
      return;
    }

    auto [it, inserted] = m_privateSymbolRenames.emplace(std::string(symbol),
                                                         std::string());
    if (inserted) {
      it->second = std::format("_{:016x}{}", m_unitHash, symbol);
    }
  };

  auto registerTopLevelDefinition = [&](std::string_view symbol) {
    if (symbol.empty() || symbol.front() == '.') {
      return;
    }

    std::string symbolName(symbol);
    if (m_exportedSymbols.contains(symbolName)) {
      m_definedExportedSymbols.try_emplace(
          symbolName,
          m_weakExportedSymbols.contains(symbolName)
              ? ExportedSymbolLinkage::Weak
              : ExportedSymbolLinkage::Strong);
    } else {
      registerPrivateSymbol(symbolName);
    }

    m_topLevelDefinedSymbols.emplace(getEmittedSymbolName(symbolName));
  };

  for (const auto &line : inputFile.lines) {
    std::string_view p = normalizeAssemblyLine(line);

    if (p.empty())
      continue;

    std::string commonSymbol;
    uint32_t sizeVal = 0;
    uint32_t alignVal = 1;
    if (parseCommonSymbolDirective(p, commonSymbol, sizeVal, alignVal)) {
      registerTopLevelDefinition(commonSymbol);
      continue;
    }

    // check if line ends with ':' indicating a label
    if (p.back() == ':') {
      std::string_view label = p.substr(0, p.size() - 1);
      registerTopLevelDefinition(label);
    }
  }
}

void AssemblyConverter::finalize(TextFile &outputTextFile) {
  if (!m_patchDirectives.empty()) {
    outputTextFile.lines.emplace_back("");
    outputTextFile.lines.emplace_back("; --- Address Patches ---");
    for (const auto &directive : m_patchDirectives) {
      outputTextFile.lines.emplace_back(directive);
    }
    printf("Emitted %d address patch directive(s).\n",
           (int)m_patchDirectives.size());
    fflush(stdout);
  }
}
