@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set PATH=%QT_ROOT%\bin;%PATH%

cd /d C:\dev\optiflow5\fl5-lib

echo === Testing moc directly ===
echo Current directory: %CD%

echo.
echo Checking if files exist:
if exist api\fileio.h (echo api\fileio.h: EXISTS) else (echo api\fileio.h: MISSING)
if exist moc\moc_predefs.h (echo moc\moc_predefs.h: EXISTS) else (echo moc\moc_predefs.h: MISSING)

echo.
echo Running moc...
"%QT_ROOT%\bin\moc.exe" -DUNICODE -D_UNICODE -DWIN32 -D_ENABLE_EXTENDED_ALIGNED_STORAGE -DWIN64 -DFL5LIB_LIBRARY -DQT_DISABLE_DEPRECATED_BEFORE=0x060000 -DWIN_OS -DOPENBLAS "-Dlapack_complex_float=std::complex<float>" "-Dlapack_complex_double=std::complex<double>" -DNDEBUG -DQT_NO_DEBUG -DQT_CORE_LIB -D_WINDLL --compiler-flavor=msvc --include C:/dev/optiflow5/fl5-lib/moc/moc_predefs.h -IC:/Qt/6.6.2/msvc2019_64/mkspecs/win32-msvc -IC:/dev/optiflow5/fl5-lib -IC:/dev/optiflow5/XFoil-lib -IC:/dev/optiflow5/fl5-lib/api -IC:/Users/johe2/bin/OpenBLAS/include -IC:/Users/johe2/bin/OCCT/occt-vc14-64/inc -IC:/Qt/6.6.2/msvc2019_64/include -IC:/Qt/6.6.2/msvc2019_64/include/QtCore api\fileio.h -o moc\moc_fileio.cpp

if errorlevel 1 (
    echo.
    echo MOC FAILED with error code %errorlevel%
) else (
    echo.
    echo MOC SUCCESS
    if exist moc\moc_fileio.cpp dir moc\moc_fileio.cpp
)
