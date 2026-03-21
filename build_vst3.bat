@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Cyber-Denoiser PRO VST3 Build Script
echo ===================================================
echo.

:: 1. Add typical locations to PATH temporarily
set "PATH=%PATH%;C:\Program Files\CMake\bin;C:\Program Files (x86)\CMake\bin"

:: 2. Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [INFO] CMake not found in PATH.
    echo Trying to install CMake via winget (Windows Package Manager)...
    winget install Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Failed to install CMake automatically. 
        echo Please download it manually from https://cmake.org/download/
        pause
        exit /b 1
    )
    echo [INFO] CMake installed successfully.
    :: Attempt to map path again after winget install
    set "PATH=!PATH!;C:\Program Files\CMake\bin;C:\Program Files (x86)\CMake\bin"
) else (
    echo [OK] CMake is already installed.
)

:: 3. Check for Visual Studio MSVC Tools
set "VS_PATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" goto skip_vswhere1
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)
:skip_vswhere1

if not "%VS_PATH%"=="" goto vs_installed

echo [INFO] Visual Studio C++ Build Tools not found.
echo Installing via winget... (This will download several gigabytes and may take 10+ minutes)
winget install Microsoft.VisualStudio.2022.BuildTools --silent --accept-package-agreements --accept-source-agreements --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Failed to install Visual Studio Build Tools automatically.
    echo Please install "Visual Studio 2022 Community" manually with the "Desktop Development with C++" workload.
    pause
    exit /b 1
)
echo [INFO] Visual Studio Build Tools installed.
:: Re-fetch the path just in case
if not exist "%VSWHERE%" goto skip_vswhere2
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)
:skip_vswhere2

:vs_installed

if "%VS_PATH%"=="" (
    echo [WARNING] Could not locate VS_PATH after install, build might fail.
    goto skip_vcvars
)

echo [OK] Visual Studio C++ Tools found at: %VS_PATH%
echo Setting up MSVC Community/BuildTools environment...
if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

:skip_vcvars

echo.
echo ===================================================
echo Configuring the JUCE Project with CMake...
echo (This will automatically download JUCE framework)
echo ===================================================
:: Remove existing build cache to avoid NMake conflicts
if exist build rmdir /s /q build

cmake -B build -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo.
echo ===================================================
echo Compiling the VST3 Plugin...
echo ===================================================
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo ===================================================
echo BUILD SUCCESSFUL!
echo ===================================================
echo Your native 64-bit VST3 plugin is ready. You can find it here:
echo %CD%\build\CyberDenoiserPro_artefacts\Release\VST3\CyberDenoiserPro.vst3
echo.
echo You can copy this file to your DAW's VST3 plugins folder:
echo C:\Program Files\Common Files\VST3\
echo.
pause
