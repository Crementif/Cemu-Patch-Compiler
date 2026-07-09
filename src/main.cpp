#include "StringParser.h"
#include "common.h"

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

#include <set>

struct TextFile {
  std::vector<std::string> lines;
};

struct SourceFileInput {
  std::string compileContents;
  uint32_t moduleMatches = 0;
};

static constexpr uint32_t kDefaultModuleMatches = 0x6267BFD0;

static std::string_view trimAsciiWhitespace(std::string_view text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  return text;
}

static std::string_view stripUtf8Bom(std::string_view text) {
  if (text.size() >= 3 &&
      static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);
  }
  return text;
}

static std::optional<uint32_t> parseModuleMatchesDirective(
    std::string_view firstLine, std::string &errorMessage) {
  firstLine = stripUtf8Bom(firstLine);
  if (!firstLine.starts_with("//")) {
    return kDefaultModuleMatches;
  }

  std::string_view payload = trimAsciiWhitespace(firstLine.substr(2));
  static constexpr std::string_view kDirectiveName = "moduleMatches";
  if (!payload.starts_with(kDirectiveName)) {
    return kDefaultModuleMatches;
  }

  payload.remove_prefix(kDirectiveName.size());
  payload = trimAsciiWhitespace(payload);
  if (!payload.empty() && payload.front() == '=') {
    payload.remove_prefix(1);
  }
  payload = trimAsciiWhitespace(payload);

  if (payload.size() < 10 || payload[0] != '0' ||
      (payload[1] != 'x' && payload[1] != 'X')) {
    errorMessage =
        "Invalid moduleMatches directive. Expected first line like "
        "'// moduleMatches = 0x12345678'.";
    return std::nullopt;
  }

  payload.remove_prefix(2);
  size_t hexLength = 0;
  while (hexLength < payload.size() &&
         std::isxdigit(static_cast<unsigned char>(payload[hexLength]))) {
    hexLength++;
  }

  if (hexLength != 8) {
    errorMessage =
        "Module checksum directive must contain exactly 8 hexadecimal digits.";
    return std::nullopt;
  }

  std::string_view trailing = trimAsciiWhitespace(payload.substr(hexLength));
  if (!trailing.empty()) {
    errorMessage =
        "Unexpected trailing characters in module checksum directive.";
    return std::nullopt;
  }

  uint32_t moduleMatches = 0;
  auto [ptr, ec] = std::from_chars(payload.data(), payload.data() + hexLength,
                                   moduleMatches, 16);
  if (ec != std::errc() || ptr != payload.data() + hexLength) {
    errorMessage = "Failed to parse module checksum from directive.";
    return std::nullopt;
  }

  return moduleMatches;
}

static std::optional<SourceFileInput> loadSourceFileInput(
    const std::filesystem::path &sourcePath, std::string &errorMessage) {
  std::ifstream file(sourcePath, std::ios::binary);
  if (!file.is_open()) {
    errorMessage =
        std::format("Failed to open source file '{}'.",
                    sourcePath.generic_string());
    return std::nullopt;
  }

  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  std::string_view contentsView(contents);

  size_t firstLineEnd = contentsView.find('\n');
  std::string_view firstLine =
      firstLineEnd == std::string_view::npos
          ? contentsView
          : contentsView.substr(0, firstLineEnd);
  if (!firstLine.empty() && firstLine.back() == '\r') {
    firstLine.remove_suffix(1);
  }

  auto moduleMatches = parseModuleMatchesDirective(firstLine, errorMessage);
  if (!moduleMatches) {
    errorMessage = std::format("{} Source file: '{}'.", errorMessage,
                               sourcePath.generic_string());
    return std::nullopt;
  }

  return SourceFileInput{std::move(contents), *moduleMatches};
}

std::unique_ptr<TextFile> util_readTextFile(std::string_view path) {
  std::ifstream file(path.data(), std::ios::binary);
  if (!file.is_open()) {
    __debugbreak();
  }

  auto textFile = std::make_unique<TextFile>();
  std::string line;
  while (std::getline(file, line)) {
    // Remove trailing '\r' if present (CRLF format)
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    // Remove leading '\r' if present
    if (!line.empty() && line.front() == '\r') {
      line.erase(line.begin());
    }
    textFile->lines.push_back(std::move(line));
  }
  return textFile;
}

