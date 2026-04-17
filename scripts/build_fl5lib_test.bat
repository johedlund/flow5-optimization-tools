@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set PATH=%QT_ROOT%\bin;%PATH%
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OPENBLAS_INCLUDE=C:\Users\johe2\bin\OpenBLAS\include
set OPENBLAS_LIB=C:\Users\johe2\bin\OpenBLAS\lib

cd /d C:\dev\optiflow5\fl5-lib

echo === Testing compilation with full INCLUDE ===
echo INCLUDE first 100 chars: %INCLUDE:~0,100%

echo.
echo Running nmake (Makefile should already exist from previous qmake)...
nmake

if exist fl5-lib1.dll (
    echo SUCCESS: fl5-lib1.dll created
    dir fl5-lib1.dll
) else (
    echo FAILED
)
