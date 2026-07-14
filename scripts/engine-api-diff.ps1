# Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
<#
.SYNOPSIS
    Compares the Slate/UMG widget API surface between two Unreal Engine versions.
    The Unreal analogue of the Unity sibling's automation~/unity-api-diff.ps1.

.DESCRIPTION
    C++ has no assembly reflection, so this scans the engines' PUBLIC headers —
    Runtime/SlateCore, Runtime/Slate, and Runtime/UMG — and diffs the surface
    ReactiveUI wraps:
      - Slate widget classes (an S-class deriving from another S-class; UE 5.4+
        exports per-FUNCTION, so widget classes carry no *_API macro)
      - Per-widget FArguments surface: SLATE_ARGUMENT / SLATE_ATTRIBUTE / SLATE_EVENT,
        associated with the nearest preceding widget class in the same header
      - UMG widget classes (U-classes deriving from a *Widget base)

    It IS a text scan, not a compiler — exact for the engine's conventions, stated
    honestly. Output is structured JSON for both human review and AI consumption;
    feed the report to the `engine-catchup` skill's classification step.

.PARAMETER From
    The "old" engine version, e.g. "5.6". Resolved to
    "C:\Program Files\Epic Games\UE_<From>" unless -FromRoot is given.

.PARAMETER To
    The "new" engine version, e.g. "5.7".

.PARAMETER FromRoot
    Explicit engine root for the old version (the folder containing Engine\). Overrides -From.

.PARAMETER ToRoot
    Explicit engine root for the new version. Overrides -To.

.PARAMETER OutFile
    Optional path for the JSON report. If omitted, writes to stdout.
    Convention: scripts\diff-reports\<from>-to-<to>.json (gitignored).

.EXAMPLE
    .\scripts\engine-api-diff.ps1 -From 5.6 -To 5.7 -OutFile .\scripts\diff-reports\5.6-to-5.7.json
