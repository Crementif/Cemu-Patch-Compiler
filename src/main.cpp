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



struct TextFile {
  std::vector<std::string> lines;
};

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

static std::unique_ptr<TextFile> compileCppWithEmbeddedClang(const std::string& sourceFilePath) {
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

  const TextFile *getOutput() const { return m_outputTextFile.get(); }

  void start() {
    m_outputTextFile = std::make_unique<TextFile>();
    m_outputTextFile->lines.emplace_back("[AutoGenerated]");
    m_outputTextFile->lines.emplace_back("moduleMatches = 0x6267BFD0");
    m_outputTextFile->lines.emplace_back(".origin = codecave");
    m_outputTextFile->lines.emplace_back("");
  }

  void convertCppToCemu(std::string_view sourceFilePath,
                        std::string_view moduleName) {
    std::filesystem::path sourcePath(sourceFilePath);
    printf("Compiling %s...\n", sourcePath.filename().string().c_str());
    fflush(stdout);

    auto inputFile = compileCppWithEmbeddedClang(std::string(sourceFilePath));
    if (!inputFile) {
      printf("Error: Compilation failed using embedded Clang.\n");
      ExitProcess(1);
    }

    findLabels(*inputFile);
    convertClangToCemu(moduleName, *inputFile, *m_outputTextFile);
  }

  void convertClangToCemu(std::string_view moduleName,
                          const TextFile &inputFile, TextFile &outputTextFile) {
    printf("Starting convertClangToCemu for module %s...\n",
           std::string(moduleName).c_str());
    fflush(stdout);

    // calc hash
    uint64_t unitHash = 0;
    for (uint32_t t = 0; t < 8; t++) {
      for (auto &itr : moduleName) {
        unitHash += (uint64_t)itr;
        unitHash = _rotr64(unitHash, 3);
      }
    }

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
          parser.matchWordI(".addrsig_sym") || parser.matchWordI(".machine")) {
        continue;
      }

      inputLine = AsmUpdater_fixClangLocalLabels(unitHash, inputLine);
      parser = StringTokenParser(inputLine.data(), (int32_t)inputLine.size());

      // some commands have to be translated
      if (parser.matchWordI(".p2align")) {
        uint32_t val;
        if (!parser.parseU32(val))
          __debugbreak();
        outputTextFile.lines.emplace_back(std::format("\t.align {}", 1 << val));
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
      if (parser.matchWordI(".zero")) {
        uint32_t zeroSize;
        if (!parser.parseU32(zeroSize))
          __debugbreak();
        if (zeroSize > 0) {
          uint32_t numInts = zeroSize / 4;
          uint32_t remainder = zeroSize % 4;
          if (numInts > 0) {
            std::string line = "\t.int 0";
            for (uint32_t i = 1; i < numInts; i++)
              line.append(",0");
            outputTextFile.lines.emplace_back(std::move(line));
          }
          if (remainder >= 2) {
            outputTextFile.lines.emplace_back("\t.short 0");
            remainder -= 2;
          }
          if (remainder == 1) {
            outputTextFile.lines.emplace_back("\t.byte 0");
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

  void findLabels(const TextFile &inputFile) {
    printf("Starting findLabels for %d lines...\n",
           (int)inputFile.lines.size());
    fflush(stdout);
    for (const auto &line : inputFile.lines) {
      std::string_view p = line;

      // trim whitespaces
      while (!p.empty() && (p.front() == ' ' || p.front() == '\t'))
        p.remove_prefix(1);
      while (!p.empty() && (p.back() == ' ' || p.back() == '\t'))
        p.remove_suffix(1);

      // cut off comments
      size_t commentPos = p.find('#');
      if (commentPos != std::string_view::npos)
        p = p.substr(0, commentPos);

      // trim whitespaces again
      while (!p.empty() && (p.front() == ' ' || p.front() == '\t'))
        p.remove_prefix(1);
      while (!p.empty() && (p.back() == ' ' || p.back() == '\t'))
        p.remove_suffix(1);

      if (p.empty())
        continue;

      // check if line ends with ':' indicating a label
      if (p.back() == ':') {
        std::string_view label = p.substr(0, p.size() - 1);
        printf("%s\n", std::string(label).c_str());
      }
    }
  }

  void finalize() {
    if (!m_patchDirectives.empty()) {
      m_outputTextFile->lines.emplace_back("");
      m_outputTextFile->lines.emplace_back("; --- Address Patches ---");
      for (const auto &directive : m_patchDirectives) {
        m_outputTextFile->lines.emplace_back(directive);
      }
      printf("Emitted %d address patch directive(s).\n",
             (int)m_patchDirectives.size());
      fflush(stdout);
    }
  }

private:
  std::unique_ptr<TextFile> m_outputTextFile;
  std::vector<std::string> m_patchDirectives;
};

void processDirectory(std::string_view path, std::string_view outputPatchFile) {
  AssemblyConverter converter;
  converter.start();

  int compiledCount = 0;
  std::error_code ec;
  for (auto &p : std::filesystem::recursive_directory_iterator(path, ec)) {
    auto &entryPath = p.path();
    if (!entryPath.has_extension())
      continue;
    auto ext = entryPath.extension();
    if (ext != ".cpp" && ext != ".c" && ext != ".cc" && ext != ".cxx")
      continue;

    converter.convertCppToCemu(entryPath.string(),
                               entryPath.filename().generic_string());
    compiledCount++;
  }

  if (compiledCount > 0) {
    converter.finalize();
    util_writeFile(outputPatchFile, converter.getOutput());
  } else {
    printf("No source files (.cpp, .c, .cc, .cxx) found in %s\n",
           std::string(path).c_str());
  }
}

int main(int argc, char *argv[]) {
  std::filesystem::path exeDir = getExecutableDir();
  std::filesystem::path configPath = "";

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

  std::filesystem::path sourceDir = "";
  std::filesystem::path outFile = "";

  if (argc > 1) {
    // Drag & drop or CLI arguments
    std::filesystem::path argPath(argv[1]);
    if (std::filesystem::is_directory(argPath)) {
      sourceDir = argPath;
      outFile = argPath / "patch_compiled.asm";
    } else {
      sourceDir = argPath.parent_path();
      outFile = sourceDir / "patch_compiled.asm";
    }

    if (argc > 2) {
      outFile = argv[2];
    }
  } else {
    // Check CWD for C/C++ source files
    bool hasSourceFilesCwd = false;
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(".", ec)) {
      if (entry.is_regular_file()) {
        auto ext = entry.path().extension();
        if (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx") {
          hasSourceFilesCwd = true;
          break;
        }
      }
    }

    if (hasSourceFilesCwd) {
      sourceDir = ".";
      outFile = "./patch_compiled.asm";
    } else {
      // Load paths from config.ini
      if (configPath.empty()) {
        configPath = exeDir / "config.ini";
        writeDefaultIniFile(configPath);
        config = parseIniFile(configPath);
        configDir = configPath.parent_path();
      }

      std::string srcDirStr =
          config.get("Paths", "SourceDir", "examples/camera");
      std::string outValStr =
          config.get("Paths", "OutFile", "examples/camera/patch_compiled.asm");

      sourceDir = resolvePath(configDir, srcDirStr);
      outFile = resolvePath(configDir, outValStr);
    }
  }

  printf("Source Directory: %s\n", sourceDir.generic_string().c_str());
  printf("Output File:      %s\n", outFile.generic_string().c_str());

  std::error_code ec;
  if (std::filesystem::exists(sourceDir, ec)) {
    processDirectory(sourceDir.string(), outFile.string());
  } else {
    printf("Error: Source directory does not exist: %s\n",
           sourceDir.generic_string().c_str());
  }

  printf("done\n");
  for (int i = 0; i < 3; i++)
    Sleep(1000);

  return 0;
}