[CmdletBinding()]
param([switch]$SkipBuild)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolkit = Join-Path $projectRoot 'third_party\xboxrecomp'
$python = Join-Path $projectRoot '.venv\Scripts\python.exe'
$dsp56300 = Join-Path $projectRoot 'third_party\dsp56300'
$xbe = Join-Path $projectRoot 'game\default.xbe'
$expectedXbe = 'DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6'

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Program @Arguments
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) { throw "$Program failed with exit code $exitCode" }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git must be installed and available on PATH.' }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake 3.21 or later must be installed and available on PATH.' }

# The launcher comes first: it is where the disc is set up
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-launcher.ps1')
    if (-not $?) { throw 'The launcher could not be built.' }
}
if (-not (Test-Path -LiteralPath $xbe -PathType Leaf)) {
    Write-Output "Next: open $(Join-Path $projectRoot 'BLACK Launcher.exe'), choose your disc, then run Build BLACK again."
    return
}
if ((Get-FileHash -LiteralPath $xbe -Algorithm SHA256).Hash -ne $expectedXbe) {
    throw 'default.xbe does not match the supported retail Xbox release; see README.md.'
}

# XboxRecomp (PauloVx's fork, branch black) and the DSP56300 core are submodules at the commits this
# checkout records. Only missing ones are fetched, so a checked-out runtime branch with new commits is left alone.
# A release archive without .git ships them already in place.
$required = @((Join-Path $toolkit 'CMakeLists.txt'), (Join-Path $dsp56300 'crates\emu-ffi\Cargo.toml'))
if (($required | Where-Object { -not (Test-Path -LiteralPath $_) }) -and (Test-Path -LiteralPath (Join-Path $projectRoot '.git'))) {
    Invoke-Checked git @('-C', $projectRoot, 'submodule', 'update', '--init', '--recursive')
}
foreach ($file in $required) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "$file is missing. Clone with --recursive, or run: git submodule update --init --recursive"
    }
}
if (-not (Get-Command cargo -ErrorAction SilentlyContinue) -or
    -not (Get-Command rustup -ErrorAction SilentlyContinue)) {
    throw 'Rustup and Cargo are required to build the DSP core. Install the x86_64-pc-windows-msvc toolchain and rerun setup.'
}

if (-not (Test-Path -LiteralPath $python)) {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        Invoke-Checked py @('-3', '-m', 'venv', (Join-Path $projectRoot '.venv'))
    } elseif (Get-Command python -ErrorAction SilentlyContinue) {
        Invoke-Checked python @('-m', 'venv', (Join-Path $projectRoot '.venv'))
    } else {
        throw 'Python 3.10 or later must be installed and available as py or python.'
    }
}
Invoke-Checked $python @('-m', 'pip', 'install', 'capstone==5.0.7')
$env:PYTHONUTF8 = '1'
Invoke-Checked $python @((Join-Path $PSScriptRoot 'generate.py'))

if (-not $SkipBuild) {
    $build = Join-Path $projectRoot 'build-vs2022'
    Invoke-Checked cmake @('-S', $projectRoot, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64')
    Invoke-Checked cmake @('--build', $build, '--config', 'Release', '--target', 'black_xbox_recomp')
}
& (Join-Path $PSScriptRoot 'localization\build-language-banks.ps1') -Python $python
Write-Output "Ready: $(Join-Path $projectRoot 'BLACK Launcher.exe')"

