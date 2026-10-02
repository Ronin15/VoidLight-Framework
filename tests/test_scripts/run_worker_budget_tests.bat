@echo off
:: Script to run WorkerBudget tests
:: Copyright (c) 2025 Hammer Forged Games, MIT License

setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%SCRIPT_DIR%..\.."
set "VERBOSE=false"
set "TEST_FILTER="

:parse_args
if "%~1"=="" goto :done_parsing
if /i "%~1"=="--verbose" set "VERBOSE=true"& shift& goto :parse_args
if /i "%~1:~0,11%"=="--run_test=" set "TEST_FILTER=%~1"& shift& goto :parse_args
if /i "%~1"=="--help" (
    echo WorkerBudget Tests Runner
    echo Usage: run_worker_budget_tests.bat [--verbose] [--run_test=^<name^>] [--help]
    echo.
    echo Options:
    echo   --verbose           Run tests with verbose output
    echo   --run_test=^<name^>   Run a specific test case
    echo   --help              Show this help message
    echo.
    echo Test Coverage:
    echo   WorkerBudgetStateTransitionTests:
    echo     - prepareForStateTransition resets learned threshold and batch multiplier
    echo     - All seven SystemType variants reset independently
    echo   WorkerBudgetAccessorTests:
    echo     - getBatchMultiplier / getExpectedThroughput / getLearnedThreshold
    echo     - markFrameStart per-frame cache invalidation
    echo   WorkerBudgetSystemCoverageTests:
    echo     - Collision, BackgroundSim, ProjectileSim SystemType budgets
    echo     - Batch strategy coverage and independent per-system thresholds
    exit /b 0
)
shift
goto :parse_args

:done_parsing

set "TEST_EXECUTABLE=%PROJECT_DIR%\bin\debug\worker_budget_tests.exe"
if not exist "!TEST_EXECUTABLE!" (
    echo Error: Test executable not found: !TEST_EXECUTABLE!
    echo Make sure you have built the project with tests enabled.
    echo Run: cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug ^&^& ninja -C build
    exit /b 1
)

if not exist "%PROJECT_DIR%\test_results" mkdir "%PROJECT_DIR%\test_results"
set "RESULTS_FILE=%PROJECT_DIR%\test_results\worker_budget_tests_results.txt"

echo ======================================================
echo          WorkerBudget Tests
echo ======================================================

if "!VERBOSE!"=="true" (
    "!TEST_EXECUTABLE!" --log_level=all !TEST_FILTER! 2>&1 | tee "!RESULTS_FILE!"
    set RESULT=!ERRORLEVEL!
) else (
    "!TEST_EXECUTABLE!" --log_level=test_suite !TEST_FILTER! > "!RESULTS_FILE!" 2>&1
    set RESULT=!ERRORLEVEL!
)

echo.
echo ======================================================
if !RESULT! equ 0 (
    echo All WorkerBudget tests passed!
    echo.
    echo Test Coverage:
    echo   [OK] State-transition reset of learned threshold and batch multiplier
    echo   [OK] Accessor correctness ^(batch multiplier, throughput, threshold^)
    echo   [OK] Per-system-type budget coverage and independence
) else (
    echo Some WorkerBudget tests failed
    echo Check the detailed results in: !RESULTS_FILE!
)
echo Results saved to: !RESULTS_FILE!
echo ======================================================

exit /b !RESULT!
