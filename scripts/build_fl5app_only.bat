@echo off
REM Build only fl5-app (assumes dependencies are already built)

setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set PATH=%QT_ROOT%\bin;%PATH%
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OPENBLAS_INCLUDE=C:\Users\johe2\bin\OpenBLAS\include
set OPENBLAS_LIB=C:\Users\johe2\bin\OpenBLAS\lib

echo === Building fl5-app ===
cd /d "%~dp0..\fl5-app"
if exist Makefile del Makefile

REM Save original INCLUDE using delayed expansion
set "ORIG_INCLUDE=!INCLUDE!"

REM Clear INCLUDE for qmake to prevent system paths in moc command
set INCLUDE=

echo Running qmake with NO_GMSH...
"%QT_ROOT%\bin\qmake.exe" fl5-app.pro CONFIG+=release CONFIG+=CI_OPENBLAS CONFIG+=NO_GMSH
if errorlevel 1 (
    echo qmake FAILED
    exit /b 1
)
echo qmake succeeded

REM Restore INCLUDE for nmake compilation
set "INCLUDE=!ORIG_INCLUDE!"
echo Restored INCLUDE for compilation

echo Running nmake...
nmake
if errorlevel 1 (
    echo nmake FAILED
    exit /b 1
)

if exist release\flow5.exe (
    echo SUCCESS: flow5.exe created
    dir release\flow5.exe
) else (
    echo FAILED: flow5.exe not found in release
    exit /b 1
)
