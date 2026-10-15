[CmdletBinding()]
param([string]$OutputDirectory)

# Builds the launcher (launcher/, C++ on Walnut) and puts "BLACK Launcher.exe" in the game folder, or in
# -OutputDirectory. Needs CMake and Visual Studio 2022 with the C++ workload; nothing else.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $projectRoot 'launcher'
if (-not $OutputDirectory) { $OutputDirectory = $projectRoot }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

if (-not (Test-Path -LiteralPath (Join-Path $launcher 'CMakeLists.txt'))) {
    # A release archive has the launcher already built, and not its sources
    if (Test-Path -LiteralPath (Join-Path $OutputDirectory 'BLACK Launcher.exe')) { return }
    throw 'The launcher''s sources are missing, and so is BLACK Launcher.exe.'
}
if (-not (Test-Path -LiteralPath (Join-Path $launcher 'vendor\walnut\Walnut')) -and
    (Test-Path -LiteralPath (Join-Path $projectRoot '.git'))) {
    git -C $projectRoot submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw 'Could not fetch the launcher''s submodules.' }
}
Push-Location $launcher
try {
    cmake --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw 'Could not configure the launcher.' }
    cmake --build --preset windows-release --target black_launcher
    if ($LASTEXITCODE -ne 0) { throw 'Could not build the launcher.' }
} finally {
    Pop-Location
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$built = Join-Path $projectRoot 'build-launcher\bin\Release\BLACK Launcher.exe'
Copy-Item -LiteralPath $built -Destination (Join-Path $OutputDirectory 'BLACK Launcher.exe') -Force
Write-Output "Built $(Join-Path $OutputDirectory 'BLACK Launcher.exe')"
