[CmdletBinding()]
param([switch]$Force)

# Fetches what the game needs of the NVIDIA DLSS SDK (headers, the import library and the runtime, nvngx_dlss.dll)
# into third_party\dlss, from NVIDIA's own repository at a pinned release. The next build of the game links DLSS
# and copies the runtime next to it; without these files DLAA and the DLSS modes fall back to FXAA. The SDK is
# NVIDIA's (LICENSE.txt, fetched with it) and is never committed here.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$target = Join-Path $projectRoot 'third_party\dlss'
$release = 'v310.9.1'
$files = [ordered]@{
    'include/nvsdk_ngx.h' = 40289
    'include/nvsdk_ngx_defs.h' = 44313
    'include/nvsdk_ngx_params.h' = 7744
    'include/nvsdk_ngx_helpers.h' = 5317
    'include/nvsdk_ngx_helpers_d3d.h' = 31976
    'include/nvsdk_ngx_loader.h' = 13252
    'include/nvsdk_ngx_defs_dlssd.h' = 14693
    'include/nvsdk_ngx_helpers_dlssd.h' = 732
    'include/nvsdk_ngx_params_dlssd.h' = 1199
    'lib/Windows_x86_64/x64/nvsdk_ngx_d.lib' = 4090730
    'lib/Windows_x86_64/rel/nvngx_dlss.dll' = 58956912
    'LICENSE.txt' = 26620
}

$present = $true
foreach ($file in $files.Keys) {
    $path = Join-Path $target ($file -replace '/', '\')
    if (-not (Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -ne $files[$file]) { $present = $false }
}
if ($present -and -not $Force) { Write-Output "DLSS SDK $release already in $target"; return }

$ProgressPreference = 'SilentlyContinue'
foreach ($file in $files.Keys) {
    $path = Join-Path $target ($file -replace '/', '\')
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/NVIDIA/DLSS/$release/$file" -OutFile $path
    if ((Get-Item -LiteralPath $path).Length -ne $files[$file]) { throw "DLSS SDK: $file is not the expected size" }
}
$signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $target 'lib\Windows_x86_64\rel\nvngx_dlss.dll')
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=NVIDIA Corporation') {
    throw 'DLSS SDK: nvngx_dlss.dll is not signed by NVIDIA'
}
Write-Output "DLSS SDK $release in $target"
