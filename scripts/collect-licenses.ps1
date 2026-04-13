#Requires -Version 7.0
<#
.SYNOPSIS
    Collect LICENSE files from all OSS dependencies.

.DESCRIPTION
    Copies LICENSE (and related) files from each dependency into a
    licenses/<library>/ directory tree.  The output is suitable for
    inclusion in release archives.

    Submodule dependencies are read from externals/.
    FetchContent dependencies (libavif, libaom) are read from the
    plugins build tree and therefore require a prior build.

.PARAMETER OutputDir
    Root directory for the collected licenses. Defaults to 'licenses'.

.EXAMPLE
    ./scripts/collect-licenses.ps1
    ./scripts/collect-licenses.ps1 -OutputDir dist/licenses
#>

[CmdletBinding()]
param(
    [string]$OutputDir = "licenses"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)

# Resolve output directory relative to project root
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $ProjectRoot $OutputDir
}

# ---------------------------------------------------------------------------
# License manifest
# ---------------------------------------------------------------------------
# Each entry: [library name, source directory (relative to $ProjectRoot), files to copy]

$LicenseManifest = @(
    # --- Submodule dependencies ---
    @{ Name = "bit7z";        Source = "externals/bit7z";        Files = @("LICENSE") }
    @{ Name = "spdlog";       Source = "externals/spdlog";       Files = @("LICENSE") }
    @{ Name = "zstd";         Source = "externals/zstd";         Files = @("LICENSE", "COPYING") }
    @{ Name = "tomlplusplus";  Source = "externals/tomlplusplus"; Files = @("LICENSE") }

    # --- FetchContent dependencies (require prior build) ---
    @{ Name = "libavif"; Source = "plugins/build/release/_deps/libavif-src"; Files = @("LICENSE") }
    @{ Name = "libaom";  Source = "plugins/build/release/_deps/libaom_patch-src"; Files = @("LICENSE", "PATENTS") }
)

# ---------------------------------------------------------------------------
# Collect
# ---------------------------------------------------------------------------
$errors = @()

foreach ($entry in $LicenseManifest) {
    $name    = $entry.Name
    $srcDir  = Join-Path $ProjectRoot $entry.Source
    $destDir = Join-Path $OutputDir $name

    if (-not (Test-Path $srcDir)) {
        $errors += "[warn] Source not found for ${name}: $srcDir"
        continue
    }

    # Ensure destination directory exists
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    foreach ($file in $entry.Files) {
        $srcFile = Join-Path $srcDir $file
        if (Test-Path $srcFile) {
            Copy-Item $srcFile -Destination (Join-Path $destDir $file) -Force
        } else {
            $errors += "[warn] File not found: $srcFile"
        }
    }

    Write-Host "[ok]   $name" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
if ($errors.Count -gt 0) {
    Write-Host ""
    foreach ($e in $errors) {
        Write-Host $e -ForegroundColor Yellow
    }
    Write-Host ""
    throw "Some license files could not be collected. See warnings above."
}

Write-Host ""
Write-Host "[done] Licenses collected to: $OutputDir" -ForegroundColor Green
