[CmdletBinding()]
param([switch]$FromScratch)

# Builds BLACK Launcher, then has it build the game (BLACK Launcher --build): the same steps as the launcher's
# Build the game page, which players use instead. -FromScratch translates the code from the disc again.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $projectRoot 'BLACK Launcher.exe'

& (Join-Path $PSScriptRoot 'build-launcher.ps1')
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'game\default.xbe') -PathType Leaf)) {
    Write-Output "Next: open $launcher and choose your disc; it builds the game from there too."
    return
}
$arguments = @('--build')
if ($FromScratch) { $arguments += '--from-scratch' }
# Piped, so PowerShell waits for the (windowed) launcher and shows what it prints
& $launcher @arguments | ForEach-Object { $_ }
if ($LASTEXITCODE -ne 0) { throw 'The game could not be built: see above, and the build log in reports\.' }
Write-Output "Ready: $launcher"
