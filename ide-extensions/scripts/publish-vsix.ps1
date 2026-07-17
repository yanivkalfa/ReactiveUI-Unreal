# Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
<#
.SYNOPSIS
    Build, package, and publish the UETKX Visual Studio 2022 extension.
    LOCAL mirror of publish.yml's vs2022 leg — ported from the ReactiveUI-Godot repo's
    publish-vsix.ps1; unusable until Phase 5 creates ide-extensions/visual-studio/UetkxVsix/.

.DESCRIPTION
    1. Build the TypeScript language server + bundle it into UetkxVsix/server
    2. Bundle a Node runtime (fetch-node.ps1) so the VSIX is self-contained
    3. Generate overview.md from the centralized changelog
    4. msbuild the VSIX (Restore / Build / CreateVsixContainer)
    5. (unless -LocalOnly) VsixPublisher publish to the Visual Studio Marketplace

    Requires VS2022 with the "Visual Studio extension development" workload and Node on PATH.
    Token resolves from -PAT, then $env:VS_MARKETPLACE_TOKEN, then publisher-secrets.json
    { vsMarketplaceToken }.

.PARAMETER PAT              VS Marketplace PAT. Falls back to env / secrets file.
.PARAMETER LocalOnly        Build the VSIX only; do not publish.
.PARAMETER SkipServerBuild  Reuse an existing UetkxVsix/server bundle.
#>
[CmdletBinding()]
param(
    [string] $PAT = '',
    [switch] $LocalOnly,
    [switch] $SkipServerBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir    = Split-Path -Parent $MyInvocation.MyCommand.Path
$ideRoot      = Split-Path -Parent $scriptDir
$repoRoot     = Split-Path -Parent $ideRoot
$vsixDir      = Join-Path $ideRoot 'visual-studio\UetkxVsix'
$serverSrcDir = Join-Path $ideRoot 'lsp-server'
$extensionDir = Join-Path $ideRoot 'vscode-uetkx'
$changelogTool= Join-Path $scriptDir 'changelog.mjs'
$secretsPath  = Join-Path $repoRoot 'publisher-secrets.json'

if (-not (Test-Path $vsixDir)) {
    Write-Error ("VSIX directory not found: $vsixDir`n" +
                 'The VS2022 extension is created in Phase 5 (MASTER_PLAN) — this script is its local publish rail.')
    exit 1
}

function Write-Step([string] $m) { Write-Host ''; Write-Host "-- $m" -ForegroundColor Cyan }
function Invoke-InDir([string] $cwd, [string] $cmd) {
    Write-Host "   > $cmd" -ForegroundColor DarkGray
    $prev = $PWD; Set-Location $cwd
    try { cmd.exe /c $cmd; if ($LASTEXITCODE -ne 0) { throw "exited $LASTEXITCODE" } } finally { Set-Location $prev }
}

# ── 1. server bundle ──────────────────────────────────────────────────────────
if (-not $SkipServerBuild) {
    Write-Step 'Building + bundling the language server into the VSIX'
    Invoke-InDir $serverSrcDir 'npm install'
    Invoke-InDir $serverSrcDir 'npm run build'
    Invoke-InDir $extensionDir 'npm install'
    Invoke-InDir $extensionDir 'node scripts/bundle-server.mjs'
    $dest = Join-Path $vsixDir 'server'
    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    Copy-Item -Recurse -Force (Join-Path $extensionDir 'server') $dest
    Write-Host '  Server bundled into UetkxVsix/server.' -ForegroundColor Green

    # Bundle a Node runtime so the VSIX is self-contained (no Node on the end user's PATH).
    # Without this, -LocalOnly silently produces a VSIX missing server/node.exe — the exact
    # failure the sibling repo's publish job verifies against.
    Write-Step 'Bundling a Node runtime (fetch-node.ps1)'
    & (Join-Path $vsixDir 'fetch-node.ps1')
}

# §5: bundle the pinned clangd (22.1.6, sha256-verified) so embedded-C++ intelligence works
# with zero machine setup. Cached zip makes repeat runs cheap.
Write-Step 'Bundling clangd (fetch-clangd.mjs --target vs2022)'
& node (Join-Path $scriptDir 'fetch-clangd.mjs') --target vs2022
if ($LASTEXITCODE -ne 0) { throw 'fetch-clangd.mjs failed' }

# ── 2. overview.md ────────────────────────────────────────────────────────────
Write-Step 'Generating overview.md'
& node $changelogTool extract-overview --ide vs2022 --template (Join-Path $vsixDir 'overview-template.md') --out (Join-Path $vsixDir 'overview.md')

# ── 3. msbuild VSIX ───────────────────────────────────────────────────────────
Write-Step 'Building the VSIX'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Write-Error 'vswhere.exe not found — install Visual Studio 2022.'; exit 1 }
$vsInstall = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsInstall) { Write-Error 'No Visual Studio with MSBuild found.'; exit 1 }
$msbuild = Join-Path $vsInstall 'MSBuild\Current\Bin\MSBuild.exe'
$csproj = Join-Path $vsixDir 'UetkxVsix.csproj'
& $msbuild $csproj -t:Restore -p:Configuration=Release -v:minimal
& $msbuild $csproj -t:Build -p:Configuration=Release -p:DeployExtension=false -v:minimal
& $msbuild $csproj -t:CreateVsixContainer -p:Configuration=Release -p:DeployExtension=false -v:minimal

$vsix = Get-ChildItem -Path $vsixDir -Recurse -Filter '*.vsix' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $vsix) { Write-Error 'No .vsix produced.'; exit 1 }
Write-Host "  Built: $($vsix.FullName)" -ForegroundColor Green

# ── 4. publish ────────────────────────────────────────────────────────────────
if ($LocalOnly) { Write-Host '  Marketplace publish skipped (-LocalOnly).' -ForegroundColor DarkGray; return }

$token = $PAT
if ([string]::IsNullOrWhiteSpace($token)) { $token = $env:VS_MARKETPLACE_TOKEN }
if ([string]::IsNullOrWhiteSpace($token) -and (Test-Path $secretsPath)) {
    try { $token = (Get-Content $secretsPath -Raw | ConvertFrom-Json).vsMarketplaceToken } catch { }
}
if ([string]::IsNullOrWhiteSpace($token)) { Write-Error 'No VS Marketplace token (-PAT / $env:VS_MARKETPLACE_TOKEN / secrets file).'; exit 1 }

Write-Step 'Publishing to the Visual Studio Marketplace'
$pub = Get-ChildItem -Path "$env:USERPROFILE\.nuget\packages\microsoft.vssdk.buildtools" -Recurse -Filter 'VsixPublisher.exe' -ErrorAction SilentlyContinue |
       Where-Object { $_.DirectoryName -match 'tools[\\/]vssdk[\\/]bin$' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $pub) { Write-Error 'VsixPublisher.exe not found in the NuGet cache.'; exit 1 }
$out = & $pub.FullName publish -payload $vsix.FullName -publishManifest (Join-Path $vsixDir 'publishManifest.json') -personalAccessToken $token 2>&1 | Out-String
Write-Host $out
if ($out -match 'Uploaded') { Write-Host '  Marketplace upload confirmed.' -ForegroundColor Green }
else { Write-Warning 'No upload confirmation found in VsixPublisher output.' }
