@echo off
REM Script to run sidecar storage and integration tests
REM Copyright (c) 2025 Hammer Forged Games, MIT License

setlocal EnableDelayedExpansion

REM Enable ANSI escape sequences (Windows 10+)
for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
set "GREEN=%ESC%[92m"
set "YELLOW=%ESC%[93m"
set "RED=%ESC%[91m"
set "NC=%ESC%[0m"

set "BUILD_TYPE=Debug"
set "VERBOSE=false"

:parse_args
if "%~1"=="" goto :done_parsing
if /i "%~1"=="--debug" (
    set "BUILD_TYPE=Debug"
    shift
    goto :parse_args
)
if /i "%~1"=="--release" (
    set "BUILD_TYPE=Release"
    shift
    goto :parse_args
)
if /i "%~1"=="--verbose" (
    set "VERBOSE=true"
    shift
    goto :parse_args
)
if /i "%~1"=="--help" (
    echo Usage: %~nx0 [--debug] [--release] [--verbose] [--help]
    exit /b 0
)
echo Unknown option: %~1
exit /b 1

:done_parsing

REM Navigate to script directory, then resolve project root
cd /d "%~dp0"
set "SCRIPT_DIR=%cd%"
cd /d "%SCRIPT_DIR%\..\.."
set "PROJECT_ROOT=%cd%"

if "%BUILD_TYPE%"=="Debug" (
    set "BIN_DIR=%PROJECT_ROOT%\bin\debug"
) else (
    set "BIN_DIR=%PROJECT_ROOT%\bin\release"
)

if not exist "%PROJECT_ROOT%\test_results" mkdir "%PROJECT_ROOT%\test_results"

if "%VERBOSE%"=="true" (
    set "TEST_OPTS=--catch_system_errors=no --log_level=all --report_level=detailed"
) else (
    set "TEST_OPTS=--catch_system_errors=no --report_level=short"
)

set "FINAL_RESULT=0"
echo !YELLOW!Running sidecar tests...!NC!

for %%E in (sparse_sidecar_tests knockback_sidecar_tests) do (
    set "TEST_EXECUTABLE=%BIN_DIR%\%%E.exe"
    set "OUTPUT_FILE=%PROJECT_ROOT%\test_results\%%E_output.txt"

    if not exist "!TEST_EXECUTABLE!" (
        echo !RED!Error: Test executable not found at '!TEST_EXECUTABLE!'!NC!
        exit /b 1
    )

    "!TEST_EXECUTABLE!" !TEST_OPTS! > "!OUTPUT_FILE!" 2>&1
    set "TEST_RESULT=!ERRORLEVEL!"
    type "!OUTPUT_FILE!"

    set "TEST_FAILED=false"
    if !TEST_RESULT! neq 0 set "TEST_FAILED=true"
    findstr /r /c:"failure" /c:"test cases failed" /c:"fatal error" "!OUTPUT_FILE!" >nul 2>&1
    if !ERRORLEVEL! equ 0 set "TEST_FAILED=true"

    if "!TEST_FAILED!"=="true" (
        echo !RED!Some tests failed in %%E! See !OUTPUT_FILE! for details.!NC!
        set "FINAL_RESULT=1"
    ) else (
        echo !GREEN!%%E passed!!NC!
    )
)

exit /b !FINAL_RESULT!
