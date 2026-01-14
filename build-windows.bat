@echo off
REM Flow5 Windows Build Script (local development)
REM Run from Developer Command Prompt for VS 2022 or after vcvarsall.bat

setlocal enabledelayedexpansion

REM === Configuration ===
set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OPENBLAS_INCLUDE=C:\Users\johe2\bin\OpenBLAS\include
set OPENBLAS_LIB=C:\Users\johe2\bin\OpenBLAS\lib

REM === Verify tools ===
echo === Checking environment ===
if not exist "%QT_ROOT%\bin\qmake.exe" (
    echo ERROR: Qt not found at %QT_ROOT%
    exit /b 1
)
echo Qt: %QT_ROOT%

if not exist "%OCCT_DIR%\inc" (
    echo ERROR: OCCT not found at %OCCT_DIR%
    exit /b 1
)
echo OCCT: %OCCT_DIR%

if not exist "%OPENBLAS_INCLUDE%\cblas.h" (
    echo ERROR: OpenBLAS not found at %OPENBLAS_INCLUDE%
    exit /b 1
)
echo OpenBLAS: %OPENBLAS_INCLUDE%

where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: MSVC cl.exe not found. Run from Developer Command Prompt or vcvarsall.bat first.
    exit /b 1
)
echo MSVC: OK

REM === Add Qt to PATH ===
set PATH=%QT_ROOT%\bin;%PATH%

echo.
echo === Building XFoil-lib ===
cd /d "%~dp0XFoil-lib"
if exist Makefile del Makefile
"%QT_ROOT%\bin\qmake.exe" XFoil-lib.pro CONFIG+=release
if errorlevel 1 (
    echo qmake failed for XFoil-lib
    exit /b 1
)
nmake
if errorlevel 1 (
    echo Build failed for XFoil-lib
    exit /b 1
)
if not exist XFoil1.dll (
    echo ERROR: XFoil1.dll not created
    exit /b 1
)
echo SUCCESS: XFoil1.dll

echo.
echo === Building fl5-lib ===
cd /d "%~dp0fl5-lib"
if exist Makefile del Makefile
"%QT_ROOT%\bin\qmake.exe" fl5-lib.pro CONFIG+=release CONFIG+=CI_OPENBLAS
if errorlevel 1 (
    echo qmake failed for fl5-lib
    exit /b 1
)
nmake
if errorlevel 1 (
    echo Build failed for fl5-lib
    exit /b 1
)
if not exist fl5-lib1.dll (
    echo ERROR: fl5-lib1.dll not created
    exit /b 1
)
echo SUCCESS: fl5-lib1.dll

echo.
echo === Building fl5-app ===
cd /d "%~dp0fl5-app"
if exist Makefile del Makefile
"%QT_ROOT%\bin\qmake.exe" fl5-app.pro CONFIG+=release CONFIG+=CI_OPENBLAS CONFIG+=NO_GMSH
if errorlevel 1 (
    echo qmake failed for fl5-app
    exit /b 1
)
nmake
if errorlevel 1 (
    echo Build failed for fl5-app
    exit /b 1
)
if not exist flow5.exe (
    echo ERROR: flow5.exe not created
    exit /b 1
)
echo SUCCESS: flow5.exe

echo.
echo === BUILD COMPLETE ===
cd /d "%~dp0"
