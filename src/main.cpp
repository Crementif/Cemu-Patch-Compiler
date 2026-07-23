#include "CompilerDriver.h"
#include "InstructionAssembler.h"

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

static void loadHardcodedInstructionsFromConfig(const Config &config) {
  static constexpr std::array<std::string_view, 6> kSettingKeys = {
      "hardcodeInstructions", "hardcode_instructions", "hardcode",
      "hardcodeInt", "hardcode_int", "instructions"};
  for (auto key : kSettingKeys) {
    std::string val = config.get("Settings", std::string(key));
    if (!val.empty()) {
      addHardcodedInstructions(val);
    }
  }

  static constexpr std::array<std::string_view, 4> kSections = {
      "HardcodeInstructions", "Hardcode", "HardcodeInt", "Instructions"};
  for (auto sec : kSections) {
    auto entries = config.getEntries(std::string(sec));
    for (const auto &[k, v] : entries) {
      if (k == "instruction" || k == "instructions" ||
          k == "hardcodeInstructions" || k == "hardcode") {
        addHardcodedInstructions(v);
      } else {
        addHardcodedInstructions(k);
        if (!v.empty()) {
          addHardcodedInstructions(v);
        }
      }
    }
  }
}

static void printHelp(const char *execName) {
  printf("Cemu Patch Compiler - Compiles C/C++ sources into Cemu PPC assembly patches\n\n");
  printf("Usage:\n");
  printf("  %s [options] [input_path] [output_path]\n\n", execName);
  printf("Options:\n");
  printf("  -h, --help                 Show this help text and exit.\n");
  printf("  -v, --version              Show version information.\n");
  printf("  -o, --output <file|dir>    Specify output patch file or directory.\n");
  printf("  -c, --checksum <hex>       Override target module checksum (e.g. 0x6267BFD0).\n");
  printf("  -i, --hardcode-instructions <list> Extend instructions to hardcode as .int (stop-gap for Cemu's assembler, e.g. -i SUBF,MULLW).\n\n");
  printf("Execution Modes:\n");
  printf("  1. Drag & Drop:            Drop a source folder or file onto executable.\n");
  printf("  2. Portable Scanning:      Scans src/ or source/ subfolders in working directory.\n");
  printf("  3. Config File:            Reads config.ini (sourceFolder = OutputFolder format).\n");
  printf("  4. Command Line (CLI):     Pass input folder/file and options (-o, -c, -i, etc.).\n\n");
  printf("Checksum Resolution Priority:\n");
  printf("  1. CLI Flag (-c / --checksum 0xHEX)\n");
  printf("  2. First line header in source: // moduleMatches = 0x6267BFD0\n");
  printf("  3. Auto-inherited from existing .asm / .txt patch files in source directory\n");
  printf("  4. Built-in default (0x6267BFD0)\n");
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
    loadHardcodedInstructionsFromConfig(config);
  }

  std::filesystem::path inputPath = "";
  std::filesystem::path outputTarget = "";
  bool explicitOutputTarget = false;
  std::optional<uint32_t> cliChecksum = std::nullopt;

  std::vector<std::string> positionalArgs;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help" || arg == "/?") {
      printHelp(argv[0]);
      return 0;
    } else if (arg == "-v" || arg == "--version") {
      printf("Cemu Patch Compiler v1.1.0\n");
      return 0;
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 < argc) {
        outputTarget = argv[++i];
        explicitOutputTarget = true;
      } else {
        printf("Error: Missing argument for %s\n", arg.c_str());
        return 1;
      }
    } else if (arg == "-c" || arg == "--checksum" || arg == "--module-matches") {
      if (i + 1 < argc) {
        std::string hexStr = argv[++i];
        if (hexStr.starts_with("0x") || hexStr.starts_with("0X")) {
          hexStr = hexStr.substr(2);
        }
        if (hexStr.size() != 8) {
          printf("Error: Checksum must be an 8-digit hexadecimal value (e.g. 0x6267BFD0).\n");
          return 1;
        }
        uint32_t val = 0;
        auto [ptr, ec] = std::from_chars(hexStr.data(), hexStr.data() + hexStr.size(), val, 16);
        if (ec != std::errc() || ptr != hexStr.data() + hexStr.size()) {
          printf("Error: Invalid hexadecimal checksum: %s\n", argv[i]);
          return 1;
        }
        cliChecksum = val;
      } else {
        printf("Error: Missing argument for %s\n", arg.c_str());
        return 1;
      }
    } else if (arg == "-i" || arg == "--hardcode-instructions" || arg == "--hardcode-instruction" || arg == "--hardcode" || arg == "--hardcode-int") {
      if (i + 1 < argc) {
        addHardcodedInstructions(argv[++i]);
      } else {
        printf("Error: Missing argument for %s\n", arg.c_str());
        return 1;
      }
    } else if (arg.starts_with("-")) {
      printf("Error: Unknown flag '%s'. Run with --help for usage.\n", arg.c_str());
      return 1;
    } else {
      positionalArgs.push_back(arg);
    }
  }

  if (!positionalArgs.empty()) {
    std::filesystem::path argPath(positionalArgs[0]);
    inputPath = argPath;
    if (!explicitOutputTarget) {
      if (std::filesystem::is_directory(argPath)) {
        outputTarget = argPath;
      } else {
        outputTarget = argPath.parent_path();
      }
    }
    if (positionalArgs.size() > 1 && !explicitOutputTarget) {
      outputTarget = positionalArgs[1];
      explicitOutputTarget = true;
    }
    printf("Input Path:       %s\n", inputPath.generic_string().c_str());
    bool success = compileSingleTarget(inputPath, outputTarget, cliChecksum, explicitOutputTarget);
    if (!success) return 1;
    printf("done\n");
    return 0;
  }

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
    if (!explicitOutputTarget) {
      outputTarget = portableSourceDir->parent_path();
    }
  } else if (directoryHasCompilableSourceFiles(currentDir, false)) {
    inputPath = currentDir;
    if (!explicitOutputTarget) {
      outputTarget = currentDir;
    }
  }

  if (!inputPath.empty()) {
    printf("Input Path:       %s\n", inputPath.generic_string().c_str());
    bool success = compileSingleTarget(inputPath, outputTarget, cliChecksum, explicitOutputTarget);
    if (!success) return 1;
    printf("done\n");
    return 0;
  }

  // Load paths from config.ini
  if (configPath.empty()) {
    configPath = exeDir / "config.ini";
    configDir = configPath.parent_path();
    shouldCreateDefaultConfigOnFailure = true;
  }

  auto pathEntries = config.getEntries("Paths");
  if (pathEntries.empty() && shouldCreateDefaultConfigOnFailure) {
    std::error_code configExistsEc;
    if (!std::filesystem::exists(configPath, configExistsEc)) {
      writeDefaultIniFile(configPath);
      printf("Created default config.ini at %s\n",
             configPath.generic_string().c_str());
    }
    config = parseIniFile(configPath);
    configDir = configPath.parent_path();
    loadHardcodedInstructionsFromConfig(config);
    pathEntries = config.getEntries("Paths");
  }

  if (pathEntries.empty()) {
    printf("Error: No valid sourceFolder = OutputFolder entries found in config.ini (%s)\n",
           configPath.generic_string().c_str());
    return 1;
  }

  bool allSuccess = true;
  size_t total = pathEntries.size();
  for (size_t i = 0; i < total; ++i) {
    const auto& [srcDirStr, outValStr] = pathEntries[i];
    std::filesystem::path targetInput = resolvePath(configDir, srcDirStr);
    std::filesystem::path targetOutput = explicitOutputTarget ? outputTarget : resolvePath(configDir, outValStr);

    if (total > 1) {
      printf("\n[%zu/%zu] Target: %s -> %s\n", i + 1, total,
             targetInput.generic_string().c_str(), targetOutput.generic_string().c_str());
    } else {
      printf("Input Path:       %s\n", targetInput.generic_string().c_str());
    }

    bool success = compileSingleTarget(targetInput, targetOutput, cliChecksum, explicitOutputTarget);
    if (!success) {
      allSuccess = false;
      printf("Error: Compilation failed for target %s\n", targetInput.generic_string().c_str());
    }
  }

  if (!allSuccess) {
    return 1;
  }

  printf("done\n");
  return 0;
}
