$ErrorActionPreference = "Stop"

Write-Host "==================================================="
Write-Host "Cyber-Denoiser PRO VST3 Build Script (PowerShell)"
Write-Host "==================================================="
Write-Host ""

# 1. Check for CMake
$cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
if (!$cmakePath) {
    # Check default install locations
    if (Test-Path "C:\Program Files\CMake\bin\cmake.exe") {
        $env:PATH += ";C:\Program Files\CMake\bin"
    } elseif (Test-Path "C:\Program Files (x86)\CMake\bin\cmake.exe") {
        $env:PATH += ";C:\Program Files (x86)\CMake\bin"
    } else {
        Write-Host "[INFO] CMake not found. Installing via winget..."
        winget install Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
        $env:PATH += ";C:\Program Files\CMake\bin;C:\Program Files (x86)\CMake\bin"
    }
}
Write-Host "[OK] CMake is ready."

# 2. Check for Visual Studio MSVC Tools
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = ""
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}

if (-not $vsPath) {
    Write-Host "[INFO] Visual Studio C++ Build Tools not found. Installing via winget..."
    winget install Microsoft.VisualStudio.2022.BuildTools --silent --accept-package-agreements --accept-source-agreements --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
}

if ($vsPath) {
    Write-Host "[OK] Visual Studio C++ Tools found at: $vsPath"
} else {
    Write-Host "[WARNING] Could not locate Visual Studio installation."
}

Write-Host ""
Write-Host "==================================================="
Write-Host "Configuring the JUCE Project with CMake..."
Write-Host "==================================================="

cmake -B build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed."
    exit 1
}

Write-Host ""
Write-Host "==================================================="
Write-Host "Compiling the VST3 Plugin..."
Write-Host "==================================================="

cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed."
    exit 1
}

Write-Host ""
Write-Host "==================================================="
Write-Host "BUILD SUCCESSFUL!"
Write-Host "==================================================="
Write-Host "Your native 64-bit VST3 plugin is ready. You can find it here:"
Write-Host "$PWD\build\CyberDenoiserPro_artefacts\Release\VST3\Cyber-Denoiser PRO.vst3"
Write-Host ""
