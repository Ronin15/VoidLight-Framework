@echo off
rem AIManager EDM Integration Test Runner
rem Copyright 2025 Hammer Forged Games

setlocal enabledelayedexpansion

rem Enable ANSI escape sequences (Windows 10+)
for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
rem Color codes for Windows
set "RED=%ESC%[91m"
set "GREEN=%ESC%[92m"
set "YELLOW=%ESC%[93m"
set "BLUE=%ESC%[94m"
set "NC=%ESC%[0m"

rem Process command line arguments
set VERBOSE=false

:parse_args
if "%~1"=="" goto done_parsing
if /i "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo !BLUE!AIManager EDM Integration Test Runner!NC!
    echo Usage: run_ai_manager_edm_integration_tests.bat [options]
    echo.
    echo Options:
    echo   --verbose      Run tests with verbose output
    echo   --help         Show this help message
    exit /b 0
)
shift
goto parse_args

:done_parsing

echo !BLUE!Running AIManager EDM Integration Tests!NC!

rem Track overall result
set OVERALL_RESULT=0

rem Check if test executable exists
set "TEST_EXECUTABLE=%~dp0..\..\bin\debug\ai_manager_edm_integration_tests.exe"
if not exist "!TEST_EXECUTABLE!" (
    set "TEST_EXECUTABLE=%~dp0..\..\bin\debug\ai_manager_edm_integration_tests"
    if not exist "!TEST_EXECUTABLE!" (
        echo !RED!Error: Test executable not found: ai_manager_edm_integration_tests!NC!
        echo !YELLOW!Please build the project first with: cmake -B build -G Ninja ^&^& ninja -C build!NC!
        exit /b 1
    )
)

rem Set test options
set TEST_OPTS=--log_level=test_suite --catch_system_errors=no
if "%VERBOSE%"=="true" (
    set TEST_OPTS=--log_level=all --report_level=detailed
)

rem Create test results directory
if not exist "%~dp0..\..\test_results" mkdir "%~dp0..\..\test_results"

rem Run the test from the project root so resource-relative paths resolve correctly
echo Running ai_manager_edm_integration_tests...
pushd "%~dp0..\.."
if "%VERBOSE%"=="true" (
    "!TEST_EXECUTABLE!" !TEST_OPTS!
) else (
    "!TEST_EXECUTABLE!" !TEST_OPTS! > "%~dp0..\..\test_results\ai_manager_edm_integration_tests_output.txt" 2>&1
)
set TEST_RESULT=!ERRORLEVEL!
popd

if !TEST_RESULT! neq 0 (
    echo !RED!ai_manager_edm_integration_tests failed with exit code !TEST_RESULT!!NC!
    set OVERALL_RESULT=1
) else (
    echo !GREEN!ai_manager_edm_integration_tests completed successfully!NC!
)

rem Show summary
if !OVERALL_RESULT! neq 0 (
    echo.
    echo !RED!AIManager EDM Integration tests failed!!NC!
    exit /b 1
) else (
    echo.
    echo !GREEN!All AIManager EDM Integration tests completed successfully!!NC!
    exit /b 0
)
