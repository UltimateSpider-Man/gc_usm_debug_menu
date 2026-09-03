[CmdletBinding()]
param(
    [string]$InputDol = "",
    [string]$OutputDol = "",
    [string]$PythonPath = "",
    [switch]$AutoOpenForTest,
    [ValidateSet(
        "Root", "Warp", "Game", "Missions", "DebugRender", "NglDebug", "AI",
        "EntityAnimations", "Script", "Progression", "SavedSettings",
        "Devopts", "CharSelect", "Options", "LevelSelect", "Memory",
        "SaveLoad", "Screenshot", "DistrictVariants"
    )]
    [string]$AutoOpenMenuForTest = "Root"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$WorkspaceRoot = Split-Path -Parent $ProjectRoot
$ExpectedCleanDolHash = "3DD0AD5EDE2EF9DF27A1ABA1BE9BF9CD5DFD7A14CAA4BB8DA396F2574C89565C"
if (-not $InputDol) {
    $DefaultDol = Join-Path $WorkspaceRoot "Ultimate Spider-Man [GUTE52]\sys\main.dol"
    $RecoveredCleanDol = Join-Path $ProjectRoot "build\recovered_clean_main.dol"
    $InputDol = @($DefaultDol, $RecoveredCleanDol) | Where-Object {
        (Test-Path -LiteralPath $_ -PathType Leaf) -and
        ((Get-FileHash -Algorithm SHA256 -LiteralPath $_).Hash -eq $ExpectedCleanDolHash)
    } | Select-Object -First 1
    if (-not $InputDol) {
        $InputDol = $DefaultDol
    }
}
if (-not $OutputDol) {
    $OutputDol = Join-Path $ProjectRoot "dist\sys\main.dol"
}

$GcAddress = Join-Path $WorkspaceRoot "gc_address.txt"
$XboxDecompile = Join-Path $WorkspaceRoot "Ultimatexbox_debug.c"
$Generated = Join-Path $ProjectRoot "generated\generated_data.h"
$Source = Join-Path $ProjectRoot "src\debug_menu_payload.c"
$Ds4MappingSource = Join-Path $ProjectRoot "src\dinput_ds4_mapping.c"
$Linker = Join-Path $ProjectRoot "linker.ld"
$BootstrapSource = Join-Path $ProjectRoot "src\bootstrap.S"
$BootstrapLinker = Join-Path $ProjectRoot "bootstrap.ld"
$BuildDir = Join-Path $ProjectRoot "build"
$Object = Join-Path $BuildDir "debug_menu_payload.o"
$Ds4MappingObject = Join-Path $BuildDir "dinput_ds4_mapping.o"
$Elf = Join-Path $BuildDir "payload.elf"
$Binary = Join-Path $BuildDir "payload.bin"
$Map = Join-Path $BuildDir "payload.map"
$BootstrapObject = Join-Path $BuildDir "bootstrap.o"
$BootstrapElf = Join-Path $BuildDir "bootstrap.elf"
$BootstrapBinary = Join-Path $BuildDir "bootstrap.bin"
$BootstrapMap = Join-Path $BuildDir "bootstrap.map"
$StagedBinary = Join-Path $BuildDir "staged_payload.bin"

foreach ($RequiredPath in @(
    $InputDol, $GcAddress, $XboxDecompile, $Source, $Ds4MappingSource, $Linker,
    (Join-Path $ProjectRoot "src\script_debug_catalog.inc"),
    $BootstrapSource, $BootstrapLinker
)) {
    if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
        throw "Required file not found: $RequiredPath"
    }
}

$ToolchainCandidates = @(
    "C:\SysGCC\powerpc-eabi\bin",
    "C:\devkitPro\devkitPPC\bin"
)
$Toolchain = $ToolchainCandidates | Where-Object {
    Test-Path -LiteralPath (Join-Path $_ "powerpc-eabi-gcc.exe") -PathType Leaf
} | Select-Object -First 1
if (-not $Toolchain) {
    throw "powerpc-eabi toolchain not found (checked C:\SysGCC and C:\devkitPro)."
}

$PythonCandidates = @()
if ($PythonPath) {
    $PythonCandidates += $PythonPath
}
foreach ($CommandName in @("python.exe", "python3.exe")) {
    $PythonCommand = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($PythonCommand) {
        $PythonCandidates += $PythonCommand.Source
    }
}
if ($env:LOCALAPPDATA) {
    $PythonInstallRoot = Join-Path $env:LOCALAPPDATA "Programs\Python"
    if (Test-Path -LiteralPath $PythonInstallRoot -PathType Container) {
        $PythonCandidates += Get-ChildItem -LiteralPath $PythonInstallRoot -Directory |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "python.exe" }
    }
}

