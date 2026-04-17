@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set PATH=%QT_ROOT%\bin;%PATH%
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OPENBLAS_INCLUDE=C:\Users\johe2\bin\OpenBLAS\include
set OPENBLAS_LIB=C:\Users\johe2\bin\OpenBLAS\lib

echo === Building fl5-lib ===
cd /d C:\dev\optiflow5\fl5-lib
if exist Makefile del Makefile

REM Save original INCLUDE using delayed expansion
set "ORIG_INCLUDE=!INCLUDE!"

REM Clear INCLUDE for qmake to prevent system paths in moc command
set INCLUDE=

echo Running qmake with empty INCLUDE...
"%QT_ROOT%\bin\qmake.exe" fl5-lib.pro CONFIG+=release CONFIG+=CI_OPENBLAS
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

if exist fl5-lib1.dll (
    echo SUCCESS: fl5-lib1.dll created
    dir fl5-lib1.dll
) else (
    echo FAILED: fl5-lib1.dll not found
    exit /b 1
)
