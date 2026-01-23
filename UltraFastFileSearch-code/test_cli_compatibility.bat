@echo off
REM Test script for CLI11 migration compatibility
REM Run this after building to verify command-line interface works correctly

setlocal enabledelayedexpansion

set EXE=x64\COM\uffs.com
set PASSED=0
set FAILED=0

echo ============================================
echo CLI11 Migration Compatibility Tests
echo ============================================
echo.

echo === Diagnostics ===
echo Current directory: %CD%
echo Binary path: %EXE%
echo Full path: %CD%\%EXE%
echo.

if exist "%EXE%" (
    echo Binary EXISTS
    echo File size:
    for %%A in ("%EXE%") do echo   %%~zA bytes
) else (
    echo ERROR: Binary NOT FOUND at %EXE%
    echo.
    echo Looking for alternatives...
    if exist "x64\EXE\UltraFastFileSearch.exe" echo   Found: x64\EXE\UltraFastFileSearch.exe
    if exist "x64\COM\UltraFastFileSearch.com" echo   Found: x64\COM\UltraFastFileSearch.com
    if exist "x64\Release\uffs.com" echo   Found: x64\Release\uffs.com
    if exist "x64\Debug\uffs.com" echo   Found: x64\Debug\uffs.com
    dir /s /b *.com *.exe 2>nul | findstr /i "uffs\|ultrafast"
    echo.
    echo Please update EXE variable in this script to point to the correct binary.
    exit /b 1
)
echo.
echo ============================================
echo.

REM Test 1: Version flag
echo [TEST 1] --version flag
echo   Command: %EXE% --version
%EXE% --version
set RESULT=%ERRORLEVEL%
echo   Exit code: %RESULT%
if %RESULT% EQU 0 (
    echo   PASSED: --version works
    set /a PASSED+=1
) else (
    echo   FAILED: --version returned error code %RESULT%
    set /a FAILED+=1
)

REM Test 2: Help flag
echo [TEST 2] --help flag
%EXE% --help >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: --help works
    set /a PASSED+=1
) else (
    echo   FAILED: --help returned error
    set /a FAILED+=1
)

REM Test 3: Short help flag
echo [TEST 3] -h flag
%EXE% -h >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: -h works
    set /a PASSED+=1
) else (
    echo   FAILED: -h returned error
    set /a FAILED+=1
)

REM Test 4: Basic search (console output)
echo [TEST 4] Basic search with console output
%EXE% "C:/*.txt" --columns=name 2>&1 | findstr /C:"." >nul
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: Basic search works
    set /a PASSED+=1
) else (
    echo   FAILED: Basic search returned error
    set /a FAILED+=1
)

REM Test 5: Multiple columns
echo [TEST 5] Multiple columns
%EXE% "C:/*.txt" --columns=name,path,size 2>&1 | findstr /C:"," >nul
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: Multiple columns work
    set /a PASSED+=1
) else (
    echo   FAILED: Multiple columns returned error
    set /a FAILED+=1
)

REM Test 6: Custom separator
echo [TEST 6] Custom separator
%EXE% "C:/*.txt" --columns=name,size --sep="|" 2>&1 | findstr /C:"|" >nul
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: Custom separator works
    set /a PASSED+=1
) else (
    echo   FAILED: Custom separator returned error
    set /a FAILED+=1
)

REM Test 7: File output
echo [TEST 7] File output
%EXE% "C:/*.txt" --out=test_output.csv --columns=name,path 2>&1
if exist test_output.csv (
    echo   PASSED: File output works
    set /a PASSED+=1
    del test_output.csv
) else (
    echo   FAILED: File output did not create file
    set /a FAILED+=1
)

REM Test 8: Extension filter
echo [TEST 8] Extension filter
%EXE% "C:/" --ext=txt --columns=name 2>&1 | findstr /C:"." >nul
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: Extension filter works
    set /a PASSED+=1
) else (
    echo   FAILED: Extension filter returned error
    set /a FAILED+=1
)

REM Test 9: Drive specification
echo [TEST 9] Drive specification
%EXE% "/" --drives=C --columns=name 2>&1 | findstr /C:"." >nul
if %ERRORLEVEL% EQU 0 (
    echo   PASSED: Drive specification works
    set /a PASSED+=1
) else (
    echo   FAILED: Drive specification returned error
    set /a FAILED+=1
)

echo.
echo ============================================
echo Results: %PASSED% passed, %FAILED% failed
echo ============================================

if %FAILED% GTR 0 (
    exit /b 1
) else (
    exit /b 0
)