$Python = $null
foreach ($Candidate in $PythonCandidates | Select-Object -Unique) {
    if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) {
        continue
    }
    $SavedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $Candidate -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)" 2>$null
    $CandidateExitCode = $LASTEXITCODE
    $ErrorActionPreference = $SavedErrorActionPreference
    if ($CandidateExitCode -eq 0) {
        $Python = $Candidate
        break
    }
}
if (-not $Python) {
    throw "Python 3.8+ was not found. Pass -PythonPath or install Python for the current user."
}
$Gcc = Join-Path $Toolchain "powerpc-eabi-gcc.exe"
$Objcopy = Join-Path $Toolchain "powerpc-eabi-objcopy.exe"
$Nm = Join-Path $Toolchain "powerpc-eabi-nm.exe"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputDol) | Out-Null

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Program,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

Invoke-Checked $Python @(
    (Join-Path $ProjectRoot "tools\generate_data.py"),
    "--gc-address", $GcAddress,
    "--xbox-decompile", $XboxDecompile,
    "--dol", $InputDol,
    "--output", $Generated
)

$CompileFlags = @(
    "-std=c11", "-Os", "-mcpu=750", "-meabi", "-mhard-float", "-msdata=none",
    "-ffreestanding", "-fno-builtin", "-fno-common", "-fno-pic",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
    "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror"
)
if ($AutoOpenForTest) {
    $CompileFlags += "-DDEBUG_MENU_AUTO_OPEN=1"
    switch ($AutoOpenMenuForTest) {
        "Warp" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_WARP=1" }
        "Game" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_GAME=1" }
        "Missions" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_MISSIONS=1" }
        "DebugRender" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_DEBUG_RENDER=1" }
        "NglDebug" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_NGL_DEBUG=1" }
        "AI" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_AI=1" }
        "EntityAnimations" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_ENTITY_ANIMATIONS=1" }
        "Script" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_SCRIPT=1" }
        "Progression" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_PROGRESSION=1" }
        "SavedSettings" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_SAVED_SETTINGS=1" }
        "Devopts" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_DEVOPTS=1" }
        "CharSelect" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_CHAR_SELECT=1" }
        "Options" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_OPTIONS=1" }
        "LevelSelect" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_LEVEL_SELECT=1" }
        "Memory" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_MEMORY=1" }
        "SaveLoad" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_SAVE_LOAD=1" }
        "Screenshot" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_SCREENSHOT=1" }
        "DistrictVariants" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_DISTRICT_VARIANTS=1" }
    }
}
$CompileFlags += @("-I$($ProjectRoot)\generated", "-c")
Invoke-Checked $Gcc ($CompileFlags + @($Source, "-o", $Object))
Invoke-Checked $Gcc ($CompileFlags + @($Ds4MappingSource, "-o", $Ds4MappingObject))

$LinkFlags = @(
    "-nostdlib", "-nodefaultlibs", "-nostartfiles",
    "-mcpu=750", "-meabi", "-mhard-float", "-msdata=none",
    "-Wl,-T,$Linker", "-Wl,-Map,$Map", "-Wl,--gc-sections",
    "-o", $Elf, $Object, $Ds4MappingObject, "-lgcc"
)
Invoke-Checked $Gcc $LinkFlags
Invoke-Checked $Objcopy @("-O", "binary", "--gap-fill", "0", $Elf, $Binary)

$Symbols = & $Nm -n $Elf
if ($LASTEXITCODE -ne 0) {
    throw "$Nm failed with exit code $LASTEXITCODE"
}
if (-not ($Symbols | Where-Object { $_ -match '^81780000\s+[Tt]\s+debug_menu_hook$' })) {
    throw "Payload entry symbol is not linked at reserved address 0x81780000."
}
$PayloadSize = (Get-Item -LiteralPath $Binary).Length
if ($PayloadSize -le 0 -or $PayloadSize -gt 0x12000) {
    throw "Payload size $PayloadSize crosses the first runtime cache at 0x81792000."
}

