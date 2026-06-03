@echo off
REM Cppcheck Analysis Script for VoidLight-Framework (Windows)
REM This script runs cppcheck with optimized settings to focus on real issues

setlocal enabledelayedexpansion

REM Configuration
set PROJECT_ROOT=%~dp0..\..
set INCLUDE_DIRS=include src
set LIBRARY_CONFIG=cppcheck_lib.cfg
set OUTPUT_DIR=%PROJECT_ROOT%\test_results
for /f "tokens=2 delims==" %%I in ('wmic OS Get localdatetime /value') do set datetime=%%I
set TIMESTAMP=%datetime:~0,8%_%datetime:~8,6%

REM Create output directory if it doesn't exist
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo === VoidLight-Framework - Cppcheck Analysis ===
echo Project Root: %PROJECT_ROOT%
echo Timestamp: %TIMESTAMP%
echo.

REM Check if cppcheck is available
cppcheck --version >nul 2>&1
if errorlevel 1 (
    echo Error: cppcheck not found in PATH
    echo Please install cppcheck and ensure it's in your PATH
    pause
    exit /b 1
)

echo Checking cppcheck version...
cppcheck --version

echo Validating configuration files...
if not exist "%LIBRARY_CONFIG%" (
    echo Error: Library config file '%LIBRARY_CONFIG%' not found
    pause
    exit /b 1
)

echo Configuration files validated
echo.

REM Function to run cppcheck analysis
set CRITICAL_OUTPUT=%OUTPUT_DIR%\cppcheck_critical_%TIMESTAMP%.xml
set CRITICAL_SUMMARY=%OUTPUT_DIR%\cppcheck_critical_summary_%TIMESTAMP%.txt
set FULL_OUTPUT=%OUTPUT_DIR%\cppcheck_full_%TIMESTAMP%.xml
set FULL_SUMMARY=%OUTPUT_DIR%\cppcheck_full_summary_%TIMESTAMP%.txt
set STYLE_OUTPUT=%OUTPUT_DIR%\cppcheck_style_%TIMESTAMP%.xml
set STYLE_SUMMARY=%OUTPUT_DIR%\cppcheck_style_summary_%TIMESTAMP%.txt
set FINAL_SUMMARY=%OUTPUT_DIR%\cppcheck_final_summary_%TIMESTAMP%.txt

REM Analysis 1: Critical Issues Only
echo === ANALYSIS 1: Critical Issues Only ===
echo Running critical analysis...

cppcheck --enable=warning,performance,portability --library=std,posix --library="%LIBRARY_CONFIG%" --include-path=..\..\include --include-path=..\..\src --platform=win64 --std=c++20 --verbose --xml --suppress=information --suppress=style ..\..\src\ ..\..\include\ 2>"%CRITICAL_OUTPUT%"

if exist "%CRITICAL_OUTPUT%" (
    REM Count errors in critical analysis
    findstr /c:"<error" "%CRITICAL_OUTPUT%" >nul 2>&1
    if errorlevel 1 (
        set CRITICAL_COUNT=0
    ) else (
        for /f %%i in ('findstr /c:"<error" "%CRITICAL_OUTPUT%"') do set CRITICAL_COUNT=%%i
    )

    REM Generate critical summary
    echo === Cppcheck Critical Analysis Summary === > "%CRITICAL_SUMMARY%"
    echo Generated: %date% %time% >> "%CRITICAL_SUMMARY%"
    echo Configuration: %LIBRARY_CONFIG% >> "%CRITICAL_SUMMARY%"
    echo. >> "%CRITICAL_SUMMARY%"
    echo Results: >> "%CRITICAL_SUMMARY%"
    echo   Total Critical Issues: !CRITICAL_COUNT! >> "%CRITICAL_SUMMARY%"
    echo. >> "%CRITICAL_SUMMARY%"

    if !CRITICAL_COUNT! gtr 0 (
        echo Critical issues found: !CRITICAL_COUNT!
        echo Review required - see %CRITICAL_OUTPUT%
    ) else (
        echo No critical issues found!
    )
) else (
    echo Failed to generate critical analysis
    set CRITICAL_COUNT=999
)

echo.

REM Analysis 2: Full Analysis
echo === ANALYSIS 2: Full Analysis ===
echo Running full analysis...

cppcheck --enable=warning,style,performance,portability,information --library=std,posix --library="%LIBRARY_CONFIG%" --include-path=..\..\include --include-path=..\..\src --platform=win64 --std=c++20 --verbose --xml ..\..\src\ ..\..\include\ 2>"%FULL_OUTPUT%"

