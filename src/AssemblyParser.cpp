#include "AssemblyParser.h"
#include "StringParser.h"

std::string_view trimAsciiWhitespace(std::string_view text) {
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

std::string_view stripUtf8Bom(std::string_view text) {
  if (text.size() >= 3 &&
      static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);
  }
  return text;
}

std::optional<ParsedChecksumResult> parseModuleMatchesDirective(
    std::string_view firstLine, std::string &errorMessage) {
  firstLine = stripUtf8Bom(firstLine);
  if (!firstLine.starts_with("//")) {
    return ParsedChecksumResult{false, kDefaultModuleMatches};
  }

  std::string_view payload = trimAsciiWhitespace(firstLine.substr(2));
  static constexpr std::string_view kDirectiveName = "moduleMatches";
  if (!payload.starts_with(kDirectiveName)) {
    return ParsedChecksumResult{false, kDefaultModuleMatches};
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

  return ParsedChecksumResult{true, moduleMatches};
}

std::optional<uint32_t> findModuleMatchesInContent(std::string_view content) {
  size_t pos = 0;
  while ((pos = content.find("moduleMatches", pos)) != std::string_view::npos) {
    size_t start = pos + 13;
    pos = start;
    std::string_view rest = content.substr(start);
    rest = trimAsciiWhitespace(rest);
    if (!rest.empty() && rest.front() == '=') {
      rest.remove_prefix(1);
      rest = trimAsciiWhitespace(rest);
    }
    if (rest.size() >= 10 && rest[0] == '0' && (rest[1] == 'x' || rest[1] == 'X')) {
      rest.remove_prefix(2);
      size_t hexLen = 0;
      while (hexLen < rest.size() && std::isxdigit(static_cast<unsigned char>(rest[hexLen]))) {
        hexLen++;
      }
      if (hexLen == 8) {
        uint32_t val = 0;
        auto [ptr, ec] = std::from_chars(rest.data(), rest.data() + 8, val, 16);
        if (ec == std::errc()) {
          return val;
        }
      }
    }
  }
  return std::nullopt;
}

std::optional<uint32_t> findModuleMatchesInDirectory(const std::filesystem::path &dir) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
    return std::nullopt;
  }

  // 1. Scan existing assembly/patch files (*.asm, *.txt) first
  for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec)) continue;
    auto ext = entry.path().extension();
    if (ext == ".asm" || ext == ".txt") {
      std::ifstream f(entry.path(), std::ios::binary);
      if (f.is_open()) {
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto matched = findModuleMatchesInContent(content);
        if (matched) return matched;
      }
    }
  }

  // 2. Scan sibling source/header files (.cpp, .c, .cc, .cxx, .h, .hpp)
  for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec)) continue;
    auto ext = entry.path().extension();
    if (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx" || ext == ".h" || ext == ".hpp") {
      std::ifstream f(entry.path(), std::ios::binary);
      if (f.is_open()) {
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto matched = findModuleMatchesInContent(content);
        if (matched) return matched;
      }
    }
  }

  // 3. Scan parent directory as fallback if dir has a parent
  if (dir.has_parent_path() && dir.parent_path() != dir) {
    std::filesystem::path parentDir = dir.parent_path();
    for (const auto &entry : std::filesystem::directory_iterator(parentDir, ec)) {
      if (ec) break;
      if (!entry.is_regular_file(ec)) continue;
      auto ext = entry.path().extension();
      if (ext == ".asm" || ext == ".txt") {
        std::ifstream f(entry.path(), std::ios::binary);
        if (f.is_open()) {
          std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
          auto matched = findModuleMatchesInContent(content);
          if (matched) return matched;
        }
      }
    }
  }

  return std::nullopt;
}

std::optional<SourceFileInput> loadSourceFileInput(
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

  auto checksumRes = parseModuleMatchesDirective(firstLine, errorMessage);
  if (!checksumRes) {
    errorMessage = std::format("{} Source file: '{}'.", errorMessage,
                               sourcePath.generic_string());
    return std::nullopt;
  }

  return SourceFileInput{std::move(contents), checksumRes->moduleMatches, checksumRes->hasDirective};
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

std::unique_ptr<TextFile> parseAssemblyFromMemory(std::string_view content) {
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

void appendTextFile(TextFile &destination, std::unique_ptr<TextFile> source) {
  if (!destination.lines.empty())
    destination.lines.emplace_back("");

  for (auto &line : source->lines) {
    destination.lines.push_back(std::move(line));
  }
}

uint64_t calculateUnitHash(std::string_view moduleName) {
  uint64_t unitHash = 0;
  for (uint32_t t = 0; t < 8; t++) {
    for (auto &itr : moduleName) {
      unitHash += (uint64_t)itr;
      unitHash = _rotr64(unitHash, 3);
    }
  }
  return unitHash;
}

bool isAssemblySymbolChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.';
}

bool shouldUniquifyPrivateSymbol(
    std::string_view symbol, const std::set<std::string> &exportedSymbols) {
  if (symbol.empty() || symbol.front() == '.') {
    return false;
  }
  return !exportedSymbols.contains(std::string(symbol));
}

std::string rewritePrivateSymbols(
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

std::string_view normalizeAssemblyLine(std::string_view line) {
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

bool parseCommonSymbolDirective(std::string_view line, std::string &symbol,
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

std::optional<std::string>
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

void emitZeroInitializedData(TextFile &outputTextFile,
                            std::string_view symbol, uint32_t sizeVal,
                            uint32_t alignVal, uint32_t fillVal) {
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