# Keep every fixed MEM1 work area disjoint. These ends mirror the C layouts:
# 1024 x 0x14 mission rows; five variant arrays ending at +0x4800; and two
# two 256-entry actor arrays plus the AI-core pointer cache ending at +0xC00.
$PayloadBase = 0x81780000L
$AuxCacheBase = 0x81792000L
$AuxCacheEnd = 0x81792C80L
$AnimationCacheBase = 0x8179E000L
$AnimationCacheEnd = $AnimationCacheBase + (512L * 4L)
$MissionCacheBase = 0x817A0000L
$MissionCacheEnd = $MissionCacheBase + (1024L * 0x14L)
$VariantCacheBase = 0x817B0000L
$VariantCacheEnd = $VariantCacheBase + 0x4800L
$ActorCacheBase = 0x817B6000L
$ActorCacheEnd = $ActorCacheBase + 0x0C00L
$Mem1End = 0x81800000L
if ($PayloadBase + [long]$PayloadSize -gt $AuxCacheBase) {
    throw "Payload overlaps the first auxiliary cache."
}
if ($AuxCacheEnd -gt $AnimationCacheBase) {
    throw "Auxiliary caches overlap the animation cache."
}
if ($AnimationCacheEnd -gt $MissionCacheBase) {
    throw "Animation cache overlaps the mission cache."
}
if ($MissionCacheEnd -gt $VariantCacheBase) {
    throw "Mission cache overlaps the variant cache."
}
if ($VariantCacheEnd -gt $ActorCacheBase) {
    throw "Variant cache overlaps the actor cache."
}
if ($ActorCacheEnd -gt $Mem1End) {
    throw "Actor cache crosses the physical end of MEM1."
}

$PayloadSizeHex = "0x{0:X}" -f $PayloadSize
Invoke-Checked $Gcc @(
    "-x", "assembler-with-cpp", "-mcpu=750", "-meabi", "-mhard-float",
    "-ffreestanding", "-DPAYLOAD_COPY_SIZE=$PayloadSizeHex",
    "-c", $BootstrapSource, "-o", $BootstrapObject
)
Invoke-Checked $Gcc @(
    "-nostdlib", "-nodefaultlibs", "-nostartfiles",
    "-mcpu=750", "-meabi", "-mhard-float",
    "-Wl,-T,$BootstrapLinker", "-Wl,-Map,$BootstrapMap",
    "-Wl,--gc-sections", "-o", $BootstrapElf, $BootstrapObject
)
Invoke-Checked $Objcopy @("-O", "binary", "--gap-fill", "0", $BootstrapElf, $BootstrapBinary)

$BootstrapSymbols = & $Nm -n $BootstrapElf
if ($LASTEXITCODE -ne 0) {
    throw "$Nm failed with exit code $LASTEXITCODE"
}
if (-not ($BootstrapSymbols | Where-Object { $_ -match '^80626000\s+[Tt]\s+bootstrap_entry$' })) {
    throw "Bootstrap entry symbol is not linked at staging address 0x80626000."
}
$BootstrapBytes = [IO.File]::ReadAllBytes($BootstrapBinary)
$PayloadBytes = [IO.File]::ReadAllBytes($Binary)
if ($BootstrapBytes.Length -le 0 -or ($BootstrapBytes.Length % 32) -ne 0) {
    throw "Bootstrap size must be a positive multiple of 32 bytes."
}
$StagedBytes = New-Object byte[] ($BootstrapBytes.Length + $PayloadBytes.Length)
[Buffer]::BlockCopy($BootstrapBytes, 0, $StagedBytes, 0, $BootstrapBytes.Length)
[Buffer]::BlockCopy(
    $PayloadBytes, 0, $StagedBytes, $BootstrapBytes.Length, $PayloadBytes.Length
)
if (0x80626000 + $StagedBytes.Length -gt 0x80636000) {
    throw "Staged image crosses the retail startup stack at 0x80636000."
}
[IO.File]::WriteAllBytes($StagedBinary, $StagedBytes)

Invoke-Checked $Python @(
    (Join-Path $ProjectRoot "tools\inject_dol.py"),
    $InputDol, $StagedBinary, $OutputDol
)

Write-Host "Build complete: $OutputDol"
Write-Host ("Payload size: 0x{0:X} bytes" -f $PayloadSize)
Write-Host ("Bootstrap size: 0x{0:X} bytes" -f $BootstrapBytes.Length)
Write-Host ("Staged image size: 0x{0:X} bytes" -f $StagedBytes.Length)
