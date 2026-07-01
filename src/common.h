#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <regex>
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
inline std::string g_clangPath = "P:\\Cemu\\BotW-VR-Project\\ClangToPatch\\llvm\\clang++.exe";

inline fs::path getExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return fs::path(buffer).parent_path();
}

inline void detectCompiler() {
    fs::path exeDir = getExecutableDir();
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
        std::error_code ec;
        fs::create_directories(compilersPath / "GCC", ec);
        fs::create_directories(compilersPath / "clang", ec);
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