inline std::unique_ptr<TextFile> parseAssemblyFromMemory(std::string_view content) {
  auto textFile = std::make_unique<TextFile>();
  size_t pos = 0;
  while (pos < content.size()) {
    size_t nextNL = content.find('\n', pos);
    std::string_view lineSV;
    if (nextNL == std::string_view::npos) {
      lineSV = content.substr(pos);
      pos = content.size();
    } else {
      lineSV = content.substr(pos, nextNL - pos);
      pos = nextNL + 1;
    }
    std::string line(lineSV);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty() && line.front() == '\r') {
      line.erase(line.begin());
    }
    textFile->lines.push_back(std::move(line));
  }
  return textFile;
}

void util_writeFile(std::string_view path, const TextFile *textFile) {
  std::filesystem::path p(path);
  std::error_code ec;
  std::filesystem::create_directories(p.parent_path(), ec);

  std::ofstream file(path.data(), std::ios::binary);
  if (!file.is_open()) {
    __debugbreak();
  }

  for (const auto &line : textFile->lines) {
    file.write(line.data(), line.size());
    file.put('\n');
  }
}

bool util_stringReplace(std::string &str, const std::string &from,
                        const std::string &to) {
  size_t start_pos = str.find(from);
  if (start_pos == std::string::npos)
    return false;
  str.replace(start_pos, from.length(), to);
  return true;
}

std::string AsmUpdater_fixInstruction(std::string_view instructionText);
std::string AsmUpdater_fixClangLocalLabels(uint64_t unitHash,
                                           std::string instructionText);

static uint64_t calculateUnitHash(std::string_view moduleName) {
  uint64_t unitHash = 0;
  for (uint32_t t = 0; t < 8; t++) {
    for (auto &itr : moduleName) {
      unitHash += (uint64_t)itr;
      unitHash = _rotr64(unitHash, 3);
    }
  }
  return unitHash;
}

static bool isAssemblySymbolChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.';
}

static bool shouldUniquifyPrivateSymbol(
    std::string_view symbol, const std::set<std::string> &exportedSymbols) {
  if (symbol.empty() || symbol.front() == '.') {
    return false;
  }
  return !exportedSymbols.contains(std::string(symbol));
}

static std::string rewritePrivateSymbols(
    const std::map<std::string, std::string> &symbolRenames,
    std::string_view line) {
  if (symbolRenames.empty()) {
    return std::string(line);
  }

  std::string result;
  result.reserve(line.size());

  size_t index = 0;
  while (index < line.size()) {
    if (!isAssemblySymbolChar(line[index])) {
      result.push_back(line[index]);
      index++;
      continue;
    }

    size_t tokenEnd = index + 1;
    while (tokenEnd < line.size() && isAssemblySymbolChar(line[tokenEnd])) {
      tokenEnd++;
    }

    std::string_view token = line.substr(index, tokenEnd - index);
    auto renameIt = symbolRenames.find(std::string(token));
    if (renameIt != symbolRenames.end()) {
      result.append(renameIt->second);
    } else {
      result.append(token);
    }

    index = tokenEnd;
  }

  return result;
}

enum class ExportedSymbolLinkage {
  Strong,
  Weak,
};

