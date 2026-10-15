[CmdletBinding()]
param([Parameter(Mandatory)][string]$Python)

# Makes the game's menu text in each language under local\language-banks\<language>\MainUS.bin, from the retail
# English bank of the player's disc (kept first as en-US). The launcher installs the chosen one before each run.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$englishBank = Join-Path $projectRoot 'local\language-banks\en-US\MainUS.bin'
$ptBrSource = Join-Path $projectRoot 'translations\pt-BR\MainUS.json'
$gameCatalogDir = Join-Path $projectRoot 'localization\game'

. (Join-Path $PSScriptRoot 'prepare_english_bank.ps1')
Initialize-EnglishLanguageBank -InstalledBank (Join-Path $projectRoot 'game\language\strings\MainUS.bin') -EnglishBank $englishBank
if (-not (Test-Path -LiteralPath $englishBank)) {
    Write-Output 'Skipped the menu languages: provide local\language-banks\en-US\MainUS.bin to make them.'
    return
}
# The banks are small: made again every time, so a fix to a builder always reaches them
& $Python (Join-Path $PSScriptRoot 'build_pt_br_bank.py') $englishBank $ptBrSource (Join-Path $projectRoot 'local\language-banks\pt-BR\MainUS.bin')
if ($LASTEXITCODE -ne 0) { throw 'Could not make the Brazilian Portuguese menu text.' }
foreach ($locale in @('es', 'fr', 'de', 'it')) {
    $bank = Join-Path $projectRoot ('local\language-banks\' + $locale + '\MainUS.bin')
    & $Python (Join-Path $PSScriptRoot 'build_mainus.py') $englishBank (Join-Path $gameCatalogDir ($locale + '.jsonl')) $bank
    if ($LASTEXITCODE -ne 0) { throw "Could not make the $locale menu text." }
}
