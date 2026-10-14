#pragma once

#include "AssemblyConverter.h"
#include "ModuleLinker.h"

std::vector<std::filesystem::path> collectCompilableSourceFiles(
    const std::filesystem::path &path);

std::filesystem::path getMergedOutputFilePath(
    const std::filesystem::path &outputTarget);

std::filesystem::path getSingleFileOutputPath(
    const std::filesystem::path &sourceFile,
    const std::filesystem::path &outputTarget, bool explicitOutputTarget);

std::vector<std::string> findUnresolvedSymbols(const TextFile &patch);

bool compileSources(const std::filesystem::path &sourceRoot,
                    const std::vector<std::filesystem::path> &sourceFiles,
                    const std::filesystem::path &outputFilePath,
                    std::optional<uint32_t> cliChecksumOverride,
                    const CompileOptions &options);

bool compileSingleTarget(const std::filesystem::path &inputPath,
                         const std::filesystem::path &outputTarget,
                         std::optional<uint32_t> cliChecksum,
                         bool explicitOutputTarget,
                         const CompileOptions &options);
