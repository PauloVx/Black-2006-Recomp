[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\build-direct-launcher'),
    [string]$LauncherDirectory,
    [string]$OwnedGameRoot,
    [switch]$LegacyLauncher
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$scripts = Join-Path $projectRoot 'scripts'
$testOutput = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Path $testOutput -Force | Out-Null
if (-not $LauncherDirectory) { $LauncherDirectory = $projectRoot }
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$executable = Join-Path $testOutput 'direct-launcher-test.exe'
$catalog = Join-Path $scripts 'localization\launcher-locales.json'
& $compiler /nologo /target:exe /platform:x64 /optimize+ /main:BlackXboxLauncher.LauncherDirectTests `
    /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
    "/resource:$catalog,BlackXboxLauncher.launcher-locales.json" "/out:$executable" `
    (Join-Path $scripts 'launcher.cs') (Join-Path $scripts 'save-profile-tab.cs') (Join-Path $scripts 'profile_editor.cs') `
    (Join-Path $scripts 'game-language.cs') (Join-Path $scripts 'launcher-language.cs') `
    (Join-Path $PSScriptRoot 'test_direct_launch.cs')
if ($LASTEXITCODE -ne 0) { throw 'Direct-launcher fixture compilation failed.' }
$testArguments = @($testOutput, [IO.Path]::GetFullPath($LauncherDirectory))
if ($OwnedGameRoot) { $testArguments += [IO.Path]::GetFullPath($OwnedGameRoot) }
if ($LegacyLauncher) {
    if (-not $OwnedGameRoot) { throw 'The legacy executable fixture requires an owned game root.' }
    $testArguments += '--legacy'
}
& $executable @testArguments
if ($LASTEXITCODE -ne 0) { throw 'Direct-launcher fixture failed.' }
# Fixtures contain ordinary copied files and are retained for review.
