@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set PATH=%QT_ROOT%\bin;%PATH%
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OPENBLAS_INCLUDE=C:\Users\johe2\bin\OpenBLAS\include
set OPENBLAS_LIB=C:\Users\johe2\bin\OpenBLAS\lib

cd /d C:\dev\optiflow5\fl5-lib

echo === Testing single file compilation ===
echo Compiling math\testmatrix.cpp...

cl -c -nologo -Zc:wchar_t -FS -Zc:rvalueCast -Zc:inline -Zc:strictStrings -Zc:throwingNew -permissive- -Zc:__cplusplus -Zc:externConstexpr -O2 -MD -std:c++17 -utf-8 -W3 -EHsc -DUNICODE -DWIN32 -DWIN64 -DFL5LIB_LIBRARY -DWIN_OS -DOPENBLAS "-Dlapack_complex_float=std::complex<float>" "-Dlapack_complex_double=std::complex<double>" -DNDEBUG -DQT_NO_DEBUG -DQT_CORE_LIB -D_WINDLL -I. -I..\XFoil-lib -Iapi -I%OPENBLAS_INCLUDE% -I%OCCT_DIR%\inc -I%QT_ROOT%\include -I%QT_ROOT%\include\QtCore -Imoc -I%QT_ROOT%\mkspecs\win32-msvc -Foobjects\testmatrix.obj math\testmatrix.cpp

if errorlevel 1 (
    echo FAILED
    exit /b 1
) else (
    echo SUCCESS
    if exist objects\testmatrix.obj dir objects\testmatrix.obj
)
