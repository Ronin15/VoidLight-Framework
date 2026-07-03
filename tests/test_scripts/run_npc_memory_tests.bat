@echo off
rem NPC Memory System Test Runner
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
set RUN_ALL=true
set RUN_MEMORY=false
set RUN_AI_INTEGRATION=false

:parse_args
if "%~1"=="" goto done_parsing
if /i "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto parse_args
)
if /i "%~1"=="--memory" (
    set RUN_ALL=false
    set RUN_MEMORY=true
    shift
    goto parse_args
)
if /i "%~1"=="--ai-integration" (
    set RUN_ALL=false
    set RUN_AI_INTEGRATION=true
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo !BLUE!NPC Memory System Test Runner!NC!
    echo Usage: run_npc_memory_tests.bat [options]
    echo.
    echo Options:
    echo   --verbose         Run tests with verbose output
    echo   --memory          Run only NPC memory tests
    echo   --ai-integration  Run only AI manager integration tests
    echo   --help            Show this help message
    exit /b 0
)
shift
goto parse_args

:done_parsing

echo !BLUE!Running NPC Memory System Tests!NC!

rem Track overall result
set OVERALL_RESULT=0

rem Run selected tests
if "%RUN_ALL%"=="true" (
    call :run_single_test npc_memory_tests
    call :run_single_test ai_manager_edm_integration_tests
) else (
    if "%RUN_MEMORY%"=="true" call :run_single_test npc_memory_tests
    if "%RUN_AI_INTEGRATION%"=="true" call :run_single_test ai_manager_edm_integration_tests
)

goto show_summary

:run_single_test
set test_name=%~1

rem Check if test executable exists
set "TEST_EXECUTABLE=%~dp0..\..\bin\debug\%test_name%.exe"
if not exist "!TEST_EXECUTABLE!" (
    set "TEST_EXECUTABLE=%~dp0..\..\bin\debug\%test_name%"
    if not exist "!TEST_EXECUTABLE!" (
        echo !RED!Error: Test executable not found: %test_name%!NC!
        set OVERALL_RESULT=1
        goto :eof
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
echo Running %test_name%...
pushd "%~dp0..\.."
if "%VERBOSE%"=="true" (
    "!TEST_EXECUTABLE!" !TEST_OPTS!
) else (
    "!TEST_EXECUTABLE!" !TEST_OPTS! > "%~dp0..\..\test_results\%test_name%_output.txt" 2>&1
)
set TEST_RESULT=!ERRORLEVEL!
popd

if !TEST_RESULT! neq 0 (
    echo !RED!%test_name% failed with exit code !TEST_RESULT!!NC!
    set OVERALL_RESULT=1
) else (
    echo !GREEN!%test_name% completed successfully!NC!
)

goto :eof

:show_summary
if !OVERALL_RESULT! neq 0 (
    echo.
    echo !RED!Some NPC Memory tests failed!!NC!
    exit /b 1
) else (
    echo.
    echo !GREEN!All NPC Memory tests completed successfully!!NC!
    echo !GREEN!  NPC Memory tests!NC!
    echo !GREEN!  AI Manager integration tests!NC!
    exit /b 0
)
