# Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
# UETKX Visual Studio Extension — Local Build (Unity-sibling build-local.ps1, Node-server flavor).
# Builds the language server + client bundle, mirrors it into UetkxVsix\server, builds the VSIX.
#
# Usage:
#   .\build-local.ps1              # Build only
#   .\build-local.ps1 -Install     # Build + install into the main VS hive
#   .\build-local.ps1 -Debug       # Build + launch the VS experimental instance
#
# Prerequisites:
#   - Visual Studio 2022 with the "Visual Studio extension development" workload
#   - Node.js on PATH; one-time `npm ci` in ide-extensions\lsp-server and ide-extensions\vscode-uetkx

param(
    [switch]$Install,
    [switch]$Debug,
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repoRoot  = Resolve-Path "$PSScriptRoot\..\.."
$vsDir     = "$PSScriptRoot\UetkxVsix"
$serverDst = "$vsDir\server"
$lspDir    = "$repoRoot\ide-extensions\lsp-server"
$clientDir = "$repoRoot\ide-extensions\vscode-uetkx"
$vsixProj  = "$vsDir\UetkxVsix.csproj"

# -- Locate VS 2022 -----------------------------------------------------------
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath  = & $vsWhere -version "[17.0,18.0)" -property installationPath -latest 2>$null
if (-not $vsPath) {
    Write-Error "Visual Studio 2022 not found. Install it or adjust the version range."
}
$devenv = "$vsPath\Common7\IDE\devenv.exe"
$devCmd = "$vsPath\Common7\Tools\VsDevCmd.bat"

Write-Host "=== UETKX VS Extension - Local Build ===" -ForegroundColor Cyan
Write-Host "  Repo root:  $repoRoot"
Write-Host "  VS 2022:    $vsPath"
Write-Host "  Config:     $Configuration"
Write-Host ""

# -- Step 1: Build + bundle the language server -------------------------------
Write-Host "[1/3] Building language server + client bundle..." -ForegroundColor Yellow
foreach ($dir in $lspDir, $clientDir) {
    if (-not (Test-Path "$dir\node_modules")) {
        Write-Error "$dir\node_modules missing - run 'npm ci' there first (one-time prep)."
    }
}
# npm via cmd /d /c: stock PowerShell resolves npm to npm.ps1, which ExecutionPolicy blocks.
cmd /d /c "cd /d `"$lspDir`" && npm run build"
if ($LASTEXITCODE -ne 0) { Write-Error "lsp-server build failed." }
cmd /d /c "cd /d `"$clientDir`" && npm run build"
if ($LASTEXITCODE -ne 0) { Write-Error "client build (tsc + bundle-server.mjs) failed." }

# Mirror the bundle into the VSIX (static copy - the csproj packs server\** as Content).
if (Test-Path $serverDst) { Remove-Item $serverDst -Recurse -Force }
Copy-Item "$clientDir\server" $serverDst -Recurse
if (-not (Test-Path "$serverDst\server.js")) { Write-Error "mirror failed - $serverDst\server.js missing." }
Write-Host "[1/3] Server bundled and mirrored into UetkxVsix\server." -ForegroundColor Green
Write-Host ""

# -- Step 2: Build VSIX --------------------------------------------------------
Write-Host "[2/3] Building VSIX..." -ForegroundColor Yellow

# Clean obj to force VSIX regeneration (Unity-sibling scar: incremental build sometimes
# skips VSIX creation). /restore because PackageReference needs it on a fresh clone.
$objDir = "$vsDir\obj"
if (Test-Path $objDir) { Remove-Item $objDir -Recurse -Force }

# The Installer dir on PATH quiets VsDevCmd's internal "'vswhere.exe' is not recognized"
# stderr noise (it looks like a failure and buries real errors).
cmd /c "set `"PATH=%PATH%;${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer`" && call `"$devCmd`" -no_logo && msbuild `"$vsixProj`" /restore /p:Configuration=$Configuration /p:DeployExtension=false /t:Rebuild /v:minimal"
if ($LASTEXITCODE -ne 0) { Write-Error "VSIX build failed." }

$vsixPath = "$vsDir\bin\$Configuration\UetkxVsix.vsix"
if (-not (Test-Path $vsixPath)) {
    Write-Error "VSIX not found at $vsixPath after build."
}

$size = [math]::Round((Get-Item $vsixPath).Length / 1024)
Write-Host "[2/3] VSIX built: $vsixPath (${size}KB)" -ForegroundColor Green
Write-Host ""

# -- Step 3: Install or Debug ---------------------------------------------------
if ($Install) {
    Write-Host "[3/3] Installing into the main VS hive..." -ForegroundColor Yellow
    & "$vsPath\Common7\IDE\VSIXInstaller.exe" $vsixPath
} elseif ($Debug) {
    Write-Host "[3/3] Launching the VS 2022 experimental instance..." -ForegroundColor Yellow
    Write-Host "       Open a .uetkx file to activate the extension."
    Start-Process $devenv -ArgumentList "/rootsuffix Exp"
} else {
    Write-Host "[3/3] Build complete. Next steps:" -ForegroundColor Yellow
    Write-Host "  Install:  .\build-local.ps1 -Install"
    Write-Host "  Debug:    .\build-local.ps1 -Debug"
    Write-Host "  Manual:   Open UetkxVsix.csproj in VS2022, press F5 (StartAction is committed)"
}

Write-Host ""
Write-Host "Done." -ForegroundColor Cyan
