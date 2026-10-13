@echo off
setlocal enabledelayedexpansion

echo =======================================================================
echo  Building Cemu PatchCompiler (Release)
echo =======================================================================

rem Find VS installation path using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if not exist "!VSWHERE!" (
    echo [ERROR] Visual Studio Installer/vswhere.exe not found!
    echo Please make sure Visual Studio is installed on this computer.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%" == "" (
    echo [ERROR] Could not find any Visual Studio installation with MSBuild.
    exit /b 1
)

echo Found Visual Studio at: %VS_PATH%

rem Find vcvarsall.bat
set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "!VCVARS!" (
    echo [ERROR] vcvarsall.bat not found at !VCVARS!
    exit /b 1
)

echo Initializing Developer Environment...
call "!VCVARS!" amd64

rem Ensure cmake is in the PATH, fallback to VS bundled cmake if not available
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    set "PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
)

rem Ensure ninja is in the PATH, fallback to VS bundled ninja if not available
where ninja >nul 2>nul
if %ERRORLEVEL% neq 0 (
    set "PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
)

rem llvm is built from the vcpkg submodule on the first configure
if not exist "dependencies\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo Fetching the vcpkg submodule...
    git submodule update --init
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Could not fetch the vcpkg submodule!
        exit /b 1
    )
)

if exist "build\CMakeCache.txt" (
    echo Cleaning stale CMake cache...
    del /f /q "build\CMakeCache.txt" >nul 2>&1
)

echo.
echo Configuring CMake...
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo.
echo Building PatchCompiler...
cmake --build build --config Release
set "BUILD_EXIT_CODE=%ERRORLEVEL%"

if %BUILD_EXIT_CODE% neq 0 (
    echo =======================================================================
    echo  [FAIL] Build failed!
    echo =======================================================================
) else (
    echo =======================================================================
    echo  [PASS] Build completed successfully!
    echo  Executable output: build\CemuPatchCompiler-release.exe
    echo =======================================================================
)

exit /b %BUILD_EXIT_CODE%