#>
[CmdletBinding()]
param(
    [string]$From,
    [string]$To,
    [string]$FromRoot,
    [string]$ToRoot,
    [string]$OutFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-EngineRoot([string]$Version, [string]$Explicit) {
    if (-not [string]::IsNullOrWhiteSpace($Explicit)) {
        if (-not (Test-Path $Explicit)) { throw "Engine root not found: $Explicit" }
        return (Resolve-Path $Explicit).Path
    }
    if ([string]::IsNullOrWhiteSpace($Version)) { throw 'Provide -From/-To versions or -FromRoot/-ToRoot paths.' }
    $candidate = "C:\Program Files\Epic Games\UE_$Version"
    if (-not (Test-Path $candidate)) { throw "UE $Version not found at $candidate (install via the Epic launcher, or pass -FromRoot/-ToRoot)." }
    return $candidate
}

# A widget = an S-class deriving from another S-class (skips forward decls + non-widget S types;
# SWidget itself derives from a non-S base and is deliberately out — it isn't wrappable).
$script:ClassRx = [regex]'(?m)^\s*class\s+(?:[A-Z_]+_API\s+)?(?:UE_DEPRECATED\([^)]*\)\s+)?(S[A-Z]\w*)\s*(?:final\s*)?[\r\n\t ]*:[\r\n\t ]*(?:public|protected)\s+S\w+'
$script:ArgRx = [regex]'SLATE_ARGUMENT(?:_DEPRECATED)?\s*\(\s*[^,]+,\s*(\w+)\s*[,)]'
$script:AttrRx = [regex]'SLATE_ATTRIBUTE\s*\(\s*[^,]+,\s*(\w+)\s*[,)]'
$script:EventRx = [regex]'SLATE_EVENT\s*\(\s*[^,]+,\s*(\w+)\s*[,)]'
$script:UmgRx = [regex]'(?m)^\s*class\s+(?:[A-Z_]+_API\s+)?(?:UE_DEPRECATED\([^)]*\)\s+)?(U[A-Z]\w*)\s*(?:final\s*)?[\r\n\t ]*:[\r\n\t ]*public\s+U\w*Widget\w*'

# Associate each macro match with the nearest PRECEDING widget class declared in the same file.
function Add-MacroOwners($Text, $ClassMatches, $Rx, $Widgets, [string]$Bucket) {
    foreach ($m in $Rx.Matches($Text)) {
        $owner = $null
        foreach ($cm in $ClassMatches) {
            if ($cm.Index -lt $m.Index) { $owner = $cm.Groups[1].Value } else { break }
        }
        if ($null -ne $owner) {
            $Widgets[$owner][$Bucket] += $m.Groups[1].Value
        }
    }
}

function Get-EngineSurface([string]$EngineRoot) {
    $slateDirs = @(
        (Join-Path $EngineRoot 'Engine\Source\Runtime\SlateCore\Public'),
        (Join-Path $EngineRoot 'Engine\Source\Runtime\Slate\Public')
    )
    $umgDir = Join-Path $EngineRoot 'Engine\Source\Runtime\UMG\Public'

    $slateWidgets = @{}
    foreach ($dir in $slateDirs) {
        if (-not (Test-Path $dir)) { continue }
        foreach ($file in (Get-ChildItem -Path $dir -Recurse -Filter '*.h')) {
            $text = [System.IO.File]::ReadAllText($file.FullName)
            $classMatches = @($script:ClassRx.Matches($text))
            if ($classMatches.Count -eq 0) { continue }
            foreach ($cm in $classMatches) {
                $name = $cm.Groups[1].Value
                if (-not $slateWidgets.ContainsKey($name)) {
                    $slateWidgets[$name] = @{ Arguments = @(); Attributes = @(); Events = @() }
                }
            }
            Add-MacroOwners $text $classMatches $script:ArgRx $slateWidgets 'Arguments'
            Add-MacroOwners $text $classMatches $script:AttrRx $slateWidgets 'Attributes'
            Add-MacroOwners $text $classMatches $script:EventRx $slateWidgets 'Events'
        }
    }

    $umgWidgets = @()
    if (Test-Path $umgDir) {
        foreach ($file in (Get-ChildItem -Path $umgDir -Recurse -Filter '*.h')) {
            $text = [System.IO.File]::ReadAllText($file.FullName)
            foreach ($m in $script:UmgRx.Matches($text)) {
                $umgWidgets += $m.Groups[1].Value
            }
        }
    }

    return @{ SlateWidgets = $slateWidgets; UmgWidgets = @($umgWidgets | Sort-Object -Unique) }
}

function Get-ListDiff([string[]]$Old, [string[]]$New) {
    $oldSet = @{}; foreach ($o in $Old) { $oldSet[$o] = $true }
    $newSet = @{}; foreach ($n in $New) { $newSet[$n] = $true }
    $added = @(); foreach ($n in $New) { if (-not $oldSet.ContainsKey($n)) { $added += $n } }
    $removed = @(); foreach ($o in $Old) { if (-not $newSet.ContainsKey($o)) { $removed += $o } }
    return @{ Added = @($added | Sort-Object -Unique); Removed = @($removed | Sort-Object -Unique) }
}

$fromRootResolved = Resolve-EngineRoot $From $FromRoot
$toRootResolved = Resolve-EngineRoot $To $ToRoot
Write-Host "-- scanning FROM: $fromRootResolved" -ForegroundColor Cyan
$old = Get-EngineSurface $fromRootResolved
Write-Host "-- scanning TO:   $toRootResolved" -ForegroundColor Cyan
$new = Get-EngineSurface $toRootResolved

$widgetDiff = Get-ListDiff @($old.SlateWidgets.Keys) @($new.SlateWidgets.Keys)
$changedWidgets = @()
foreach ($name in ($new.SlateWidgets.Keys | Sort-Object)) {
    if (-not $old.SlateWidgets.ContainsKey($name)) { continue }
    $argDiff = Get-ListDiff $old.SlateWidgets[$name].Arguments $new.SlateWidgets[$name].Arguments
    $attrDiff = Get-ListDiff $old.SlateWidgets[$name].Attributes $new.SlateWidgets[$name].Attributes
    $eventDiff = Get-ListDiff $old.SlateWidgets[$name].Events $new.SlateWidgets[$name].Events
    if ($argDiff.Added.Count -or $argDiff.Removed.Count -or $attrDiff.Added.Count -or $attrDiff.Removed.Count -or $eventDiff.Added.Count -or $eventDiff.Removed.Count) {
        $changedWidgets += @{
            Widget = $name
            Arguments = $argDiff
            Attributes = $attrDiff
            Events = $eventDiff
        }
    }
}
$umgDiff = Get-ListDiff $old.UmgWidgets $new.UmgWidgets

$report = @{
    Tool = 'engine-api-diff.ps1 (header scan - Slate FArguments surface + widget class sets)'
    From = $fromRootResolved
    To = $toRootResolved
    GeneratedAt = (Get-Date).ToString('yyyy-MM-ddTHH:mm:ss')
    Summary = @{
        SlateWidgetsFrom = $old.SlateWidgets.Count
        SlateWidgetsTo = $new.SlateWidgets.Count
        SlateWidgetsAdded = $widgetDiff.Added.Count
        SlateWidgetsRemoved = $widgetDiff.Removed.Count
        SlateWidgetsChanged = $changedWidgets.Count
        UmgWidgetsAdded = $umgDiff.Added.Count
        UmgWidgetsRemoved = $umgDiff.Removed.Count
    }
    SlateWidgets = @{ Added = $widgetDiff.Added; Removed = $widgetDiff.Removed; Changed = $changedWidgets }
    UmgWidgets = $umgDiff
}

$json = $report | ConvertTo-Json -Depth 8
if ([string]::IsNullOrWhiteSpace($OutFile)) {
    $json
} else {
    $dir = Split-Path -Parent $OutFile
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
    [System.IO.File]::WriteAllText($OutFile, $json, (New-Object System.Text.UTF8Encoding $false))
    Write-Host "-- report written: $OutFile" -ForegroundColor Green
    Write-Host ("   Slate: +{0} / -{1} widgets, {2} changed; UMG: +{3} / -{4}" -f `
        $report.Summary.SlateWidgetsAdded, $report.Summary.SlateWidgetsRemoved, $report.Summary.SlateWidgetsChanged, `
        $report.Summary.UmgWidgetsAdded, $report.Summary.UmgWidgetsRemoved)
}
