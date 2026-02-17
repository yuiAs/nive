#Requires -Version 7.0
<#
.SYNOPSIS
    Build nive in Release configuration and package into a ZIP archive.

.DESCRIPTION
    This script sets up the Visual Studio Developer Shell, builds the project
    in Release configuration, and creates a distributable ZIP archive.

.PARAMETER OutputDir
    Directory where the ZIP archive is created. Defaults to 'dist'.

.PARAMETER SkipBuild
    Skip the build step and package from existing build output.

.EXAMPLE
    ./scripts/build-release.ps1
    ./scripts/build-release.ps1 -OutputDir artifacts
#>

[CmdletBinding()]
param(
    [string]$OutputDir = "dist",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)

# ---------------------------------------------------------------------------
# Visual Studio Developer Shell setup
# ---------------------------------------------------------------------------
function Enter-DevShell {
    # Skip if cl.exe is already available (e.g. running inside Developer PowerShell)
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        Write-Host "[info] MSVC toolchain already available, skipping DevShell setup." -ForegroundColor Cyan
        return
    }

    Write-Host "[info] Setting up Visual Studio Developer Shell..." -ForegroundColor Cyan

    # Ensure VSSetup module is available
    if (-not (Get-Module -ListAvailable -Name VSSetup)) {
        Set-PSRepository -Name PSGallery -InstallationPolicy Trusted
        Install-Module -Name VSSetup -Scope CurrentUser -AcceptLicense -Force
    }

    $vsInstance = Get-VSSetupInstance | Select-VSSetupInstance -Version '[18.0,19.0)'
    if (-not $vsInstance) {
        throw "Visual Studio 2026 (v18.x) not found."
    }

    $devShellModule = Join-Path $vsInstance.InstallationPath "Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
    if (-not (Test-Path $devShellModule)) {
        throw "DevShell module not found at: $devShellModule"
    }

    Import-Module $devShellModule
    Enter-VsDevShell $vsInstance.InstanceId -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "MSVC toolchain not available after DevShell setup."
    }
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
function Invoke-Build {
    # --- App build ---
    Write-Host "[info] Configuring App CMake (Release)..." -ForegroundColor Cyan
    cmake --preset release -S $ProjectRoot
    if ($LASTEXITCODE -ne 0) { throw "App CMake configure failed." }

    Write-Host "[info] Building App (Release)..." -ForegroundColor Cyan
    cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw "App build failed." }

    # --- Plugins build ---
    $pluginsDir = Join-Path $ProjectRoot "plugins"
    Push-Location $pluginsDir
    try {
        Write-Host "[info] Configuring Plugins CMake (Release)..." -ForegroundColor Cyan
        cmake --preset release
        if ($LASTEXITCODE -ne 0) { throw "Plugins CMake configure failed." }

        Write-Host "[info] Building Plugins (Release)..." -ForegroundColor Cyan
        cmake --build --preset release
        if ($LASTEXITCODE -ne 0) { throw "Plugins build failed." }
    } finally {
        Pop-Location
    }
}

# ---------------------------------------------------------------------------
# Package
# ---------------------------------------------------------------------------
function New-Package {
    $appBuildBin     = Join-Path $ProjectRoot "build/release/bin"
    $pluginsBuildBin = Join-Path $ProjectRoot "plugins/build/release/bin"
    $version  = (Select-String -Path "$ProjectRoot/CMakeLists.txt" -Pattern 'VERSION\s+(\d+\.\d+\.\d+)' |
                 Select-Object -First 1).Matches.Groups[1].Value

    $archiveName = "nive-v${version}-win-x64.zip"
    $outDir      = Join-Path $ProjectRoot $OutputDir
    $stagingDir  = Join-Path $outDir "nive-v${version}"

    # Clean staging area
    if (Test-Path $stagingDir) { Remove-Item $stagingDir -Recurse -Force }
    New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

    # Copy app executable
    $exe = Join-Path $appBuildBin "nive.exe"
    if (-not (Test-Path $exe)) { throw "nive.exe not found at: $exe" }
    Copy-Item $exe $stagingDir

    # Copy libavif runtime DLL (avif.dll)
    $avifDll = Join-Path $pluginsBuildBin "avif.dll"
    if (Test-Path $avifDll) {
        Copy-Item $avifDll $stagingDir
    } else {
        Write-Host "[warn] avif.dll not found at: $avifDll" -ForegroundColor Yellow
    }

    # plugins directory
    $pluginsStagingDir = Join-Path $stagingDir "plugins"
    New-Item -ItemType Directory -Path $pluginsStagingDir -Force | Out-Null

    # Copy plugin DLLs
    $niveAvifDll = Join-Path $pluginsBuildBin "plugins/nive_avif.dll"
    if (Test-Path $niveAvifDll) {
        Copy-Item $niveAvifDll $pluginsStagingDir
    } else {
        Write-Host "[warn] nive_avif.dll not found at: $niveAvifDll" -ForegroundColor Yellow
    }

    # locales directory
    $localesDir = Join-Path $ProjectRoot "locales"
    if (Test-Path $localesDir) {
        Copy-Item $localesDir -Destination (Join-Path $stagingDir "locales") -Recurse
    } else {
        Write-Host "[warn] locales directory not found at: $localesDir" -ForegroundColor Yellow
    }

    # LICENSE
    Copy-Item (Join-Path $ProjectRoot "LICENSE") $stagingDir

    # README.md
    Copy-Item (Join-Path $ProjectRoot "README.md") $stagingDir

    # Plugin API headers
    $includeDir = Join-Path $stagingDir "include/nive"
    New-Item -ItemType Directory -Path $includeDir -Force | Out-Null
    Copy-Item (Join-Path $ProjectRoot "include/nive/plugin_api.h") $includeDir

    # Create ZIP
    $archivePath = Join-Path $outDir $archiveName
    if (Test-Path $archivePath) { Remove-Item $archivePath -Force }
    Compress-Archive -Path $stagingDir -DestinationPath $archivePath

    # Clean staging
    Remove-Item $stagingDir -Recurse -Force

    Write-Host "[done] Package created: $archivePath" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
Push-Location $ProjectRoot
try {
    Enter-DevShell

    if (-not $SkipBuild) {
        Invoke-Build
    }

    New-Package
} finally {
    Pop-Location
}
