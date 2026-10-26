#include "CompilerDriver.h"

std::vector<std::filesystem::path> collectCompilableSourceFiles(
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

std::filesystem::path getMergedOutputFilePath(
    const std::filesystem::path &outputTarget) {
  if (outputTarget.has_extension())
    return outputTarget;
  return outputTarget / "patch_compiled.asm";
}

std::filesystem::path getSingleFileOutputPath(
    const std::filesystem::path &sourceFile,
    const std::filesystem::path &outputTarget, bool explicitOutputTarget) {
  if (!explicitOutputTarget)
    return sourceFile.parent_path() / "patch_compiled.asm";
  if (outputTarget.has_extension())
    return outputTarget;
  return outputTarget / "patch_compiled.asm";
}

static bool isAsmSymbolStart(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$';
}

static bool isAsmSymbolChar(char ch) {
  return isAsmSymbolStart(ch) || ch == '.' || std::isdigit(static_cast<unsigned char>(ch));
}

// Cemu fails to apply a patch group that references an undefined symbol, so report them here
std::vector<std::string> findUnresolvedSymbols(const TextFile &patch) {
  std::set<std::string> defined;
  std::set<std::string> referenced;
  for (const auto &line : patch.lines) {
    std::string_view text = normalizeAssemblyLine(line);
    if (size_t comment = text.find(';'); comment != std::string_view::npos)
      text = trimAsciiWhitespace(text.substr(0, comment));
    if (text.empty())
      continue;
    if (text.back() == ':') {
      defined.emplace(text.substr(0, text.size() - 1));
      continue;
    }
    // patch directives: "0x1234 = bla symbol" and ".callback entry symbol"
    if (size_t assign = text.find(" = "); assign != std::string_view::npos) {
      text = text.substr(assign + 3);
    } else if (text.starts_with(".callback entry ")) {
      referenced.emplace(trimAsciiWhitespace(text.substr(16)));
      continue;
    }
    if (text.starts_with("."))
      continue;

    size_t space = text.find_first_of(" \t");
    std::string_view mnemonic = text.substr(0, space);
    std::string_view operands = space == std::string_view::npos
                                    ? std::string_view()
                                    : trimAsciiWhitespace(text.substr(space));
    bool isBranch = mnemonic == "b" || mnemonic == "bl" || mnemonic == "ba" || mnemonic == "bla";
    if (isBranch && !operands.empty() && isAsmSymbolStart(operands.front()))
      referenced.emplace(operands);

    for (size_t at = operands.find('@'); at != std::string_view::npos;
         at = operands.find('@', at + 1)) {
      size_t start = at;
      while (start > 0 && isAsmSymbolChar(operands[start - 1]))
        start--;
      if (start < at && isAsmSymbolStart(operands[start]))
        referenced.emplace(operands.substr(start, at - start));
    }
  }

  std::vector<std::string> unresolved;
  for (const auto &symbol : referenced) {
    if (!defined.contains(symbol) && !symbol.starts_with("import."))
      unresolved.push_back(symbol);
  }
  return unresolved;
}

bool compileSources(const std::filesystem::path &sourceRoot,
                    const std::vector<std::filesystem::path> &sourceFiles,
                    const std::filesystem::path &outputFilePath,
                    std::optional<uint32_t> cliChecksumOverride,
                    const CompileOptions &options) {
  std::vector<SourceUnit> units;
  std::optional<uint32_t> sourceChecksum;
  std::filesystem::path checksumSource;
  for (const auto &sourceFile : sourceFiles) {
    std::string errorMessage;
    auto sourceInput = loadSourceFileInput(sourceFile, errorMessage);
    if (!sourceInput) {
      printf("Error: %s\n", errorMessage.c_str());
      return false;
    }
    if (sourceInput->hasExplicitChecksum) {
      if (sourceChecksum && *sourceChecksum != sourceInput->moduleMatches) {
        printf("Error: '%s' and '%s' declare different moduleMatches checksums, but all sources are linked into one patch.\n",
               checksumSource.generic_string().c_str(), sourceFile.generic_string().c_str());
        return false;
      }
      sourceChecksum = sourceInput->moduleMatches;
      checksumSource = sourceFile;
    }
    units.push_back({sourceFile, std::move(sourceInput->compileContents)});
  }

  uint32_t moduleMatches = kDefaultModuleMatches;
  if (cliChecksumOverride) {
    moduleMatches = *cliChecksumOverride;
  } else if (sourceChecksum) {
    moduleMatches = *sourceChecksum;
  } else if (auto found = findModuleMatchesInDirectory(sourceFiles.front().parent_path())) {
    moduleMatches = *found;
  } else if (auto foundRoot = findModuleMatchesInDirectory(sourceRoot)) {
    moduleMatches = *foundRoot;
  }

  printf("Target Checksum:  0x%08X\n", moduleMatches);
  printf("Output File:      %s\n", outputFilePath.generic_string().c_str());
  fflush(stdout);

  auto assembly = compileAndLinkSources(units, options);
  if (!assembly)
    return false;

  AssemblyConverter converter;
  converter.setSourceComments(options.sourceComments);
  converter.prepare(std::move(assembly), sourceRoot.generic_string());
  auto patch = converter.emitPatch("AutoGenerated", moduleMatches);
  if (!patch)
    return false;

  auto unresolved = findUnresolvedSymbols(*patch);
  if (!unresolved.empty()) {
    printf("Warning: %d symbol(s) are referenced but not defined in this patch, so another patch file of the graphic pack has to define them:\n",
           (int)unresolved.size());
    for (const auto &symbol : unresolved)
      printf("  %s\n", symbol.c_str());
  }

  util_writeFile(outputFilePath.string(), patch.get());
  return true;
}

bool compileSingleTarget(const std::filesystem::path &inputPath,
                         const std::filesystem::path &outputTarget,
                         std::optional<uint32_t> cliChecksum,
                         bool explicitOutputTarget,
                         const CompileOptions &options) {
  std::error_code ec;
  if (!std::filesystem::exists(inputPath, ec)) {
    printf("Error: Input path does not exist: %s\n",
           inputPath.generic_string().c_str());
    return false;
  }

  if (std::filesystem::is_directory(inputPath, ec)) {
    auto sourceFiles = collectCompilableSourceFiles(inputPath);
    if (sourceFiles.empty()) {
      printf("No source files (.cpp, .c, .cc, .cxx) found in %s\n",
             inputPath.generic_string().c_str());
      return true;
    }
    return compileSources(inputPath, sourceFiles, getMergedOutputFilePath(outputTarget),
                          cliChecksum, options);
  }
  return compileSources(inputPath.parent_path(), {inputPath},
                        getSingleFileOutputPath(inputPath, outputTarget, explicitOutputTarget),
                        cliChecksum, options);
}
