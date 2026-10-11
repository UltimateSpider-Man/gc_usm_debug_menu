[CmdletBinding()]
param(
    [string]$InputDol = "",
    [string]$BuildDir = "",
    [string]$OutputDol = "",
    [string]$BackupDol = "",
    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string]$ExpectedDolSha256 = "3dd0ad5ede2ef9df27a1aba1be9bf9cd5dfd7a14caa4bb8da396f2574c89565c",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build\gamecube" }
$SourceRoot = Join-Path $RepoRoot "gamecube"
$LinkerScript = Join-Path $SourceRoot "linker.ld"
$Injector = Join-Path $RepoRoot "tools\inject_gamecube_payload.py"

if (-not $InputDol) {
    $RootBackup = Join-Path $RepoRoot "main.original.dol"
    $InputDol = if (Test-Path -LiteralPath $RootBackup -PathType Leaf) {
        $RootBackup
    } else {
        Join-Path $RepoRoot "main.dol"
    }
}

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function Resolve-DevkitTool {
    param([Parameter(Mandatory = $true)][string]$Name)

    $Candidates = [Collections.Generic.List[string]]::new()
    if ($env:DEVKITPPC) {
        $Candidates.Add((Join-Path $env:DEVKITPPC "bin\$Name.exe"))
        $Candidates.Add((Join-Path $env:DEVKITPPC "bin\$Name"))
    }
    if ($env:DEVKITPRO) {
        $Candidates.Add((Join-Path $env:DEVKITPRO "devkitPPC\bin\$Name.exe"))
        $Candidates.Add((Join-Path $env:DEVKITPRO "devkitPPC\bin\$Name"))
    }
    $Candidates.Add((Join-Path "C:\devkitPro\devkitPPC\bin" "$Name.exe"))

    foreach ($Candidate in $Candidates) {
        if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            return (Get-Item -LiteralPath $Candidate).FullName
        }
    }
    $Command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($Command) {
        return $Command.Source
    }
    throw "Cannot find $Name. Install devkitPPC or set DEVKITPPC/DEVKITPRO."
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE"
    }
}

$InputDol = Get-AbsolutePath $InputDol
$BuildDir = Get-AbsolutePath $BuildDir
if (-not $OutputDol) {
    $OutputDol = Join-Path $BuildDir "main_patched.dol"
} else {
    $OutputDol = Get-AbsolutePath $OutputDol
}
if (-not $BackupDol) {
    $BackupDol = Join-Path $BuildDir "main_original.dol"
} else {
    $BackupDol = Get-AbsolutePath $BackupDol
}

foreach ($RequiredFile in @($InputDol, $LinkerScript, $Injector)) {
    if (-not (Test-Path -LiteralPath $RequiredFile -PathType Leaf)) {
        throw "Required file not found: $RequiredFile"
    }
}

try {
    $Cxx = Resolve-DevkitTool "powerpc-eabi-g++"
} catch {
    $Wsl = Get-Command wsl -ErrorAction SilentlyContinue
    if (-not $Wsl) { throw }
    function Convert-ToWslPath([string]$Path) {
        $Converted = & $Wsl.Source --exec wslpath -a -u $Path
        if ($LASTEXITCODE -ne 0) { throw "Cannot translate WSL path: $Path" }
        return $Converted.Trim()
    }
    $WslArguments = @(
        "--exec", "bash", (Convert-ToWslPath (Join-Path $RepoRoot "build_gamecube.sh")),
        "--input", (Convert-ToWslPath $InputDol),
        "--build-dir", (Convert-ToWslPath $BuildDir),
        "--output", (Convert-ToWslPath $OutputDol),
        "--backup", (Convert-ToWslPath $BackupDol),
        "--expected-sha256", $ExpectedDolSha256
    )
    if ($DryRun) { $WslArguments += "--dry-run" }
    Invoke-Checked $Wsl.Source $WslArguments
    return
}
$Cc = Resolve-DevkitTool "powerpc-eabi-gcc"
$Objcopy = Resolve-DevkitTool "powerpc-eabi-objcopy"
$Nm = Resolve-DevkitTool "powerpc-eabi-nm"

