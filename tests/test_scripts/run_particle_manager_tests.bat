@echo off
setlocal enabledelayedexpansion

REM Script to run all Particle Manager tests
REM Copyright (c) 2025 Hammer Forged Games, MIT License

REM Process command line arguments
set VERBOSE=false
set BUILD_TYPE=Debug
set RUN_ALL=true
set RUN_CORE=false
set RUN_WEATHER=false
set RUN_PERFORMANCE=false
set RUN_THREADING=false

:parse_args
if "%~1"=="" goto :done_parsing
if "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto :parse_args
)
if "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if "%~1"=="--release" (
    set BUILD_TYPE=Release
    shift
    goto :parse_args
)
if "%~1"=="--core" (
    set RUN_ALL=false
    set RUN_CORE=true
    shift
    goto :parse_args
)
if "%~1"=="--weather" (
    set RUN_ALL=false
    set RUN_WEATHER=true
    shift
    goto :parse_args
)
if "%~1"=="--performance" (
    set RUN_ALL=false
    set RUN_PERFORMANCE=true
    shift
    goto :parse_args
)
if "%~1"=="--threading" (
    set RUN_ALL=false
    set RUN_THREADING=true
    shift
    goto :parse_args
)
if "%~1"=="--help" (
    echo Particle Manager Test Runner
    echo Usage: run_particle_manager_tests.bat [options]
    echo.
    echo Options:
    echo   --verbose      Run tests with verbose output
    echo   --debug        Use debug build (default)
    echo   --release      Use release build
    echo   --core         Run only core functionality tests
    echo   --weather      Run only weather integration tests
    echo   --performance  Run only performance tests
    echo   --threading    Run only threading tests
    echo   --help         Show this help message
    echo.
    echo Test Suites:
    echo   Core Tests:        Basic ParticleManager functionality (14 tests)
    echo   Weather Tests:     Weather integration and effects (9 tests)
    echo   Performance Tests: Performance benchmarks and scaling (8 tests) - requires explicit --performance flag
    echo   Threading Tests:   Multi-threading safety (7 tests)
    echo.
    echo Execution Time:
    echo   Core tests:        ~30 seconds
    echo   Weather tests:     ~45 seconds
    echo   Performance tests: ~2-3 minutes
    echo   Threading tests:   ~1-2 minutes
    echo   All tests:         ~2-3 minutes ^(core+weather+threading, excludes performance^)
    echo.
    echo Examples:
    echo   run_particle_manager_tests.bat              # Run core, weather, and threading tests
    echo   run_particle_manager_tests.bat --core       # Quick core validation
    echo   run_particle_manager_tests.bat --weather    # Weather functionality only
    echo   run_particle_manager_tests.bat --performance # Performance benchmarks only
    echo   run_particle_manager_tests.bat --verbose    # All tests with detailed output
    goto :eof
)
shift
goto :parse_args

:done_parsing

echo ======================================================
echo            Particle Manager Test Runner             
echo ======================================================

REM Get project root - calculate relative to script location
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\..\"

REM Define test executables based on what to run
REM Note: Performance tests are excluded from RUN_ALL by default (use --performance explicitly)
set TEST_EXECUTABLES=
if "%RUN_ALL%"=="true" (
    set TEST_EXECUTABLES=particle_manager_core_tests particle_manager_weather_tests particle_manager_threading_tests
) else (
    if "%RUN_CORE%"=="true" set TEST_EXECUTABLES=!TEST_EXECUTABLES! particle_manager_core_tests
    if "%RUN_WEATHER%"=="true" set TEST_EXECUTABLES=!TEST_EXECUTABLES! particle_manager_weather_tests
    if "%RUN_THREADING%"=="true" set TEST_EXECUTABLES=!TEST_EXECUTABLES! particle_manager_threading_tests
)
REM Performance tests require explicit flag (not included in --all by default)
if "%RUN_PERFORMANCE%"=="true" set TEST_EXECUTABLES=!TEST_EXECUTABLES! particle_manager_performance_tests

