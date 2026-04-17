@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set PATH=%QT_ROOT%\bin;%PATH%
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OPENBLAS_INCLUDE=C:\Users\johe2\bin\OpenBLAS\include
set OPENBLAS_LIB=C:\Users\johe2\bin\OpenBLAS\lib

cd /d C:\dev\optiflow5\fl5-lib

echo === Clean build of fl5-lib ===

REM Delete old build artifacts
if exist Makefile del Makefile
if exist Makefile.Debug del Makefile.Debug
if exist Makefile.Release del Makefile.Release
if exist .qmake.stash del .qmake.stash
if exist objects rmdir /s /q objects
if exist moc rmdir /s /q moc
mkdir objects
mkdir moc

REM Save INCLUDE using delayed expansion
set "SAVE_INCLUDE=!INCLUDE!"

REM Run qmake with empty INCLUDE to avoid moc path issues
set INCLUDE=
echo Running qmake (INCLUDE cleared)...
"%QT_ROOT%\bin\qmake.exe" fl5-lib.pro CONFIG+=release CONFIG+=CI_OPENBLAS -o Makefile
if errorlevel 1 (
    echo qmake FAILED
    exit /b 1
)
echo qmake succeeded

REM Restore INCLUDE using delayed expansion
set "INCLUDE=!SAVE_INCLUDE!"
echo INCLUDE restored (first 50 chars): !INCLUDE:~0,50!...

REM Run nmake
echo.
echo Running nmake...
nmake /NOLOGO

if exist fl5-lib1.dll (
    echo.
    echo === SUCCESS ===
    dir fl5-lib1.dll
) else (
    echo.
    echo === FAILED ===
    exit /b 1
)
