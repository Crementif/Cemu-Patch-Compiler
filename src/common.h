#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <intrin.h>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

namespace fs = std::filesystem;

inline fs::path getExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return fs::path(buffer).parent_path();
}

struct Config {
	std::map<std::string, std::vector<std::pair<std::string, std::string>>> sectionEntries;

	std::string get(const std::string& section, const std::string& key, const std::string& defaultValue = "") const {
		auto sIt = sectionEntries.find(section);
		if (sIt != sectionEntries.end()) {
			for (const auto& [k, v] : sIt->second) {
				if (k == key) {
					return v;
				}
			}
		}
		return defaultValue;
	}

	std::vector<std::pair<std::string, std::string>> getEntries(const std::string& section) const {
		auto sIt = sectionEntries.find(section);
		if (sIt != sectionEntries.end()) {
			return sIt->second;
		}
		return {};
	}
};

inline Config parseIniFile(const fs::path& path) {
	Config config;
	std::ifstream file(path.string(), std::ios::binary);
	if (!file.is_open()) return config;

	std::string line;
	std::string currentSection = "";
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(line.begin());
		while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();

		if (line.empty() || line[0] == ';' || line[0] == '#') continue;

		if (line.front() == '[' && line.back() == ']') {
			currentSection = line.substr(1, line.size() - 2);
			while (!currentSection.empty() && (currentSection.front() == ' ' || currentSection.front() == '\t')) currentSection.erase(currentSection.begin());
			while (!currentSection.empty() && (currentSection.back() == ' ' || currentSection.back() == '\t')) currentSection.pop_back();
		} else {
			size_t eqPos = line.find('=');
			if (eqPos != std::string::npos) {
				std::string key = line.substr(0, eqPos);
				std::string value = line.substr(eqPos + 1);

				while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
				while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
				while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
				while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();

				config.sectionEntries[currentSection].push_back({key, value});
			}
		}
	}
	return config;
}

inline void writeDefaultIniFile(const fs::path& path) {
	std::ofstream file(path.string(), std::ios::binary);
	if (!file.is_open()) return;

	bool isBinDir = (path.parent_path().filename() == "bin");

	file << "[Paths]\r\n";
	file << "; Format: sourceFolder = OutputFolder (or OutputFile)\r\n";
	if (isBinDir) {
		file << "../examples/camera = ../examples/camera/patch_compiled.asm\r\n\r\n";
	} else {
		file << "examples/camera = examples/camera/patch_compiled.asm\r\n\r\n";
	}

	file << "[Settings]\r\n";
	file << "; Stop-gap solution: Comma-separated list of instructions to convert to .int when Cemu's assembler lacks support\r\n";
	file << "; hardcodeInstructions = SUBF, MULLW\r\n";
}

inline fs::path resolvePath(const fs::path& basePath, const std::string& pathStr) {
	fs::path p(pathStr);
	if (p.is_absolute()) {
		return p;
	}
	return basePath / p;
}


#ifdef PUBLIC_RELEASE
inline void cemu_assert_debug(bool condition) {
}
#else
inline void cemu_assert_debug(bool condition) {
    if (!condition)
        __debugbreak();
}
#endif
