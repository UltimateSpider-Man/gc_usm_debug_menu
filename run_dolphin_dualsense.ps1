[CmdletBinding()]
param(
    [string]$DolphinExe,
    [string]$IsoPath,
    [string]$UserDirectory
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
    $UserDirectory = Join-Path $projectRoot 'build\dolphin_dualsense_user'
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
        "A second Dolphin process may not receive the DualSense PS/Guide input.")
    exit 2
}

$configDirectory = Join-Path $UserDirectory 'Config'
New-Item -ItemType Directory -Path $configDirectory -Force | Out-Null

foreach ($configName in @('Dolphin.ini', 'GCPadNew.ini', 'Logger.ini')) {
    $configSource = Join-Path $projectRoot "dolphin\$configName"
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

Write-Host "Started Dolphin PID $($process.Id) with the isolated DualSense profile:"
Write-Host $UserDirectory
