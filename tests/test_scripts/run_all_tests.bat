@echo off
:: Script to run all test shell scripts sequentially
setlocal EnableDelayedExpansion

:: Enable ANSI escape sequences (Windows 10+)
for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
set "RED=%ESC%[91m"
set "GREEN=%ESC%[92m"
set "YELLOW=%ESC%[93m"
set "BLUE=%ESC%[94m"
set "MAGENTA=%ESC%[95m"
set "CYAN=%ESC%[96m"
set "NC=%ESC%[0m"

:: Directory where all scripts are located
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%" 2>nul

:: Process command line arguments
set VERBOSE=false
set ERRORS_ONLY=false
set RUN_CORE=true
set RUN_BENCHMARKS=true

:parse_args
if "%~1"=="" goto :done_parsing
if /i "%~1"=="--verbose" set VERBOSE=true& shift& goto :parse_args
if /i "%~1"=="--errors-only" set ERRORS_ONLY=true& shift& goto :parse_args
if /i "%~1"=="--core-only" set RUN_CORE=true& set RUN_BENCHMARKS=false& shift& goto :parse_args
if /i "%~1"=="--benchmarks-only" set RUN_CORE=false& set RUN_BENCHMARKS=true& shift& goto :parse_args
if /i "%~1"=="--no-benchmarks" set RUN_CORE=true& set RUN_BENCHMARKS=false& shift& goto :parse_args
if /i "%~1"=="--help" (
    echo Usage: run_all_tests.bat [options]
    echo   --verbose         Run tests with verbose output
    echo   --errors-only     Filter output to show only warnings and errors
    echo   --core-only       Run only core functionality tests
    echo   --benchmarks-only Run only performance benchmarks
    echo   --no-benchmarks   Run core tests but skip benchmarks
    echo   --help            Show this help message
    echo.
    echo Benchmarks include AI, Event, UI, ParticleManager, Collision, Pathfinder,
    echo SIMD, GPU frame timing, Integrated system, and adaptive threading analysis.
    exit /b 0
)
shift
goto :parse_args

:done_parsing

:: Setup results
set "RESULTS_DIR=%SCRIPT_DIR%..\..\test_results\combined"
if not exist "!RESULTS_DIR!" mkdir "!RESULTS_DIR!"
set "COMBINED_RESULTS=!RESULTS_DIR!\all_tests_results.txt"
echo All Tests Run %date% %time%> "!COMBINED_RESULTS!"

set PASSED_COUNT=0
set FAILED_COUNT=0

echo !BLUE!======================================================!NC!
echo !BLUE!              Running Test Scripts                    !NC!
echo !BLUE!======================================================!NC!

:: Run core tests
if not "!RUN_CORE!"=="true" goto :skip_core

echo.
echo !MAGENTA!Starting core functionality tests...!NC!

