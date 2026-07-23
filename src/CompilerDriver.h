#pragma once

#include "AssemblyConverter.h"

struct PreparedSourceModule {
  std::filesystem::path sourceFilePath;
  std::string moduleGroupName;
  uint32_t moduleMatches = 0;
  AssemblyConverter converter;
};

std::vector<std::filesystem::path> collectCompilableSourceFiles(
    const std::filesystem::path &path);

std::filesystem::path getRelativeSourcePath(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath);

std::string getModuleIdentifier(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath);

std::string sanitizeModuleGroupName(std::string_view name);

std::string getModuleGroupBaseName(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath,
    bool useSourceSpecificName);

std::filesystem::path getMergedOutputFilePath(
    const std::filesystem::path &outputTarget);

std::filesystem::path getSingleFileOutputPath(
    const std::filesystem::path &sourceFile,
    const std::filesystem::path &outputTarget, bool explicitOutputTarget);

std::optional<PreparedSourceModule> prepareSourceModule(
    const std::filesystem::path &sourceRoot,
    const std::filesystem::path &sourceFilePath,
    std::string_view moduleGroupName,
    std::optional<uint32_t> cliChecksumOverride,
    std::string &errorMessage);

bool resolveDiscardedExportedSymbols(
    const std::vector<PreparedSourceModule> &preparedModules,
    std::vector<std::set<std::string>> &discardedDefinitionsPerModule,
    std::string &errorMessage);

bool processSourceFile(const std::filesystem::path &sourceFilePath,
                      const std::filesystem::path &outputFilePath,
                      std::optional<uint32_t> cliChecksumOverride);

bool processDirectory(const std::filesystem::path &path,
                     const std::filesystem::path &outputTarget,
                     std::optional<uint32_t> cliChecksumOverride);

bool compileSingleTarget(const std::filesystem::path &inputPath,
                         const std::filesystem::path &outputTarget,
                         std::optional<uint32_t> cliChecksum,
                         bool explicitOutputTarget);
