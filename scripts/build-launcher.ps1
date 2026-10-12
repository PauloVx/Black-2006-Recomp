[CmdletBinding()]
param([switch]$Launch, [switch]$Force)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $PSScriptRoot 'launcher.cs'
$languageSource = Join-Path $PSScriptRoot 'game-language.cs'
$uiLanguageSource = Join-Path $PSScriptRoot 'launcher-language.cs'
$output = Join-Path $projectRoot 'BLACK PC Launcher.exe'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework 4 compiler is missing.' }
if (-not (Test-Path -LiteralPath $languageSource)) { throw 'The launcher language-bank source is missing.' }
if (-not (Test-Path -LiteralPath $uiLanguageSource)) { throw 'The launcher UI language source is missing.' }
if ($Force -or -not (Test-Path -LiteralPath $output) -or
    (Get-Item -LiteralPath $source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc -or
    (Get-Item -LiteralPath $languageSource).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc -or
    (Get-Item -LiteralPath $uiLanguageSource).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc) {
    & $compiler /nologo /target:winexe /platform:x64 /optimize+ `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll `
        /reference:System.Web.Extensions.dll "/out:$output" $languageSource $uiLanguageSource $source
    if ($LASTEXITCODE -ne 0) { throw "Launcher compilation failed (exit $LASTEXITCODE)." }
    Write-Output "Built $output"
}
if ($Launch) {
    Start-Process -FilePath $output -WorkingDirectory $projectRoot
}
