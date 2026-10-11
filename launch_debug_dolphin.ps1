param(
    [string]$SourceIso = "$env:USERPROFILE\Desktop\POCO F7\GameCube iso\Ultimate Spider-Man [GUTE52].iso",
    [string]$GameIni = "$env:USERPROFILE\Desktop\POCO F7\Download\Ultimate Spider-Man [GUTP52]\root\game.ini",
    [string]$Dolphin = "$env:USERPROFILE\Desktop\usm_debug_menu gamecube\Dolphin-x64\Dolphin.exe",
    [string]$Python,
    [string]$UserDirectory
)
$ErrorActionPreference = 'Stop'
if (-not $Python) {
    $candidate = Get-Command python.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.Source -notlike '*\WindowsApps\*' } | Select-Object -First 1
    if ($candidate) { $Python = $candidate.Source }
    else {
        $bundled = "$env:LOCALAPPDATA\CodexTools\blender-4.5.3-windows-x64\4.5\python\bin\python.exe"
        if (Test-Path -LiteralPath $bundled -PathType Leaf) { $Python = $bundled }
        else { throw 'Pass -Python with the path to a 64-bit Python 3.11+ executable.' }
    }
}
foreach ($inputFile in @($SourceIso, $GameIni, $Dolphin, $Python)) {
    if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) { throw "File not found: $inputFile" }
}
$companionArguments = @(
    (Join-Path $PSScriptRoot 'tools\dolphin_ini_companion.py'),
    '--source', $SourceIso, '--game-ini', $GameIni, '--dolphin', $Dolphin,
    '--dol', (Join-Path $PSScriptRoot 'main.dol'),
    '--symbols', (Join-Path $PSScriptRoot 'build\gamecube\usm_debug_menu.symbols.txt'),
    '--iso', (Join-Path $PSScriptRoot 'build\debug-disc\usm-debug-game-replay.iso')
)
if ($UserDirectory) { $companionArguments += @('--user', $UserDirectory) }
& $Python @companionArguments
if ($LASTEXITCODE -ne 0) { throw "Dolphin INI companion exited with code $LASTEXITCODE" }
