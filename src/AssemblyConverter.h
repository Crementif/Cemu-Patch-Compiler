#pragma once

#include "AssemblyParser.h"

class AssemblyConverter {
public:
  AssemblyConverter() = default;

  void prepare(std::unique_ptr<TextFile> assembly, std::string_view moduleName);

  std::unique_ptr<TextFile> emitPatch(std::string_view moduleGroupName,
                                      uint32_t moduleMatches);

private:
  std::string getEmittedSymbolName(std::string_view symbol) const;

  void convertClangToCemu(const TextFile &inputFile, TextFile &outputTextFile);

  void analyzeSymbols(const TextFile &inputFile);

  bool splitIntoPatchGroups(const TextFile &body, std::string_view moduleGroupName,
                            uint32_t moduleMatches, TextFile &output);

  void finalize(TextFile &outputTextFile);

  std::unique_ptr<TextFile> m_inputTextFile;
  std::string m_moduleName;
  uint64_t m_unitHash = 0;
  std::vector<std::string> m_patchDirectives;
  std::set<std::string> m_exportedSymbols;
  std::map<std::string, std::string> m_privateSymbolRenames;
};
