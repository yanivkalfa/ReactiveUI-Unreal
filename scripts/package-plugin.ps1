# Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
<#
.SYNOPSIS
    Package the ReactiveUI plugin per engine version — the LOCAL rail for what publish.yml's
    engine legs do in CI (MASTER_PLAN D-29; engine CI is default-unarmed, so this is the
    expected v1 path).

.DESCRIPTION
    For each engine install found (or passed), runs:
      RunUAT.bat BuildPlugin -Plugin=<ReactiveUI.uplugin> -Package=<staging> -TargetPlatforms=Win64 -Rocket -StrictIncludes
    then strips Binaries/ and Intermediate/ (Fab: "Epic's toolchain builds the binaries") and
    zips  dist/ReactiveUI-<VersionName>-UE<major.minor>.zip.

    -StrictIncludes is non-negotiable: marketplace builds compile with -DisableUnity -NoPCH,
    so include-leakage that works locally fails at Fab otherwise.

.PARAMETER EngineRoots  Engine install roots. Default: every "C:\Program Files\Epic Games\UE_5.*".
.PARAMETER OutDir       Output directory for zips. Default: <repo>\dist
.EXAMPLE  .\scripts\package-plugin.ps1
.EXAMPLE  .\scripts\package-plugin.ps1 -EngineRoots "C:\Program Files\Epic Games\UE_5.6"
#>
[CmdletBinding()]
param(
    [string[]] $EngineRoots = @(),
    [string]   $OutDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$uplugin  = Join-Path $repoRoot 'Plugins\ReactiveUI\ReactiveUI.uplugin'
if (-not (Test-Path $uplugin)) { Write-Error "Not found: $uplugin"; exit 1 }
$versionName = (Get-Content $uplugin -Raw | ConvertFrom-Json).VersionName
if ([string]::IsNullOrWhiteSpace($OutDir)) { $OutDir = Join-Path $repoRoot 'dist' }
New-Item -ItemType Directory -Force $OutDir | Out-Null

if ($EngineRoots.Count -eq 0) {
    $EngineRoots = @(Get-ChildItem 'C:\Program Files\Epic Games' -Directory -Filter 'UE_5.*' -ErrorAction SilentlyContinue |
                     Select-Object -ExpandProperty FullName)
}
if ($EngineRoots.Count -eq 0) {
    Write-Error ('No engine installs found under "C:\Program Files\Epic Games\UE_5.*". ' +
                 'Install UE via the Epic Games Launcher or pass -EngineRoots explicitly.')
    exit 1
}

foreach ($engine in $EngineRoots) {
    $runUat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'
    if (-not (Test-Path $runUat)) { Write-Warning "Skipping $engine (no RunUAT.bat)"; continue }
    $engineVer = Split-Path $engine -Leaf                # UE_5.6
    $shortVer  = $engineVer -replace '^UE_', ''          # 5.6
    $staging   = Join-Path $OutDir "staging-$shortVer"
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }

    Write-Host ''
    Write-Host "-- BuildPlugin against $engineVer" -ForegroundColor Cyan
    & cmd.exe /c "`"$runUat`" BuildPlugin -Plugin=`"$uplugin`" -Package=`"$staging`" -TargetPlatforms=Win64 -Rocket -StrictIncludes"
    if ($LASTEXITCODE -ne 0) { Write-Error "BuildPlugin failed for $engineVer (exit $LASTEXITCODE)"; exit 1 }

    # Fab wants source-only zips — Epic compiles the distributed binaries themselves.
    foreach ($d in 'Binaries', 'Intermediate', 'Saved') {
        $p = Join-Path $staging $d
        if (Test-Path $p) { Remove-Item $p -Recurse -Force }
    }

    $zip = Join-Path $OutDir "ReactiveUI-$versionName-UE$shortVer.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip
    Write-Host "   -> $zip" -ForegroundColor Green
}

Write-Host ''
Write-Host "Done. Remember: each zip's .uplugin must carry the matching `"EngineVersion`" before" -ForegroundColor Yellow
Write-Host "Fab upload (the release-process skill's checklist covers this + the compliance items)." -ForegroundColor Yellow
