[CmdletBinding()]
param(
    [string]$LauncherSource = (Join-Path $PSScriptRoot '..\..\scripts\launcher.cs'),
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\build-ambient-occlusion-launcher')
)
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework 4 compiler is missing.' }
$testOutput = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Path $testOutput -Force | Out-Null
$executable = Join-Path $testOutput 'ao-launcher-test.exe'
$launcherDirectory = Split-Path -Parent ([IO.Path]::GetFullPath($LauncherSource))
$languageTests = Join-Path $PSScriptRoot '..\launcher_language'
$localeCatalog = Join-Path $PSScriptRoot '..\..\scripts\localization\launcher-locales.json'
& $compiler /nologo /target:exe /platform:x64 /optimize+ `
    /main:BlackXboxLauncher.AmbientOcclusionTests `
    /reference:System.Windows.Forms.dll /reference:System.Drawing.dll `
    /reference:System.Web.Extensions.dll "/resource:$localeCatalog,BlackXboxLauncher.launcher-locales.json" "/out:$executable" `
    ([IO.Path]::GetFullPath($LauncherSource)) `
    (Join-Path $launcherDirectory 'game-language.cs') `
    (Join-Path $launcherDirectory 'launcher-language.cs') `
    (Join-Path $PSScriptRoot 'test_launcher.cs') `
    (Join-Path $languageTests 'test_banks.cs') `
    (Join-Path $languageTests 'test_controls.cs')
if ($LASTEXITCODE -ne 0) { throw "Launcher test compilation failed (exit $LASTEXITCODE)." }
& $executable $testOutput
if ($LASTEXITCODE -ne 0) { throw "Launcher tests failed (exit $LASTEXITCODE)." }
