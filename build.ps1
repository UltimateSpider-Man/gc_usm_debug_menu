[CmdletBinding()]
param(
    [string]$InputDol = "",
    [string]$OutputDol = "",
    [switch]$AutoOpenForTest,
    [ValidateSet("Root", "Pause", "Missions", "CharSelect")]
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
$Linker = Join-Path $ProjectRoot "linker.ld"
$BootstrapSource = Join-Path $ProjectRoot "src\bootstrap.S"
$BootstrapLinker = Join-Path $ProjectRoot "bootstrap.ld"
$BuildDir = Join-Path $ProjectRoot "build"
$Object = Join-Path $BuildDir "debug_menu_payload.o"
$Elf = Join-Path $BuildDir "payload.elf"
$Binary = Join-Path $BuildDir "payload.bin"
$Map = Join-Path $BuildDir "payload.map"
$BootstrapObject = Join-Path $BuildDir "bootstrap.o"
$BootstrapElf = Join-Path $BuildDir "bootstrap.elf"
$BootstrapBinary = Join-Path $BuildDir "bootstrap.bin"
$BootstrapMap = Join-Path $BuildDir "bootstrap.map"
$StagedBinary = Join-Path $BuildDir "staged_payload.bin"

foreach ($RequiredPath in @(
    $InputDol, $GcAddress, $XboxDecompile, $Source, $Linker,
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

$PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $PythonCommand) {
    throw "python.exe was not found on PATH."
}
$Python = $PythonCommand.Source
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
    "-std=c11", "-O2", "-mcpu=750", "-meabi", "-mhard-float", "-msdata=none",
    "-ffreestanding", "-fno-builtin", "-fno-common", "-fno-pic",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
    "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror"
)
if ($AutoOpenForTest) {
    $CompileFlags += "-DDEBUG_MENU_AUTO_OPEN=1"
    switch ($AutoOpenMenuForTest) {
        "Pause" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_PAUSE=1" }
        "Missions" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_MISSIONS=1" }
        "CharSelect" { $CompileFlags += "-DDEBUG_MENU_AUTO_TARGET_CHAR_SELECT=1" }
    }
}
$CompileFlags += @("-I$($ProjectRoot)\generated", "-c", $Source, "-o", $Object)
Invoke-Checked $Gcc $CompileFlags

$LinkFlags = @(
    "-nostdlib", "-nodefaultlibs", "-nostartfiles",
    "-mcpu=750", "-meabi", "-mhard-float", "-msdata=none",
    "-Wl,-T,$Linker", "-Wl,-Map,$Map", "-Wl,--gc-sections",
    "-o", $Elf, $Object
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
if ($PayloadSize -le 0 -or $PayloadSize -gt 0x80000) {
    throw "Payload size $PayloadSize is outside the reserved 512 KiB region."
}

$PayloadSizeHex = "0x{0:X}" -f $PayloadSize
Invoke-Checked $Gcc @(
    "-x", "assembler-with-cpp", "-mcpu=750", "-meabi", "-mhard-float",
    "-ffreestanding", "-c", $BootstrapSource, "-o", $BootstrapObject
)
Invoke-Checked $Gcc @(
    "-nostdlib", "-nodefaultlibs", "-nostartfiles",
    "-mcpu=750", "-meabi", "-mhard-float",
    "-Wl,-T,$BootstrapLinker", "-Wl,-Map,$BootstrapMap",
    "-Wl,--defsym,payload_copy_size=$PayloadSizeHex",
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