for %%T in (
    run_thread_tests.bat
    run_buffer_utilization_tests.bat
    run_thread_safe_ai_tests.bat
    run_thread_safe_ai_integration_tests.bat
    run_ai_optimization_tests.bat
    run_behavior_functionality_tests.bat
    run_save_tests.bat
    run_settings_tests.bat
    run_game_state_manager_tests.bat
    run_event_tests.bat
    run_weather_event_tests.bat
    run_game_time_tests.bat
    run_controller_tests.bat
    run_particle_manager_tests.bat
    run_json_reader_tests.bat
    run_resource_tests.bat
    run_resource_edge_case_tests.bat
    run_world_generator_tests.bat
    run_world_manager_event_integration_tests.bat
    run_world_manager_tests.bat
    run_world_resource_manager_tests.bat
    run_collision_tests.bat
    run_pathfinding_tests.bat
    run_collision_pathfinding_integration_tests.bat
    run_pathfinder_ai_contention_tests.bat
    run_camera_tests.bat
    run_input_manager_tests.bat
    run_simd_correctness_tests.bat
    run_buffer_reuse_tests.bat
    run_rendering_pipeline_tests.bat
    run_loading_state_tests.bat
    run_ui_manager_functional_tests.bat
    run_ai_collision_integration_tests.bat
    run_event_coordination_integration_tests.bat
    run_entity_state_manager_tests.bat
    run_entity_data_manager_tests.bat
    run_ai_manager_edm_integration_tests.bat
    run_sidecar_tests.bat
    run_collision_manager_edm_integration_tests.bat
    run_pathfinder_manager_edm_integration_tests.bat
    run_pathfinder_manager_tests.bat
    run_projectile_manager_tests.bat
    run_npc_memory_tests.bat
    run_gpu_tests.bat
    run_crowd_runtime_tests.bat
    run_manager_runtime_tests.bat
    run_frame_profiler_tests.bat
) do (
    echo.
    echo Running test: %%T

    if not exist "%%T" (
        echo ERROR: Script not found: %%T
        set /a FAILED_COUNT+=1
        echo FAILED: %%T>> "!COMBINED_RESULTS!"
    ) else (
        set "test_args="
        if "!VERBOSE!"=="true" set "test_args=--verbose"

        if "!ERRORS_ONLY!"=="true" (
            cmd /c "!SCRIPT_DIR!%%T" !test_args! >nul 2>&1
        ) else (
            cmd /c "!SCRIPT_DIR!%%T" !test_args!
        )

        if !ERRORLEVEL! equ 0 (
            echo PASSED: %%T
            set /a PASSED_COUNT+=1
            echo PASSED: %%T>> "!COMBINED_RESULTS!"
        ) else (
            echo FAILED: %%T
            set /a FAILED_COUNT+=1
            echo FAILED: %%T>> "!COMBINED_RESULTS!"
        )
    )
)

:skip_core

:: Run benchmarks
if not "!RUN_BENCHMARKS!"=="true" goto :skip_benchmarks

echo.
echo !MAGENTA!Starting performance benchmarks...!NC!

for %%T in (
    run_event_scaling_benchmark.bat
    run_ai_benchmark.bat
    run_particle_manager_benchmark.bat
    run_collision_scaling_benchmark.bat
    run_pathfinder_benchmark.bat
    run_simd_benchmark.bat
    run_gpu_frame_benchmark.bat
    run_integrated_benchmark.bat
    run_background_simulation_manager_benchmark.bat
    run_adaptive_threading_analysis.bat
    run_projectile_benchmark.bat
) do (
    echo.
    echo Running benchmark: %%T

    if not exist "%%T" (
        echo ERROR: Script not found: %%T
        set /a FAILED_COUNT+=1
        echo FAILED: %%T>> "!COMBINED_RESULTS!"
    ) else (
        set "test_args="
        if "!VERBOSE!"=="true" set "test_args=--verbose"

        if "!ERRORS_ONLY!"=="true" (
            cmd /c "!SCRIPT_DIR!%%T" !test_args! >nul 2>&1
        ) else (
            cmd /c "!SCRIPT_DIR!%%T" !test_args!
        )

        if !ERRORLEVEL! equ 0 (
            echo PASSED: %%T
            set /a PASSED_COUNT+=1
            echo PASSED: %%T>> "!COMBINED_RESULTS!"
        ) else (
            echo FAILED: %%T
            set /a FAILED_COUNT+=1
            echo FAILED: %%T>> "!COMBINED_RESULTS!"
        )
    )
)

:skip_benchmarks

:: Summary
echo.
echo !BLUE!======================================================!NC!
echo !BLUE!                  Test Summary                       !NC!
echo !BLUE!======================================================!NC!
echo !GREEN!Passed: !PASSED_COUNT!!NC!
echo !RED!Failed: !FAILED_COUNT!!NC!

echo.>> "!COMBINED_RESULTS!"
echo Summary: Passed=!PASSED_COUNT! Failed=!FAILED_COUNT!>> "!COMBINED_RESULTS!"

if !FAILED_COUNT! gtr 0 (
    echo.
    echo !RED!Some tests failed.!NC!
    exit /b 1
)

echo.
echo !GREEN!All tests passed!!NC!
exit /b 0