REM Show execution plan
if "%RUN_ALL%"=="true" (
    echo Execution Plan: Core Particle Manager tests
    echo Includes: Core, Weather, Threading tests ^(excludes Performance benchmarks^)
    echo For performance benchmarks, use: run_particle_manager_benchmark.bat
) else if "%RUN_CORE%"=="true" (
    echo Execution Plan: Core functionality tests only
    echo Fast execution mode - basic ParticleManager validation
) else if "%RUN_WEATHER%"=="true" (
    echo Execution Plan: Weather integration tests only
    echo Testing weather effects and particle integration
) else if "%RUN_PERFORMANCE%"=="true" (
    echo Execution Plan: Performance tests only
    echo Note: This will take 2-3 minutes to complete
    echo Consider using: run_particle_manager_benchmark.bat
) else if "%RUN_THREADING%"=="true" (
    echo Execution Plan: Threading tests only
    echo Testing multi-threading safety and concurrency
)

echo Build type: %BUILD_TYPE%

REM Track overall success
set OVERALL_SUCCESS=true
set PASSED_COUNT=0
set FAILED_COUNT=0
set TOTAL_COUNT=0

REM Count total executables
for %%e in (%TEST_EXECUTABLES%) do (
    set /a TOTAL_COUNT+=1
)

REM Create test results directory
if not exist "%PROJECT_ROOT%\test_results\particle_manager" (
    mkdir "%PROJECT_ROOT%\test_results\particle_manager"
)

set COMBINED_RESULTS=%PROJECT_ROOT%\test_results\particle_manager\all_particle_tests_results.txt
echo Particle Manager Tests Run %date% %time% > "%COMBINED_RESULTS%"

REM Run each test suite
for %%e in (%TEST_EXECUTABLES%) do (
    call :run_particle_test "%%e"
    
    REM Add delay between test suites for resource cleanup
    echo Allowing time for resource cleanup...
    ping -n 3 127.0.0.1 >nul
)

REM Print summary
echo.
echo ======================================================
echo             Particle Manager Test Summary            
echo ======================================================
echo Total test suites: %TOTAL_COUNT%
echo Passed: %PASSED_COUNT%
echo Failed: %FAILED_COUNT%

REM Save summary to results file
echo. >> "%COMBINED_RESULTS%"
echo Summary: >> "%COMBINED_RESULTS%"
echo Total: %TOTAL_COUNT% >> "%COMBINED_RESULTS%"
echo Passed: %PASSED_COUNT% >> "%COMBINED_RESULTS%"
echo Failed: %FAILED_COUNT% >> "%COMBINED_RESULTS%"
echo Completed at: %date% %time% >> "%COMBINED_RESULTS%"

REM Exit with appropriate status code and summary
if "%OVERALL_SUCCESS%"=="true" (
    if "%RUN_ALL%"=="true" (
        echo.
        echo Particle Manager core tests completed successfully!
        echo ✓ Core functionality: Verified
        echo ✓ Weather integration: Verified
        echo ✓ Threading safety: Verified
        echo To run performance benchmarks: run_particle_manager_benchmark.bat
    ) else if "%RUN_CORE%"=="true" (
        echo.
        echo Core Particle Manager tests completed successfully!
        echo To run weather tests: run_particle_manager_tests.bat --weather
    ) else if "%RUN_WEATHER%"=="true" (
        echo.
        echo Weather integration tests completed successfully!
        echo To run performance benchmarks: run_particle_manager_benchmark.bat
    ) else if "%RUN_PERFORMANCE%"=="true" (
        echo.
        echo Performance benchmarks completed successfully!
        echo Consider using run_particle_manager_benchmark.bat for dedicated benchmarks
    ) else if "%RUN_THREADING%"=="true" (
        echo.
        echo Threading safety tests completed successfully!
        echo To run all tests: run_particle_manager_tests.bat
    )
    echo Test results saved to: test_results\particle_manager\
    exit /b 0
) else (
    echo.
    echo Some Particle Manager tests failed!
    echo Please check the individual test results in test_results\particle_manager\
    echo Combined results saved to: %COMBINED_RESULTS%
    exit /b 1
)

:run_particle_test
set exec_name=%~1
set test_type=

