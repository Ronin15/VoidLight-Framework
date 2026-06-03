@echo off
rem Controller Test Runner (all controller tests)
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
set RUN_REGISTRY=false
set RUN_WEATHER=false
set RUN_DAYNIGHT=false
set RUN_HARVEST=false
set RUN_NPCRENDER=false
set RUN_INVENTORY=false
set RUN_COMBAT=false
set RUN_RESOURCERENDER=false
set RUN_SOCIAL=false

:parse_args
if "%~1"=="" goto done_parsing
if /i "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto parse_args
)
if /i "%~1"=="--registry" (
    set RUN_ALL=false
    set RUN_REGISTRY=true
    shift
    goto parse_args
)
if /i "%~1"=="--weather" (
    set RUN_ALL=false
    set RUN_WEATHER=true
    shift
    goto parse_args
)
if /i "%~1"=="--daynight" (
    set RUN_ALL=false
    set RUN_DAYNIGHT=true
    shift
    goto parse_args
)
if /i "%~1"=="--harvest" (
    set RUN_ALL=false
    set RUN_HARVEST=true
    shift
    goto parse_args
)
if /i "%~1"=="--npcrender" (
    set RUN_ALL=false
    set RUN_NPCRENDER=true
    shift
    goto parse_args
)
if /i "%~1"=="--inventory" (
    set RUN_ALL=false
    set RUN_INVENTORY=true
    shift
    goto parse_args
)
if /i "%~1"=="--combat" (
    set RUN_ALL=false
    set RUN_COMBAT=true
    shift
    goto parse_args
)
if /i "%~1"=="--resourcerender" (
    set RUN_ALL=false
    set RUN_RESOURCERENDER=true
    shift
    goto parse_args
)
if /i "%~1"=="--social" (
    set RUN_ALL=false
    set RUN_SOCIAL=true
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo !BLUE!Controller Test Runner!NC!
    echo Usage: run_controller_tests.bat [options]
    echo.
    echo Options:
    echo   --verbose      Run tests with verbose output
    echo   --registry     Run only ControllerRegistry tests
    echo   --weather      Run only WeatherController tests
    echo   --daynight     Run only DayNightController tests
    echo   --harvest      Run only HarvestController tests
    echo   --npcrender    Run only NPCRenderController tests
    echo   --inventory    Run only InventoryController tests
    echo   --combat       Run only CombatController tests
    echo   --resourcerender Run only ResourceRenderController tests
    echo   --social       Run only SocialController tests
    echo   --help         Show this help message
    exit /b 0
)
shift
goto parse_args

:done_parsing

echo !BLUE!Running Controller Tests!NC!

rem Track overall result
set OVERALL_RESULT=0

rem Run selected tests
if "%RUN_ALL%"=="true" (
    call :run_single_test controller_registry_tests
    call :run_single_test weather_controller_tests
    call :run_single_test day_night_controller_tests
    call :run_single_test harvest_controller_tests
    call :run_single_test npc_render_controller_tests
    call :run_single_test inventory_controller_tests
    call :run_single_test combat_controller_tests
    call :run_single_test resource_render_controller_tests
    call :run_single_test social_controller_tests
) else (
    if "%RUN_REGISTRY%"=="true" call :run_single_test controller_registry_tests
    if "%RUN_WEATHER%"=="true" call :run_single_test weather_controller_tests
    if "%RUN_DAYNIGHT%"=="true" call :run_single_test day_night_controller_tests
    if "%RUN_HARVEST%"=="true" call :run_single_test harvest_controller_tests
    if "%RUN_NPCRENDER%"=="true" call :run_single_test npc_render_controller_tests
    if "%RUN_INVENTORY%"=="true" call :run_single_test inventory_controller_tests
    if "%RUN_COMBAT%"=="true" call :run_single_test combat_controller_tests
    if "%RUN_RESOURCERENDER%"=="true" call :run_single_test resource_render_controller_tests
    if "%RUN_SOCIAL%"=="true" call :run_single_test social_controller_tests
)

goto show_summary

:run_single_test
set test_name=%~1

rem Check if test executable exists
set "TEST_EXECUTABLE=..\..\bin\debug\%test_name%.exe"
if not exist "!TEST_EXECUTABLE!" (
    set "TEST_EXECUTABLE=..\..\bin\debug\%test_name%"
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
if not exist "..\..\test_results" mkdir "..\..\test_results"

rem Run the test
echo Running %test_name%...
if "%VERBOSE%"=="true" (
    "!TEST_EXECUTABLE!" !TEST_OPTS!
) else (
    "!TEST_EXECUTABLE!" !TEST_OPTS! > "..\..\test_results\%test_name%_output.txt" 2>&1
)

set TEST_RESULT=!ERRORLEVEL!

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
    echo !RED!Some controller tests failed!!NC!
    exit /b 1
) else (
    echo.
    echo !GREEN!All controller tests completed successfully!!NC!
    echo !GREEN!  ControllerRegistry tests!NC!
    echo !GREEN!  WeatherController tests!NC!
    echo !GREEN!  DayNightController tests!NC!
    echo !GREEN!  HarvestController tests!NC!
    echo !GREEN!  NPCRenderController tests!NC!
    echo !GREEN!  InventoryController tests!NC!
    echo !GREEN!  CombatController tests!NC!
    echo !GREEN!  ResourceRenderController tests!NC!
    echo !GREEN!  SocialController tests!NC!
    exit /b 0
)
