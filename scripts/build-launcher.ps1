[CmdletBinding()]
param([switch]$Launch, [switch]$Force)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $PSScriptRoot 'launcher.cs'
$languageSource = Join-Path $PSScriptRoot 'game-language.cs'
$uiLanguageSource = Join-Path $PSScriptRoot 'launcher-language.cs'
$localeCatalog = Join-Path $PSScriptRoot 'localization\launcher-locales.json'
$launcherKeys = Join-Path $projectRoot 'localization\launcher\en-pt-BR.jsonl'
$launcherCatalogDir = Join-Path $projectRoot 'localization\launcher'
$catalogBuilder = Join-Path $PSScriptRoot 'localization\build_launcher_catalog.py'
$bankBuilder = Join-Path $PSScriptRoot 'localization\build_mainus.py'
$bankBootstrap = Join-Path $PSScriptRoot 'localization\prepare_english_bank.ps1'
$englishBank = Join-Path $projectRoot 'local\language-banks\en-US\MainUS.bin'
$gameCatalogDir = Join-Path $projectRoot 'localization\game'
$output = Join-Path $projectRoot 'BLACK PC Launcher.exe'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework 4 compiler is missing.' }
if (-not (Test-Path -LiteralPath $languageSource)) { throw 'The launcher language-bank source is missing.' }
if (-not (Test-Path -LiteralPath $uiLanguageSource)) { throw 'The launcher UI language source is missing.' }
if (-not (Test-Path -LiteralPath $localeCatalog)) { throw 'The generated launcher locale catalog is missing.' }

$python = $null
$pythonArgs = @()
$pythonLauncher = Get-Command py -ErrorAction SilentlyContinue
if ($pythonLauncher) {
    $pythonVersion = & $pythonLauncher.Source -3 --version 2>&1
    if ($LASTEXITCODE -eq 0 -and ($pythonVersion -join ' ') -match 'Python 3\.') {
        $python = $pythonLauncher
        $pythonArgs = @('-3')
    }
}
if (-not $python) {
    $pythonCandidate = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCandidate) {
        $pythonVersion = & $pythonCandidate.Source --version 2>&1
        if ($LASTEXITCODE -eq 0 -and ($pythonVersion -join ' ') -match 'Python 3\.') { $python = $pythonCandidate }
    }
}
if ($python) {
    if ((Test-Path -LiteralPath $launcherKeys) -and (Test-Path -LiteralPath $catalogBuilder)) {
        & $python.Source @pythonArgs $catalogBuilder $launcherKeys $launcherCatalogDir $localeCatalog
        if ($LASTEXITCODE -ne 0) { throw 'Could not generate launcher locale catalogs.' }
    }
    # Setup builds the launcher before its first run. Preserve the retail source
    # now so newly offered locales can be generated on that first setup too.
    . $bankBootstrap
    Initialize-EnglishLanguageBank -InstalledBank (Join-Path $projectRoot 'game\language\strings\MainUS.bin') -EnglishBank $englishBank
    if (Test-Path -LiteralPath $englishBank) {
        foreach ($locale in @('es', 'fr', 'de', 'it')) {
            $catalog = Join-Path $gameCatalogDir ($locale + '.jsonl')
            $bank = Join-Path $projectRoot ('local\language-banks\' + $locale + '\MainUS.bin')
            if (-not (Test-Path -LiteralPath $catalog)) { throw "The $locale game catalog is missing." }
            if ($Force -or -not (Test-Path -LiteralPath $bank) -or
                (Get-Item -LiteralPath $englishBank).LastWriteTimeUtc -gt (Get-Item -LiteralPath $bank).LastWriteTimeUtc -or
                (Get-Item -LiteralPath $catalog).LastWriteTimeUtc -gt (Get-Item -LiteralPath $bank).LastWriteTimeUtc) {
                & $python.Source @pythonArgs $bankBuilder $englishBank $catalog $bank
                if ($LASTEXITCODE -ne 0) { throw "Could not generate the $locale MainUS bank." }
            }
        }
    } else {
        Write-Output 'Skipped generated game-language banks: provide local\language-banks\en-US\MainUS.bin to build them.'
    }
} else {
    Write-Output 'Python is unavailable; using the checked-in launcher catalog and skipping generated game-language banks.'
}

if ($Force -or -not (Test-Path -LiteralPath $output) -or
    (Get-Item -LiteralPath $source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc -or
    (Get-Item -LiteralPath $languageSource).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc -or
    (Get-Item -LiteralPath $uiLanguageSource).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc -or
    (Get-Item -LiteralPath $localeCatalog).LastWriteTimeUtc -gt (Get-Item -LiteralPath $output).LastWriteTimeUtc) {
    & $compiler /nologo /target:winexe /platform:x64 /optimize+ `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll `
        /reference:System.Web.Extensions.dll "/resource:$localeCatalog,BlackXboxLauncher.launcher-locales.json" "/out:$output" $languageSource $uiLanguageSource $source
    if ($LASTEXITCODE -ne 0) { throw "Launcher compilation failed (exit $LASTEXITCODE)." }
    Write-Output "Built $output"
}
if ($Launch) {
    Start-Process -FilePath $output -WorkingDirectory $projectRoot
}