$PythonCommand = Get-Command python -ErrorAction SilentlyContinue
$PythonPrefix = @()
if (-not $PythonCommand) {
    $PythonCommand = Get-Command py -ErrorAction SilentlyContinue
    if (-not $PythonCommand) {
        throw "Python 3 was not found in PATH."
    }
    $PythonPrefix = @("-3")
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$ObjectRoot = Join-Path $BuildDir "obj"
New-Item -ItemType Directory -Force -Path $ObjectRoot | Out-Null

$RootMain = Get-Item -LiteralPath (Join-Path $RepoRoot "main.cpp")
$GameCubeSources = @(
    Get-ChildItem -LiteralPath $SourceRoot -Recurse -File |
        Where-Object { $_.Extension -cin @(".cpp", ".c", ".S") } |
        Sort-Object FullName
)
$Sources = @($RootMain) + $GameCubeSources
if ($Sources.Count -eq 0) {
    throw "No GameCube payload sources found"
}

$CommonFlags = @(
    "-mcpu=750",
    "-meabi",
    "-mbig-endian",
    "-mhard-float",
    "-msdata=none",
    "-ffreestanding",
    "-fno-pic",
    "-fno-stack-protector",
    "-fno-builtin",
    "-ffunction-sections",
    "-fdata-sections",
    "-fno-common",
    "-Os",
    "-Wall",
    "-Wextra",
    "-DUSM_GAMECUBE=1",
    "-I$SourceRoot"
)
$CxxFlags = @(
    "-std=gnu++17",
    "-fno-exceptions",
    "-fno-rtti",
    "-fno-threadsafe-statics",
    "-fno-use-cxa-atexit",
    "-fno-unwind-tables",
    "-fno-asynchronous-unwind-tables"
)

$Objects = [Collections.Generic.List[string]]::new()
foreach ($Source in $Sources) {
    if ($Source.FullName.Equals($RootMain.FullName, [StringComparison]::OrdinalIgnoreCase)) {
        $Relative = "main.cpp"
        $ObjectRelative = "root\main.o"
    } else {
        $GameCubeRelative = $Source.FullName.Substring($SourceRoot.Length + 1)
        $Relative = Join-Path "gamecube" $GameCubeRelative
        $ObjectRelative = Join-Path "gamecube" ([IO.Path]::ChangeExtension($GameCubeRelative, ".o"))
    }
    $ObjectPath = Join-Path $ObjectRoot $ObjectRelative
    $ObjectDirectory = Split-Path -Parent $ObjectPath
    New-Item -ItemType Directory -Force -Path $ObjectDirectory | Out-Null

    Write-Host "CXX/CC $Relative"
    if ($Source.Extension -ceq ".cpp") {
        $Arguments = @($CommonFlags + $CxxFlags + @("-c", $Source.FullName, "-o", $ObjectPath))
        Invoke-Checked $Cxx $Arguments
    } else {
        $LanguageFlags = if ($Source.Extension -ceq ".c") { @("-std=gnu11") } else { @() }
        $Arguments = @($CommonFlags + $LanguageFlags + @("-c", $Source.FullName, "-o", $ObjectPath))
        Invoke-Checked $Cc $Arguments
    }
    $Objects.Add($ObjectPath)
}

$ElfPath = Join-Path $BuildDir "usm_debug_menu.elf"
$BinaryPath = Join-Path $BuildDir "usm_debug_menu.bin"
$MapPath = Join-Path $BuildDir "usm_debug_menu.map"
$SymbolsPath = Join-Path $BuildDir "usm_debug_menu.symbols.txt"

$LinkArguments = @(
    "-mcpu=750",
    "-meabi",
    "-mbig-endian",
    "-mhard-float",
    "-msdata=none",
    "-nostdlib",
    "-Wl,-T,$LinkerScript",
    "-Wl,-Map,$MapPath",
    "-Wl,--gc-sections",
    "-Wl,--build-id=none",
    "-Wl,--no-undefined",
    "-Wl,--undefined=usm_gc_arena_lo_wrapper",
    "-Wl,--undefined=usm_debug_menu_hook",
    "-Wl,--undefined=usm_debug_menu_update_hook",
    "-Wl,--undefined=usm_debug_menu_pad_read_hook"
)
$LinkArguments += $Objects
$LinkArguments += @("-lgcc", "-o", $ElfPath)

Write-Host "LD  $ElfPath"
Invoke-Checked $Cxx $LinkArguments
Invoke-Checked $Objcopy @("-O", "binary", $ElfPath, $BinaryPath)

$SymbolLines = @(& $Nm -n --defined-only $ElfPath)
if ($LASTEXITCODE -ne 0) {
    throw "$Nm failed with exit code $LASTEXITCODE"
}
[IO.File]::WriteAllLines(
    $SymbolsPath,
    [string[]]$SymbolLines,
    [Text.ASCIIEncoding]::new()
)

$InjectorArguments = @(
    $Injector,
    "--dol", $InputDol,
    "--payload", $ElfPath,
    "--symbols", $SymbolsPath,
    "--output", $OutputDol,
    "--backup", $BackupDol,
    "--expected-sha256", $ExpectedDolSha256,
    "--load-address", "0x80636040",
    "--hook-symbol", "usm_debug_menu_hook",
    "--update-hook-symbol", "usm_debug_menu_update_hook",
    "--pad-hook-symbol", "usm_debug_menu_pad_read_hook",
    "--arena-symbol", "usm_gc_arena_lo_wrapper",
    "--end-symbol", "__payload_end"
)
if ($DryRun) {
    $InjectorArguments += "--dry-run"
}

Write-Host "INJECT $OutputDol"
& $PythonCommand.Source @PythonPrefix @InjectorArguments
if ($LASTEXITCODE -ne 0) {
    throw "GameCube payload injection failed with exit code $LASTEXITCODE"
}

Write-Host "ELF:     $ElfPath"
Write-Host "Binary:  $BinaryPath"
Write-Host "Symbols: $SymbolsPath"
if (-not $DryRun) {
    Write-Host "Patched: $OutputDol"
    Write-Host "Backup:  $BackupDol"
}
