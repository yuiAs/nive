#Requires -Version 7.0
<#
.SYNOPSIS
    Package nive into an MSIX archive for Microsoft Store submission.

.DESCRIPTION
    Consumes the ZIP staging directory produced by build-release.ps1 (or any
    directory laid out the same way) and produces a .msix package.

    The produced package is unsigned on purpose: the Microsoft Store signs
    submissions on its side. To test-install locally, sign with signtool
    separately or use an App Installer with a trusted certificate.

    Visual assets (Square44x44Logo etc.) are generated on the fly from a
    single high-resolution source PNG via System.Drawing.

.PARAMETER StagingDir
    Directory containing the laid-out app files (nive.exe, locales/, etc.).
    Required. Typically produced by build-release.ps1.

.PARAMETER OutputDir
    Directory where the .msix file will be written. Defaults to 'dist'.

.PARAMETER Version
    4-part version (e.g. 0.4.0.0). If a 3-part value is passed it is
    extended with '.0'. If omitted, read from CMakeLists.txt.

.PARAMETER PackageName
    <Identity Name=".."> value. Must match the Package Identity Name
    assigned in Partner Center for Store submissions.

.PARAMETER Publisher
    <Identity Publisher=".."> value (X.500 DN, e.g. "CN=<GUID>"). Must match
    the Publisher ID assigned in Partner Center.

.PARAMETER PublisherDisplayName
    Display name shown in Store listing and system dialogs. Must match
    Partner Center registration.

.PARAMETER IconSource
    Source PNG used to generate visual assets. Must be large enough to
    produce a clean 310x310 downscale (ideally >= 1024x1024).

.PARAMETER ManifestTemplate
    Path to AppxManifest.xml template with {{PLACEHOLDER}} tokens.

.EXAMPLE
    ./scripts/create-msix.ps1 -StagingDir ./dist/nive-v0.4.0
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StagingDir,

    [string]$OutputDir = "dist",
    [string]$Version,
    [string]$PackageName = "sincereadvice.nive",
    [string]$Publisher = "CN=C096EDCE-7855-49B0-A1A5-EEB9F454B046",
    [string]$PublisherDisplayName = "sincere advice",
    [string]$IconSource,
    [string]$ManifestTemplate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)

# Resolve defaults relative to project root
if (-not $IconSource) {
    $IconSource = Join-Path $ProjectRoot "resources/icons/nive.png"
}
if (-not $ManifestTemplate) {
    $ManifestTemplate = Join-Path $ProjectRoot "packaging/msix/AppxManifest.xml"
}
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $ProjectRoot $OutputDir
}

# ---------------------------------------------------------------------------
# Asset size table
# ---------------------------------------------------------------------------
# Each entry defines one PNG to generate under Assets/.
# Sizes follow the Microsoft Store guidance for packaged desktop apps.
$AssetSpec = @(
    @{ Name = "Square44x44Logo.png";  Width =  44; Height =  44 }
    @{ Name = "Square150x150Logo.png"; Width = 150; Height = 150 }
    @{ Name = "SmallTile.png";         Width =  71; Height =  71 }
    @{ Name = "LargeTile.png";         Width = 310; Height = 310 }
    @{ Name = "Wide310x150Logo.png";   Width = 310; Height = 150 }
    @{ Name = "StoreLogo.png";         Width =  50; Height =  50 }
    @{ Name = "SplashScreen.png";      Width = 620; Height = 300 }
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Find-MakeAppx {
    # Prefer makeappx.exe on PATH (DevShell, SDK-configured environments)
    $onPath = Get-Command makeappx.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    # Fall back to scanning Windows SDK bin directories
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits/10/bin"
    if (-not (Test-Path $sdkRoot)) {
        throw "Windows SDK not found at: $sdkRoot"
    }

    $candidate = Get-ChildItem $sdkRoot -Directory |
                 Where-Object { $_.Name -match '^10\.' } |
                 Sort-Object { [Version]$_.Name } -Descending |
                 ForEach-Object { Join-Path $_.FullName "x64/makeappx.exe" } |
                 Where-Object { Test-Path $_ } |
                 Select-Object -First 1

    if (-not $candidate) {
        throw "makeappx.exe not found. Install the Windows 10/11 SDK."
    }
    return $candidate
}

function Get-ProjectVersion {
    $cmakeLists = Join-Path $ProjectRoot "CMakeLists.txt"
    $match = Select-String -Path $cmakeLists -Pattern 'VERSION\s+(\d+\.\d+\.\d+)' |
             Select-Object -First 1
    if (-not $match) { throw "Unable to read VERSION from CMakeLists.txt" }
    return $match.Matches.Groups[1].Value
}

function ConvertTo-FourPartVersion {
    param([string]$Value)
    $parts = $Value -split '\.'
    switch ($parts.Count) {
        3 { return "$Value.0" }
        4 { return $Value }
        default { throw "Version must be 3 or 4 dotted parts, got: $Value" }
    }
}

function New-ResizedPng {
    param(
        [string]$SourcePath,
        [string]$DestPath,
        [int]$Width,
        [int]$Height
    )
    $src = [System.Drawing.Image]::FromFile($SourcePath)
    try {
        $bmp = New-Object System.Drawing.Bitmap $Width, $Height
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bmp)
            try {
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.DrawImage($src, 0, 0, $Width, $Height)
            } finally {
                $graphics.Dispose()
            }
            $bmp.Save($DestPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $bmp.Dispose()
        }
    } finally {
        $src.Dispose()
    }
}

