@echo off
REM Simple focused cppcheck analysis for VoidLight-Framework (Windows)
REM This script runs cppcheck with optimized settings to show only real issues

setlocal enabledelayedexpansion

echo === VoidLight-Framework - Focused Cppcheck Analysis ===
echo.

REM Check if cppcheck is available
cppcheck --version >nul 2>&1
if errorlevel 1 (
    echo Error: cppcheck not found in PATH
    echo Please install cppcheck and ensure it's in your PATH
    echo.
    echo Download from: https://cppcheck.sourceforge.io/
    exit /b 1
)
REM Verify configuration files exist
if not exist "cppcheck_lib.cfg" (
    echo Error: cppcheck_lib.cfg not found
    echo Please ensure you're running this from the tests\cppcheck directory
    exit /b 1
)
if not exist "cppcheck_suppressions.txt" (
    echo Error: cppcheck_suppressions.txt not found
    echo Please ensure you're running this from the tests\cppcheck directory
    exit /b 1
)

echo Running focused analysis ^(errors, warnings, performance issues only^)...
echo.

REM Create temp file for output
set "TEMP_OUTPUT=%TEMP%\cppcheck_output_%RANDOM%.txt"

REM Run focused analysis - only real issues
cppcheck ^
    --enable=warning,style,performance,portability ^
    --library=std,posix ^
    --library=cppcheck_lib.cfg ^
    --suppressions-list=cppcheck_suppressions.txt ^
    -I..\..\include ^
    -I..\..\src ^
    --platform=win64 ^
    --std=c++20 ^
    --quiet ^
    --template="{file}:{line}: [{severity}] {message}" ^
    ..\..\src\ ..\..\include\ 2>"%TEMP_OUTPUT%"

REM Display output and count issues
type "%TEMP_OUTPUT%"

REM Count issues by severity
set ERROR_COUNT=0
set WARNING_COUNT=0
set STYLE_COUNT=0
set PERFORMANCE_COUNT=0
set PORTABILITY_COUNT=0

REM Count each type by finding matches and using find /c
for /f %%i in ('findstr "[error]" "%TEMP_OUTPUT%" 2^>nul ^| find /c "[error]"') do set ERROR_COUNT=%%i
for /f %%i in ('findstr "[warning]" "%TEMP_OUTPUT%" 2^>nul ^| find /c "[warning]"') do set WARNING_COUNT=%%i
for /f %%i in ('findstr "[style]" "%TEMP_OUTPUT%" 2^>nul ^| find /c "[style]"') do set STYLE_COUNT=%%i
for /f %%i in ('findstr "[performance]" "%TEMP_OUTPUT%" 2^>nul ^| find /c "[performance]"') do set PERFORMANCE_COUNT=%%i
for /f %%i in ('findstr "[portability]" "%TEMP_OUTPUT%" 2^>nul ^| find /c "[portability]"') do set PORTABILITY_COUNT=%%i

set /a TOTAL_COUNT=ERROR_COUNT+WARNING_COUNT+STYLE_COUNT+PERFORMANCE_COUNT+PORTABILITY_COUNT

REM Clean up temp file
del "%TEMP_OUTPUT%" 2>nul

echo.
echo Analysis complete!
echo.

REM Dynamic summary based on actual results
if !TOTAL_COUNT! equ 0 (
    echo ✓ No issues found!
    echo Status: cppcheck reported no warnings, errors, or performance issues
) else (
    echo Found !TOTAL_COUNT! issues:
    if !ERROR_COUNT! gtr 0 (echo   Errors: !ERROR_COUNT!)
    if !WARNING_COUNT! gtr 0 (echo   Warnings: !WARNING_COUNT!)
    if !STYLE_COUNT! gtr 0 (echo   Style: !STYLE_COUNT!)
    if !PERFORMANCE_COUNT! gtr 0 (echo   Performance: !PERFORMANCE_COUNT!)
    if !PORTABILITY_COUNT! gtr 0 (echo   Portability: !PORTABILITY_COUNT!)
    echo.
    set /a CRITICAL_COUNT=ERROR_COUNT+WARNING_COUNT
    if !CRITICAL_COUNT! equ 0 (
        echo Good news: No critical errors or warnings!
        echo Only style/performance suggestions remain
    ) else (
        echo Priority: Address errors and warnings first
    )
)

echo.
echo Note: This configuration keeps the output focused on genuine code quality issues.
echo.
