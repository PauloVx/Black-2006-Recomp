function Initialize-EnglishLanguageBank {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$InstalledBank,
        [Parameter(Mandatory = $true)][string]$EnglishBank,
        [string]$ExpectedSha256 = 'B23EEB4AD86A72894B40E7239D0EA8842E5A6BC05936A936401B608070E8C7BB'
    )

    if (Test-Path -LiteralPath $EnglishBank -PathType Leaf) { return }
    if (-not (Test-Path -LiteralPath $InstalledBank -PathType Leaf)) { return }

    $null = New-Item -ItemType Directory -Path (Split-Path -Parent $EnglishBank) -Force
    $temporary = $EnglishBank + '.launcher-tmp-' + [Guid]::NewGuid().ToString('N')
    try {
        $inputBank = [IO.File]::OpenRead($InstalledBank)
        try {
            $outputBank = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
            try { $inputBank.CopyTo($outputBank) } finally { $outputBank.Dispose() }
        } finally { $inputBank.Dispose() }
        $hash = [Security.Cryptography.SHA256]::Create()
        try {
            $copiedBank = [IO.File]::OpenRead($temporary)
            try { $actualSha256 = [BitConverter]::ToString($hash.ComputeHash($copiedBank)).Replace('-', '') }
            finally { $copiedBank.Dispose() }
        } finally { $hash.Dispose() }
        if ($actualSha256 -ne $ExpectedSha256) {
            throw 'The installed MainUS.bin is not the recognized original English bank. Restore the original English MainUS.bin under local\language-banks\en-US\ before building language banks.'
        }
        # File.Move publishes a complete fallback without replacing one that
        # another build or launcher may have preserved in the meantime.
        try { [IO.File]::Move($temporary, $EnglishBank) }
        catch [IO.IOException] { if (-not [IO.File]::Exists($EnglishBank)) { throw } }
    } finally {
        if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
    }
}
