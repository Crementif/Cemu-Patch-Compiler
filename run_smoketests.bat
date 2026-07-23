@echo off
setlocal enabledelayedexpansion

echo =======================================================================
echo  Running Cemu PatchCompiler Smoke Tests
echo =======================================================================

rem Find the directory of this batch file
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

rem Run the PowerShell test runner script
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%testing\run_homebrew_test.ps1"
set "TEST_EXIT_CODE=%ERRORLEVEL%"

if %TEST_EXIT_CODE% neq 0 (
    echo =======================================================================
    echo  [FAIL] Smoke tests failed!
    echo =======================================================================
) else (
    echo =======================================================================
    echo  [PASS] All smoke tests completed successfully!
    echo =======================================================================
)

exit /b %TEST_EXIT_CODE%
