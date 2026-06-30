$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$miniaudio = Join-Path $repoRoot 'Sources/sdk/miniaudio/miniaudio.h'
$openBackend = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendOpen.cpp'
$failures = @()

if (-not (Test-Path $miniaudio)) {
    $failures += 'Sources/sdk/miniaudio/miniaudio.h is missing.'
}

$source = Get-Content -Raw $openBackend
foreach ($required in @('MINIAUDIO_IMPLEMENTATION', 'ma_engine', 'ma_sound', 'ma_audio_buffer', 'ma_sound_start', 'ma_sound_stop')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp is missing '$required'."
    }
}

if ($source -match 'int PlaySample\([^\)]*\)\s*\{[^}]*return -1;') {
    $failures += 'PlaySample still always returns -1.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX open backend miniaudio integration is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX open backend miniaudio integration is in place.'
