$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$openBackend = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendOpen.cpp'
$source = Get-Content -Raw $openBackend
$failures = @()

foreach ($required in @('ma_sound_init_from_file', 'ma_sound_set_end_callback', 'OpenStreamEndCallback', 'pStream', 'PlayStream')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp is missing '$required'."
    }
}

if ($source -notmatch 'ma_sound_init_from_file\([^\r\n]+pOpenStream->szFileName\.c_str\(\)') {
    $failures += 'PlayStream does not initialize a miniaudio sound from the stream file path.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX open backend stream support is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX open backend stream support is in place.'
