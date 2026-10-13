[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\build-save-profile'),
    [string]$ProfileEditorSource = (Join-Path $PSScriptRoot '..\..\scripts\profile_editor.cs'),
    [string]$SaveTabSource = (Join-Path $PSScriptRoot '..\..\scripts\save-profile-tab.cs')
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$scripts = Join-Path $projectRoot 'scripts'
$testOutput = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Path $testOutput -Force | Out-Null
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework 4 compiler is missing.' }
$profileSource = [IO.Path]::GetFullPath($ProfileEditorSource)
$saveSource = [IO.Path]::GetFullPath($SaveTabSource)
$fixtureSource = Join-Path $PSScriptRoot 'ProfileTestData.cs'
$localeCatalog = Join-Path $scripts 'localization\launcher-locales.json'
$documentExecutable = Join-Path $testOutput 'save-profile-document-test.exe'
& $compiler /nologo /target:exe /platform:x64 /optimize+ /main:BlackXboxLauncher.SaveProfileDocumentTests `
    /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
    "/resource:$localeCatalog,BlackXboxLauncher.launcher-locales.json" "/out:$documentExecutable" `
    (Join-Path $scripts 'launcher.cs') $saveSource $profileSource `
    (Join-Path $scripts 'game-language.cs') (Join-Path $scripts 'launcher-language.cs') `
    $fixtureSource (Join-Path $PSScriptRoot 'test_document.cs')
if ($LASTEXITCODE -ne 0) { throw 'Save Profile document fixture compilation failed.' }
& $documentExecutable $testOutput
if ($LASTEXITCODE -ne 0) { throw 'Save Profile document tests failed.' }

$uiExecutable = Join-Path $testOutput 'save-profile-ui-test.exe'
& $compiler /nologo /target:exe /platform:x64 /optimize+ /main:BlackXboxLauncher.SaveProfileUiTests `
    /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
    "/resource:$localeCatalog,BlackXboxLauncher.launcher-locales.json" "/out:$uiExecutable" `
    (Join-Path $scripts 'launcher.cs') $saveSource $profileSource `
    (Join-Path $scripts 'game-language.cs') (Join-Path $scripts 'launcher-language.cs') `
    $fixtureSource (Join-Path $PSScriptRoot 'test_ui.cs')
if ($LASTEXITCODE -ne 0) { throw 'Save Profile UI fixture compilation failed.' }
& $uiExecutable $testOutput
if ($LASTEXITCODE -ne 0) { throw 'Save Profile UI tests failed.' }
# Synthetic fixtures and logs are retained for review; no recursive cleanup.
