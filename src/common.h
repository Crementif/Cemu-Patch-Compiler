#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

namespace fs = std::filesystem;

enum class CompilerMode {
    None,
    Clang,
    GCC
};

inline CompilerMode g_compilerMode = CompilerMode::None;
inline std::string g_gccPath = "";
inline std::string g_clangPath = "";

inline fs::path getExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return fs::path(buffer).parent_path();
}

struct Config {
	std::map<std::string, std::map<std::string, std::string>> sections;

	std::string get(const std::string& section, const std::string& key, const std::string& defaultValue = "") const {
		auto sIt = sections.find(section);
		if (sIt != sections.end()) {
			auto kIt = sIt->second.find(key);
			if (kIt != sIt->second.end()) {
				return kIt->second;
			}
		}
		return defaultValue;
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

				config.sections[currentSection][key] = value;
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
	file << "; Directory containing C/C++ source files to compile (relative to config or absolute)\r\n";
	if (isBinDir) {
		file << "SourceDir = ../examples/camera\r\n\r\n";
	} else {
		file << "SourceDir = examples/camera\r\n\r\n";
	}
	file << "; Output file path for compiled assembly patch\r\n";
	if (isBinDir) {
		file << "OutFile = ../examples/camera/patch_compiled.asm\r\n\r\n";
	} else {
		file << "OutFile = examples/camera/patch_compiled.asm\r\n\r\n";
	}
	file << "[Compilers]\r\n";
	file << "; [Optional] Custom compiler paths (relative to config or absolute)\r\n";
	if (isBinDir) {
		file << ";GCC = ../compilers/GCC/bin/powerpc-eabi-gcc.exe\r\n";
		file << ";Clang = ../compilers/clang/bin/clang++.exe\r\n";
	} else {
		file << ";GCC = compilers/GCC/bin/powerpc-eabi-gcc.exe\r\n";
		file << ";Clang = compilers/clang/bin/clang++.exe\r\n";
	}
}

inline fs::path resolvePath(const fs::path& basePath, const std::string& pathStr) {
	fs::path p(pathStr);
	if (p.is_absolute()) {
		return p;
	}
	return basePath / p;
}

inline void detectCompiler(const fs::path& configDir, const Config& config) {
    fs::path exeDir = getExecutableDir();

    std::string customGcc = config.get("Compilers", "GCC");
    std::string customClang = config.get("Compilers", "Clang");

    if (!customGcc.empty()) {
        fs::path resolvedGcc = resolvePath(configDir, customGcc);
        std::error_code ec;
        if (fs::exists(resolvedGcc, ec) && !fs::is_directory(resolvedGcc, ec)) {
            g_compilerMode = CompilerMode::GCC;
            g_gccPath = resolvedGcc.string();
            return;
        } else {
            printf("Warning: Configured GCC path not found or invalid: %s\n", resolvedGcc.generic_string().c_str());
        }
    }

    if (!customClang.empty()) {
        fs::path resolvedClang = resolvePath(configDir, customClang);
        std::error_code ec;
        if (fs::exists(resolvedClang, ec) && !fs::is_directory(resolvedClang, ec)) {
            g_compilerMode = CompilerMode::Clang;
            g_clangPath = resolvedClang.string();
            return;
        } else {
            printf("Warning: Configured Clang path not found or invalid: %s\n", resolvedClang.generic_string().c_str());
        }
    }

    // Fallback to auto-detection
    fs::path compilersPath = "";
    std::vector<fs::path> searchPaths = {
        "compilers",
        "../compilers",
        exeDir / "compilers",
        exeDir / "../compilers",
        exeDir / "../../compilers"
    };

    for (const auto& path : searchPaths) {
        if (fs::exists(path / "GCC") && fs::exists(path / "clang")) {
            compilersPath = path;
            break;
        }
    }

    if (compilersPath.empty()) {
        if (fs::current_path().filename() == "bin") {
            compilersPath = "../compilers";
        } else if (exeDir.filename() == "bin") {
            compilersPath = exeDir / "../compilers";
        } else {
            compilersPath = "compilers";
        }
    }

    // Prefer GCC
    fs::path gccExePath = compilersPath / "GCC" / "bin" / "powerpc-eabi-gcc.exe";
    if (fs::exists(gccExePath)) {
        g_compilerMode = CompilerMode::GCC;
        g_gccPath = gccExePath.string();
        return;
    }

    // Fallback to Clang
    fs::path clangPath1 = compilersPath / "clang" / "bin" / "clang++.exe";
    fs::path clangPath2 = compilersPath / "clang" / "clang++.exe";
    if (fs::exists(clangPath1)) {
        g_compilerMode = CompilerMode::Clang;
        g_clangPath = clangPath1.string();
        return;
    } else if (fs::exists(clangPath2)) {
        g_compilerMode = CompilerMode::Clang;
        g_clangPath = clangPath2.string();
        return;
    }

    // Final fallback to hardcoded path
    if (!g_clangPath.empty() && fs::exists(g_clangPath)) {
        g_compilerMode = CompilerMode::Clang;
    } else {
        g_compilerMode = CompilerMode::None;
    }
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
