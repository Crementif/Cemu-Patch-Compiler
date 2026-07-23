#pragma once

struct TextFile {
  std::vector<std::string> lines;
};

struct SourceFileInput {
  std::string compileContents;
  uint32_t moduleMatches = 0;
  bool hasExplicitChecksum = false;
};

static constexpr uint32_t kDefaultModuleMatches = 0x6267BFD0;

std::string_view trimAsciiWhitespace(std::string_view text);
std::string_view stripUtf8Bom(std::string_view text);

std::unique_ptr<TextFile> util_readTextFile(std::string_view path);
std::unique_ptr<TextFile> parseAssemblyFromMemory(std::string_view content);
void util_writeFile(std::string_view path, const TextFile *textFile);
bool util_stringReplace(std::string &str, const std::string &from,
                        const std::string &to);
void appendTextFile(TextFile &destination, std::unique_ptr<TextFile> source);

struct ParsedChecksumResult {
  bool hasDirective = false;
  uint32_t moduleMatches = kDefaultModuleMatches;
};

std::optional<ParsedChecksumResult> parseModuleMatchesDirective(
    std::string_view firstLine, std::string &errorMessage);
std::optional<uint32_t> findModuleMatchesInContent(std::string_view content);
std::optional<uint32_t> findModuleMatchesInDirectory(const std::filesystem::path &dir);
std::optional<SourceFileInput> loadSourceFileInput(
    const std::filesystem::path &sourcePath, std::string &errorMessage);

uint64_t calculateUnitHash(std::string_view moduleName);
bool isAssemblySymbolChar(char ch);
bool shouldUniquifyPrivateSymbol(
    std::string_view symbol, const std::set<std::string> &exportedSymbols);
std::string rewritePrivateSymbols(
    const std::map<std::string, std::string> &symbolRenames,
    std::string_view line);
std::string_view normalizeAssemblyLine(std::string_view line);
bool parseCommonSymbolDirective(std::string_view line, std::string &symbol,
                                uint32_t &sizeVal, uint32_t &alignVal);
std::optional<std::string>
parseTopLevelSymbolDefinition(std::string_view normalizedLine);
void emitZeroInitializedData(TextFile &outputTextFile,
                            std::string_view symbol, uint32_t sizeVal,
                            uint32_t alignVal, uint32_t fillVal = 0);
