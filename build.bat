@echo off
setlocal enabledelayedexpansion

set "ORIG_VCPKG_ROOT=%VCPKG_ROOT%"

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

rem Find vcpkg toolchain file
set "VCPKG_TOOLCHAIN="

if not "%ORIG_VCPKG_ROOT%" == "" (
    set "VCPKG_TOOLCHAIN=%ORIG_VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
)

if "!VCPKG_TOOLCHAIN!" == "" (
    rem Check common locations first
    if exist "C:\Programs\vcpkg\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_TOOLCHAIN=C:\Programs\vcpkg\scripts\buildsystems\vcpkg.cmake"
    ) else if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_TOOLCHAIN=C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
    ) else if exist "C:\src\vcpkg\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_TOOLCHAIN=C:\src\vcpkg\scripts\buildsystems\vcpkg.cmake"
    )
)

if "!VCPKG_TOOLCHAIN!" == "" (
    rem Fall back to searching PATH (might find VS bundled one)
    for /f "usebackq tokens=*" %%i in (`where vcpkg 2^>nul`) do (
        set "VCPKG_EXE_PATH=%%i"
    )
    if not "!VCPKG_EXE_PATH!" == "" (
        for %%j in ("!VCPKG_EXE_PATH!") do set "VCPKG_DIR=%%~dpj"
        set "VCPKG_TOOLCHAIN=!VCPKG_DIR!scripts\buildsystems\vcpkg.cmake"
    )
)

if "!VCPKG_TOOLCHAIN!" == "" (
    echo [ERROR] Could not locate vcpkg toolchain!
    echo Please set the VCPKG_ROOT environment variable to your vcpkg folder.
    exit /b 1
)

echo Found vcpkg toolchain: %VCPKG_TOOLCHAIN%

if exist "build\CMakeCache.txt" (
    echo Cleaning stale CMake cache...
    del /f /q "build\CMakeCache.txt" >nul 2>&1
)

echo.
echo Configuring CMake...
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"
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
