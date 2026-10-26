#include "AssemblyConverter.h"
#include "StringParser.h"

#include "AsmUpdater.h"

static void decodeGasStringLiterals(std::string_view sv, std::vector<uint8_t>& out) {
  size_t i = 0;
  while (i < sv.size()) {
    if (sv[i] != '"') {
      i++;
      continue;
    }
    i++;
    while (i < sv.size() && sv[i] != '"') {
      char c = sv[i++];
      if (c != '\\' || i >= sv.size()) {
        out.push_back((uint8_t)c);
        continue;
      }
      char e = sv[i++];
      switch (e) {
      case 'n': out.push_back('\n'); break;
      case 't': out.push_back('\t'); break;
      case 'r': out.push_back('\r'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'v': out.push_back('\v'); break;
      case 'a': out.push_back('\a'); break;
      case 'x': {
        uint32_t v = 0;
        while (i < sv.size() && isxdigit((unsigned char)sv[i])) {
          char h = sv[i++];
          v = v * 16 + (isdigit((unsigned char)h) ? h - '0' : (tolower(h) - 'a' + 10));
        }
        out.push_back((uint8_t)v);
        break;
      }
      default:
        if (e >= '0' && e <= '7') {
          uint32_t v = e - '0';
          for (int n = 0; n < 2 && i < sv.size() && sv[i] >= '0' && sv[i] <= '7'; n++)
            v = v * 8 + (sv[i++] - '0');
          out.push_back((uint8_t)v);
        } else {
          out.push_back((uint8_t)e);
        }
      }
    }
    i++;
  }
}

void AssemblyConverter::prepare(std::unique_ptr<TextFile> assembly,
                                std::string_view moduleName) {
  m_inputTextFile = std::move(assembly);
  m_moduleName = moduleName;
  m_unitHash = calculateUnitHash(moduleName);
  analyzeSymbols(*m_inputTextFile);
}

std::unique_ptr<TextFile>
AssemblyConverter::emitPatch(std::string_view moduleGroupName, uint32_t moduleMatches) {
  cemu_assert_debug(m_inputTextFile != nullptr);

  TextFile body;
  m_patchDirectives.clear();
  convertClangToCemu(*m_inputTextFile, body);

  auto outputTextFile = std::make_unique<TextFile>();
  if (!splitIntoPatchGroups(body, moduleGroupName, moduleMatches, *outputTextFile))
    return nullptr;
  finalize(*outputTextFile);
  return outputTextFile;
}

static size_t countOperands(std::string_view operands) {
  return operands.empty() ? 0 : 1 + std::count(operands.begin(), operands.end(), ',');
}

// Size in bytes Cemu's assembler gives a line placed at the given codecave offset
static uint32_t getAssembledLineSize(std::string_view line, uint32_t offset) {
  std::string_view text = normalizeAssemblyLine(line);
  if (size_t comment = text.find(';'); comment != std::string_view::npos)
    text = trimAsciiWhitespace(text.substr(0, comment));
  if (text.empty() || text.back() == ':')
    return 0;
  if (text.front() != '.')
    return 4;

  size_t space = text.find_first_of(" \t");
  std::string_view directive = text.substr(0, space);
  std::string_view operands =
      space == std::string_view::npos ? std::string_view() : trimAsciiWhitespace(text.substr(space));
  if (directive == ".align") {
    uint32_t alignment = 1;
    std::from_chars(operands.data(), operands.data() + operands.size(), alignment);
    return alignment > 1 ? (alignment - offset % alignment) % alignment : 0;
  }
  if (directive == ".byte")
    return (uint32_t)countOperands(operands);
  if (directive == ".short")
    return (uint32_t)countOperands(operands) * 2;
  if (directive == ".int" || directive == ".float")
    return (uint32_t)countOperands(operands) * 4;
  if (directive == ".double")
    return (uint32_t)countOperands(operands) * 8;
  printf("Error: Can't size the directive '%s' to place it in a code cave.\n", std::string(text).c_str());
  return UINT32_MAX;
}

static bool isLabelLine(std::string_view line) {
  std::string_view text = normalizeAssemblyLine(line);
  return !text.empty() && text.back() == ':';
}

static bool isAlignOrBlankLine(std::string_view line) {
  std::string_view text = normalizeAssemblyLine(line);
  return text.empty() || text.starts_with(".align");
}

// Function local labels look like _<unit hash>_L...
static bool isLocalLabel(std::string_view line) {
  std::string_view text = normalizeAssemblyLine(line);
  return text.size() > 19 && text[0] == '_' && text[17] == '_' && text[18] == 'L';
}

// Cemu only places offsets below 1 MB of a patch group in its code cave, and resolving a group
// takes time quadratic in its entry count, since resolved entries are erased one by one from a
// vector. Patches are therefore split into 128 KB groups, before a top level symbol together with
// its alignment and alias labels. Groups resolve each other's symbols.
bool AssemblyConverter::splitIntoPatchGroups(const TextFile &body,
                                             std::string_view moduleGroupName,
                                             uint32_t moduleMatches, TextFile &output) {
  constexpr uint32_t kCodeCaveLimit = 0x100000;
  constexpr uint32_t kGroupSize = 0x20000;
  const auto &lines = body.lines;

  std::vector<size_t> chunkStarts = {0};
  for (size_t i = 1; i < lines.size(); i++) {
    if (!isLabelLine(lines[i]) || isLocalLabel(lines[i]))
      continue;
    size_t start = i;
    while (start > chunkStarts.back() + 1 && isAlignOrBlankLine(lines[start - 1]))
      start--;
    if (start > chunkStarts.back() && !isLabelLine(lines[start - 1]))
      chunkStarts.push_back(start);
  }
  chunkStarts.push_back(lines.size());

  int groupCount = 0;
  uint32_t offset = 0;
  auto beginGroup = [&] {
    groupCount++;
    if (groupCount > 1)
      output.lines.emplace_back("");
    output.lines.emplace_back(groupCount == 1
                                  ? std::format("[{}]", moduleGroupName)
                                  : std::format("[{}_{}]", moduleGroupName, groupCount));
    output.lines.emplace_back(std::format("moduleMatches = 0x{:08X}", moduleMatches));
    output.lines.emplace_back(".origin = codecave");
    output.lines.emplace_back("");
    offset = 0;
  };
  beginGroup();

  for (size_t chunk = 0; chunk + 1 < chunkStarts.size(); chunk++) {
    uint32_t chunkOffset = offset;
    for (size_t i = chunkStarts[chunk]; i < chunkStarts[chunk + 1]; i++) {
      uint32_t size = getAssembledLineSize(lines[i], chunkOffset);
      if (size == UINT32_MAX)
        return false;
      chunkOffset += size;
    }
    if (chunkOffset > kGroupSize && offset > 0) {
      beginGroup();
      chunkOffset = 0;
      for (size_t i = chunkStarts[chunk]; i < chunkStarts[chunk + 1]; i++)
        chunkOffset += getAssembledLineSize(lines[i], chunkOffset);
    }
    if (chunkOffset > kCodeCaveLimit) {
      printf("Error: A single symbol needs more than the 1 MB a patch group's code cave can hold.\n");
      return false;
    }
    for (size_t i = chunkStarts[chunk]; i < chunkStarts[chunk + 1]; i++)
      output.lines.emplace_back(lines[i]);
    offset = chunkOffset;
  }

  if (groupCount > 1) {
    printf("Split into %d patch groups.\n", groupCount);
    fflush(stdout);
  }
  return true;
}

std::string AssemblyConverter::getEmittedSymbolName(std::string_view symbol) const {
  auto renameIt = m_privateSymbolRenames.find(std::string(symbol));
  if (renameIt != m_privateSymbolRenames.end()) {
    return renameIt->second;
  }
  return std::string(symbol);
}

// Comments the code with its source lines, from the .file and .loc directives of -gline-directives-only
class SourceLineComments {
public:
  // Returns whether the line was line info
  bool handleLine(std::string_view line, std::vector<std::string>& output) {
    std::string_view args;
    if (matchDirective(line, ".file", args))
      addFile(args);
    else if (matchDirective(line, ".loc", args))
      addLocation(args, output);
    // Line info also brings unwind directives and labels for the line table
    else if (!line.starts_with(".cfi_") && !(line.starts_with(".Ltmp") && line.ends_with(':')))
      return false;
    return true;
  }

private:
  static bool matchDirective(std::string_view line, std::string_view name, std::string_view& args) {
    if (!line.starts_with(name) || line.size() <= name.size() || (line[name.size()] != ' ' && line[name.size()] != '\t'))
      return false;
    args = line.substr(name.size());
    return true;
  }

  // .file <index> "<directory>" "<file>"
  void addFile(std::string_view args) {
    uint32_t index = 0;
    if (!parseNumber(args, index))
      return;
    std::vector<std::string> parts = quotedStrings(args);
    if (parts.empty())
      return;
    fs::path path(parts[0]);
    if (parts.size() > 1)
      path /= parts[1];
    m_files[index] = path;
  }

  // .loc <file index> <line> [<column>] [<options>]
  void addLocation(std::string_view args, std::vector<std::string>& output) {
    uint32_t index = 0, lineNumber = 0;
    if (!parseNumber(args, index) || !parseNumber(args, lineNumber) || lineNumber == 0)
      return;
    if (index == m_lastFile && lineNumber == m_lastLine)
      return;
    auto file = m_files.find(index);
    if (file == m_files.end())
      return;
    const std::vector<std::string>& lines = readLines(file->second);
    if (lineNumber > lines.size())
      return;
    m_lastFile = index;
    m_lastLine = lineNumber;
    StringTokenParser text(lines[lineNumber - 1]);
    text.skipWhitespaces();
    text.trimWhitespaces();
    if (text.isEndOfString())
      return;
    output.emplace_back(std::format("\t; {}:{}: {}", file->second.filename().generic_string(), lineNumber, text.getView()));
  }

  // parseU32 reads a leading 0 as hex, which breaks on file index 0
  static bool parseNumber(std::string_view& args, uint32_t& value) {
    while (!args.empty() && (args.front() == ' ' || args.front() == '\t'))
      args.remove_prefix(1);
    auto [end, ec] = std::from_chars(args.data(), args.data() + args.size(), value);
    if (ec != std::errc())
      return false;
    args.remove_prefix(end - args.data());
    return true;
  }

  static std::vector<std::string> quotedStrings(std::string_view line) {
    std::vector<std::string> result;
    for (size_t i = 0; i < line.size(); i++) {
      if (line[i] != '"')
        continue;
      std::string value;
      for (i++; i < line.size() && line[i] != '"'; i++) {
        if (line[i] == '\\' && i + 1 < line.size())
          i++;
        value.push_back(line[i]);
      }
      result.push_back(std::move(value));
    }
    return result;
  }

  const std::vector<std::string>& readLines(const fs::path& path) {
    auto [it, inserted] = m_sources.try_emplace(path.generic_string());
    if (inserted) {
      std::ifstream file(path);
      for (std::string text; std::getline(file, text);)
        it->second.push_back(std::move(text));
    }
    return it->second;
  }

  std::map<uint32_t, fs::path> m_files;
  std::map<std::string, std::vector<std::string>> m_sources;
  uint32_t m_lastFile = 0;
  uint32_t m_lastLine = 0;
};

void AssemblyConverter::convertClangToCemu(const TextFile &inputFile,
                                           TextFile &outputTextFile) {
  printf("Starting convertClangToCemu for module %s...\n",
         m_moduleName.c_str());
  fflush(stdout);

  std::set<size_t> stringDataLines;
  SourceLineComments sourceLines;
  std::vector<std::pair<std::string, std::string>> symbolAliases;
  size_t firstOutputLine = outputTextFile.lines.size();
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
      // Before the renames below, which would also apply to the file names
      if (m_sourceComments && sourceLines.handleLine(trimmed, outputTextFile.lines))
        continue;
    }

    inputLine = AsmUpdater_fixClangLocalLabels(m_unitHash, inputLine);
    inputLine = rewritePrivateSymbols(m_privateSymbolRenames, inputLine);
    std::string_view normalizedInputLine = normalizeAssemblyLine(inputLine);

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
        parser.matchWordI(".local")) {
      continue;
    }
    if (parser.matchWordI(".set")) {
      std::string rest = inputLine.substr(inputLine.find(".set") + 4);
      size_t comma = rest.find(',');
      if (comma != std::string::npos) {
        auto trim = [](std::string v) {
          size_t a = v.find_first_not_of(" \t");
          size_t e = v.find_last_not_of(" \t");
          return a == std::string::npos ? std::string() : v.substr(a, e - a + 1);
        };
        std::string alias = trim(rest.substr(0, comma));
        std::string target = trim(rest.substr(comma + 1));
        if (!alias.empty() && !target.empty() && (std::isalpha(static_cast<unsigned char>(target[0])) || target[0] == '_'))
          symbolAliases.emplace_back(alias, target);
      }
      continue;
    }
    parser = StringTokenParser(inputLine.data(), (int32_t)inputLine.size());

    // some commands have to be translated
    if (parser.matchWordI(".p2align")) {
      uint32_t val;
      if (!parser.parseU32(val))
        fatalError(std::format("Malformed directive '{}'", inputLine));
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
    bool isAscii = parser.matchWordI(".ascii");
    bool isString = !isAscii && (parser.matchWordI(".string") || parser.matchWordI(".asciz"));
    if (isAscii || isString) {
      parser.skipWhitespaces();
      std::vector<uint8_t> bytes;
      decodeGasStringLiterals(parser.getView(), bytes);
      if (isString)
        bytes.push_back(0);
      for (size_t off = 0; off < bytes.size(); off += 64) {
        std::string line = "\t.byte ";
        for (size_t i = off; i < bytes.size() && i < off + 64; i++) {
          if (i > off)
            line.append(",");
          line.append(std::to_string(bytes[i]));
        }
        stringDataLines.insert(outputTextFile.lines.size());
        outputTextFile.lines.emplace_back(std::move(line));
      }
      continue;
    }
    if (parser.matchWordI(".zero") || parser.matchWordI(".space")) {
      uint32_t zeroSize;
      if (!parser.parseU32(zeroSize))
        fatalError(std::format("Malformed directive '{}'", inputLine));
      uint32_t fillVal = 0;
      parser.skipWhitespaces();
      if (parser.compareCharacter(0, ',')) {
        parser.skipCharacters(1);
        if (!parser.parseU32(fillVal))
          fatalError(std::format("Malformed directive '{}'", inputLine));
      }
      if (zeroSize > 0) {
        uint32_t numInts = zeroSize >= 64 ? zeroSize / 4 : 0;
        uint32_t remainder = zeroSize - numInts * 4;
        if (numInts > 0) {
          std::string line = std::format("\t.int {}", fillVal);
          for (uint32_t i = 1; i < numInts; i++)
            line.append(std::format(",{}", fillVal));
          outputTextFile.lines.emplace_back(std::move(line));
        }
        if (remainder > 0) {
          std::string line = std::format("\t.byte {}", fillVal & 0xFF);
          for (uint32_t i = 1; i < remainder; i++)
            line.append(std::format(",{}", fillVal & 0xFF));
          outputTextFile.lines.emplace_back(std::move(line));
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
    for (size_t at = inputLine.find("_IMPORT_"); at != std::string::npos; at = inputLine.find("_IMPORT_", at + 1)) {
      size_t end = at + 8;
      while (end < inputLine.size() && (std::isupper(static_cast<unsigned char>(inputLine[end])) || std::isdigit(static_cast<unsigned char>(inputLine[end]))))
        end++;
      if (end == at + 8 || end >= inputLine.size() || inputLine[end] != '_')
        continue;
      std::string module = inputLine.substr(at + 8, end - at - 8);
      for (char& c : module)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      inputLine.replace(at, end + 1 - at, "import." + module + ".");
    }

    // Keep the final instruction normalization idempotent so instructions that
    // passed through one of the directive/parser paths are normalized as well.
    inputLine = AsmUpdater_fixInstruction(inputLine);

    for (size_t i = 1; i < inputLine.size(); i++) {
      char prev = inputLine[i - 1];
      if (inputLine[i] == '$' && (std::isalnum(static_cast<unsigned char>(prev)) || prev == '_')) {
        inputLine.replace(i, 1, "_S_");
        i += 2;
      }
    }

    // a function that ends in a noreturn call would fall through into its constant pool, which Cemu's function
    // boundary scan then decodes (0x4b000000 is a branch 16 MB back), so end it with an invalid instruction
    if (inputLine.ends_with(':') && inputLine.find("Lfunc_end") != std::string::npos) {
      for (size_t i = outputTextFile.lines.size(); i > firstOutputLine; i--) {
        std::string_view last = normalizeAssemblyLine(outputTextFile.lines[i - 1]);
        if (last.empty() || last.back() == ':')
          continue;
        if (last.starts_with("bl ") || last.starts_with("bl\t") || last.starts_with("bla ") || last.starts_with("bla\t"))
          outputTextFile.lines.emplace_back("\t.int 0");
        break;
      }
    }
    outputTextFile.lines.emplace_back(inputLine);
  }

  if (!stringDataLines.empty()) {
    // a string followed by more data is part of one object (e.g. llvm's merged string pool, addressed as pool+offset),
    // so padding there would shift everything after it
    auto isDataLine = [&](size_t i) {
      if (i >= outputTextFile.lines.size())
        return false;
      std::string_view next = normalizeAssemblyLine(outputTextFile.lines[i]);
      return next.starts_with(".byte") || next.starts_with(".short") || next.starts_with(".int") ||
             next.starts_with(".long") || next.starts_with(".float") || next.starts_with(".double");
    };
    std::vector<std::string> aligned;
    aligned.reserve(outputTextFile.lines.size() + stringDataLines.size());
    for (size_t i = 0; i < outputTextFile.lines.size(); i++) {
      aligned.emplace_back(std::move(outputTextFile.lines[i]));
      if (i >= firstOutputLine && stringDataLines.count(i) && !isDataLine(i + 1))
        aligned.emplace_back("\t.align 4");
    }
    outputTextFile.lines = std::move(aligned);
  }

  for (auto& [alias, target] : symbolAliases) {
    std::string targetLabel = target + ":";
    for (size_t i = firstOutputLine; i < outputTextFile.lines.size(); i++) {
      if (outputTextFile.lines[i] == targetLabel) {
        outputTextFile.lines.insert(outputTextFile.lines.begin() + i, alias + ":");
        break;
      }
    }
  }
}

void AssemblyConverter::analyzeSymbols(const TextFile &inputFile) {
  printf("Starting findLabels for %d lines...\n",
         (int)inputFile.lines.size());
  fflush(stdout);
  m_privateSymbolRenames.clear();
  m_exportedSymbols.clear();

  for (const auto &line : inputFile.lines) {
    std::string_view p = normalizeAssemblyLine(line);
    if (p.empty())
      continue;

    StringTokenParser parser(p);
    if (!parser.matchWordI(".weak") && !parser.matchWordI(".globl")) {
      continue;
    }

    const char *symbolStr;
    int32_t symbolLength;
    if (parser.parseSymbolName(symbolStr, symbolLength)) {
      m_exportedSymbols.emplace(symbolStr, symbolLength);
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

    if (!m_exportedSymbols.contains(std::string(symbol))) {
      registerPrivateSymbol(symbol);
    }
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

void AssemblyConverter::finalize(TextFile &outputTextFile) {
  if (!m_patchDirectives.empty()) {
    outputTextFile.lines.emplace_back("");
    outputTextFile.lines.emplace_back("; --- Patch Directives ---");
    for (const auto &directive : m_patchDirectives) {
      outputTextFile.lines.emplace_back(directive);
    }
    printf("Emitted %d patch directive(s).\n",
           (int)m_patchDirectives.size());
    fflush(stdout);
  }
}