function Expand-ManifestTemplate {
    param(
        [string]$TemplatePath,
        [string]$DestPath,
        [hashtable]$Replacements
    )
    $content = Get-Content -Raw -Path $TemplatePath
    foreach ($key in $Replacements.Keys) {
        $token = "{{${key}}}"
        $content = $content.Replace($token, $Replacements[$key])
    }
    # Reject any remaining {{...}} placeholder to catch typos early
    if ($content -match '\{\{[A-Z_]+\}\}') {
        throw "Unresolved placeholder in manifest: $($Matches[0])"
    }
    [System.IO.File]::WriteAllText($DestPath, $content, [System.Text.UTF8Encoding]::new($false))
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if (-not (Test-Path $StagingDir)) {
    throw "StagingDir not found: $StagingDir"
}
if (-not (Test-Path $IconSource)) {
    throw "IconSource not found: $IconSource"
}
if (-not (Test-Path $ManifestTemplate)) {
    throw "ManifestTemplate not found: $ManifestTemplate"
}

# Load System.Drawing for image resize (Windows-only path)
Add-Type -AssemblyName System.Drawing

if (-not $Version) { $Version = Get-ProjectVersion }
$msixVersion = ConvertTo-FourPartVersion -Value $Version

$makeAppx = Find-MakeAppx
Write-Host "[info] Using makeappx: $makeAppx" -ForegroundColor Cyan

# Work directory (kept separate from ZIP staging to avoid cross-contamination)
$workDir = Join-Path $OutputDir "msix-work"
if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

# Copy app payload from staging
Write-Host "[info] Copying payload from: $StagingDir" -ForegroundColor Cyan
Copy-Item -Path (Join-Path $StagingDir "*") -Destination $workDir -Recurse -Force

# Generate visual assets
$assetsDir = Join-Path $workDir "Assets"
New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null
Write-Host "[info] Generating visual assets from: $IconSource" -ForegroundColor Cyan
foreach ($asset in $AssetSpec) {
    $dest = Join-Path $assetsDir $asset.Name
    New-ResizedPng -SourcePath $IconSource -DestPath $dest -Width $asset.Width -Height $asset.Height
    Write-Host "[ok]   $($asset.Name) ($($asset.Width)x$($asset.Height))" -ForegroundColor Green
}

# Expand manifest into work directory
$manifestDest = Join-Path $workDir "AppxManifest.xml"
Write-Host "[info] Expanding manifest template" -ForegroundColor Cyan
Expand-ManifestTemplate -TemplatePath $ManifestTemplate -DestPath $manifestDest -Replacements @{
    PACKAGE_NAME           = $PackageName
    PUBLISHER              = $Publisher
    VERSION                = $msixVersion
    PUBLISHER_DISPLAY_NAME = $PublisherDisplayName
}

# Pack
$msixName = "nive-v${Version}-win-x64.msix"
$msixPath = Join-Path $OutputDir $msixName
if (Test-Path $msixPath) { Remove-Item $msixPath -Force }

Write-Host "[info] Packing MSIX..." -ForegroundColor Cyan
& $makeAppx pack /d $workDir /p $msixPath /o
if ($LASTEXITCODE -ne 0) {
    throw "makeappx pack failed (exit $LASTEXITCODE)"
}

# Clean up work directory
Remove-Item $workDir -Recurse -Force

Write-Host "[done] MSIX created: $msixPath" -ForegroundColor Green
Write-Host "[note] Package is unsigned — Microsoft Store will sign on submission." -ForegroundColor Yellow
