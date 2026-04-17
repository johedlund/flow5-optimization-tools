@echo off
REM Flow5 Windows Deployment Script
REM Creates a distributable package with all required DLLs

setlocal enabledelayedexpansion

set QT_ROOT=C:\Qt\6.6.2\msvc2019_64
set OCCT_DIR=C:\Users\johe2\bin\OCCT\occt-vc14-64
set OCCT_3RDPARTY=C:\Users\johe2\bin\OCCT\3rdparty-vc14-64
set OPENBLAS_DIR=C:\Users\johe2\bin\OpenBLAS

set DEPLOY_DIR=%~dp0deploy
set ZIP_NAME=flow5-windows.zip

echo === Flow5 Windows Deployment ===
echo.

REM Check if flow5.exe exists
if not exist "%~dp0fl5-app\flow5.exe" (
    echo ERROR: flow5.exe not found. Run build-windows.bat first.
    exit /b 1
)

REM Create clean deploy directory
echo Creating deploy directory...
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

REM Copy flow5.exe
echo Copying flow5.exe...
copy "%~dp0fl5-app\flow5.exe" "%DEPLOY_DIR%\" >nul

REM Run windeployqt
echo Deploying Qt DLLs...
"%QT_ROOT%\bin\windeployqt.exe" "%DEPLOY_DIR%\flow5.exe" --no-translations >nul

REM Copy project DLLs
echo Copying project DLLs...
copy "%~dp0XFoil-lib\XFoil1.dll" "%DEPLOY_DIR%\" >nul
copy "%~dp0fl5-lib\fl5-lib.dll" "%DEPLOY_DIR%\" >nul

REM Copy OpenBLAS
echo Copying OpenBLAS...
copy "%OPENBLAS_DIR%\bin\libopenblas.dll" "%DEPLOY_DIR%\" >nul

REM Copy OCCT DLLs
echo Copying OCCT DLLs...
copy "%OCCT_DIR%\win64\vc14\bin\TK*.dll" "%DEPLOY_DIR%\" >nul 2>&1

REM Copy OCCT 3rdparty DLLs
echo Copying OCCT 3rdparty DLLs...

REM TBB
copy "%OCCT_3RDPARTY%\tbb-2021.13.0-x64\bin\tbb12.dll" "%DEPLOY_DIR%\" >nul 2>&1
copy "%OCCT_3RDPARTY%\tbb-2021.13.0-x64\bin\tbbmalloc.dll" "%DEPLOY_DIR%\" >nul 2>&1

REM jemalloc
copy "%OCCT_3RDPARTY%\jemalloc-vc14-64\bin\jemalloc.dll" "%DEPLOY_DIR%\" >nul 2>&1

REM freetype
for /r "%OCCT_3RDPARTY%" %%f in (freetype.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1

REM FreeImage
for /r "%OCCT_3RDPARTY%" %%f in (FreeImage.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1
for /r "%OCCT_3RDPARTY%" %%f in (FreeImagePlus.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1

REM OpenVR
for /r "%OCCT_3RDPARTY%" %%f in (openvr_api.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1

REM FFmpeg
for /r "%OCCT_3RDPARTY%" %%f in (avcodec-57.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1
for /r "%OCCT_3RDPARTY%" %%f in (avformat-57.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1
for /r "%OCCT_3RDPARTY%" %%f in (avutil-55.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1
for /r "%OCCT_3RDPARTY%" %%f in (avdevice-57.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1
for /r "%OCCT_3RDPARTY%" %%f in (avfilter-6.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1
for /r "%OCCT_3RDPARTY%" %%f in (swscale-4.dll) do copy "%%f" "%DEPLOY_DIR%\" >nul 2>&1

REM Create ZIP
echo.
echo Creating ZIP archive...
if exist "%~dp0%ZIP_NAME%" del "%~dp0%ZIP_NAME%"
powershell -Command "Compress-Archive -Path '%DEPLOY_DIR%\*' -DestinationPath '%~dp0%ZIP_NAME%' -Force"

if exist "%~dp0%ZIP_NAME%" (
    echo.
    echo === SUCCESS ===
    echo Created: %~dp0%ZIP_NAME%
    for %%A in ("%~dp0%ZIP_NAME%") do echo Size: %%~zA bytes
) else (
    echo.
    echo ERROR: Failed to create ZIP
    exit /b 1
)

echo.
echo Deploy directory: %DEPLOY_DIR%
echo You can test by running: %DEPLOY_DIR%\flow5.exe