if exist "%FULL_OUTPUT%" (
    REM Count errors in full analysis
    findstr /c:"<error" "%FULL_OUTPUT%" >nul 2>&1
    if errorlevel 1 (
        set FULL_COUNT=0
    ) else (
        for /f %%i in ('findstr /c:"<error" "%FULL_OUTPUT%"') do set FULL_COUNT=%%i
    )

    REM Generate full summary
    echo === Cppcheck Full Analysis Summary === > "%FULL_SUMMARY%"
    echo Generated: %date% %time% >> "%FULL_SUMMARY%"
    echo Configuration: %LIBRARY_CONFIG% >> "%FULL_SUMMARY%"
    echo. >> "%FULL_SUMMARY%"
    echo Results: >> "%FULL_SUMMARY%"
    echo   Total Issues: !FULL_COUNT! >> "%FULL_SUMMARY%"
    echo. >> "%FULL_SUMMARY%"

    if !FULL_COUNT! gtr 0 (
        echo Full analysis found: !FULL_COUNT! issues
        echo See %FULL_OUTPUT% for details
    ) else (
        echo No issues found in full analysis!
    )
) else (
    echo Failed to generate full analysis
    set FULL_COUNT=999
)

echo.

REM Analysis 3: Style Analysis
echo === ANALYSIS 3: Style and Best Practices ===
echo Running style analysis...

cppcheck --enable=style --library=std,posix --library="%LIBRARY_CONFIG%" --include-path=..\..\include --include-path=..\..\src --platform=win64 --std=c++20 --verbose --xml --suppress=information ..\..\src\ ..\..\include\ 2>"%STYLE_OUTPUT%"

if exist "%STYLE_OUTPUT%" (
    REM Count errors in style analysis
    findstr /c:"<error" "%STYLE_OUTPUT%" >nul 2>&1
    if errorlevel 1 (
        set STYLE_COUNT=0
    ) else (
        for /f %%i in ('findstr /c:"<error" "%STYLE_OUTPUT%"') do set STYLE_COUNT=%%i
    )

    REM Generate style summary
    echo === Cppcheck Style Analysis Summary === > "%STYLE_SUMMARY%"
    echo Generated: %date% %time% >> "%STYLE_SUMMARY%"
    echo Configuration: %LIBRARY_CONFIG% >> "%STYLE_SUMMARY%"
    echo. >> "%STYLE_SUMMARY%"
    echo Results: >> "%STYLE_SUMMARY%"
    echo   Total Style Issues: !STYLE_COUNT! >> "%STYLE_SUMMARY%"
    echo. >> "%STYLE_SUMMARY%"

    if !STYLE_COUNT! gtr 0 (
        echo Style analysis found: !STYLE_COUNT! issues
        echo See %STYLE_OUTPUT% for details
    ) else (
        echo No style issues found!
    )
) else (
    echo Failed to generate style analysis
    set STYLE_COUNT=999
)

echo.

REM Generate final summary
echo === FINAL CPPCHECK SUMMARY === > "%FINAL_SUMMARY%"
echo Generated: %date% %time% >> "%FINAL_SUMMARY%"
echo Project: VoidLight-Framework >> "%FINAL_SUMMARY%"
echo. >> "%FINAL_SUMMARY%"
echo Analysis Results: >> "%FINAL_SUMMARY%"
echo   Critical Issues: !CRITICAL_COUNT! >> "%FINAL_SUMMARY%"
echo   Full Analysis Issues: !FULL_COUNT! >> "%FINAL_SUMMARY%"
echo   Style Issues: !STYLE_COUNT! >> "%FINAL_SUMMARY%"
echo. >> "%FINAL_SUMMARY%"

if !CRITICAL_COUNT! equ 0 (
    echo STATUS: PASSED - No critical issues found >> "%FINAL_SUMMARY%"
    echo The codebase appears to be free of critical defects. >> "%FINAL_SUMMARY%"
    set EXIT_CODE=0
) else if !CRITICAL_COUNT! lss 5 (
    echo STATUS: REVIEW NEEDED - Few critical issues found >> "%FINAL_SUMMARY%"
    echo Review the critical issues report and address if necessary. >> "%FINAL_SUMMARY%"
    set EXIT_CODE=1
) else (
    echo STATUS: ACTION REQUIRED - Multiple critical issues found >> "%FINAL_SUMMARY%"
    echo Please review and address the critical issues before proceeding. >> "%FINAL_SUMMARY%"
    set EXIT_CODE=2
)

echo. >> "%FINAL_SUMMARY%"
echo Next Steps: >> "%FINAL_SUMMARY%"
echo 1. Review critical issues first (highest priority) >> "%FINAL_SUMMARY%"
echo 2. Consider style improvements for code quality >> "%FINAL_SUMMARY%"
echo 3. Re-run analysis after fixes >> "%FINAL_SUMMARY%"

REM Display final summary
echo === FINAL SUMMARY ===
type "%FINAL_SUMMARY%"

echo.
echo Analysis complete! Reports saved in %OUTPUT_DIR%\
echo.

if !CRITICAL_COUNT! equ 0 (
    echo Cppcheck analysis completed successfully!
) else if !CRITICAL_COUNT! lss 5 (
    echo Cppcheck analysis completed with minor issues
) else (
    echo Cppcheck analysis found significant issues
)

echo.
echo Press any key to continue...
pause >nul

exit /b %EXIT_CODE%
