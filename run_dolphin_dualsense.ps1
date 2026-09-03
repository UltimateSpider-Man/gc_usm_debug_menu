[CmdletBinding()]
param(
    [string]$DolphinExe,
    [string]$IsoPath,
    [string]$UserDirectory,
    [ValidateSet('DualSense', 'DualShock4')]
    [string]$Controller = 'DualSense'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

if ([string]::IsNullOrWhiteSpace($DolphinExe)) {
    $DolphinExe = Join-Path $projectRoot '..\Dolphin-x64\Dolphin.exe'
}
if ([string]::IsNullOrWhiteSpace($IsoPath)) {
    $IsoPath = Join-Path $projectRoot 'dist\USM_GUTE52_DebugMenu_final.iso'
}
if ([string]::IsNullOrWhiteSpace($UserDirectory)) {
    $profileSuffix = if ($Controller -eq 'DualShock4') { 'ds4' } else { 'dualsense' }
    $UserDirectory = Join-Path $projectRoot "build\dolphin_${profileSuffix}_user"
}

$DolphinExe = [System.IO.Path]::GetFullPath($DolphinExe)
$IsoPath = [System.IO.Path]::GetFullPath($IsoPath)
$UserDirectory = [System.IO.Path]::GetFullPath($UserDirectory)

if (-not (Test-Path -LiteralPath $DolphinExe -PathType Leaf)) {
    throw "Dolphin executable not found: $DolphinExe"
}
if (-not (Test-Path -LiteralPath $IsoPath -PathType Leaf)) {
    throw "Patched ISO not found: $IsoPath"
}

$dolphinProcessName = [System.IO.Path]::GetFileNameWithoutExtension($DolphinExe)
$runningDolphin = @(
    Get-Process -Name $dolphinProcessName -ErrorAction SilentlyContinue |
        Where-Object {
            try {
                [string]::Equals(
                    [System.IO.Path]::GetFullPath($_.Path),
                    $DolphinExe,
                    [System.StringComparison]::OrdinalIgnoreCase)
            } catch {
                $false
            }
        }
)
if ($runningDolphin.Count -ne 0) {
    $runningIds = ($runningDolphin.Id | Sort-Object) -join ', '
    [Console]::Error.WriteLine(
        "Close the other Dolphin window(s) first (PID: $runningIds). " +
        "A second Dolphin process may not receive the controller Select/Guide input.")
    exit 2
}

$configDirectory = Join-Path $UserDirectory 'Config'
New-Item -ItemType Directory -Path $configDirectory -Force | Out-Null

$controllerConfigName = if ($Controller -eq 'DualShock4') {
    'GCPadNew_DS4.ini'
} else {
    'GCPadNew.ini'
}
foreach ($configName in @('Dolphin.ini', 'GCPadNew.ini', 'Logger.ini')) {
    $sourceName = if ($configName -eq 'GCPadNew.ini') {
        $controllerConfigName
    } else {
        $configName
    }
    $configSource = Join-Path $projectRoot "dolphin\$sourceName"
    if (-not (Test-Path -LiteralPath $configSource -PathType Leaf)) {
        throw "Required controller profile file not found: $configSource"
    }
    Copy-Item -LiteralPath $configSource -Destination (Join-Path $configDirectory $configName) -Force
}

$process = Start-Process -FilePath $DolphinExe -ArgumentList @(
    '-u', ('"' + $UserDirectory + '"'),
    '-b',
    '-e', ('"' + $IsoPath + '"')
) -PassThru

Write-Host "Started Dolphin PID $($process.Id) with the isolated $Controller profile:"
Write-Host $UserDirectory