REM Determine test type for better messaging
if "%exec_name%"=="particle_manager_core_tests" set test_type=Core Functionality
if "%exec_name%"=="particle_manager_weather_tests" set test_type=Weather Integration
if "%exec_name%"=="particle_manager_performance_tests" set test_type=Performance Benchmarks
if "%exec_name%"=="particle_manager_threading_tests" set test_type=Threading Safety

echo.
echo =====================================================
echo Running Particle Manager %test_type% Tests
echo Test Suite: %exec_name%
echo =====================================================

REM Determine the correct path to the test executable
if "%BUILD_TYPE%"=="Debug" (
    set TEST_EXECUTABLE=%PROJECT_ROOT%\bin\debug\%exec_name%.exe
) else (
    set TEST_EXECUTABLE=%PROJECT_ROOT%\bin\release\%exec_name%.exe
)

REM Check if test executable exists
if not exist "%TEST_EXECUTABLE%" (
    echo Test executable not found at %TEST_EXECUTABLE%
    echo FAILED: %exec_name% - executable not found >> "%COMBINED_RESULTS%"
    set OVERALL_SUCCESS=false
    set /a FAILED_COUNT+=1
    goto :eof
)

REM Set test command options
set TEST_OPTS=--log_level=test_suite --catch_system_errors=no
if "%VERBOSE%"=="true" (
    set TEST_OPTS=--log_level=all --report_level=detailed
)

REM Create output file
set OUTPUT_FILE=%PROJECT_ROOT%\test_results\particle_manager\%exec_name%_output.txt

echo Running with options: %TEST_OPTS%

REM Run the tests
"%TEST_EXECUTABLE%" %TEST_OPTS% > "%OUTPUT_FILE%" 2>&1
set test_result=%ERRORLEVEL%

REM Display test output
type "%OUTPUT_FILE%"

echo ====================================

REM Save results with timestamp
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set mydate=%%c%%a%%b)
for /f "tokens=1-2 delims=/:" %%a in ('time /t') do (set mytime=%%a%%b)
set TIMESTAMP=%mydate%_%mytime%

echo Saving test results for %exec_name%...
copy "%OUTPUT_FILE%" "%PROJECT_ROOT%\test_results\particle_manager\%exec_name%_output_%TIMESTAMP%.txt"
if %ERRORLEVEL% neq 0 (
    echo WARNING: Failed to copy test output to timestamped file
)

REM Extract test summary
echo Extracting test summary...
findstr /i "TestCase Running.*test.*cases failures.*detected No.*errors.*detected" "%OUTPUT_FILE%" > "%PROJECT_ROOT%\test_results\particle_manager\%exec_name%_summary.txt"
if %ERRORLEVEL% neq 0 (
    echo WARNING: No test summary found in test output
)
REM Check test results
findstr /i "failure test.*cases.*failed errors.*detected.*[1-9]" "%OUTPUT_FILE%" >nul
set failure_found=%ERRORLEVEL%

if %test_result%==0 (
    if %failure_found% neq 0 (
        echo.
        echo ✓ %test_type% tests completed successfully
        REM Extract test count information
        for /f "tokens=2" %%a in ('findstr /i "Running.*test.*cases" "%OUTPUT_FILE%" 2^>nul') do (
            echo ✓ All %%a test cases passed
            goto :test_success
        )
        :test_success
        echo PASSED: %exec_name% >> "%COMBINED_RESULTS%"
        set /a PASSED_COUNT+=1
        goto :eof
    )
)

REM If we reach here, the test failed
echo.
echo ✗ %test_type% tests failed
REM Show failure summary
echo.
echo Failure Summary:
findstr /i "failure FAILED error.*in.*:" "%OUTPUT_FILE%" | findstr /n "^" | findstr "^[1-5]:"
if %ERRORLEVEL% neq 0 (
    echo No specific failure details found in output
    echo This may indicate a runtime crash or early termination
)
echo FAILED: %exec_name% (exit code: %test_result%) >> "%COMBINED_RESULTS%"
set OVERALL_SUCCESS=false
set /a FAILED_COUNT+=1

goto :eof
