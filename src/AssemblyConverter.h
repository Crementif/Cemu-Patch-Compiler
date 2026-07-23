#pragma once

#include "AssemblyParser.h"

enum class ExportedSymbolLinkage {
  Strong,
  Weak,
};

std::unique_ptr<TextFile> compileCppWithEmbeddedClang(
    const std::string &sourceFilePath, std::string_view sourceFileContents);

class AssemblyConverter {
public:
  AssemblyConverter() = default;

  bool prepareCppForLinking(std::string_view sourceFilePath,
                            std::string_view moduleName,
                            std::string_view sourceContents);

  std::unique_ptr<TextFile>
  emitPatch(std::string_view moduleGroupName, uint32_t moduleMatches,
            const std::set<std::string> &discardedDefinitions = {});

  const std::map<std::string, ExportedSymbolLinkage> &
  getDefinedExportedSymbols() const {
    return m_definedExportedSymbols;
  }

private:
  std::string getEmittedSymbolName(std::string_view symbol) const;

  void convertClangToCemu(const TextFile &inputFile, TextFile &outputTextFile,
                          const std::set<std::string> &discardedDefinitions);

  void analyzeSymbols(const TextFile &inputFile);

  void finalize(TextFile &outputTextFile);

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