static std::string_view normalizeAssemblyLine(std::string_view line) {
  while (!line.empty() &&
         (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
    line.remove_suffix(1);
  }

  size_t commentPos = line.find('#');
  if (commentPos != std::string_view::npos) {
    line = line.substr(0, commentPos);
  }

  while (!line.empty() &&
         (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
    line.remove_suffix(1);
  }

  return line;
}

static bool parseCommonSymbolDirective(std::string_view line, std::string &symbol,
                                       uint32_t &sizeVal, uint32_t &alignVal) {
  StringTokenParser parser(line);
  if (!parser.matchWordI(".lcomm") && !parser.matchWordI(".comm")) {
    return false;
  }

  const char *symbolStr;
  int32_t symbolLength;
  if (!parser.parseSymbolName(symbolStr, symbolLength)) {
    __debugbreak();
  }
  symbol.assign(symbolStr, symbolLength);

  parser.skipWhitespaces();
  if (parser.compareCharacter(0, ',')) {
    parser.skipCharacters(1);
  }

  sizeVal = 0;
  if (!parser.parseU32(sizeVal)) {
    __debugbreak();
  }

  alignVal = 1;
  parser.skipWhitespaces();
  if (parser.compareCharacter(0, ',')) {
    parser.skipCharacters(1);
    if (!parser.parseU32(alignVal)) {
      __debugbreak();
    }
  }

  return true;
}

static std::optional<std::string>
parseTopLevelSymbolDefinition(std::string_view normalizedLine) {
  std::string symbol;
  uint32_t sizeVal = 0;
  uint32_t alignVal = 1;
  if (parseCommonSymbolDirective(normalizedLine, symbol, sizeVal, alignVal)) {
    return symbol;
  }

  if (normalizedLine.empty() || normalizedLine.back() != ':') {
    return std::nullopt;
  }

  std::string_view label = normalizedLine.substr(0, normalizedLine.size() - 1);
  if (label.empty() || label.front() == '.') {
    return std::nullopt;
  }

  return std::string(label);
}

static void emitZeroInitializedData(TextFile &outputTextFile,
                                    std::string_view symbol, uint32_t sizeVal,
                                    uint32_t alignVal, uint32_t fillVal = 0) {
  if (alignVal > 1) {
    outputTextFile.lines.emplace_back(std::format("\t.align {}", alignVal));
  }
  outputTextFile.lines.emplace_back(std::format("{}:", symbol));

  if (sizeVal == 0) {
    return;
  }

  uint32_t numInts = sizeVal / 4;
  uint32_t remainder = sizeVal % 4;
  if (numInts > 0) {
    std::string line = std::format("\t.int {}", fillVal);
    for (uint32_t i = 1; i < numInts; i++) {
      line.append(std::format(",{}", fillVal));
    }
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

static std::unique_ptr<TextFile> compileCppWithEmbeddedClang(
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

class AssemblyConverter {
public:
  AssemblyConverter() = default;

  bool prepareCppForLinking(std::string_view sourceFilePath,
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
  emitPatch(std::string_view moduleGroupName, uint32_t moduleMatches,
            const std::set<std::string> &discardedDefinitions = {}) {
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

  const std::map<std::string, ExportedSymbolLinkage> &
  getDefinedExportedSymbols() const {
    return m_definedExportedSymbols;
  }

private:
  std::string getEmittedSymbolName(std::string_view symbol) const {
    auto renameIt = m_privateSymbolRenames.find(std::string(symbol));
    if (renameIt != m_privateSymbolRenames.end()) {
      return renameIt->second;
    }
    return std::string(symbol);
  }

  void convertClangToCemu(const TextFile &inputFile, TextFile &outputTextFile,
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

  void analyzeSymbols(const TextFile &inputFile) {
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

  void finalize(TextFile &outputTextFile) {
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

  std::unique_ptr<TextFile> m_inputTextFile;
  std::string m_moduleName;
  uint64_t m_unitHash = 0;
  std::vector<std::string> m_patchDirectives;
  std::set<std::string> m_exportedSymbols;
  std::set<std::string> m_weakExportedSymbols;
  std::set<std::string> m_topLevelDefinedSymbols;
  std::map<std::string, ExportedSymbolLinkage> m_definedExportedSymbols;
  std::map<std::string, std::string> m_privateSymbolRenames;
};

static std::vector<std::filesystem::path> collectCompilableSourceFiles(
    const std::filesystem::path &path) {
  std::vector<std::filesystem::path> sourceFiles;
  std::error_code ec;
  for (auto &p : std::filesystem::recursive_directory_iterator(path, ec)) {
    auto &entryPath = p.path();
    if (!entryPath.has_extension())
      continue;
    auto ext = entryPath.extension();
    if (ext != ".cpp" && ext != ".c" && ext != ".cc" && ext != ".cxx")
      continue;
    sourceFiles.push_back(entryPath);
  }

  std::sort(sourceFiles.begin(), sourceFiles.end(),
            [](const auto &lhs, const auto &rhs) {
              return lhs.generic_string() < rhs.generic_string();
            });
  return sourceFiles;
}

static std::filesystem::path getRelativeSourcePath(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath) {
  auto relativePath = sourceFilePath.lexically_relative(sourceRoot);
  if (relativePath.empty())
    return sourceFilePath.filename();
  return relativePath;
}

static std::string getModuleIdentifier(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath) {
  return getRelativeSourcePath(sourceRoot, sourceFilePath).generic_string();
}

static std::string sanitizeModuleGroupName(std::string_view name) {
  std::string result;
  result.reserve(name.size());

  bool previousWasUnderscore = false;
  for (char ch : name) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      result.push_back(ch);
      previousWasUnderscore = false;
    } else if (!previousWasUnderscore) {
      result.push_back('_');
      previousWasUnderscore = true;
    }
  }

  while (!result.empty() && result.back() == '_')
    result.pop_back();

  if (result.empty())
    return "module";
  return result;
}

static std::string getModuleGroupBaseName(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath,
    bool useSourceSpecificName) {
  if (!useSourceSpecificName)
    return "AutoGenerated";

  auto relativePath = getRelativeSourcePath(sourceRoot, sourceFilePath);
  relativePath.replace_extension();
  return std::format("AutoGenerated_{}",
                     sanitizeModuleGroupName(relativePath.generic_string()));
}

static std::filesystem::path getMergedOutputFilePath(
    const std::filesystem::path &outputTarget) {
  if (outputTarget.has_extension())
    return outputTarget;
  return outputTarget / "patch_compiled.asm";
}

static std::filesystem::path getSingleFileOutputPath(
    const std::filesystem::path &sourceFile,
    const std::filesystem::path &outputTarget, bool explicitOutputTarget) {
  if (!explicitOutputTarget)
    return sourceFile.parent_path() / "patch_compiled.asm";
  if (outputTarget.has_extension())
    return outputTarget;
  return outputTarget / "patch_compiled.asm";
}

struct PreparedSourceModule {
  std::filesystem::path sourceFilePath;
  std::string moduleGroupName;
  uint32_t moduleMatches = 0;
  AssemblyConverter converter;
};

static std::optional<PreparedSourceModule> prepareSourceModule(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath,
    std::string_view moduleGroupName, std::string &errorMessage) {
  auto sourceInput = loadSourceFileInput(sourceFilePath, errorMessage);
  if (!sourceInput) {
    return std::nullopt;
  }

  AssemblyConverter converter;
  if (!converter.prepareCppForLinking(
          sourceFilePath.string(), getModuleIdentifier(sourceRoot, sourceFilePath),
          sourceInput->compileContents)) {
    errorMessage = std::format("Compilation failed for '{}'.",
                               sourceFilePath.generic_string());
    return std::nullopt;
  }

  PreparedSourceModule preparedModule;
  preparedModule.sourceFilePath = sourceFilePath;
  preparedModule.moduleGroupName = std::string(moduleGroupName);
  preparedModule.moduleMatches = sourceInput->moduleMatches;
  preparedModule.converter = std::move(converter);
  return preparedModule;
}

static void appendTextFile(TextFile &destination, std::unique_ptr<TextFile> source) {
  if (!destination.lines.empty())
    destination.lines.emplace_back("");

  for (auto &line : source->lines) {
    destination.lines.push_back(std::move(line));
  }
}

static bool resolveDiscardedExportedSymbols(
    const std::vector<PreparedSourceModule> &preparedModules,
    std::vector<std::set<std::string>> &discardedDefinitionsPerModule,
    std::string &errorMessage) {
  struct SymbolDefinitionSite {
    size_t moduleIndex = 0;
    ExportedSymbolLinkage linkage = ExportedSymbolLinkage::Strong;
  };

  std::map<std::string, std::vector<SymbolDefinitionSite>> symbolDefinitions;
  for (size_t moduleIndex = 0; moduleIndex < preparedModules.size();
       moduleIndex++) {
    for (const auto &[symbolName, linkage] :
         preparedModules[moduleIndex].converter.getDefinedExportedSymbols()) {
      symbolDefinitions[symbolName].push_back({moduleIndex, linkage});
    }
  }

  discardedDefinitionsPerModule.assign(preparedModules.size(), {});
  for (const auto &[symbolName, definitionSites] : symbolDefinitions) {
    std::vector<size_t> strongDefinitionModules;
    for (const auto &definitionSite : definitionSites) {
      if (definitionSite.linkage == ExportedSymbolLinkage::Strong) {
        strongDefinitionModules.push_back(definitionSite.moduleIndex);
      }
    }

    if (strongDefinitionModules.size() > 1) {
      errorMessage = std::format(
          "Symbol '{}' has multiple strong definitions in '{}' and '{}'.",
          symbolName,
          preparedModules[strongDefinitionModules[0]]
              .sourceFilePath.generic_string(),
          preparedModules[strongDefinitionModules[1]]
              .sourceFilePath.generic_string());
      return false;
    }

    size_t keptModuleIndex = definitionSites.front().moduleIndex;
    if (!strongDefinitionModules.empty()) {
      keptModuleIndex = strongDefinitionModules.front();
    }

    for (const auto &definitionSite : definitionSites) {
      if (definitionSite.moduleIndex != keptModuleIndex) {
        discardedDefinitionsPerModule[definitionSite.moduleIndex].emplace(
            symbolName);
      }
    }
  }

  return true;
}

static bool processSourceFile(const std::filesystem::path &sourceFilePath,
                              const std::filesystem::path &outputFilePath) {
  printf("Output File:      %s\n", outputFilePath.generic_string().c_str());
  fflush(stdout);

  std::string errorMessage;
  auto preparedModule = prepareSourceModule(
      sourceFilePath.parent_path(), sourceFilePath, "AutoGenerated",
      errorMessage);
  if (!preparedModule) {
    printf("Error: %s\n", errorMessage.c_str());
    return false;
  }

  auto compiledPatch = preparedModule->converter.emitPatch(
      preparedModule->moduleGroupName, preparedModule->moduleMatches);
  util_writeFile(outputFilePath.string(), compiledPatch.get());
  return true;
}

static bool processDirectory(const std::filesystem::path &path,
                             const std::filesystem::path &outputTarget) {
  auto sourceFiles = collectCompilableSourceFiles(path);
  if (sourceFiles.empty()) {
    printf("No source files (.cpp, .c, .cc, .cxx) found in %s\n",
           path.generic_string().c_str());
    return true;
  }

  auto outputFilePath = getMergedOutputFilePath(outputTarget);
  printf("Output File:      %s\n", outputFilePath.generic_string().c_str());
  fflush(stdout);

  bool useSourceSpecificNames = sourceFiles.size() > 1;
  std::map<std::string, int> groupNameCounts;
  std::vector<PreparedSourceModule> preparedModules;
  preparedModules.reserve(sourceFiles.size());

  for (const auto &sourceFile : sourceFiles) {
    auto baseGroupName =
        getModuleGroupBaseName(path, sourceFile, useSourceSpecificNames);
    int groupCount = ++groupNameCounts[baseGroupName];
    std::string groupName =
        groupCount == 1 ? baseGroupName
                        : std::format("{}_{}", baseGroupName, groupCount);

    printf("Module Group:     %s\n", groupName.c_str());
    fflush(stdout);

    std::string errorMessage;
    auto preparedModule =
        prepareSourceModule(path, sourceFile, groupName, errorMessage);
    if (!preparedModule) {
      printf("Error: %s\n", errorMessage.c_str());
      return false;
    }
    preparedModules.emplace_back(std::move(*preparedModule));
  }

  std::string errorMessage;
  std::vector<std::set<std::string>> discardedDefinitionsPerModule;
  if (!resolveDiscardedExportedSymbols(preparedModules,
                                       discardedDefinitionsPerModule,
                                       errorMessage)) {
    printf("Error: %s\n", errorMessage.c_str());
    return false;
  }

  TextFile mergedOutput;
  for (size_t moduleIndex = 0; moduleIndex < preparedModules.size();
       moduleIndex++) {
    auto compiledPatch = preparedModules[moduleIndex].converter.emitPatch(
        preparedModules[moduleIndex].moduleGroupName,
        preparedModules[moduleIndex].moduleMatches,
        discardedDefinitionsPerModule[moduleIndex]);
    appendTextFile(mergedOutput, std::move(compiledPatch));
  }

  util_writeFile(outputFilePath.string(), &mergedOutput);
  return true;
}

bool isCompilableSourceFile(const std::filesystem::path& path) {
  auto ext = path.extension();
  return ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx";
}

bool directoryHasCompilableSourceFiles(const std::filesystem::path& dir,
                                       bool recursive) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
    return false;

  if (recursive) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
      if (ec)
        break;
      if (entry.is_regular_file(ec) && isCompilableSourceFile(entry.path()))
        return true;
    }
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (entry.is_regular_file(ec) && isCompilableSourceFile(entry.path()))
      return true;
  }

  return false;
}

std::optional<std::filesystem::path> findPortableSourceDirectory(
    const std::vector<std::filesystem::path>& searchRoots) {
  static constexpr std::array<std::string_view, 2> kSourceDirNames = {"src", "source"};

  for (const auto& root : searchRoots) {
    for (auto dirName : kSourceDirNames) {
      auto candidate = root / dirName;
      if (directoryHasCompilableSourceFiles(candidate, true))
        return candidate;
    }
  }

  return std::nullopt;
}

int main(int argc, char *argv[]) {
  std::filesystem::path exeDir = getExecutableDir();
  std::filesystem::path configPath = "";
  bool shouldCreateDefaultConfigOnFailure = false;

  // Check if config.ini exists in current directory or next to executable
  if (std::filesystem::exists("config.ini")) {
    configPath = "config.ini";
  } else if (std::filesystem::exists(exeDir / "config.ini")) {
    configPath = exeDir / "config.ini";
  }

  Config config;
  std::filesystem::path configDir = exeDir;
  if (!configPath.empty()) {
    config = parseIniFile(configPath);
    configDir = configPath.parent_path();
  }

  std::filesystem::path inputPath = "";
  std::filesystem::path outputTarget = "";
  bool explicitOutputTarget = false;

  if (argc > 1) {
    // Drag & drop or CLI arguments
    std::filesystem::path argPath(argv[1]);
    if (std::filesystem::is_directory(argPath)) {
      inputPath = argPath;
      outputTarget = argPath;
    } else {
      inputPath = argPath;
      outputTarget = argPath.parent_path();
    }

    if (argc > 2) {
      outputTarget = argv[2];
      explicitOutputTarget = true;
    }
  } else {
    std::error_code currentDirEc;
    auto currentDir = std::filesystem::current_path(currentDirEc);
    if (currentDirEc) {
      currentDir = ".";
    }

    std::vector<std::filesystem::path> portableSearchRoots = {currentDir};
    if (exeDir != currentDir) {
      portableSearchRoots.push_back(exeDir);
    }
    auto portableSourceDir = findPortableSourceDirectory(portableSearchRoots);
    if (portableSourceDir) {
      inputPath = *portableSourceDir;
      outputTarget = portableSourceDir->parent_path();
    } else if (directoryHasCompilableSourceFiles(currentDir, false)) {
      inputPath = currentDir;
      outputTarget = currentDir;
    }

    if (inputPath.empty()) {
      // Load paths from config.ini
      if (configPath.empty()) {
        configPath = exeDir / "config.ini";
        configDir = configPath.parent_path();
        shouldCreateDefaultConfigOnFailure = true;
      }

      std::string srcDirStr =
          config.get("Paths", "SourceDir", "examples/camera");
      std::string outValStr =
          config.get("Paths", "OutFile", "examples/camera/patch_compiled.asm");

      inputPath = resolvePath(configDir, srcDirStr);
      outputTarget = resolvePath(configDir, outValStr);
    }
  }

  printf("Input Path:       %s\n", inputPath.generic_string().c_str());
  printf("Output Target:    %s\n", outputTarget.generic_string().c_str());

  auto createDefaultConfigOnFailure = [&]() {
    if (!shouldCreateDefaultConfigOnFailure)
      return;

    std::error_code configExistsEc;
    if (!std::filesystem::exists(configPath, configExistsEc)) {
      writeDefaultIniFile(configPath);
      printf("Created default config.ini at %s\n",
             configPath.generic_string().c_str());
    }
  };

  std::error_code ec;
  if (!std::filesystem::exists(inputPath, ec)) {
    createDefaultConfigOnFailure();
    printf("Error: Input path does not exist: %s\n",
           inputPath.generic_string().c_str());
    return 1;
  }

  bool success = false;
  if (std::filesystem::is_directory(inputPath, ec)) {
    success = processDirectory(inputPath, outputTarget);
  } else {
    auto outputFilePath =
        getSingleFileOutputPath(inputPath, outputTarget, explicitOutputTarget);
    success = processSourceFile(inputPath, outputFilePath);
  }

  if (!success) {
    createDefaultConfigOnFailure();
    return 1;
  }

  printf("done\n");
  for (int i = 0; i < 3; i++)
    Sleep(1000);

  return 0;
}
