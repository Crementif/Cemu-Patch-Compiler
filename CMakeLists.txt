cmake_minimum_required(VERSION 3.22)
project(CemuPatchCompiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force MSVC to link statically with the runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Find LLVM and Clang packages
find_package(Clang CONFIG REQUIRED)
find_package(LLVM CONFIG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/AssemblyParser.cpp
    src/AssemblyConverter.cpp
    src/CompilerDriver.cpp
    src/AsmUpdater.cpp
    src/InstructionAssembler.cpp
    src/AssemblyParser.h
    src/AssemblyConverter.h
    src/CompilerDriver.h
    src/AsmUpdater.h
    src/common.h
    src/InstructionAssembler.h
    src/StringParser.h
)

# Header search paths
include_directories(
    src
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

add_definitions(${LLVM_DEFINITIONS})

# Add executable target
add_executable(CemuPatchCompiler ${SOURCES})

# Map required LLVM components to library names
llvm_map_components_to_libnames(llvm_libs
    Core
    MC
    MCParser
    Support
    TargetParser
    PowerPCCodeGen
    PowerPCAsmParser
    PowerPCDesc
    PowerPCInfo
)

# Specify Clang libraries
set(clang_libs
    clangFrontend
    clangCodeGen
    clangSerialization
    clangDriver
    clangParse
    clangSema
    clangAnalysis
    clangEdit
    clangAST
    clangLex
    clangBasic
    clangSupport
)

# Link everything
target_link_libraries(CemuPatchCompiler PRIVATE
    ${clang_libs}
    ${llvm_libs}
    version
)

# Match target output names from MSBuild project
set_target_properties(CemuPatchCompiler PROPERTIES
    OUTPUT_NAME_DEBUG "CemuPatchCompiler-debug"
    OUTPUT_NAME_RELEASE "CemuPatchCompiler-release"
)

# Use common.h as precompiled header
target_precompile_headers(CemuPatchCompiler PRIVATE src/common.h)

# Also disable MSVC security warnings if needed
target_compile_definitions(CemuPatchCompiler PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)

# Add automatic testing integration
enable_testing()
add_test(
    NAME run_homebrew
    COMMAND powershell.exe -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/test_compile_examples.ps1"
)
